// Old vs new add_overflow check strategies.
// Old:  (mask ^ l) < r   with mask = ~B(0)
// New:  (~static_cast<B>(l)) < r
// Library xer::add_overflow currently uses the new form; both are mirrored here
// so the comparison does not depend on editing include/.
//
// Build (optimized):
//   MSVC:  cl /std:c++latest /O2 /DNDEBUG /EHsc /Iinclude /Fe:tests\numeric\xer_add_overflow_perf_msvc.exe tests\numeric\xer_add_overflow_perf.cpp
//   GCC:   g++ -std=c++23 -O3 -DNDEBUG -pthread -Iinclude -o tests/numeric/xer_add_overflow_perf_gcc tests/numeric/xer_add_overflow_perf.cpp
//   Clang: clang++ -std=c++23 -O3 -DNDEBUG -pthread -Iinclude -o tests/numeric/xer_add_overflow_perf_clang tests/numeric/xer_add_overflow_perf.cpp

#if defined(_MSC_VER)
#  if defined(_DEBUG) || !defined(NDEBUG)
#    error "Build this perf test with /O2 /DNDEBUG"
#  endif
#elif !defined(__OPTIMIZE__) || !defined(NDEBUG)
#  error "Build this perf test with -O3 -DNDEBUG"
#endif

#include "xer/numeric/xer_add_overflow.h"
#include "xer/macro/xer_macro.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <type_traits>
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

