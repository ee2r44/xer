// xer::bitset vs std::bitset throughput.
//
// Build (optimized):
//   MSVC:  cl /std:c++latest /O2 /DNDEBUG /EHsc /Iinclude /Fe:tests\container\xer_bitset_perf_msvc.exe tests\container\xer_bitset_perf.cpp
//   GCC:   g++ -std=c++23 -O3 -DNDEBUG -pthread -Iinclude -o tests/container/xer_bitset_perf_gcc tests/container/xer_bitset_perf.cpp
//   Clang: clang++ -std=c++23 -O3 -DNDEBUG -pthread -Iinclude -o tests/container/xer_bitset_perf_clang tests/container/xer_bitset_perf.cpp

#if defined(_MSC_VER)
#  if defined(_DEBUG) || !defined(NDEBUG)
#    error "Build this perf test with /O2 /DNDEBUG"
#  endif
#elif !defined(__OPTIMIZE__) || !defined(NDEBUG)
#  error "Build this perf test with -O3 -DNDEBUG"
#endif

#include "xer/container/xer_bitset.h"
#include "xer/macro/xer_macro.h"

#include <algorithm>
#include <bitset>
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

void fill_indices(std::vector<std::size_t>& idx, std::size_t n, std::size_t bits) {
  idx.resize(n);
  std::uint64_t s = 0x9E3779B97F4A7C15ull ^ (bits * 0xC0FFEEULL);
  for (std::size_t i = 0; i < n; ++i) {
    s = s * 6364136223846793005ull + 1ull;
    idx[i] = static_cast<std::size_t>(s % bits);
  }
}

template <std::size_t N>
int check_equivalence(const std::vector<std::size_t>& idx) {
  xer::bitset<N> x{};
  std::bitset<N> s;
  int mismatches = 0;
  for (std::size_t i = 0; i < idx.size(); ++i) {
    const std::size_t p = idx[i];
    const bool v = (i & 1u) != 0u;
    x.set_unchecked(p, v);
    s[p] = v;
  }
  for (std::size_t p = 0; p < N; ++p) {
    if (x.get_unchecked(p) != s[p]) {
      if (mismatches < 3) {
        std::printf("  mismatch N=%zu p=%zu xer=%d std=%d\n", N, p,
                    static_cast<int>(x.get_unchecked(p)),
                    static_cast<int>(s[p]));
      }
      ++mismatches;
    }
  }
  x.flip_all();
  s.flip();
  for (std::size_t p = 0; p < N; ++p) {
    if (x.get_unchecked(p) != s[p]) {
      ++mismatches;
    }
  }
  return mismatches;
}

// ---- per-bit set(true) ----
template <std::size_t N>
XER_NOINLINE std::uint64_t run_xer_set(const std::size_t* idx, std::size_t n,
                                       std::uint64_t rounds) {
  xer::bitset<N> bs{};
  for (std::uint64_t r = 0; r < rounds; ++r) {
    for (std::size_t i = 0; i < n; ++i) {
      bs.set_unchecked(idx[i]);
    }
  }
  std::uint64_t acc = 0;
  for (std::size_t p = 0; p < N; ++p) {
    acc += static_cast<std::uint64_t>(bs.get_unchecked(p));
  }
  escape_u64(acc);
  return acc;
}

template <std::size_t N>
XER_NOINLINE std::uint64_t run_std_set(const std::size_t* idx, std::size_t n,
                                       std::uint64_t rounds) {
  std::bitset<N> bs;
  for (std::uint64_t r = 0; r < rounds; ++r) {
    for (std::size_t i = 0; i < n; ++i) {
      bs[idx[i]] = true;
    }
  }
  std::uint64_t acc = 0;
  for (std::size_t p = 0; p < N; ++p) {
    acc += static_cast<std::uint64_t>(bs[p]);
  }
  escape_u64(acc);
  return acc;
}

// ---- per-bit get ----
template <std::size_t N>
XER_NOINLINE std::uint64_t run_xer_get(xer::bitset<N>& bs, const std::size_t* idx,
                                       std::size_t n, std::uint64_t rounds) {
  std::uint64_t acc = 0;
  for (std::uint64_t r = 0; r < rounds; ++r) {
    for (std::size_t i = 0; i < n; ++i) {
      acc += static_cast<std::uint64_t>(bs.get_unchecked(idx[i]));
    }
  }
  escape_u64(acc);
  return acc;
}

