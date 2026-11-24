// xer::countr_zero vs std::countr_zero throughput.
//
// Build (optimized) - six environments:
//   MSVC:        cl /std:c++latest /O2 /DNDEBUG /EHsc /Iinclude /Fe:tests\bit\xer_countr_zero_perf_msvc.exe tests\bit\xer_countr_zero_perf.cpp include\xer\env\xer_env.cpp
//   MSVC+AVX2:   cl /std:c++latest /O2 /arch:AVX2 /DNDEBUG /EHsc /Iinclude /Fe:tests\bit\xer_countr_zero_perf_msvc_avx2.exe tests\bit\xer_countr_zero_perf.cpp include\xer\env\xer_env.cpp
//   GCC:         g++ -std=c++23 -O3 -DNDEBUG -pthread -Iinclude -o tests/bit/xer_countr_zero_perf_gcc tests/bit/xer_countr_zero_perf.cpp include/xer/env/xer_env.cpp
//   GCC+BMI:     g++ -std=c++23 -O3 -mbmi -DNDEBUG -pthread -Iinclude -o tests/bit/xer_countr_zero_perf_gcc_bmi tests/bit/xer_countr_zero_perf.cpp include/xer/env/xer_env.cpp
//   Clang:       clang++ -std=c++23 -O3 -DNDEBUG -pthread -Iinclude -o tests/bit/xer_countr_zero_perf_clang tests/bit/xer_countr_zero_perf.cpp include/xer/env/xer_env.cpp
//   Clang+BMI:   clang++ -std=c++23 -O3 -mbmi -DNDEBUG -pthread -Iinclude -o tests/bit/xer_countr_zero_perf_clang_bmi tests/bit/xer_countr_zero_perf.cpp include/xer/env/xer_env.cpp

#if defined(_MSC_VER)
#  if defined(_DEBUG) || !defined(NDEBUG)
#    error "Build this perf test with /O2 /DNDEBUG"
#  endif
#elif !defined(__OPTIMIZE__) || !defined(NDEBUG)
#  error "Build this perf test with -O3 -DNDEBUG"
#endif

#include "xer/bit/xer_countr_zero.h"
#include "xer/env/xer_env.h"
#include "xer/macro/xer_macro.h"

#include <algorithm>
#include <bit>
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

enum class ValPat { Mixed, OftenZero, NeverZero };

template <typename T>
void fill(std::vector<T>& data, std::size_t n, ValPat pat) {
  data.resize(n);
  std::uint64_t s = 0xC0FFEEULL ^ (sizeof(T) * 0x9E3779B97F4A7C15ull);
  constexpr std::uint32_t bits = static_cast<std::uint32_t>(sizeof(T) * 8u);
  for (std::size_t i = 0; i < n; ++i) {
    s = s * 6364136223846793005ull + 1ull;
    const T raw = static_cast<T>(s);
    switch (pat) {
    case ValPat::Mixed:
      data[i] = raw;
      break;
    case ValPat::OftenZero:
      data[i] = ((s & 3u) == 0u) ? T{0} : static_cast<T>(raw | T{1});
      break;
    case ValPat::NeverZero: {
      const std::uint32_t sh = static_cast<std::uint32_t>(s % bits);
      data[i] = static_cast<T>(T{1} << sh);
      break;
    }
    }
  }
}

template <typename T>
int check_equivalence(const std::vector<T>& data) {
  int mismatches = 0;
  for (std::size_t i = 0; i < data.size(); ++i) {
    const auto x = xer::countr_zero(data[i]);
    const auto s = static_cast<std::uint32_t>(std::countr_zero(data[i]));
    if (x != s) {
      if (mismatches < 3) {
        std::printf("  mismatch i=%zu v=%llu xer=%u std=%u\n", i,
                    static_cast<unsigned long long>(data[i]), x, s);
      }
      ++mismatches;
    }
  }
  return mismatches;
}

template <typename T>
XER_NOINLINE std::uint64_t run_xer(const T* data, std::size_t n,
                                   std::uint64_t rounds) {
  std::uint64_t acc = 0;
  for (std::uint64_t r = 0; r < rounds; ++r) {
    const T salt = static_cast<T>(r);
    for (std::size_t i = 0; i < n; ++i) {
      acc += xer::countr_zero(static_cast<T>(data[i] + salt));
    }
  }
  escape_u64(acc);
  return acc;
}

