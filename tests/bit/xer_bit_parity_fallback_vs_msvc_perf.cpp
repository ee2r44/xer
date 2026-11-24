// A/B on MSVC: details_::bit_parity_fallback vs (details_::popcount_msvc & 1)
//
// Build (optimized):
//   cl /std:c++latest /O2 /DNDEBUG /EHsc /Iinclude /Fe:tests\bit\xer_bit_parity_fallback_vs_msvc_perf_msvc.exe tests\bit\xer_bit_parity_fallback_vs_msvc_perf.cpp include\xer\env\xer_env.cpp
// Optional: add /arch:AVX2 for a second binary (popcount_msvc still callable).

#if !defined(_MSC_VER)
#  error "This A/B is for MSVC only"
#endif
#if defined(_DEBUG) || !defined(NDEBUG)
#  error "Build with /O2 /DNDEBUG"
#endif

#include "xer/bit/xer_bit_parity.h"
#include "xer/bit/xer_popcount.h"
#include "xer/macro/xer_macro.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <vector>

#ifndef NOMINMAX
#  define NOMINMAX
#endif
#include <windows.h>

namespace {

volatile std::uint64_t g_sink = 0;

constexpr int kWarmupPairs = 4;
constexpr int kSamplePairs = 21;

XER_NOINLINE void escape_u64(std::uint64_t v) noexcept { g_sink += v; }

void pin_thread() noexcept {
  SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
  SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
  const DWORD cpu = GetCurrentProcessorNumber();
  SetThreadAffinityMask(GetCurrentThread(), DWORD_PTR{1} << (cpu & 63));
  std::printf("pin cpu=%lu\n", static_cast<unsigned long>(cpu));
}

struct Stats {
  double med_ns;
  double iqr_ns;
};

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

using Clock = std::chrono::steady_clock;

template <typename F>
double ns_total(F&& f) {
  const auto t0 = Clock::now();
  f();
  const auto t1 = Clock::now();
  return std::chrono::duration<double, std::nano>(t1 - t0).count();
}

enum class ValPat { Mixed, Sparse, Dense };

template <typename T>
void fill(std::vector<T>& data, std::size_t n, ValPat pat) {
  data.resize(n);
  std::uint64_t s = 0xB171A117ULL ^ (sizeof(T) * 0x9E3779B97F4A7C15ull);
  constexpr std::uint32_t bits = static_cast<std::uint32_t>(sizeof(T) * 8u);
  for (std::size_t i = 0; i < n; ++i) {
    s = s * 6364136223846793005ull + 1ull;
    switch (pat) {
      case ValPat::Mixed:
        data[i] = static_cast<T>(s);
        break;
      case ValPat::Sparse: {
        T v{0};
        const unsigned k = static_cast<unsigned>(s % 4u);
        for (unsigned b = 0; b < k; ++b) {
          s = s * 6364136223846793005ull + 1ull;
          v = static_cast<T>(v | (T{1} << (s % bits)));
        }
        data[i] = v;
        break;
      }
      case ValPat::Dense:
        data[i] = static_cast<T>(~static_cast<T>(s & 0xFull));
        break;
    }
  }
}

template <typename T>
XER_NOINLINE std::uint64_t run_fallback(const T* data, std::size_t n,
                                        std::uint64_t rounds) {
  std::uint64_t acc = 0;
  for (std::uint64_t r = 0; r < rounds; ++r) {
    const T salt = static_cast<T>(r);
    for (std::size_t i = 0; i < n; ++i) {
      acc += xer::details_::bit_parity_fallback(static_cast<T>(data[i] + salt));
    }
  }
  escape_u64(acc);
  return acc;
}

template <typename T>
XER_NOINLINE std::uint64_t run_msvc_popcnt_and1(const T* data, std::size_t n,
                                               std::uint64_t rounds) {
  std::uint64_t acc = 0;
  for (std::uint64_t r = 0; r < rounds; ++r) {
    const T salt = static_cast<T>(r);
    for (std::size_t i = 0; i < n; ++i) {
      acc += xer::details_::popcount_msvc(static_cast<T>(data[i] + salt)) & 1u;
    }
  }
  escape_u64(acc);
  return acc;
}

template <typename T>
void bench(const char* name, ValPat pat, std::size_t n, std::uint64_t rounds) {
  std::vector<T> data;
  fill(data, n, pat);

  // Sanity: both must agree on a sample.
  for (std::size_t i = 0; i < (std::min)(data.size(), std::size_t{4096}); ++i) {
    const auto a = xer::details_::bit_parity_fallback(data[i]);
    const auto b = xer::details_::popcount_msvc(data[i]) & 1u;
    if (a != b) {
      std::printf("%-18s EQUIV_FAIL i=%zu a=%u b=%u\n", name, i, a, b);
      return;
    }
  }

  const std::uint64_t ops = static_cast<std::uint64_t>(n) * rounds;
  Stats s[2];
  measure2(
      [&] {
        return ns_total([&] { run_fallback(data.data(), n, rounds); }) /
               static_cast<double>(ops);
      },
      [&] {
        return ns_total([&] { run_msvc_popcnt_and1(data.data(), n, rounds); }) /
               static_cast<double>(ops);
      },
      s);

  // s[0]=fallback, s[1]=popcount_msvc&1; ratio fb/msvc (<1 => fallback faster)
  const double ratio = s[1].med_ns > 0.0 ? (s[0].med_ns / s[1].med_ns) : 0.0;
  const double pct =
      s[1].med_ns > 0.0 ? (100.0 * (s[0].med_ns - s[1].med_ns) / s[1].med_ns)
                         : 0.0;
  std::printf(
      "%-18s %8zu %8llu  %8.4f %6.4f  %8.4f %6.4f  %7.3f %+7.2f%%\n", name, n,
      static_cast<unsigned long long>(rounds), s[0].med_ns, s[0].iqr_ns,
      s[1].med_ns, s[1].iqr_ns, ratio, pct);
}

}  // namespace

