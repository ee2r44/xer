// Validate vector hot-path strategies vs current xer-style baseline.
// Does not modify include/; mirrors candidate changes in-test.
//
// Strategies:
//   S1 unfortified_memcpy - __builtin_memcpy vs std::memcpy (FORTIFY)
//   S2 ptr_bump_layout    - begin/end/end_cap vs ptr/size/capacity
//   S3 combined           - S1 + S2
//   S4 realloc_grow       - realloc in place for trivial u64 (index layout)
//
// Verdict FASTER only if median gain > max(3% of old, 1.5*IQR).
//
// Build (optimized):
//   MSVC:  cl /std:c++latest /O2 /DNDEBUG /EHsc /Iinclude /Fe:tests\container\xer_vector_strategy_perf_msvc.exe tests\container\xer_vector_strategy_perf.cpp
//   GCC:   g++ -std=c++23 -O3 -DNDEBUG -pthread -Iinclude -o tests/container/xer_vector_strategy_perf_gcc tests/container/xer_vector_strategy_perf.cpp
//   Clang: clang++ -std=c++23 -O3 -DNDEBUG -pthread -Iinclude -o tests/container/xer_vector_strategy_perf_clang tests/container/xer_vector_strategy_perf.cpp

#if defined(_MSC_VER)
#  if defined(_DEBUG) || !defined(NDEBUG)
#    error "Build this perf test with /O2 /DNDEBUG"
#  endif
#elif !defined(__OPTIMIZE__) || !defined(NDEBUG)
#  error "Build this perf test with -O3 -DNDEBUG"
#endif

#include "xer/algorithm/xer_max.h"
#include "xer/macro/xer_macro.h"
#include "xer/memory/xer_raw_alloc.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#if defined(_WIN32)
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#else
#  include <pthread.h>
#  include <sched.h>
#endif

namespace {

volatile std::uint64_t g_sink = 0;

XER_NOINLINE void escape_u64(std::uint64_t v) noexcept {
#if defined(__GNUC__) || defined(__clang__)
  asm volatile("" : "+r"(v) : : "memory");
#endif
  g_sink += v;
}

void pin_thread() noexcept {
#if defined(_WIN32)
  const DWORD cpu = GetCurrentProcessorNumber();
  SetThreadAffinityMask(GetCurrentThread(), DWORD_PTR{1} << (cpu & 63));
  SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
  std::printf("pin cpu=%lu\n", static_cast<unsigned long>(cpu));
#else
  const int cpu = sched_getcpu();
  cpu_set_t set;
  CPU_ZERO(&set);
  CPU_SET(cpu < 0 ? 0 : static_cast<std::size_t>(cpu), &set);
  pthread_setaffinity_np(pthread_self(), sizeof(set), &set);
  std::printf("pin cpu=%d\n", cpu);
#endif
}

XER_NOINLINE void relocate_old(void* dst, const void* src, std::size_t bytes) noexcept {
  if (bytes == 0) return;
  std::memcpy(dst, src, bytes);
}

XER_NOINLINE void relocate_s1(void* dst, const void* src, std::size_t bytes) noexcept {
  if (bytes == 0) return;
#if defined(__GNUC__) || defined(__clang__)
  __builtin_memcpy(dst, src, bytes);
#else
  std::memcpy(dst, src, bytes);
#endif
}

enum class Reloc { Old, S1 };
enum class Layout { Index, PtrBump };
enum class Grow { AllocCopy, Realloc };

template <Reloc R, Layout L, Grow G = Grow::AllocCopy>
struct MiniVec {
  std::uint64_t* ptr_ = nullptr;
  std::size_t size_ = 0;
  std::size_t capacity_ = 0;
  std::uint64_t* begin_ = nullptr;
  std::uint64_t* end_ = nullptr;
  std::uint64_t* end_cap_ = nullptr;

  MiniVec() = default;
  MiniVec(const MiniVec&) = delete;
  MiniVec& operator=(const MiniVec&) = delete;

