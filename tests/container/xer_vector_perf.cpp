// xer::vector vs std::vector throughput.
//
// Note: xer has no allocating push_back; growth benches call reserve(cap+1)
// then push_back_unchecked, which uses the same doubling expand as xer::reserve.
//
// Build (optimized):
//   MSVC:  cl /std:c++latest /O2 /DNDEBUG /EHsc /Iinclude /Fe:tests\container\xer_vector_perf_msvc.exe tests\container\xer_vector_perf.cpp
//   GCC:   g++ -std=c++23 -O3 -DNDEBUG -pthread -Iinclude -o tests/container/xer_vector_perf_gcc tests/container/xer_vector_perf.cpp
//   Clang: clang++ -std=c++23 -O3 -DNDEBUG -pthread -Iinclude -o tests/container/xer_vector_perf_clang tests/container/xer_vector_perf.cpp

#if defined(_MSC_VER)
#  if defined(_DEBUG) || !defined(NDEBUG)
#    error "Build this perf test with /O2 /DNDEBUG"
#  endif
#elif !defined(__OPTIMIZE__) || !defined(NDEBUG)
#  error "Build this perf test with -O3 -DNDEBUG"
#endif

#include "xer/container/xer_vector.h"
#include "xer/macro/xer_macro.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <type_traits>
#include <utility>
#include <vector>

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

  double samples[2][9];
  for (int rep = 0; rep < 9; ++rep) {
    const int start = rep & 1;
    samples[start][rep] = run(start);
    samples[start ^ 1][rep] = run(start ^ 1);
  }
  for (int idx = 0; idx < 2; ++idx) {
    double* s = samples[idx];
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

struct Fat {
  std::uint64_t a, b, c, d;
  Fat() noexcept : a(0), b(0), c(0), d(0) {}
  explicit Fat(std::uint64_t x) noexcept : a(x), b(x ^ 1), c(x ^ 2), d(x ^ 3) {}
};

struct Tracker {
  std::uint64_t v;
  Tracker() noexcept : v(0) {}
  explicit Tracker(std::uint64_t x) noexcept : v(x) {}
  Tracker(const Tracker& o) noexcept : v(o.v) {}
  Tracker(Tracker&& o) noexcept : v(o.v) { o.v = 0; }
  Tracker& operator=(const Tracker& o) noexcept {
    v = o.v;
    return *this;
  }
  Tracker& operator=(Tracker&& o) noexcept {
    v = o.v;
    o.v = 0;
    return *this;
  }
  ~Tracker() { v = 0; }
};

template <typename T>
void xer_grow_push(xer::vector<T>& v, T x) noexcept {
  if (v.size() == v.capacity()) {
    const std::size_t want = v.capacity() == 0 ? 1 : v.capacity() + 1;
    (void)v.reserve(want);
  }
  v.push_back_unchecked(static_cast<T&&>(x));
}

void report(const char* name, std::uint64_t ops, const Stats s[2]) {
  const double delta = s[0].med_ns - s[1].med_ns; // xer - std
  const double pct = s[1].med_ns > 0.0 ? (100.0 * delta / s[1].med_ns) : 0.0;
  std::printf("%-18s %10llu  %10.3f %7.3f  %10.3f %7.3f  %+10.3f %+7.2f%%\n",
              name, static_cast<unsigned long long>(ops), s[0].med_ns,
              s[0].iqr_ns, s[1].med_ns, s[1].iqr_ns, delta, pct);
  std::fflush(stdout);
}

// --- push into pre-reserved buffer ---

template <typename T>
XER_NOINLINE std::uint64_t run_xer_push_reserved(std::size_t n, std::uint64_t rounds) {
  std::uint64_t acc = 0;
  for (std::uint64_t r = 0; r < rounds; ++r) {
    xer::vector<T> v;
    (void)v.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
      v.push_back_unchecked(T{static_cast<std::uint64_t>(i) + r});
    }
    acc += static_cast<std::uint64_t>(v.size());
    if constexpr (std::is_same_v<T, int> || std::is_same_v<T, std::uint64_t>) {
      acc += static_cast<std::uint64_t>(v[n - 1]);
    } else if constexpr (std::is_same_v<T, Fat>) {
      acc += v[n - 1].a;
    } else {
      acc += v[n - 1].v;
    }
  }
  escape_u64(acc);
  return acc;
}

template <typename T>
XER_NOINLINE std::uint64_t run_std_push_reserved(std::size_t n, std::uint64_t rounds) {
  std::uint64_t acc = 0;
  for (std::uint64_t r = 0; r < rounds; ++r) {
    std::vector<T> v;
    v.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
      v.push_back(T{static_cast<std::uint64_t>(i) + r});
    }
    acc += static_cast<std::uint64_t>(v.size());
    if constexpr (std::is_same_v<T, int> || std::is_same_v<T, std::uint64_t>) {
      acc += static_cast<std::uint64_t>(v[n - 1]);
    } else if constexpr (std::is_same_v<T, Fat>) {
      acc += v[n - 1].a;
    } else {
      acc += v[n - 1].v;
    }
  }
  escape_u64(acc);
  return acc;
}