int main() {
  pin_thread();
  std::printf(
      "MSVC A/B: bit_parity_fallback vs (popcount_msvc & 1)\n"
      "ns/op; fb/msvc <1 => fallback faster\n"
#if defined(__AVX2__)
      "build=msvc+avx2\n"
#else
      "build=msvc\n"
#endif
  );
  std::printf("%-18s %8s %8s  %8s %6s  %8s %6s  %7s %8s\n", "bench", "n",
              "rounds", "fb", "iqr", "msvc&1", "iqr", "fb/m", "pct");

  constexpr std::size_t N = 1 << 16;
  constexpr std::uint64_t R = 400;

  bench<std::uint8_t>("u8_mixed", ValPat::Mixed, N, R);
  bench<std::uint8_t>("u8_sparse", ValPat::Sparse, N, R);
  bench<std::uint8_t>("u8_dense", ValPat::Dense, N, R);

  bench<std::uint16_t>("u16_mixed", ValPat::Mixed, N, R);
  bench<std::uint16_t>("u16_sparse", ValPat::Sparse, N, R);
  bench<std::uint16_t>("u16_dense", ValPat::Dense, N, R);

  bench<std::uint32_t>("u32_mixed", ValPat::Mixed, N, R);
  bench<std::uint32_t>("u32_sparse", ValPat::Sparse, N, R);
  bench<std::uint32_t>("u32_dense", ValPat::Dense, N, R);

  bench<std::uint64_t>("u64_mixed", ValPat::Mixed, N, R);
  bench<std::uint64_t>("u64_sparse", ValPat::Sparse, N, R);
  bench<std::uint64_t>("u64_dense", ValPat::Dense, N, R);

  std::printf("sink=%llu\n", static_cast<unsigned long long>(g_sink));
  return 0;
}