  ~MiniVec() {
    if constexpr (L == Layout::Index) xer::raw_free(ptr_);
    else xer::raw_free(begin_);
  }

  std::size_t size() const noexcept {
    if constexpr (L == Layout::Index) return size_;
    return static_cast<std::size_t>(end_ - begin_);
  }
  std::size_t capacity() const noexcept {
    if constexpr (L == Layout::Index) return capacity_;
    return static_cast<std::size_t>(end_cap_ - begin_);
  }

  void do_relocate(std::uint64_t* dst, std::uint64_t* src, std::size_t count) noexcept {
    const std::size_t bytes = count * sizeof(std::uint64_t);
    if constexpr (R == Reloc::Old) relocate_old(dst, src, bytes);
    else relocate_s1(dst, src, bytes);
  }

  bool expand_to_at_least(std::size_t new_cap) noexcept {
    if constexpr (L == Layout::Index && G == Grow::Realloc) {
      new_cap = xer::max(new_cap, capacity_ << 1);
      void* p = std::realloc(ptr_, new_cap * sizeof(std::uint64_t));
      if (p == nullptr) return false;
      ptr_ = static_cast<std::uint64_t*>(p);
      capacity_ = new_cap;
      return true;
    } else if constexpr (L == Layout::Index) {
      new_cap = xer::max(new_cap, capacity_ << 1);
      std::uint64_t* const neu = xer::raw_alloc<std::uint64_t>(new_cap);
      if (neu == nullptr) return false;
      do_relocate(neu, ptr_, size_);
      xer::raw_free(ptr_);
      ptr_ = neu;
      capacity_ = new_cap;
      return true;
    } else {
      const std::size_t old_cap = capacity();
      const std::size_t old_size = size();
      new_cap = xer::max(new_cap, old_cap << 1);
      std::uint64_t* const neu = xer::raw_alloc<std::uint64_t>(new_cap);
      if (neu == nullptr) return false;
      do_relocate(neu, begin_, old_size);
      xer::raw_free(begin_);
      begin_ = neu;
      end_ = neu + old_size;
      end_cap_ = neu + new_cap;
      return true;
    }
  }

  void grow_push(std::uint64_t x) noexcept {
    if constexpr (L == Layout::Index) {
      if (size_ == capacity_) {
        const std::size_t want = capacity_ == 0 ? 1 : capacity_ + 1;
        (void)expand_to_at_least(want);
      }
      ptr_[size_] = x;
      ++size_;
    } else {
      if (end_ == end_cap_) {
        const std::size_t cap = capacity();
        const std::size_t want = cap == 0 ? 1 : cap + 1;
        (void)expand_to_at_least(want);
      }
      *end_ = x;
      ++end_;
    }
  }

  void push_reserved(std::uint64_t x) noexcept {
    if constexpr (L == Layout::Index) {
      ptr_[size_] = x;
      ++size_;
    } else {
      *end_ = x;
      ++end_;
    }
  }

  bool reserve(std::size_t n) noexcept {
    if constexpr (L == Layout::Index) return n <= capacity_ || expand_to_at_least(n);
    return n <= capacity() || expand_to_at_least(n);
  }