// --- geometric growth push ---

template <typename T>
XER_NOINLINE std::uint64_t run_xer_push_grow(std::size_t n, std::uint64_t rounds) {
  std::uint64_t acc = 0;
  for (std::uint64_t r = 0; r < rounds; ++r) {
    xer::vector<T> v;
    for (std::size_t i = 0; i < n; ++i) {
      xer_grow_push(v, T{static_cast<std::uint64_t>(i) + r});
    }
    acc += static_cast<std::uint64_t>(v.size()) + static_cast<std::uint64_t>(v.capacity());
  }
  escape_u64(acc);
  return acc;
}

template <typename T>
XER_NOINLINE std::uint64_t run_std_push_grow(std::size_t n, std::uint64_t rounds) {
  std::uint64_t acc = 0;
  for (std::uint64_t r = 0; r < rounds; ++r) {
    std::vector<T> v;
    for (std::size_t i = 0; i < n; ++i) {
      v.push_back(T{static_cast<std::uint64_t>(i) + r});
    }
    acc += static_cast<std::uint64_t>(v.size()) + static_cast<std::uint64_t>(v.capacity());
  }
  escape_u64(acc);
  return acc;
}

// --- sequential scan ---

template <typename T>
XER_NOINLINE std::uint64_t run_xer_scan(std::size_t n, std::uint64_t rounds) {
  xer::vector<T> v;
  (void)v.reserve(n);
  for (std::size_t i = 0; i < n; ++i) {
    v.push_back_unchecked(T{static_cast<std::uint64_t>(i)});
  }
  std::uint64_t acc = 0;
  for (std::uint64_t r = 0; r < rounds; ++r) {
    for (std::size_t i = 0; i < n; ++i) {
      if constexpr (std::is_same_v<T, int> || std::is_same_v<T, std::uint64_t>) {
        acc += static_cast<std::uint64_t>(v[i]) + r;
      } else if constexpr (std::is_same_v<T, Fat>) {
        acc += v[i].a + r;
      } else {
        acc += v[i].v + r;
      }
    }
  }
  escape_u64(acc);
  return acc;
}

template <typename T>
XER_NOINLINE std::uint64_t run_std_scan(std::size_t n, std::uint64_t rounds) {
  std::vector<T> v;
  v.reserve(n);
  for (std::size_t i = 0; i < n; ++i) {
    v.push_back(T{static_cast<std::uint64_t>(i)});
  }
  std::uint64_t acc = 0;
  for (std::uint64_t r = 0; r < rounds; ++r) {
    for (std::size_t i = 0; i < n; ++i) {
      if constexpr (std::is_same_v<T, int> || std::is_same_v<T, std::uint64_t>) {
        acc += static_cast<std::uint64_t>(v[i]) + r;
      } else if constexpr (std::is_same_v<T, Fat>) {
        acc += v[i].a + r;
      } else {
        acc += v[i].v + r;
      }
    }
  }
  escape_u64(acc);
  return acc;
}

// --- erase from front repeatedly (shifts) ---

template <typename T>
XER_NOINLINE std::uint64_t run_xer_erase_front(std::size_t n, std::uint64_t rounds) {
  std::uint64_t acc = 0;
  for (std::uint64_t r = 0; r < rounds; ++r) {
    xer::vector<T> v;
    (void)v.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
      v.push_back_unchecked(T{static_cast<std::uint64_t>(i) + r});
    }
    while (!v.is_empty()) {
      (void)v.erase(0);
    }
    acc += r + 1;
  }
  escape_u64(acc);
  return acc;
}

template <typename T>
XER_NOINLINE std::uint64_t run_std_erase_front(std::size_t n, std::uint64_t rounds) {
  std::uint64_t acc = 0;
  for (std::uint64_t r = 0; r < rounds; ++r) {
    std::vector<T> v;
    v.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
      v.push_back(T{static_cast<std::uint64_t>(i) + r});
    }
    while (!v.empty()) {
      v.erase(v.begin());
    }
    acc += r + 1;
  }
  escape_u64(acc);
  return acc;
}

// --- unordered_erase vs swap-and-pop style ---

template <typename T>
XER_NOINLINE std::uint64_t run_xer_unordered_erase(std::size_t n, std::uint64_t rounds) {
  std::uint64_t acc = 0;
  for (std::uint64_t r = 0; r < rounds; ++r) {
    xer::vector<T> v;
    (void)v.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
      v.push_back_unchecked(T{static_cast<std::uint64_t>(i) + r});
    }
    while (v.size() > 1) {
      (void)v.unordered_erase(0);
    }
    if (!v.is_empty()) {
      (void)v.erase_last();
    }
    acc += r + 1;
  }
  escape_u64(acc);
  return acc;
}