template <std::size_t N>
XER_NOINLINE std::uint64_t run_std_get(std::bitset<N>& bs, const std::size_t* idx,
                                       std::size_t n, std::uint64_t rounds) {
  std::uint64_t acc = 0;
  for (std::uint64_t r = 0; r < rounds; ++r) {
    for (std::size_t i = 0; i < n; ++i) {
      acc += static_cast<std::uint64_t>(bs[idx[i]]);
    }
  }
  escape_u64(acc);
  return acc;
}

// ---- per-bit flip ----
template <std::size_t N>
XER_NOINLINE std::uint64_t run_xer_flip(const std::size_t* idx, std::size_t n,
                                        std::uint64_t rounds) {
  xer::bitset<N> bs{};
  for (std::uint64_t r = 0; r < rounds; ++r) {
    for (std::size_t i = 0; i < n; ++i) {
      bs.flip_unchecked(idx[i]);
    }
  }
  std::uint64_t acc = 0;
  for (std::size_t p = 0; p < N; ++p) {
    acc += static_cast<std::uint64_t>(bs.get_unchecked(p));
  }
  escape_u64(acc);
  return acc;
}

template <std::size_t N>
XER_NOINLINE std::uint64_t run_std_flip(const std::size_t* idx, std::size_t n,
                                        std::uint64_t rounds) {
  std::bitset<N> bs;
  for (std::uint64_t r = 0; r < rounds; ++r) {
    for (std::size_t i = 0; i < n; ++i) {
      // operator[] path: no bounds check (fair vs xer *_unchecked).
      bs[idx[i]] = !bs[idx[i]];
    }
  }
  std::uint64_t acc = 0;
  for (std::size_t p = 0; p < N; ++p) {
    acc += static_cast<std::uint64_t>(bs[p]);
  }
  escape_u64(acc);
  return acc;
}

// ---- whole-set ops ----
template <std::size_t N>
XER_NOINLINE std::uint64_t run_xer_all(std::uint64_t rounds) {
  xer::bitset<N> bs{};
  for (std::uint64_t r = 0; r < rounds; ++r) {
    bs.set_all();
    bs.flip_all();
    bs.reset_all();
    bs.set_all(true);
    bs.set_all(false);
  }
  escape_u64(static_cast<std::uint64_t>(bs.get_unchecked(0)));
  return static_cast<std::uint64_t>(bs.get_unchecked(0));
}

template <std::size_t N>
XER_NOINLINE std::uint64_t run_std_all(std::uint64_t rounds) {
  std::bitset<N> bs;
  for (std::uint64_t r = 0; r < rounds; ++r) {
    bs.set();
    bs.flip();
    bs.reset();
    bs.set();
    bs.reset();
  }
  escape_u64(static_cast<std::uint64_t>(bs[0]));
  return static_cast<std::uint64_t>(bs[0]);
}

void print_row(const char* name, std::size_t n, std::uint64_t rounds,
               const Stats s[2]) {
  const double delta = s[0].med_ns - s[1].med_ns; // xer - std
  const double pct =
      s[1].med_ns > 0.0 ? (100.0 * delta / s[1].med_ns) : 0.0;
  std::printf("%-18s %8zu %8llu  %8.4f %6.4f  %8.4f %6.4f  %+8.4f %+7.2f%%\n",
              name, n, static_cast<unsigned long long>(rounds), s[0].med_ns,
              s[0].iqr_ns, s[1].med_ns, s[1].iqr_ns, delta, pct);
  std::fflush(stdout);
}

template <std::size_t N>
void bench_set(const char* name, std::size_t n_idx, std::uint64_t rounds) {
  std::vector<std::size_t> idx;
  fill_indices(idx, n_idx, N);
  const int mismatches = check_equivalence<N>(idx);
  if (mismatches != 0) {
    std::printf("%-18s EQUIV_FAIL mismatches=%d (skip)\n", name, mismatches);
    return;
  }
  const std::uint64_t ops = static_cast<std::uint64_t>(n_idx) * rounds;
  Stats s[2];
  measure2(
      [&] {
        return ns_total([&] { run_xer_set<N>(idx.data(), n_idx, rounds); }) /
               static_cast<double>(ops);
      },
      [&] {
        return ns_total([&] { run_std_set<N>(idx.data(), n_idx, rounds); }) /
               static_cast<double>(ops);
      },
      s);
  print_row(name, n_idx, rounds, s);
}

