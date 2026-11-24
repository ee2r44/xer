// Old: _tzcnt_u32(t | 0xFFFF0000)  vs  New: _tzcnt_u16(t)
// MSVC BMI path only (needs /arch:AVX2 so XER_BMI1_AVAILABLE and intrinsics match).
//
// Build:
//   cl /std:c++latest /O2 /arch:AVX2 /DNDEBUG /EHsc /Iinclude /Fe:tests\bit\xer_countr_zero_u16_tzcnt_perf_msvc_avx2.exe tests\bit\xer_countr_zero_u16_tzcnt_perf.cpp

#if !defined(_MSC_VER)
#  error "This microbench is for MSVC /arch:AVX2 only"
#endif
#if defined(_DEBUG) || !defined(NDEBUG)
#  error "Build with /O2 /DNDEBUG"
#endif
#if !defined(__AVX2__)
#  error "Build with /arch:AVX2"
#endif

#include "xer/macro/xer_macro.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <intrin.h>
#include <vector>

#ifndef NOMINMAX
#  define NOMINMAX
#endif
#include <windows.h>

namespace {

volatile std::uint64_t g_sink = 0;

XER_NOINLINE void escape_u64(std::uint64_t v) noexcept { g_sink += v; }

void pin_thread() noexcept {
  const DWORD cpu = GetCurrentProcessorNumber();
  SetThreadAffinityMask(GetCurrentThread(), DWORD_PTR{1} << (cpu & 63));
  SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
  std::printf("pin cpu=%lu\n", static_cast<unsigned long>(cpu));
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

XER_NOINLINE std::uint32_t tzcnt_u16_old(std::uint16_t t) noexcept {
  constexpr std::uint32_t mask = 0xFFFF0000u;
  return _tzcnt_u32(static_cast<std::uint32_t>(t) | mask);
}

XER_NOINLINE std::uint32_t tzcnt_u16_new(std::uint16_t t) noexcept {
  return static_cast<std::uint32_t>(_tzcnt_u16(t));
}

enum class ValPat { Mixed, OftenZero, NeverZero };

void fill(std::vector<std::uint16_t>& data, std::size_t n, ValPat pat) {
  data.resize(n);
  std::uint64_t s = 0xA5A5C3C3D2D2E1E1ull;
  for (std::size_t i = 0; i < n; ++i) {
    s = s * 6364136223846793005ull + 1ull;
    const auto raw = static_cast<std::uint16_t>(s);
    switch (pat) {
    case ValPat::Mixed:
      data[i] = raw;
      break;
    case ValPat::OftenZero:
      data[i] = ((s & 3u) == 0u) ? std::uint16_t{0}
                                 : static_cast<std::uint16_t>(raw | 1u);
      break;
    case ValPat::NeverZero:
      data[i] = static_cast<std::uint16_t>(1u << (s % 16u));
      break;
    }
  }
}

int check_equiv(const std::vector<std::uint16_t>& data) {
  int mismatches = 0;
  for (std::size_t i = 0; i < data.size(); ++i) {
    const auto a = tzcnt_u16_old(data[i]);
    const auto b = tzcnt_u16_new(data[i]);
    if (a != b) {
      if (mismatches < 3) {
        std::printf("  mismatch i=%zu v=%u old=%u new=%u\n", i, data[i], a, b);
      }
      ++mismatches;
    }
  }
  return mismatches;
}

XER_NOINLINE std::uint64_t run_old(const std::uint16_t* data, std::size_t n,
                                   std::uint64_t rounds) {
  std::uint64_t acc = 0;
  for (std::uint64_t r = 0; r < rounds; ++r) {
    const auto salt = static_cast<std::uint16_t>(r);
    for (std::size_t i = 0; i < n; ++i) {
      acc += tzcnt_u16_old(static_cast<std::uint16_t>(data[i] + salt));
    }
  }
  escape_u64(acc);
  return acc;
}

XER_NOINLINE std::uint64_t run_new(const std::uint16_t* data, std::size_t n,
                                   std::uint64_t rounds) {
  std::uint64_t acc = 0;
  for (std::uint64_t r = 0; r < rounds; ++r) {
    const auto salt = static_cast<std::uint16_t>(r);
    for (std::size_t i = 0; i < n; ++i) {
      acc += tzcnt_u16_new(static_cast<std::uint16_t>(data[i] + salt));
    }
  }
  escape_u64(acc);
  return acc;
}

void bench(const char* name, ValPat pat, std::size_t n, std::uint64_t rounds) {
  std::vector<std::uint16_t> data;
  fill(data, n, pat);
  const int mismatches = check_equiv(data);
  if (mismatches != 0) {
    std::printf("%-14s EQUIV_FAIL mismatches=%d (skip)\n", name, mismatches);
    return;
  }

  const std::uint64_t ops = static_cast<std::uint64_t>(n) * rounds;
  Stats s[2];
  measure2(
      [&] {
        return ns_total([&] { run_old(data.data(), n, rounds); }) /
               static_cast<double>(ops);
      },
      [&] {
        return ns_total([&] { run_new(data.data(), n, rounds); }) /
               static_cast<double>(ops);
      },
      s);

  const double delta = s[1].med_ns - s[0].med_ns; // new - old
  const double pct =
      s[0].med_ns > 0.0 ? (100.0 * delta / s[0].med_ns) : 0.0;
  std::printf("%-14s %8zu %8llu  %8.4f %6.4f  %8.4f %6.4f  %+8.4f %+7.2f%%\n",
              name, n, static_cast<unsigned long long>(rounds), s[0].med_ns,
              s[0].iqr_ns, s[1].med_ns, s[1].iqr_ns, delta, pct);
  std::fflush(stdout);
}

} // namespace

int main() {
  pin_thread();
  std::printf("u16 tzcnt old(_tzcnt_u32|mask) vs new(_tzcnt_u16) MSVC /arch:AVX2\n");
  std::printf("ns/op median of 9 rotated samples; iqr = p75-p25\n");
  std::printf("delta = new - old (negative => new faster)\n");
  std::printf("%-14s %8s %8s  %8s %6s  %8s %6s  %8s %8s\n", "bench", "n",
              "rounds", "old", "iqr", "new", "iqr", "d(n-o)", "pct");

  constexpr std::size_t N = 1 << 16;
  constexpr std::uint64_t R = 800;
  bench("mixed", ValPat::Mixed, N, R);
  bench("zeroish", ValPat::OftenZero, N, R);
  bench("nonzero", ValPat::NeverZero, N, R);

  {
    std::vector<std::uint16_t> edge{0, 1, 2, 0x8000, 0xFFFF};
    if (check_equiv(edge) != 0) {
      std::printf("edge EQUIV_FAIL\n");
      return 1;
    }
  }

  std::printf("sink=%llu\n", static_cast<unsigned long long>(g_sink));
  std::printf("PERF_DONE\n");
  return 0;
}
