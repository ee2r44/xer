// A/B: include xer::sort (ISGATE=32 + prefetch) vs xer_ab::sort (ISGATE=32, no prefetch)
// vs std::sort.
// Mirror: tests/algorithm/ab_sort_isgate16/xer_sort_ab.h (include/ untouched).
//
// Build (optimized):
//   MSVC:  cl /std:c++latest /O2 /DNDEBUG /EHsc /Iinclude /Itests\algorithm\ab_sort_isgate16 /Fe:tests\algorithm\xer_sort_prefetch_ab_perf_msvc.exe tests\algorithm\xer_sort_prefetch_ab_perf.cpp
//   GCC:   g++ -std=c++23 -O3 -DNDEBUG -pthread -Iinclude -Itests/algorithm/ab_sort_isgate16 -o tests/algorithm/xer_sort_prefetch_ab_perf_gcc tests/algorithm/xer_sort_prefetch_ab_perf.cpp
//   Clang: clang++ -std=c++23 -O3 -DNDEBUG -pthread -Iinclude -Itests/algorithm/ab_sort_isgate16 -o tests/algorithm/xer_sort_prefetch_ab_perf_clang tests/algorithm/xer_sort_prefetch_ab_perf.cpp

#if defined(_MSC_VER)
#  if defined(_DEBUG) || !defined(NDEBUG)
#    error "Build this perf test with /O2 /DNDEBUG"
#  endif
#elif !defined(__OPTIMIZE__) || !defined(NDEBUG)
#  error "Build this perf test with -O3 -DNDEBUG"
#endif

#include "xer/algorithm/xer_sort.h"
#include "xer/macro/xer_macro.h"
#include "xer_sort_ab.h"

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

constexpr int kWarmupRounds = 8;
constexpr int kSampleRounds = 31;
constexpr int kAlgos = 3;
constexpr double kMinSampleNs = 80e6;
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

