// rotl_fallback: mask-then-negate (old) vs negate-unmasked-count (new).
//
// Old:
//   c = count & mask;
//   (t << c) | (t >> ((0 - c) & mask))
// New:
//   c0 = count & mask;
//   c1 = (0 - count) & mask;
//   (t << c0) | (t >> c1)
//
// Build (optimized):
//   MSVC:  cl /std:c++latest /O2 /DNDEBUG /EHsc /Iinclude /Fe:tests\bit\xer_rotl_fallback2_perf_msvc.exe tests\bit\xer_rotl_fallback2_perf.cpp
//   GCC:   g++ -std=c++23 -O3 -DNDEBUG -pthread -Iinclude -o tests/bit/xer_rotl_fallback2_perf_gcc tests/bit/xer_rotl_fallback2_perf.cpp
//   Clang: clang++ -std=c++23 -O3 -DNDEBUG -pthread -Iinclude -o tests/bit/xer_rotl_fallback2_perf_clang tests/bit/xer_rotl_fallback2_perf.cpp

#if defined(_MSC_VER)
#  if defined(_DEBUG) || !defined(NDEBUG)
#    error "Build this perf test with /O2 /DNDEBUG"
#  endif
#elif !defined(__OPTIMIZE__) || !defined(NDEBUG)
#  error "Build this perf test with -O3 -DNDEBUG"
#endif

#include "xer/bit/xer_bit_details.h"
#include "xer/macro/xer_macro.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
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

// Old: negate the already-masked count.
template <typename T>
  requires xer::details_::bit_manipulatable<T>
XER_NOINLINE T rotl_old(const T t, const std::uint32_t count) noexcept {
  constexpr std::uint32_t bit_count = sizeof(T) << 3;
  constexpr std::uint32_t mask = bit_count - 1;
  const std::uint32_t c = count & mask;
  return static_cast<T>(
      (t << c) | (t >> ((static_cast<std::uint32_t>(0) - c) & mask)));
}

// New: left uses masked count; right uses (-count) & mask directly.
template <typename T>
  requires xer::details_::bit_manipulatable<T>
