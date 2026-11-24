// xer::arena vs std::pmr::monotonic_buffer_resource (+ pmr::vector) throughput.
//
// Build (optimized):
//   MSVC:  cl /std:c++latest /O2 /DNDEBUG /EHsc /Iinclude /Fe:tests\container\xer_arena_perf_msvc.exe tests\container\xer_arena_perf.cpp
//   GCC:   g++ -std=c++23 -O3 -DNDEBUG -pthread -Iinclude -o tests/container/xer_arena_perf_gcc tests/container/xer_arena_perf.cpp
//   Clang: clang++ -std=c++23 -O3 -DNDEBUG -pthread -Iinclude -o tests/container/xer_arena_perf_clang tests/container/xer_arena_perf.cpp

#if defined(_MSC_VER)
#  if defined(_DEBUG) || !defined(NDEBUG)
#    error "Build this perf test with /O2 /DNDEBUG"
#  endif
#elif !defined(__OPTIMIZE__) || !defined(NDEBUG)
#  error "Build this perf test with -O3 -DNDEBUG"
#endif

#include "xer/container/xer_arena.h"
#include "xer/macro/xer_macro.h"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <memory_resource>
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

volatile std::uintptr_t g_sink = 0;

XER_NOINLINE void escape_ptr(const void* p) noexcept {
#if defined(__GNUC__) || defined(__clang__)
  asm volatile("" : : "r"(p) : "memory");
#endif
  g_sink ^= reinterpret_cast<std::uintptr_t>(p);
}

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

void report(const char* tag, const char* op, std::uint64_t ops, const Stats s[2]) {
  const double xer = s[0].med_ns / static_cast<double>(ops);
  const double pmr = s[1].med_ns / static_cast<double>(ops);
  std::printf(
      "%-22s %-14s %10.3f/%-7.3f %10.3f/%-7.3f %7.3f\n",
      tag,
      op,
      xer,
      s[0].iqr_ns / static_cast<double>(ops),
      pmr,
      s[1].iqr_ns / static_cast<double>(ops),
      xer / pmr);
}

// --- workloads: xer = [0], pmr monotonic = [1] ---

// Many fixed-size bump allocs in one lifetime (no mid-frame reset).
template <std::size_t Align, std::size_t Bytes, std::uint32_t N, std::uint32_t Rounds>
void bench_raw_bump(const char* tag) {
  Stats s[2];
  measure2(
      [] {
        return ns_total([] {
          for (std::uint32_t r = 0; r < Rounds; ++r) {
            xer::arena a;
            for (std::uint32_t i = 0; i < N; ++i) {
              void* p = a.align_alloc(Align, Bytes);
              escape_ptr(p);
            }
          }
        });
      },
      [] {
        return ns_total([] {
          for (std::uint32_t r = 0; r < Rounds; ++r) {
            std::pmr::monotonic_buffer_resource mbr;
            for (std::uint32_t i = 0; i < N; ++i) {
              void* p = mbr.allocate(Bytes, Align);
              escape_ptr(p);
            }
          }
        });
      },
      s);
  report(tag, "raw_bump", static_cast<std::uint64_t>(N) * Rounds, s);
}

// Same, but reuse one resource with reset/release each round (page churn).
template <std::size_t Align, std::size_t Bytes, std::uint32_t N, std::uint32_t Rounds>
void bench_raw_reset(const char* tag) {
  Stats s[2];
  measure2(
      [] {
        return ns_total([] {
          xer::arena a;
          for (std::uint32_t r = 0; r < Rounds; ++r) {
            for (std::uint32_t i = 0; i < N; ++i) {
              void* p = a.align_alloc(Align, Bytes);
              escape_ptr(p);
            }
            a.reset();
          }
        });
      },
      [] {
        return ns_total([] {
          std::pmr::monotonic_buffer_resource mbr;
          for (std::uint32_t r = 0; r < Rounds; ++r) {
            for (std::uint32_t i = 0; i < N; ++i) {
              void* p = mbr.allocate(Bytes, Align);
              escape_ptr(p);
            }
            mbr.release();
          }
        });
      },
      s);
  report(tag, "raw_reset", static_cast<std::uint64_t>(N) * Rounds, s);
}

// Typed alloc<T> vs mbr.allocate(sizeof(T), alignof(T)).
template <typename T, std::uint32_t N, std::uint32_t Rounds>
void bench_typed(const char* tag) {
  Stats s[2];
  measure2(
      [] {
        return ns_total([] {
          for (std::uint32_t r = 0; r < Rounds; ++r) {
            xer::arena a;
            for (std::uint32_t i = 0; i < N; ++i) {
              T* p = a.alloc<T>();
              escape_ptr(p);
            }
          }
        });
      },
      [] {
        return ns_total([] {
          for (std::uint32_t r = 0; r < Rounds; ++r) {
            std::pmr::monotonic_buffer_resource mbr;
            for (std::uint32_t i = 0; i < N; ++i) {
              void* p = mbr.allocate(sizeof(T), alignof(T));
              escape_ptr(p);
            }
          }
        });
      },
      s);
  report(tag, "typed_bump", static_cast<std::uint64_t>(N) * Rounds, s);
}