template <std::size_t N>
void bench_get(const char* name, std::size_t n_idx, std::uint64_t rounds) {
  std::vector<std::size_t> idx;
  fill_indices(idx, n_idx, N);
  xer::bitset<N> xb{};
  std::bitset<N> sb;
  for (std::size_t i = 0; i < n_idx; ++i) {
    const bool v = (i & 1u) != 0u;
    xb.set_unchecked(idx[i], v);
    sb[idx[i]] = v;
  }
  const std::uint64_t ops = static_cast<std::uint64_t>(n_idx) * rounds;
  Stats s[2];
  measure2(
      [&] {
        return ns_total([&] {
                 run_xer_get<N>(xb, idx.data(), n_idx, rounds);
               }) /
               static_cast<double>(ops);
      },
      [&] {
        return ns_total([&] {
                 run_std_get<N>(sb, idx.data(), n_idx, rounds);
               }) /
               static_cast<double>(ops);
      },
      s);
  print_row(name, n_idx, rounds, s);
}

template <std::size_t N>
void bench_flip(const char* name, std::size_t n_idx, std::uint64_t rounds) {
  std::vector<std::size_t> idx;
  fill_indices(idx, n_idx, N);
  const std::uint64_t ops = static_cast<std::uint64_t>(n_idx) * rounds;
  Stats s[2];
  measure2(
      [&] {
        return ns_total([&] { run_xer_flip<N>(idx.data(), n_idx, rounds); }) /
               static_cast<double>(ops);
      },
      [&] {
        return ns_total([&] { run_std_flip<N>(idx.data(), n_idx, rounds); }) /
               static_cast<double>(ops);
      },
      s);
  print_row(name, n_idx, rounds, s);
}

template <std::size_t N>
void bench_all(const char* name, std::uint64_t rounds) {
  Stats s[2];
  measure2(
      [&] {
        return ns_total([&] { run_xer_all<N>(rounds); }) /
               static_cast<double>(rounds);
      },
      [&] {
        return ns_total([&] { run_std_all<N>(rounds); }) /
               static_cast<double>(rounds);
      },
      s);
  // n column = bit width for whole-set benches
  print_row(name, N, rounds, s);
}

template <std::size_t N>
void bench_size(const char* tag) {
  constexpr std::size_t n_idx = 1 << 12;
  constexpr std::uint64_t rounds_bit = 800;
  constexpr std::uint64_t rounds_all = 200000;

  char buf[64];
  std::snprintf(buf, sizeof(buf), "%s_set", tag);
  bench_set<N>(buf, n_idx, rounds_bit);
  std::snprintf(buf, sizeof(buf), "%s_get", tag);
  bench_get<N>(buf, n_idx, rounds_bit);
  std::snprintf(buf, sizeof(buf), "%s_flip", tag);
  bench_flip<N>(buf, n_idx, rounds_bit);
  std::snprintf(buf, sizeof(buf), "%s_all", tag);
  bench_all<N>(buf, rounds_all);
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
  std::printf("xer::bitset vs std::bitset perf compiler=%s arch=%s\n", compiler,
              XER_ARCH(64) ? "64" : "32");
  std::printf("ns/op median of 9 rotated samples; iqr = p75-p25\n");
  std::printf("delta = xer - std (negative => xer faster)\n");
  std::printf("bit ops: ns per index access; *_all: ns per whole-set sequence\n");
  std::printf("%-18s %8s %8s  %8s %6s  %8s %6s  %8s %8s\n", "bench", "n",
              "rounds", "xer", "iqr", "std", "iqr", "d(x-s)", "pct");

  // Scalar-table sizes on 64-bit; 32/64 exercise gate boundary.
  bench_size<8>("N8");
  bench_size<32>("N32");
  bench_size<64>("N64");
  // Array-table sizes
  bench_size<65>("N65");
  bench_size<128>("N128");
  bench_size<256>("N256");
  bench_size<1024>("N1024");

  std::printf("sink=%llu\n", static_cast<unsigned long long>(g_sink));
  std::printf("PERF_DONE\n");
  return 0;
}
