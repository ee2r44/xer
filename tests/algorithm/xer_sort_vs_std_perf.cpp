// xer::sort vs std::sort performance.
//
// Build (optimized):
//   MSVC:  cl /std:c++latest /O2 /DNDEBUG /EHsc /Iinclude /Fe:tests\algorithm\xer_sort_vs_std_perf_msvc.exe tests\algorithm\xer_sort_vs_std_perf.cpp
//   GCC:   g++ -std=c++23 -O3 -DNDEBUG -pthread -Iinclude -o tests/algorithm/xer_sort_vs_std_perf_gcc tests/algorithm/xer_sort_vs_std_perf.cpp
//   Clang: clang++ -std=c++23 -O3 -DNDEBUG -pthread -Iinclude -o tests/algorithm/xer_sort_vs_std_perf_clang tests/algorithm/xer_sort_vs_std_perf.cpp

#if defined(_MSC_VER)
#  if defined(_DEBUG) || !defined(NDEBUG)
#    error "Build this perf test with /O2 /DNDEBUG"
#  endif
#elif !defined(__OPTIMIZE__) || !defined(NDEBUG)
#  error "Build this perf test with -O3 -DNDEBUG"
#endif

#include "xer/algorithm/xer_sort.h"
#include "xer/macro/xer_macro.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <random>
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

constexpr int kWarmupPairs = 8;
constexpr int kSamplePairs = 31;
constexpr double kMinSampleNs = 80e6;  // ~80ms per timed sample
constexpr std::uint32_t kMaxRounds = 50000000u;

XER_NOINLINE void escape_u64(std::uint64_t v) noexcept {
#if defined(__GNUC__) || defined(__clang__)
  asm volatile("" : "+r"(v) : : "memory");
#endif
  g_sink += v;
}

XER_NOINLINE void touch_sorted(const int* p, std::size_t n) noexcept {
  if (n == 0) {
    escape_u64(0);
    return;
  }
  escape_u64(static_cast<std::uint64_t>(p[0]) ^
             static_cast<std::uint64_t>(p[n - 1]) ^
             static_cast<std::uint64_t>(p[n >> 1]) ^
             static_cast<std::uint64_t>(n));
}

XER_NOINLINE void prefault_ro(const int* p, std::size_t n) noexcept {
  if (n == 0) return;
  constexpr std::size_t stride = 1024 / sizeof(int);
  std::uint64_t acc = 0;
  for (std::size_t i = 0; i < n; i += stride) {
    acc += static_cast<std::uint64_t>(p[i]);
  }
  acc += static_cast<std::uint64_t>(p[n - 1]);
  escape_u64(acc);
}

XER_NOINLINE void prefault_rw(int* p, std::size_t n) noexcept {
  if (n == 0) return;
  constexpr std::size_t stride = 1024 / sizeof(int);
  for (std::size_t i = 0; i < n; i += stride) {
    p[i] = static_cast<int>(p[i] + 0);
  }
  p[n - 1] = static_cast<int>(p[n - 1] + 0);
  escape_u64(static_cast<std::uint64_t>(p[0]) ^
             static_cast<std::uint64_t>(p[n - 1]));
}

void timer_fence() noexcept {
  std::atomic_thread_fence(std::memory_order_seq_cst);
}