  std::uint64_t checksum() const noexcept {
    std::uint64_t a = 0;
    if constexpr (L == Layout::Index) {
      for (std::size_t i = 0; i < size_; ++i) a += ptr_[i];
    } else {
      for (std::uint64_t* p = begin_; p != end_; ++p) a += *p;
    }
    return a;
  }
};

struct Stats {
  double med_ns;
  double iqr_ns;
};

template <typename F0, typename F1>
void measure2(F0&& f0, F1&& f1, Stats out[2]) {
  auto run = [&](int idx) -> double { return idx == 0 ? f0() : f1(); };
  run(0);
  run(1);
  run(1);
  run(0);
  double raw[2][9];
  for (int rep = 0; rep < 9; ++rep) {
    const int start = rep & 1;
    raw[start][rep] = run(start);
    raw[start ^ 1][rep] = run(start ^ 1);
  }
  for (int idx = 0; idx < 2; ++idx) {
    double* s = raw[idx];
    std::sort(s, s + 9);
    out[idx].med_ns = s[4];
    out[idx].iqr_ns = s[6] - s[2];
  }
}

using Clock = std::chrono::steady_clock;

template <typename F>
double ns_total(F&& f) {
  const auto t0 = Clock::now();
  f();
  const auto t1 = Clock::now();
  return std::chrono::duration<double, std::nano>(t1 - t0).count();
}

enum class Verdict { Faster, Slower, Unclear };

Verdict decide(const Stats& old_s, const Stats& new_s) {
  const double delta = old_s.med_ns - new_s.med_ns;
  const double noise =
      1.5 * (old_s.iqr_ns > new_s.iqr_ns ? old_s.iqr_ns : new_s.iqr_ns);
  const double floor_noise = old_s.med_ns * 0.03;
  const double need = noise > floor_noise ? noise : floor_noise;
  if (delta > need) return Verdict::Faster;
  if (-delta > need) return Verdict::Slower;
  return Verdict::Unclear;
}

const char* verdict_str(Verdict v) {
  switch (v) {
  case Verdict::Faster: return "FASTER";
  case Verdict::Slower: return "SLOWER";
  default: return "UNCLEAR";
  }
}

template <typename Fold, typename Fnew>
void compare(const char* name, std::uint64_t ops, Fold&& fold, Fnew&& fnew) {
  Stats s[2];
  measure2(
      [&] { return ns_total([&] { fold(); }) / static_cast<double>(ops); },
      [&] { return ns_total([&] { fnew(); }) / static_cast<double>(ops); },
      s);
  const Verdict v = decide(s[0], s[1]);
  const double pct =
      s[0].med_ns > 0.0 ? (100.0 * (s[1].med_ns - s[0].med_ns) / s[0].med_ns)
                        : 0.0;
  std::printf("%-24s %10llu  %10.4f %7.4f  %10.4f %7.4f  %+8.2f%%  %s\n", name,
              static_cast<unsigned long long>(ops), s[0].med_ns, s[0].iqr_ns,
              s[1].med_ns, s[1].iqr_ns, pct, verdict_str(v));
  std::fflush(stdout);
}

template <Reloc R, Layout L, Grow G = Grow::AllocCopy>
XER_NOINLINE std::uint64_t run_grow(std::size_t n, std::uint64_t rounds) {
  std::uint64_t acc = 0;
  for (std::uint64_t r = 0; r < rounds; ++r) {
    MiniVec<R, L, G> v;
    for (std::size_t i = 0; i < n; ++i) {
      v.grow_push(static_cast<std::uint64_t>(i) + r);
    }
    acc += v.size() + v.capacity() + v.checksum();
  }
  escape_u64(acc);
  return acc;
}

template <Reloc R, Layout L>
XER_NOINLINE std::uint64_t run_rsv(std::size_t n, std::uint64_t rounds) {
  std::uint64_t acc = 0;
  for (std::uint64_t r = 0; r < rounds; ++r) {
    MiniVec<R, L> v;
    (void)v.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
      v.push_reserved(static_cast<std::uint64_t>(i) + r);
    }
    acc += v.checksum() + r;
  }
  escape_u64(acc);
  return acc;
}

// Mimic geometric growth copies: memcpy of 1,2,4,...,n/2 elements.
XER_NOINLINE std::uint64_t run_geom_reloc_old(std::size_t n, std::uint64_t rounds) {
  auto* a = xer::raw_alloc<std::uint64_t>(n);
  auto* b = xer::raw_alloc<std::uint64_t>(n);
  for (std::size_t i = 0; i < n; ++i) a[i] = i;
  std::uint64_t acc = 0;
  for (std::uint64_t r = 0; r < rounds; ++r) {
    for (std::size_t c = 1; c < n; c <<= 1) {
      relocate_old(b, a, c * sizeof(std::uint64_t));
      acc += b[c - 1];
    }
    acc += r;
  }
  xer::raw_free(a);
  xer::raw_free(b);
  escape_u64(acc);
  return acc;
}