template <typename F0, typename F1, typename F2>
void measure3(F0&& f0, F1&& f1, F2&& f2, Stats out[kAlgos]) {
  auto run = [&](int idx) -> double {
    if (idx == 0) return f0();
    if (idx == 1) return f1();
    return f2();
  };

  for (int w = 0; w < kWarmupRounds; ++w) {
    const int start = w % kAlgos;
    for (int k = 0; k < kAlgos; ++k) run((start + k) % kAlgos);
  }

  double samples[kAlgos][kSampleRounds];
  for (int rep = 0; rep < kSampleRounds; ++rep) {
    const int start = rep % kAlgos;
    for (int k = 0; k < kAlgos; ++k) {
      const int idx = (start + k) % kAlgos;
      samples[idx][rep] = run(idx);
    }
  }
  for (int idx = 0; idx < kAlgos; ++idx) {
    double* s = samples[idx];
    std::sort(s, s + kSampleRounds);
    out[idx].med_ns = s[kSampleRounds / 2];
    out[idx].iqr_ns = s[(3 * kSampleRounds) / 4] - s[kSampleRounds / 4];
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

void report(const char* tag, std::uint64_t elems, const Stats s[kAlgos],
            std::uint32_t rounds) {
  const double e = static_cast<double>(elems);
  const double pf = s[0].med_ns / e;    // include: prefetch
  const double nop = s[1].med_ns / e;   // mirror: no prefetch
  const double stdv = s[2].med_ns / e;
  const double ipf = s[0].iqr_ns / e;
  const double inop = s[1].iqr_ns / e;
  const double istd = s[2].iqr_ns / e;
  // pf/nop < 1 => prefetch faster than control
  std::printf(
      "%-22s %8.3f/%-6.3f %8.3f/%-6.3f %8.3f/%-6.3f %6.3f %6.3f %6.3f %8u\n",
      tag, pf, ipf, nop, inop, stdv, istd, pf / nop, pf / stdv, nop / stdv,
      rounds);
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

template <typename S0, typename S1, typename S2>
std::uint32_t calibrate_rounds(std::vector<int>& a, std::vector<int>& b,
                               std::vector<int>& c,
                               const std::vector<int>& master,
                               std::uint32_t hint, S0 s0, S1 s1, S2 s2) {
  std::uint32_t probe = hint;
  if (probe == 0) probe = 1;
  if (probe > 4096u) probe = 4096u;

  (void)time_rounds(a, master, probe, s0);
  (void)time_rounds(b, master, probe, s1);
  (void)time_rounds(c, master, probe, s2);
  const double t0 = time_rounds(a, master, probe, s0);
  const double t1 = time_rounds(b, master, probe, s1);
  const double t2 = time_rounds(c, master, probe, s2);
  const double per =
      (std::max)((std::max)(t0, t1), t2) / static_cast<double>(probe);

  std::uint64_t rounds = 1;
  if (per > 0.0) {
    rounds = static_cast<std::uint64_t>(kMinSampleNs / per + 0.5);
  }
  if (rounds < static_cast<std::uint64_t>(hint)) rounds = hint;
  if (rounds < 1) rounds = 1;
  if (rounds > kMaxRounds) rounds = kMaxRounds;
  return static_cast<std::uint32_t>(rounds);
}

void bench(std::size_t n, Pattern pat, std::uint32_t hint_rounds) {
  char tag[64];
  std::snprintf(tag, sizeof(tag), "%s_%zu", pattern_name(pat), n);
  const auto master =
      make_pattern(n, pat, 0xA15E15U ^ static_cast<std::uint32_t>(n * 23u));

  std::vector<int> a = master;
  std::vector<int> b = master;
  std::vector<int> c = master;
  prefault_ro(master.data(), n);
  prefault_rw(a.data(), n);
  prefault_rw(b.data(), n);
  prefault_rw(c.data(), n);

  auto s_pf = [](int* p, std::size_t n) { (void)xer::sort(p, n); };
  auto s_nop = [](int* p, std::size_t n) { (void)xer_ab::sort(p, n); };
  auto s_std = [](int* p, std::size_t n) {
    std::sort(p, p + static_cast<std::ptrdiff_t>(n));
  };

  const std::uint32_t rounds =
      calibrate_rounds(a, b, c, master, hint_rounds, s_pf, s_nop, s_std);

  Stats s[kAlgos];
  measure3([&] { return time_rounds(a, master, rounds, s_pf); },
           [&] { return time_rounds(b, master, rounds, s_nop); },
           [&] { return time_rounds(c, master, rounds, s_std); }, s);

  report(tag, static_cast<std::uint64_t>(n) * rounds, s, rounds);
}

}  // namespace

int main() {
  pin_thread();
  std::printf(
      "prefetch A/B: xer(+PF,ISGATE=32) vs xer_ab(no PF,ISGATE=32) vs std::sort\n"
      "ns/elem med/iqr; pf/nop <1 => prefetch faster; also pf/std, nop/std\n"
      "noise: warmup=%d, samples=%d, min_sample~%.0fms\n"
      "compiler=%s sink=%llu\n",
      kWarmupRounds, kSampleRounds, kMinSampleNs / 1e6, compiler_name(),
      static_cast<unsigned long long>(g_sink));
  std::printf("%-22s %8s/%-6s %8s/%-6s %8s/%-6s %6s %6s %6s %8s\n", "tag",
              "pf", "iqr", "nop", "iqr", "std", "iqr", "pf/np", "pf/s", "np/s",
              "rounds");

  // n<=32: insert-only (prefetch unused). n>32: qsort branch where PF applies.
  const std::size_t sizes[] = {32, 33, 64, 256, 1024, 4096, 16384, 65536, 262144};
  const std::uint32_t hints[] = {20000, 20000, 10000, 3000, 800, 200, 40, 8, 2};

  for (Pattern pat :
       {Pattern::Random, Pattern::Sorted, Pattern::Reverse, Pattern::Sawtooth}) {
    for (std::size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); ++i) {
      bench(sizes[i], pat, hints[i]);
    }
  }

  std::printf("sink=%llu\n", static_cast<unsigned long long>(g_sink));
  return 0;
}