template <typename T>
XER_NOINLINE std::uint64_t run_std(const T* data, std::size_t n,
                                   std::uint64_t rounds) {
  std::uint64_t acc = 0;
  for (std::uint64_t r = 0; r < rounds; ++r) {
    const T salt = static_cast<T>(r);
    for (std::size_t i = 0; i < n; ++i) {
      acc += static_cast<std::uint64_t>(
          std::countr_zero(static_cast<T>(data[i] + salt)));
    }
  }
  escape_u64(acc);
  return acc;
}

template <typename T>
void bench(const char* name, ValPat pat, std::size_t n, std::uint64_t rounds) {
  std::vector<T> data;
  fill(data, n, pat);

  const int mismatches = check_equivalence<T>(data);
  if (mismatches != 0) {
    std::printf("%-18s EQUIV_FAIL mismatches=%d (skip)\n", name, mismatches);
    return;
  }

  const std::uint64_t ops = static_cast<std::uint64_t>(n) * rounds;
  Stats s[2];
  measure2(
      [&] {
        return ns_total([&] { run_xer(data.data(), n, rounds); }) /
               static_cast<double>(ops);
      },
      [&] {
        return ns_total([&] { run_std(data.data(), n, rounds); }) /
               static_cast<double>(ops);
      },
      s);

  const double delta = s[0].med_ns - s[1].med_ns; // xer - std
  const double pct =
      s[1].med_ns > 0.0 ? (100.0 * delta / s[1].med_ns) : 0.0;
  std::printf("%-18s %8zu %8llu  %8.4f %6.4f  %8.4f %6.4f  %+8.4f %+7.2f%%\n",
              name, n, static_cast<unsigned long long>(rounds), s[0].med_ns,
              s[0].iqr_ns, s[1].med_ns, s[1].iqr_ns, delta, pct);
  std::fflush(stdout);
}

const char* build_tag() noexcept {
#if XER_COMPILER(MSVC)
#  if defined(__AVX2__)
  return "msvc+avx2";
#  else
  return "msvc";
#  endif
#elif XER_COMPILER(GCC)
#  if defined(__BMI__)
  return "gcc+bmi";
#  else
  return "gcc";
#  endif
#elif XER_COMPILER(CLANG)
#  if defined(__BMI__)
  return "clang+bmi";
#  else
  return "clang";
#  endif
#else
  return "unknown";
#endif
}

} // namespace

int main() {
  pin_thread();

  const int tzcnt_rt =
#if XER_CPU(X86_64) || XER_CPU(X86_32)
      static_cast<int>(xer::is_tzcnt_supported());
#else
      0;
#endif

  std::printf("xer::countr_zero vs std::countr_zero perf build=%s\n",
              build_tag());
  std::printf("XER_BMI1_AVAILABLE=%d tzcnt_runtime=%d\n",
              (int)XER_BMI1_AVAILABLE, tzcnt_rt);
  std::printf("ns/op median of 9 rotated samples; iqr = p75-p25\n");
  std::printf("delta = xer - std (negative => xer faster)\n");
  std::printf("%-18s %8s %8s  %8s %6s  %8s %6s  %8s %8s\n", "bench", "n",
              "rounds", "xer", "iqr", "std", "iqr", "d(x-s)", "pct");

  constexpr std::size_t N = 1 << 16;
  constexpr std::uint64_t R = 400;

  bench<std::uint8_t>("u8_mixed", ValPat::Mixed, N, R);
  bench<std::uint8_t>("u8_zeroish", ValPat::OftenZero, N, R);
  bench<std::uint8_t>("u8_nonzero", ValPat::NeverZero, N, R);

  bench<std::uint16_t>("u16_mixed", ValPat::Mixed, N, R);
  bench<std::uint16_t>("u16_zeroish", ValPat::OftenZero, N, R);
  bench<std::uint16_t>("u16_nonzero", ValPat::NeverZero, N, R);

  bench<std::uint32_t>("u32_mixed", ValPat::Mixed, N, R);
  bench<std::uint32_t>("u32_zeroish", ValPat::OftenZero, N, R);
  bench<std::uint32_t>("u32_nonzero", ValPat::NeverZero, N, R);

  bench<std::uint64_t>("u64_mixed", ValPat::Mixed, N, R);
  bench<std::uint64_t>("u64_zeroish", ValPat::OftenZero, N, R);
  bench<std::uint64_t>("u64_nonzero", ValPat::NeverZero, N, R);

  std::printf("sink=%llu\n", static_cast<unsigned long long>(g_sink));
  std::printf("PERF_DONE\n");
  return 0;
}