XER_NOINLINE std::uint64_t run_geom_reloc_s1(std::size_t n, std::uint64_t rounds) {
  auto* a = xer::raw_alloc<std::uint64_t>(n);
  auto* b = xer::raw_alloc<std::uint64_t>(n);
  for (std::size_t i = 0; i < n; ++i) a[i] = i;
  std::uint64_t acc = 0;
  for (std::uint64_t r = 0; r < rounds; ++r) {
    for (std::size_t c = 1; c < n; c <<= 1) {
      relocate_s1(b, a, c * sizeof(std::uint64_t));
      acc += b[c - 1];
    }
    acc += r;
  }
  xer::raw_free(a);
  xer::raw_free(b);
  escape_u64(acc);
  return acc;
}

} // namespace

int main() {
#if XER_COMPILER(MSVC)
  const char* compiler = "msvc";
#elif XER_COMPILER(CLANG)
  const char* compiler = "clang";
#elif XER_COMPILER(GCC)
  const char* compiler = "gcc";
#else
  const char* compiler = "unknown";
#endif
  pin_thread();
  std::printf("xer_vector strategy validation compiler=%s\n", compiler);
  std::printf("old = xer-style baseline (std::memcpy + index + alloc/copy grow)\n");
  std::printf("FASTER iff median gain > max(3%%, 1.5*IQR)\n");
  std::printf("%-24s %10s  %10s %7s  %10s %7s  %9s  %s\n", "cmp", "ops", "old",
              "iqr", "new", "iqr", "pct", "verdict");

  constexpr std::size_t N = 1 << 14;
  constexpr std::uint64_t R_GROW = 200;
  constexpr std::uint64_t R_RSV = 400;
  constexpr std::uint64_t R_GEOM = 4000;
  const std::uint64_t grow_ops = static_cast<std::uint64_t>(N) * R_GROW;
  const std::uint64_t rsv_ops = static_cast<std::uint64_t>(N) * R_RSV;
  // one "op" = one full geometric relocate ladder
  const std::uint64_t geom_ops = R_GEOM;

  compare(
      "S1_geom_reloc", geom_ops, [&] { run_geom_reloc_old(N, R_GEOM); },
      [&] { run_geom_reloc_s1(N, R_GEOM); });

  compare(
      "S1_grow_u64", grow_ops,
      [&] { run_grow<Reloc::Old, Layout::Index>(N, R_GROW); },
      [&] { run_grow<Reloc::S1, Layout::Index>(N, R_GROW); });

  compare(
      "S2_grow_u64", grow_ops,
      [&] { run_grow<Reloc::Old, Layout::Index>(N, R_GROW); },
      [&] { run_grow<Reloc::Old, Layout::PtrBump>(N, R_GROW); });

  compare(
      "S2_rsv_u64", rsv_ops,
      [&] { run_rsv<Reloc::Old, Layout::Index>(N, R_RSV); },
      [&] { run_rsv<Reloc::Old, Layout::PtrBump>(N, R_RSV); });

  compare(
      "S3_grow_u64", grow_ops,
      [&] { run_grow<Reloc::Old, Layout::Index>(N, R_GROW); },
      [&] { run_grow<Reloc::S1, Layout::PtrBump>(N, R_GROW); });

  compare(
      "S4_realloc_grow", grow_ops,
      [&] { run_grow<Reloc::Old, Layout::Index, Grow::AllocCopy>(N, R_GROW); },
      [&] { run_grow<Reloc::Old, Layout::Index, Grow::Realloc>(N, R_GROW); });

  std::printf("sink=%llu\n", static_cast<unsigned long long>(g_sink));
  std::printf("PERF_DONE\n");
  return 0;
}