template <typename T>
XER_NOINLINE void escape(T v) noexcept {
#if defined(__GNUC__) || defined(__clang__)
  asm volatile("" : "+r"(v) : : "memory");
#endif
  g_sink ^= static_cast<std::uint64_t>(v);
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

template <typename L, typename R>
requires xer::details_::add_overflowable<L, R>
constexpr bool add_overflow_old(const L l, const R r) noexcept {
  using B = std::conditional_t<(sizeof(L) > sizeof(R)), L, R>;
  constexpr B mask = ~static_cast<B>(0);
  return (mask ^ l) < r;
}

template <typename L, typename R>
requires xer::details_::add_overflowable<L, R>
constexpr bool add_overflow_new(const L l, const R r) noexcept {
  using B = std::conditional_t<(sizeof(L) > sizeof(R)), L, R>;
  return (~static_cast<B>(l)) < r;
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

template <typename T>
void fill_pairs(std::vector<T>& left, std::vector<T>& right, std::size_t n) {
  left.resize(n);
  right.resize(n);
  // Mix near-zero, mid, and near-max values so both overflow and non-overflow
  // paths are hot. Deterministic LCG.
  std::uint64_t s = 0x9E3779B97F4A7C15ull;
  for (std::size_t i = 0; i < n; ++i) {
    s = s * 6364136223846793005ull + 1ull;
    const T a = static_cast<T>(s);
    s = s * 6364136223846793005ull + 1ull;
    const T b = static_cast<T>(s >> 17);
    // Bias ~1/4 of samples toward overflow-ish operands.
    if ((i & 3u) == 0u) {
      left[i] = static_cast<T>(~static_cast<T>(0) - static_cast<T>(i & 0xFFu));
      right[i] = static_cast<T>((i * 17u) + 1u);
    } else {
      left[i] = a;
      right[i] = b;
    }
  }
}

template <typename T>
int check_equivalence(const std::vector<T>& left, const std::vector<T>& right) {
  int mismatches = 0;
  for (std::size_t i = 0; i < left.size(); ++i) {
    const bool o = add_overflow_old(left[i], right[i]);
    const bool n = add_overflow_new(left[i], right[i]);
    const bool lib = xer::add_overflow(left[i], right[i]);
    // Reference: T-width unsigned wrap (after usual + truncation into T).
    const T a = left[i];
    const T b = right[i];
    const T sum = static_cast<T>(a + b);
    const bool ref = sum < a;
    if (o != n || n != lib || o != ref) {
      if (mismatches < 3) {
        std::printf("  mismatch i=%zu l=%llu r=%llu old=%d new=%d lib=%d ref=%d\n",
                    i, static_cast<unsigned long long>(left[i]),
                    static_cast<unsigned long long>(right[i]), static_cast<int>(o),
                    static_cast<int>(n), static_cast<int>(lib),
                    static_cast<int>(ref));
      }
      ++mismatches;
    }
  }
  return mismatches;
}

template <typename T>
XER_NOINLINE std::uint64_t run_old(const T* left, const T* right, std::size_t n,
                                    std::uint64_t rounds) {
  std::uint64_t acc = 0;
  for (std::uint64_t r = 0; r < rounds; ++r) {
    for (std::size_t i = 0; i < n; ++i) {
      acc += static_cast<std::uint64_t>(add_overflow_old(left[i], right[i]));
    }
  }
  escape(acc);
  return acc;
}

template <typename T>
XER_NOINLINE std::uint64_t run_new(const T* left, const T* right, std::size_t n,
                                    std::uint64_t rounds) {
  std::uint64_t acc = 0;
  for (std::uint64_t r = 0; r < rounds; ++r) {
    for (std::size_t i = 0; i < n; ++i) {
      acc += static_cast<std::uint64_t>(add_overflow_new(left[i], right[i]));
    }
  }
  escape(acc);
  return acc;
}

template <typename T>
void bench_type(const char* name, std::size_t n, std::uint64_t rounds) {
  std::vector<T> left;
  std::vector<T> right;
  fill_pairs(left, right, n);

  const int mismatches = check_equivalence(left, right);
  if (mismatches != 0) {
    std::printf("%-10s EQUIV_FAIL mismatches=%d (skip timing)\n", name,
                mismatches);
    return;
  }

  const std::uint64_t ops = static_cast<std::uint64_t>(n) * rounds;
  Stats s[2];
  measure2(
      [&] {
        return ns_total([&] { run_old(left.data(), right.data(), n, rounds); }) /
               static_cast<double>(ops);
      },
      [&] {
        return ns_total([&] { run_new(left.data(), right.data(), n, rounds); }) /
               static_cast<double>(ops);
      },
      s);

  const double delta = s[1].med_ns - s[0].med_ns;
  const double pct =
      s[0].med_ns > 0.0 ? (100.0 * delta / s[0].med_ns) : 0.0;
  std::printf("%-10s %8zu %8llu  %8.4f %6.4f  %8.4f %6.4f  %+8.4f %+7.2f%%\n",
              name, n, static_cast<unsigned long long>(rounds), s[0].med_ns,
              s[0].iqr_ns, s[1].med_ns, s[1].iqr_ns, delta, pct);
  std::fflush(stdout);
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
  std::printf("xer_add_overflow strategy perf compiler=%s\n", compiler);
  std::printf("old: ( ~B(0) ^ l ) < r\n");
  std::printf("new: ( ~static_cast<B>(l) ) < r\n");
  std::printf("ns/op median of 9 rotated samples; iqr = p75-p25\n");
  std::printf(
      "%-10s %8s %8s  %8s %6s  %8s %6s  %8s %8s\n", "type", "n", "rounds",
      "old", "iqr", "new", "iqr", "d(new-old)", "pct");

  bench_type<std::uint8_t>("u8", 1 << 16, 400);
  bench_type<std::uint16_t>("u16", 1 << 16, 400);
  bench_type<std::uint32_t>("u32", 1 << 16, 400);
  bench_type<std::uint64_t>("u64", 1 << 16, 400);
  bench_type<std::size_t>("size_t", 1 << 16, 400);

  // Longer inner dependency chain stress (same op count order).
  bench_type<std::uint32_t>("u32-long", 1 << 12, 8000);
  bench_type<std::uint64_t>("u64-long", 1 << 12, 8000);

  std::printf("sink=%llu\n", static_cast<unsigned long long>(g_sink));
  std::printf("PERF_DONE\n");
  return 0;
}