struct alignas(64) CacheLinePod {
  std::uint64_t v[8];
};

// Mixed sizes/aligns in one frame.
template <std::uint32_t N, std::uint32_t Rounds>
void bench_mixed(const char* tag) {
  Stats s[2];
  measure2(
      [] {
        return ns_total([] {
          for (std::uint32_t r = 0; r < Rounds; ++r) {
            xer::arena a;
            for (std::uint32_t i = 0; i < N; ++i) {
              switch (i & 3u) {
              case 0:
                escape_ptr(a.alloc<std::uint32_t>());
                break;
              case 1:
                escape_ptr(a.alloc<std::uint64_t>());
                break;
              case 2:
                escape_ptr(a.align_alloc(16, 48));
                break;
              default:
                escape_ptr(a.alloc<CacheLinePod>());
                break;
              }
            }
          }
        });
      },
      [] {
        return ns_total([] {
          for (std::uint32_t r = 0; r < Rounds; ++r) {
            std::pmr::monotonic_buffer_resource mbr;
            for (std::uint32_t i = 0; i < N; ++i) {
              switch (i & 3u) {
              case 0:
                escape_ptr(mbr.allocate(sizeof(std::uint32_t), alignof(std::uint32_t)));
                break;
              case 1:
                escape_ptr(mbr.allocate(sizeof(std::uint64_t), alignof(std::uint64_t)));
                break;
              case 2:
                escape_ptr(mbr.allocate(48, 16));
                break;
              default:
                escape_ptr(mbr.allocate(sizeof(CacheLinePod), alignof(CacheLinePod)));
                break;
              }
            }
          }
        });
      },
      s);
  report(tag, "mixed_bump", static_cast<std::uint64_t>(N) * Rounds, s);
}

// pmr::vector push_back into monotonic vs arena alloc of N ints + fill.
// pmr path uses reserve to avoid geometric realloc waste on monotonic.
template <std::uint32_t N, std::uint32_t Rounds>
void bench_pmr_vector(const char* tag) {
  Stats s[2];
  measure2(
      [] {
        return ns_total([] {
          for (std::uint32_t r = 0; r < Rounds; ++r) {
            xer::arena a;
            int* p = a.alloc<int>(N);
            for (std::uint32_t i = 0; i < N; ++i) {
              p[i] = static_cast<int>(i);
            }
            escape_ptr(p);
            escape_u64(static_cast<std::uint64_t>(p[N - 1]));
          }
        });
      },
      [] {
        return ns_total([] {
          for (std::uint32_t r = 0; r < Rounds; ++r) {
            std::pmr::monotonic_buffer_resource mbr;
            std::pmr::vector<int> v(&mbr);
            v.reserve(N);
            for (std::uint32_t i = 0; i < N; ++i) {
              v.push_back(static_cast<int>(i));
            }
            escape_ptr(v.data());
            escape_u64(static_cast<std::uint64_t>(v.back()));
          }
        });
      },
      s);
  report(tag, "vec_fill", static_cast<std::uint64_t>(N) * Rounds, s);
}

// Large single allocation (~256KiB) to stress page growth without old BPGate.
template <std::uint32_t Rounds>
void bench_large(const char* tag) {
  constexpr std::size_t bytes = 256u * 1024u;
  Stats s[2];
  measure2(
      [] {
        return ns_total([] {
          for (std::uint32_t r = 0; r < Rounds; ++r) {
            xer::arena a;
            void* p = a.align_alloc(32, bytes);
            escape_ptr(p);
          }
        });
      },
      [] {
        return ns_total([] {
          for (std::uint32_t r = 0; r < Rounds; ++r) {
            std::pmr::monotonic_buffer_resource mbr;
            void* p = mbr.allocate(bytes, 32);
            escape_ptr(p);
          }
        });
      },
      s);
  report(tag, "large_one", Rounds, s);
}

}  // namespace

int main() {
  pin_thread();
  std::printf("xer_arena vs pmr::monotonic_buffer_resource ns/op med/iqr\n");
  std::printf("compiler=%s sink=%llu\n", compiler_name(),
              static_cast<unsigned long long>(g_sink));
  std::printf(
      "%-22s %-14s %18s %18s %7s\n",
      "tag",
      "op",
      "xer/iqr",
      "pmr/iqr",
      "x/p");

  bench_raw_bump<8, 16, 4096, 64>("bump_16x4k");
  bench_raw_bump<8, 64, 2048, 64>("bump_64x2k");
  bench_raw_bump<64, 64, 1024, 64>("bump_a64x1k");
  bench_raw_reset<8, 16, 1024, 128>("reset_16x1k");
  bench_typed<int, 4096, 64>("typed_int");
  bench_typed<CacheLinePod, 1024, 64>("typed_cl64");
  bench_mixed<2048, 64>("mixed");
  bench_pmr_vector<4096, 64>("pmr_vector_4k");
  bench_pmr_vector<65536, 16>("pmr_vector_64k");
  bench_large<256>("large_256k");

  std::printf("sink=%llu\n", static_cast<unsigned long long>(g_sink));
  return 0;
}