XER_NOINLINE T rotl_new(const T t, const std::uint32_t count) noexcept {
  constexpr std::uint32_t bit_count = sizeof(T) << 3;
  constexpr std::uint32_t mask = bit_count - 1;
  const std::uint32_t c0 = count & mask;
  const std::uint32_t c1 = (static_cast<std::uint32_t>(0) - count) & mask;
  return static_cast<T>((t << c0) | (t >> c1));
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

enum class CountPat { Mixed, OftenZero, NeverZero, LargeCount };

template <typename T>
void fill_inputs(std::vector<T>& vals, std::vector<std::uint32_t>& counts,
                 std::size_t n, CountPat pat) {
  vals.resize(n);
  counts.resize(n);
  std::uint64_t s = 0xA5A5A5A5C3C3C3C3ull ^ (sizeof(T) * 0x9E3779B97F4A7C15ull);
  constexpr std::uint32_t bits = static_cast<std::uint32_t>(sizeof(T) << 3);
  for (std::size_t i = 0; i < n; ++i) {
    s = s * 6364136223846793005ull + 1ull;
    vals[i] = static_cast<T>(s);
    s = s * 6364136223846793005ull + 1ull;
    const std::uint32_t raw = static_cast<std::uint32_t>(s);
    switch (pat) {
    case CountPat::Mixed:
      counts[i] = raw;
      break;
    case CountPat::OftenZero:
      counts[i] = ((raw & 3u) == 0u) ? (raw | 1u) : 0u;
      break;
    case CountPat::NeverZero:
      counts[i] = (raw % (bits - 1u)) + 1u;
      break;
    case CountPat::LargeCount:
      // Emphasize count outside [0, bits): new form's (-count)&mask vs (-c)&mask.
      counts[i] = raw | bits; // always >= bits
      break;
    }
  }
}

template <typename T>
int check_equiv(const std::vector<T>& vals,
                const std::vector<std::uint32_t>& counts) {
  int mismatches = 0;
  for (std::size_t i = 0; i < vals.size(); ++i) {
    const T a = rotl_old(vals[i], counts[i]);
    const T b = rotl_new(vals[i], counts[i]);
    if (a != b) {
      if (mismatches < 3) {
        std::printf("  mismatch i=%zu c=%u\n", i, counts[i]);
      }
      ++mismatches;
    }
  }
  return mismatches;
}

template <typename T>
XER_NOINLINE std::uint64_t run_old(const T* vals, const std::uint32_t* counts,
                                   std::size_t n, std::uint64_t rounds) {
  T acc = 0;
  for (std::uint64_t r = 0; r < rounds; ++r) {
    const std::uint32_t salt = static_cast<std::uint32_t>(r);
    for (std::size_t i = 0; i < n; ++i) {
      acc = static_cast<T>(acc + rotl_old(vals[i], counts[i] + salt));
    }
  }
  escape_u64(static_cast<std::uint64_t>(acc));
  return static_cast<std::uint64_t>(acc);
}

template <typename T>
XER_NOINLINE std::uint64_t run_new(const T* vals, const std::uint32_t* counts,
                                   std::size_t n, std::uint64_t rounds) {
  T acc = 0;
  for (std::uint64_t r = 0; r < rounds; ++r) {
    const std::uint32_t salt = static_cast<std::uint32_t>(r);
    for (std::size_t i = 0; i < n; ++i) {
      acc = static_cast<T>(acc + rotl_new(vals[i], counts[i] + salt));
    }
  }
  escape_u64(static_cast<std::uint64_t>(acc));
  return static_cast<std::uint64_t>(acc);
}

template <typename T>
void bench(const char* name, CountPat pat, std::size_t n, std::uint64_t rounds) {
  std::vector<T> vals;
  std::vector<std::uint32_t> counts;
  fill_inputs(vals, counts, n, pat);

  const int mismatches = check_equiv(vals, counts);
  if (mismatches != 0) {
    std::printf("%-18s EQUIV_FAIL mismatches=%d (skip)\n", name, mismatches);
    return;
  }

  const std::uint64_t ops = static_cast<std::uint64_t>(n) * rounds;
  Stats s[2];
  measure2(
      [&] {
        return ns_total([&] {
                 run_old(vals.data(), counts.data(), n, rounds);
               }) /
               static_cast<double>(ops);
      },
      [&] {
        return ns_total([&] {
                 run_new(vals.data(), counts.data(), n, rounds);
               }) /
               static_cast<double>(ops);
      },
      s);

  const double delta = s[1].med_ns - s[0].med_ns; // new - old
  const double pct =
      s[0].med_ns > 0.0 ? (100.0 * delta / s[0].med_ns) : 0.0;
  std::printf("%-18s %8zu %8llu  %8.4f %6.4f  %8.4f %6.4f  %+8.4f %+7.2f%%\n",
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
  std::printf(
      "rotl_fallback (-c)&mask vs (-count)&mask compiler=%s\n", compiler);
  std::printf("ns/op median of 9 rotated samples; iqr = p75-p25\n");
  std::printf("delta = new - old (negative => new faster)\n");
  std::printf("%-18s %8s %8s  %8s %6s  %8s %6s  %8s %8s\n", "bench", "n",
              "rounds", "old", "iqr", "new", "iqr", "d(n-o)", "pct");

  constexpr std::size_t N = 1 << 16;
  constexpr std::uint64_t R = 400;

  bench<std::uint8_t>("u8_mixed", CountPat::Mixed, N, R);
  bench<std::uint8_t>("u8_largec", CountPat::LargeCount, N, R);
  bench<std::uint8_t>("u8_nonzero", CountPat::NeverZero, N, R);

  bench<std::uint32_t>("u32_mixed", CountPat::Mixed, N, R);
  bench<std::uint32_t>("u32_largec", CountPat::LargeCount, N, R);
  bench<std::uint32_t>("u32_nonzero", CountPat::NeverZero, N, R);

  bench<std::uint64_t>("u64_mixed", CountPat::Mixed, N, R);
  bench<std::uint64_t>("u64_largec", CountPat::LargeCount, N, R);
  bench<std::uint64_t>("u64_nonzero", CountPat::NeverZero, N, R);

#if XER_HAS_BUILTIN_INT128
  bench<unsigned __int128>("u128_mixed", CountPat::Mixed, N / 2, R);
  bench<unsigned __int128>("u128_largec", CountPat::LargeCount, N / 2, R);
  bench<unsigned __int128>("u128_nonzero", CountPat::NeverZero, N / 2, R);
#endif

  std::printf("sink=%llu\n", static_cast<unsigned long long>(g_sink));
  std::printf("PERF_DONE\n");
  return 0;
}