template <typename T>
XER_NOINLINE std::uint64_t run_std_swap_pop(std::size_t n, std::uint64_t rounds) {
  std::uint64_t acc = 0;
  for (std::uint64_t r = 0; r < rounds; ++r) {
    std::vector<T> v;
    v.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
      v.push_back(T{static_cast<std::uint64_t>(i) + r});
    }
    while (v.size() > 1) {
      v.front() = std::move(v.back());
      v.pop_back();
    }
    if (!v.empty()) {
      v.pop_back();
    }
    acc += r + 1;
  }
  escape_u64(acc);
  return acc;
}

template <typename Fx, typename Fs>
void bench(const char* name, std::uint64_t ops, Fx&& fx, Fs&& fs) {
  Stats s[2];
  measure2(
      [&] { return ns_total([&] { fx(); }) / static_cast<double>(ops); },
      [&] { return ns_total([&] { fs(); }) / static_cast<double>(ops); },
      s);
  report(name, ops, s);
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
  std::printf("xer_vector vs std::vector perf compiler=%s\n", compiler);
  std::printf("ns/op median of 9 rotated samples; iqr = p75-p25\n");
  std::printf("delta = xer - std (negative => xer faster)\n");
  std::printf("%-18s %10s  %10s %7s  %10s %7s  %10s %8s\n", "bench", "ops",
              "xer", "iqr", "std", "iqr", "d(x-s)", "pct");

  constexpr std::size_t N = 1 << 14;
  constexpr std::uint64_t R_PUSH = 200;
  constexpr std::uint64_t R_SCAN = 800;
  constexpr std::size_t N_ERASE = 512;
  constexpr std::uint64_t R_ERASE = 200;

  const std::uint64_t push_ops = static_cast<std::uint64_t>(N) * R_PUSH;
  const std::uint64_t scan_ops = static_cast<std::uint64_t>(N) * R_SCAN;
  const std::uint64_t erase_ops =
      static_cast<std::uint64_t>(N_ERASE) * R_ERASE; // ~erasures per round

  bench(
      "push_rsv_u64", push_ops,
      [&] { run_xer_push_reserved<std::uint64_t>(N, R_PUSH); },
      [&] { run_std_push_reserved<std::uint64_t>(N, R_PUSH); });
  bench(
      "push_rsv_fat", push_ops, [&] { run_xer_push_reserved<Fat>(N, R_PUSH); },
      [&] { run_std_push_reserved<Fat>(N, R_PUSH); });
  bench(
      "push_rsv_trk", push_ops,
      [&] { run_xer_push_reserved<Tracker>(N, R_PUSH); },
      [&] { run_std_push_reserved<Tracker>(N, R_PUSH); });

  bench(
      "push_grow_u64", push_ops,
      [&] { run_xer_push_grow<std::uint64_t>(N, R_PUSH); },
      [&] { run_std_push_grow<std::uint64_t>(N, R_PUSH); });
  bench(
      "push_grow_trk", push_ops, [&] { run_xer_push_grow<Tracker>(N, R_PUSH); },
      [&] { run_std_push_grow<Tracker>(N, R_PUSH); });

  bench(
      "scan_u64", scan_ops, [&] { run_xer_scan<std::uint64_t>(N, R_SCAN); },
      [&] { run_std_scan<std::uint64_t>(N, R_SCAN); });
  bench(
      "scan_fat", scan_ops, [&] { run_xer_scan<Fat>(N, R_SCAN); },
      [&] { run_std_scan<Fat>(N, R_SCAN); });

  bench(
      "erase0_u64", erase_ops,
      [&] { run_xer_erase_front<std::uint64_t>(N_ERASE, R_ERASE); },
      [&] { run_std_erase_front<std::uint64_t>(N_ERASE, R_ERASE); });
  bench(
      "erase0_trk", erase_ops,
      [&] { run_xer_erase_front<Tracker>(N_ERASE, R_ERASE); },
      [&] { run_std_erase_front<Tracker>(N_ERASE, R_ERASE); });

  bench(
      "uerase_u64", erase_ops,
      [&] { run_xer_unordered_erase<std::uint64_t>(N_ERASE, R_ERASE); },
      [&] { run_std_swap_pop<std::uint64_t>(N_ERASE, R_ERASE); });
  bench(
      "uerase_trk", erase_ops,
      [&] { run_xer_unordered_erase<Tracker>(N_ERASE, R_ERASE); },
      [&] { run_std_swap_pop<Tracker>(N_ERASE, R_ERASE); });

  std::printf("sink=%llu\n", static_cast<unsigned long long>(g_sink));
  std::printf("PERF_DONE\n");
  return 0;
}
