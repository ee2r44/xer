// Allocation throughput: malloc/free vs new/delete vs raw_alloc/raw_free.
//
// Optimize or the numbers are meaningless:
//   MSVC:  cl /std:c++latest /O2 /DNDEBUG /EHsc /Iinclude /Fe:tests\memory\xer_raw_memory_perf_msvc.exe tests\memory\xer_raw_memory_perf.cpp
//   GCC:   g++ -std=c++23 -O3 -DNDEBUG -pthread -Iinclude -o tests/memory/xer_raw_memory_perf_gcc tests/memory/xer_raw_memory_perf.cpp
//   Clang: clang++ -std=c++23 -O3 -DNDEBUG -pthread -Iinclude -o tests/memory/xer_raw_memory_perf_clang tests/memory/xer_raw_memory_perf.cpp
//
// Each cell is the median ns per alloc+free pair. Samples are rotated so no
// API always runs first. IQR is the spread of the 7 samples (noise).

#if defined(_MSC_VER)
#  if defined(_DEBUG) || !defined(NDEBUG)
#    error "Build this perf test with /O2 /DNDEBUG"
#  endif
#elif !defined(__OPTIMIZE__) || !defined(NDEBUG)
#  error "Build this perf test with -O3 -DNDEBUG"
#endif

#include "xer/memory/xer_raw_alloc.h"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

#if defined(_WIN32)
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <malloc.h>
#  include <windows.h>
#else
#  include <pthread.h>
#  include <sched.h>
#endif