void pin_thread() noexcept {
#if defined(_WIN32)
  SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
  SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
  const DWORD cpu = GetCurrentProcessorNumber();
  SetThreadAffinityMask(GetCurrentThread(), DWORD_PTR{1} << (cpu & 63));
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

using Clock = std::chrono::steady_clock;

template <typename F>
double ns_total(F&& f) {
  timer_fence();
  const auto t0 = Clock::now();
  f();
  timer_fence();
  const auto t1 = Clock::now();
  return std::chrono::duration<double, std::nano>(t1 - t0).count();
}

template <typename F0, typename F1>
void measure2(F0&& f0, F1&& f1, Stats out[2]) {
  auto run = [&](int idx) -> double { return idx == 0 ? f0() : f1(); };

  for (int w = 0; w < kWarmupPairs; ++w) {
    const int start = w & 1;
    run(start);
    run(start ^ 1);
  }

  double samples[2][kSamplePairs];
  for (int rep = 0; rep < kSamplePairs; ++rep) {
    const int start = rep & 1;
    samples[start][rep] = run(start);
    samples[start ^ 1][rep] = run(start ^ 1);
  }
  for (int idx = 0; idx < 2; ++idx) {
    double* s = samples[idx];
    std::sort(s, s + kSamplePairs);
    out[idx].med_ns = s[kSamplePairs / 2];
    out[idx].iqr_ns = s[(3 * kSamplePairs) / 4] - s[kSamplePairs / 4];
  }
}

const char* compiler_name() noexcept {
#if defined(__clang__)
  return "clang";
#elif defined(__GNUC__)
  return "gcc";
#elif defined(_MSC_VER)
  return "msvc";
#else
  return "unknown";
#endif
}

void report(const char* tag, std::uint64_t elems, const Stats s[2],
            std::uint32_t rounds) {
  const double xer = s[0].med_ns / static_cast<double>(elems);
  const double stdv = s[1].med_ns / static_cast<double>(elems);
  const double xer_r =
      s[0].med_ns > 0.0 ? (100.0 * s[0].iqr_ns / s[0].med_ns) : 0.0;
  const double std_r =
      s[1].med_ns > 0.0 ? (100.0 * s[1].iqr_ns / s[1].med_ns) : 0.0;
  std::printf("%-22s %10.3f/%-7.3f %10.3f/%-7.3f %7.3f %6.2f/%-5.2f %8u\n", tag,
              xer, s[0].iqr_ns / static_cast<double>(elems), stdv,
              s[1].iqr_ns / static_cast<double>(elems), xer / stdv, xer_r,
              std_r, rounds);
}

enum class Pattern : int { Random, Sorted, Reverse, Sawtooth };

std::vector<int> make_pattern(std::size_t n, Pattern pat, std::uint32_t seed) {
  std::vector<int> v(n);
  switch (pat) {
    case Pattern::Sorted:
      for (std::size_t i = 0; i < n; ++i) v[i] = static_cast<int>(i);
      break;
    case Pattern::Reverse:
      for (std::size_t i = 0; i < n; ++i) v[i] = static_cast<int>(n - i);
      break;
    case Pattern::Sawtooth:
      for (std::size_t i = 0; i < n; ++i) v[i] = static_cast<int>(i % 7);
      break;
    case Pattern::Random:
    default: {
      std::mt19937 rng(seed);
      std::uniform_int_distribution<int> dist(-1000000, 1000000);
      for (std::size_t i = 0; i < n; ++i) v[i] = dist(rng);
      break;
    }
  }
  return v;
}

const char* pattern_name(Pattern p) noexcept {
  switch (p) {
    case Pattern::Sorted:
      return "sorted";
    case Pattern::Reverse:
      return "reverse";
    case Pattern::Sawtooth:
      return "sawtooth";
    case Pattern::Random:
    default:
      return "random";
  }
}

template <typename SortFn>
double time_rounds(std::vector<int>& buf, const std::vector<int>& master,
                   std::uint32_t rounds, SortFn sort) {
  const std::size_t n = master.size();
  return ns_total([&] {
    for (std::uint32_t r = 0; r < rounds; ++r) {
      std::memcpy(buf.data(), master.data(), n * sizeof(int));
      sort(buf.data(), n);
      touch_sorted(buf.data(), n);
    }
  });
}

template <typename XerSort, typename StdSort>
std::uint32_t calibrate_rounds(std::vector<int>& a, std::vector<int>& b,
                               const std::vector<int>& master,
                               std::uint32_t hint, XerSort xer_sort,
                               StdSort std_sort) {
  std::uint32_t probe = hint;
  if (probe == 0) probe = 1;
  if (probe > 4096u) probe = 4096u;

  (void)time_rounds(a, master, probe, xer_sort);
  (void)time_rounds(b, master, probe, std_sort);
  const double t0 = time_rounds(a, master, probe, xer_sort);
  const double t1 = time_rounds(b, master, probe, std_sort);
  const double per = (t0 > t1 ? t0 : t1) / static_cast<double>(probe);

  std::uint64_t rounds = 1;
  if (per > 0.0) {
    rounds = static_cast<std::uint64_t>(kMinSampleNs / per + 0.5);
  }
  if (rounds < static_cast<std::uint64_t>(hint)) rounds = hint;
  if (rounds < 1) rounds = 1;
  if (rounds > kMaxRounds) rounds = kMaxRounds;
  return static_cast<std::uint32_t>(rounds);
}

template <typename XerSort, typename StdSort>
void bench_pair(const char* tag, const std::vector<int>& master,
                std::uint32_t hint_rounds, XerSort xer_sort, StdSort std_sort) {
  const std::size_t n = master.size();
  std::vector<int> a = master;
  std::vector<int> b = master;
  prefault_ro(master.data(), n);
  prefault_rw(a.data(), n);
  prefault_rw(b.data(), n);

  const std::uint32_t rounds =
      calibrate_rounds(a, b, master, hint_rounds, xer_sort, std_sort);

  Stats s[2];
  measure2([&] { return time_rounds(a, master, rounds, xer_sort); },
           [&] { return time_rounds(b, master, rounds, std_sort); }, s);

  report(tag, static_cast<std::uint64_t>(n) * rounds, s, rounds);
}

void bench_sort(std::size_t n, Pattern pat, std::uint32_t hint_rounds) {
  char tag[64];
  std::snprintf(tag, sizeof(tag), "%s_%zu", pattern_name(pat), n);
  const auto master =
      make_pattern(n, pat, 0x50FEU ^ static_cast<std::uint32_t>(n * 17u));
  bench_pair(
      tag, master, hint_rounds,
      [](int* p, std::size_t n) {
        (void)xer::sort(p, n);
      },
      [](int* p, std::size_t n) { std::sort(p, p + static_cast<std::ptrdiff_t>(n)); });
}

}  // namespace

int main() {
  pin_thread();
  std::printf(
      "xer::sort vs std::sort  ns/elem med/iqr\n"
      "noise: warmup=%d pairs, samples=%d, min_sample~%.0fms, "
      "prefault+fence+calib rounds\n"
      "compiler=%s sink=%llu\n",
      kWarmupPairs, kSamplePairs, kMinSampleNs / 1e6, compiler_name(),
      static_cast<unsigned long long>(g_sink));
  std::printf("%-22s %10s/%-7s %10s/%-7s %7s %6s/%-5s %8s\n", "tag", "xer",
              "iqr", "std", "iqr", "x/s", "xr%", "sr%", "rounds");

  const std::size_t sizes[] = {16,   32,   33,    64,    256,  1024,
                               4096, 16384, 65536, 262144};
  const std::uint32_t hints[] = {40000, 20000, 20000, 10000, 3000, 800,
                                 200,   40,    8,     2};

  for (Pattern pat :
       {Pattern::Random, Pattern::Sorted, Pattern::Reverse, Pattern::Sawtooth}) {
    for (std::size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); ++i) {
      bench_sort(sizes[i], pat, hints[i]);
    }
  }

  std::printf("sink=%llu\n", static_cast<unsigned long long>(g_sink));
  return 0;
}