namespace {

enum class Kind { Malloc, NewDel, Raw };
enum class Touch { None, Ends, Fill };

volatile std::uintptr_t g_escape = 0;

inline void escape(const void* p) noexcept {
#if defined(__GNUC__) || defined(__clang__)
  asm volatile("" : : "r"(p) : "memory");
#endif
  g_escape = reinterpret_cast<std::uintptr_t>(p);
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

void* c_alloc(std::size_t bytes, std::size_t align) noexcept {
  if (align <= xer::details_::raw_alloc_basic_alignment) {
    return std::malloc(bytes);
  }
#if defined(_WIN32)
  return _aligned_malloc(bytes, align);
#else
  // Match aligned_alloc size rule used by xer::raw_alloc.
  const std::size_t rounded = (bytes + align - 1) & ~(align - 1);
  return std::aligned_alloc(align, rounded == 0 ? align : rounded);
#endif
}

void c_free(void* p, std::size_t align) noexcept {
  if (align <= xer::details_::raw_alloc_basic_alignment) {
    std::free(p);
    return;
  }
#if defined(_WIN32)
  _aligned_free(p);
#else
  std::free(p);
#endif
}

template <typename T, Kind K, std::size_t N>
T* alloc_n() noexcept(K != Kind::NewDel) {
  if constexpr (K == Kind::Malloc) {
    return static_cast<T*>(c_alloc(N * sizeof(T), alignof(T)));
  } else if constexpr (K == Kind::NewDel) {
    if constexpr (N == 1) {
      return new T;
    } else {
      return new T[N];
    }
  } else if constexpr (N == 1) {
    return xer::raw_alloc<T>();
  } else {
    return xer::raw_alloc<T>(N);
  }
}

template <typename T, Kind K, std::size_t N>
void free_n(T* p) noexcept {
  if constexpr (K == Kind::Malloc) {
    c_free(p, alignof(T));
  } else if constexpr (K == Kind::NewDel) {
    if constexpr (N == 1) {
      delete p;
    } else {
      delete[] p;
    }
  } else {
    xer::raw_free(p);
  }
}

template <typename T, Kind K>
T* alloc_runtime(std::size_t n) noexcept(K != Kind::NewDel) {
  if constexpr (K == Kind::Malloc) {
    return static_cast<T*>(c_alloc(n * sizeof(T), alignof(T)));
  } else if constexpr (K == Kind::NewDel) {
    return new T[n];
  } else {
    return xer::raw_alloc<T>(n);
  }
}

template <typename T, Kind K>
void free_runtime(T* p, std::size_t /*n*/) noexcept {
  if constexpr (K == Kind::Malloc) {
    c_free(p, alignof(T));
  } else if constexpr (K == Kind::NewDel) {
    delete[] p;
  } else {
    xer::raw_free(p);
  }
}

template <typename T, Kind K, Touch touch, std::size_t N>
void recycle(std::uint64_t iters) {
  constexpr std::size_t bytes = N * sizeof(T);
  for (std::uint64_t i = 0; i < iters; ++i) {
    T* p = alloc_n<T, K, N>();
    if (p != nullptr) {
      if constexpr (touch == Touch::Ends) {
        auto* b = reinterpret_cast<unsigned char*>(p);
        b[0] = 0xA5;
        b[bytes - 1] = 0x5A;
      } else if constexpr (touch == Touch::Fill) {
        std::memset(p, 0xA5, bytes);
      }
    }
    escape(p);
    free_n<T, K, N>(p);
  }
}

template <typename T, Kind K, std::size_t N>
void batch(T** ptrs, std::uint64_t rounds, std::size_t live, bool lifo) {
  for (std::uint64_t r = 0; r < rounds; ++r) {
    for (std::size_t i = 0; i < live; ++i) {
      ptrs[i] = alloc_n<T, K, N>();
      escape(ptrs[i]);
    }
    if (lifo) {
      for (std::size_t i = live; i-- > 0;) {
        free_n<T, K, N>(ptrs[i]);
      }
    } else {
      for (std::size_t i = 0; i < live; ++i) {
        free_n<T, K, N>(ptrs[i]);
      }
    }
  }
}

template <Kind K>
void mixed(unsigned char** ptrs, const std::size_t* sizes, std::size_t kinds,
           std::uint64_t rounds) {
  for (std::uint64_t r = 0; r < rounds; ++r) {
    for (std::size_t i = 0; i < kinds; ++i) {
      ptrs[i] = alloc_runtime<unsigned char, K>(sizes[i]);
      escape(ptrs[i]);
    }
    for (std::size_t i = kinds; i-- > 0;) {
      free_runtime<unsigned char, K>(ptrs[i], sizes[i]);
    }
  }
}

struct Stats {
  double med;
  double iqr;
};

template <typename F0, typename F1, typename F2>
void measure3(F0&& f0, F1&& f1, F2&& f2, Stats out[3]) {
  auto run = [&](int idx) -> double {
    switch (idx) {
      case 0:
        return f0();
      case 1:
        return f1();
      default:
        return f2();
    }
  };
  for (int k = 0; k < 3; ++k) {
    run(k);
  }
  for (int k = 2; k >= 0; --k) {
    run(k);
  }

  double samples[3][7];
  for (int rep = 0; rep < 7; ++rep) {
    const int start = rep % 3;
    for (int step = 0; step < 3; ++step) {
      const int idx = (start + step) % 3;
      samples[idx][rep] = run(idx);
    }
  }
  for (int idx = 0; idx < 3; ++idx) {
    double* s = samples[idx];
    std::sort(s, s + 7);
    out[idx].med = s[3];
    out[idx].iqr = s[5] - s[1];
  }
}

std::uint64_t iters_for_bytes(std::size_t bytes) noexcept {
  if (bytes <= 64) {
    return 300000;
  }
  if (bytes <= 256) {
    return 150000;
  }
  if (bytes <= 4096) {
    return 60000;
  }
  if (bytes <= 65536) {
    return 6000;
  }
  return 800;
}

using Clock = std::chrono::steady_clock;

template <typename F>
double ns_per(F&& f, std::uint64_t ops) {
  const auto t0 = Clock::now();
  f();
  const auto t1 = Clock::now();
  const double ns = std::chrono::duration<double, std::nano>(t1 - t0).count();
  return ns / static_cast<double>(ops);
}

void print_row(const char* pattern, const char* type, std::size_t bytes,
               std::size_t align, const Stats s[3]) noexcept {
  std::printf("%-14s %-14s %8zu %5zu  %8.2f %6.2f  %8.2f %6.2f  %8.2f %6.2f\n",
              pattern, type, bytes, align, s[0].med, s[0].iqr, s[1].med,
              s[1].iqr, s[2].med, s[2].iqr);
  std::fflush(stdout);
}

template <typename T, Touch touch, std::size_t N>
void bench_recycle(const char* pattern, const char* type) {
  const std::uint64_t iters = iters_for_bytes(N * sizeof(T));
  Stats s[3];
  measure3(
      [&] {
        return ns_per([&] { recycle<T, Kind::Malloc, touch, N>(iters); }, iters);
      },
      [&] {
        return ns_per([&] { recycle<T, Kind::NewDel, touch, N>(iters); }, iters);
      },
      [&] { return ns_per([&] { recycle<T, Kind::Raw, touch, N>(iters); }, iters); },
      s);
  print_row(pattern, type, N * sizeof(T), alignof(T), s);
}

template <typename T, std::size_t N>
void bench_batch(const char* pattern, const char* type, std::size_t live, bool lifo) {
  T* ptrs_store[512];
  if (live > 512) {
    std::printf("live too large\n");
    return;
  }
  const std::uint64_t rounds = iters_for_bytes(N * sizeof(T)) / 40;
  const std::uint64_t rounds_clamped = rounds < 200 ? 200 : rounds;
  const std::uint64_t ops = rounds_clamped * live;
  Stats s[3];
  measure3(
      [&] {
        return ns_per(
            [&] { batch<T, Kind::Malloc, N>(ptrs_store, rounds_clamped, live, lifo); },
            ops);
      },
      [&] {
        return ns_per(
            [&] { batch<T, Kind::NewDel, N>(ptrs_store, rounds_clamped, live, lifo); },
            ops);
      },
      [&] {
        return ns_per(
            [&] { batch<T, Kind::Raw, N>(ptrs_store, rounds_clamped, live, lifo); }, ops);
      },
      s);
  print_row(pattern, type, N * sizeof(T), alignof(T), s);
}

void bench_mixed() {
  constexpr std::size_t sizes[] = {8,   16,  24,  32,   48,   64,   96,   128,  192,
                                   256, 384, 512, 768,  1024, 1536, 2048, 3072, 4096};
  constexpr std::size_t kinds = sizeof(sizes) / sizeof(sizes[0]);
  unsigned char* ptrs[kinds];
  constexpr std::uint64_t rounds = 8000;
  const std::uint64_t ops = rounds * kinds;
  Stats s[3];
  measure3(
      [&] {
        return ns_per([&] { mixed<Kind::Malloc>(ptrs, sizes, kinds, rounds); }, ops);
      },
      [&] {
        return ns_per([&] { mixed<Kind::NewDel>(ptrs, sizes, kinds, rounds); }, ops);
      },
      [&] { return ns_per([&] { mixed<Kind::Raw>(ptrs, sizes, kinds, rounds); }, ops); },
      s);
  print_row("mixed-lifo", "u8 sizes", 0, 1, s);
}

struct B24 {
  char b[24];
};
struct B64 {
  char b[64];
};
struct B256 {
  char b[256];
};
struct B4096 {
  char b[4096];
};
struct alignas(16) A16 {
  char b[16];
};
struct alignas(64) A64 {
  char b[64];
};
struct alignas(128) A128 {
  char b[128];
};
struct alignas(256) A256 {
  char b[256];
};
struct alignas(64) A64Page {
  char b[4096];
};
struct Ntor {
  int v;
  Ntor() noexcept : v(0) {}
  ~Ntor() noexcept {}
};

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
  std::printf("xer_raw_alloc perf compiler=%s\n", compiler);
  std::printf("max_align=%zu raw_basic_align=%zu\n", alignof(std::max_align_t),
              xer::details_::raw_alloc_basic_alignment);
  std::printf(
      "ns/op is median of 7 rotated samples; iqr is sample spread (lower is stabler)\n");
  std::printf(
      "overaligned malloc column uses _aligned_malloc/aligned_alloc when align > basic\n");
  std::printf("%-14s %-14s %8s %5s  %8s %6s  %8s %6s  %8s %6s\n", "pattern",
              "type", "bytes", "align", "malloc", "iqr", "new", "iqr", "raw",
              "iqr");

  bench_recycle<char, Touch::None, 1>("recycle", "char");
  bench_recycle<std::uint32_t, Touch::None, 1>("recycle", "u32");
  bench_recycle<B24, Touch::None, 1>("recycle", "B24");
  bench_recycle<B64, Touch::None, 1>("recycle", "B64");
  bench_recycle<B256, Touch::None, 1>("recycle", "B256");
  bench_recycle<B4096, Touch::None, 1>("recycle", "B4096");
  bench_recycle<A16, Touch::None, 1>("recycle", "A16");
  bench_recycle<A64, Touch::None, 1>("recycle", "A64");
  bench_recycle<A128, Touch::None, 1>("recycle", "A128");
  bench_recycle<A256, Touch::None, 1>("recycle", "A256");
  bench_recycle<A64Page, Touch::None, 1>("recycle", "A64x4096");
  bench_recycle<Ntor, Touch::None, 1>("recycle", "Ntor");

  bench_recycle<unsigned char, Touch::None, 32>("recycle-n", "u8x32");
  bench_recycle<unsigned char, Touch::None, 128>("recycle-n", "u8x128");
  bench_recycle<unsigned char, Touch::None, 512>("recycle-n", "u8x512");
  bench_recycle<unsigned char, Touch::None, 4096>("recycle-n", "u8x4k");
  bench_recycle<unsigned char, Touch::None, 65536>("recycle-n", "u8x64k");
  bench_recycle<unsigned char, Touch::None, (1u << 20)>("recycle-n", "u8x1m");
  bench_recycle<std::uint64_t, Touch::None, 8>("recycle-n", "u64x8");
  bench_recycle<std::uint64_t, Touch::None, 128>("recycle-n", "u64x128");
  bench_recycle<std::uint64_t, Touch::None, 4096>("recycle-n", "u64x4k");
  bench_recycle<A64, Touch::None, 4>("recycle-n", "A64x4");
  bench_recycle<A64, Touch::None, 64>("recycle-n", "A64x64");
  bench_recycle<A64, Touch::None, 1024>("recycle-n", "A64x1024");

  bench_batch<char, 1>("batch-lifo", "char x128", 128, true);
  bench_batch<B64, 1>("batch-lifo", "B64 x128", 128, true);
  bench_batch<A64, 1>("batch-lifo", "A64 x64", 64, true);
  bench_batch<unsigned char, 256>("batch-lifo", "u8x256 x64", 64, true);
  bench_batch<B64, 1>("batch-fifo", "B64 x128", 128, false);

  bench_recycle<B64, Touch::Ends, 1>("touch-ends", "B64");
  bench_recycle<unsigned char, Touch::Ends, 4096>("touch-ends", "u8x4k");
  bench_recycle<A128, Touch::Ends, 1>("touch-ends", "A128");
  bench_recycle<unsigned char, Touch::Ends, (1u << 20)>("touch-ends", "u8x1m");
  bench_recycle<unsigned char, Touch::Fill, 64>("fill", "u8x64");
  bench_recycle<unsigned char, Touch::Fill, 4096>("fill", "u8x4k");

  bench_mixed();

  std::printf("sink=%llu\n", static_cast<unsigned long long>(g_escape));
  std::printf("PERF_DONE\n");
  return 0;
}
