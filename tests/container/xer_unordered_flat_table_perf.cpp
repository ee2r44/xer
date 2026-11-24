// Four-way unordered flat table perf: std / xer / Boost FOA / Abseil Swiss.
// Type matrix x hash matrix (mix + XXH3). No pre-reserve (organic growth).
//
// Paths (override with -D):
//   BOOST_ROOT, ABSL via CMake target, XXHASH_ROOT
// Default third-party root:
//   d:/ee2r44/Desktop/计算机学习资料/04_开源库与源码
//
// Prefer building via tests/container/unordered_flat_perf/CMakeLists.txt
// (links Abseil; defines XXH_INLINE_ALL; adds Boost/xxHash includes).
//
// Manual sketch (after Abseil is built/installed):
//   MSVC:  cl /std:c++latest /O2 /DNDEBUG /EHsc /DXXH_INLINE_ALL
//          /Iinclude /I"%BOOST_ROOT%" /I"%XXHASH_ROOT%" /I"%ABSL_INC%"
//          /Fe:..._msvc.exe ..._perf.cpp include\xer\env\xer_env.cpp ... absl libs
//   GCC:   g++ -std=c++23 -O3 -DNDEBUG -pthread -DXXH_INLINE_ALL ...
//   Clang: clang++ -std=c++23 -O3 -DNDEBUG -pthread -DXXH_INLINE_ALL ...

#if defined(_MSC_VER)
#  if defined(_DEBUG) || !defined(NDEBUG)
#    error "Build this perf test with /O2 /DNDEBUG"
#  endif
#elif !defined(__OPTIMIZE__) || !defined(NDEBUG)
#  error "Build this perf test with -O3 -DNDEBUG"
#endif

#ifndef XXH_INLINE_ALL
#  define XXH_INLINE_ALL
#endif
#include "xxhash.h"

#include "xer/container/xer_unordered_flat_table.h"
#include "xer/error/xer_ret.h"
#include "xer/macro/xer_macro.h"

#include <boost/unordered/unordered_flat_map.hpp>
#include <boost/unordered/unordered_flat_set.hpp>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
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

using Clock = std::chrono::steady_clock;

template <typename F>
double ns_total(F&& f) {
  const auto t0 = Clock::now();
  f();
  const auto t1 = Clock::now();
  return std::chrono::duration<double, std::nano>(t1 - t0).count();
}

template <typename F0, typename F1, typename F2, typename F3>
void measure4(F0&& f0, F1&& f1, F2&& f2, F3&& f3, Stats out[4]) {
  auto run = [&](int idx) -> double {
    switch (idx) {
    case 0:
      return f0();
    case 1:
      return f1();
    case 2:
      return f2();
    default:
      return f3();
    }
  };
  for (int i = 0; i < 4; ++i) {
    (void)run(i);
  }
  double samples[4][9];
  for (int rep = 0; rep < 9; ++rep) {
    const int start = rep & 3;
    for (int k = 0; k < 4; ++k) {
      const int idx = (start + k) & 3;
      samples[idx][rep] = run(idx);
    }
  }
  for (int idx = 0; idx < 4; ++idx) {
    double* s = samples[idx];
    std::sort(s, s + 9);
    out[idx].med_ns = s[4];
    out[idx].iqr_ns = s[6] - s[2];
  }
}

void report4(const char* tag, const char* op, std::uint64_t ops, const Stats s[4]) {
  auto per = [&](double ns) { return ops ? ns / static_cast<double>(ops) : ns; };
  std::printf("%-22s %-12s  %8.3f/%-6.3f  %8.3f/%-6.3f  %8.3f/%-6.3f  %8.3f/%-6.3f\n",
              tag, op, per(s[0].med_ns), per(s[0].iqr_ns), per(s[1].med_ns),
              per(s[1].iqr_ns), per(s[2].med_ns), per(s[2].iqr_ns),
              per(s[3].med_ns), per(s[3].iqr_ns));
  std::fflush(stdout);
}

// --- keys / values ---

struct Pod16 {
  std::uint64_t a;
  std::uint64_t b;
  friend bool operator==(const Pod16& x, const Pod16& y) noexcept {
    return x.a == y.a && x.b == y.b;
  }
};

int make_i32(std::uint32_t i) noexcept { return static_cast<int>(i); }

std::uint64_t make_u64(std::uint32_t i) noexcept {
  return (static_cast<std::uint64_t>(i) * 0x9E3779B97F4A7C15ull) ^ 0xA5A5A5A5u;
}

// Wrapper avoids ADL xer::forward vs std::forward on std::string in xer insert.
struct Str {
  std::string s;
  Str() noexcept : s() {}
  explicit Str(std::string x) : s(static_cast<std::string&&>(x)) {}
  Str(const Str& o) : s(o.s) {}
  Str(Str&& o) noexcept : s(static_cast<std::string&&>(o.s)) {}
  Str& operator=(const Str& o) {
    s = o.s;
    return *this;
  }
  Str& operator=(Str&& o) noexcept {
    s = static_cast<std::string&&>(o.s);
    return *this;
  }
  friend bool operator==(const Str& a, const Str& b) noexcept {
    return a.s == b.s;
  }
};

Str make_str(std::uint32_t i) {
  char buf[32];
  std::snprintf(buf, sizeof(buf), "k%u", i);
  return Str{std::string(buf)};
}

Pod16 make_pod(std::uint32_t i) noexcept {
  return Pod16{make_u64(i), make_u64(i ^ 0x55AA55AAu)};
}

Str make_str_val(std::uint32_t i) {
  char buf[32];
  std::snprintf(buf, sizeof(buf), "v%u", i);
  return Str{std::string(buf)};
}

// --- hash: mix ---

inline std::size_t fnv1a_bytes(const void* p, std::size_t n) noexcept {
  auto* b = static_cast<const unsigned char*>(p);
  std::uint64_t h = 14695981039346656037ull;
  for (std::size_t i = 0; i < n; ++i) {
    h ^= b[i];
    h *= 1099511628211ull;
  }
  return static_cast<std::size_t>(h);
}

struct MixHashI32 {
  MixHashI32() noexcept = default;
  explicit MixHashI32(std::size_t) noexcept {}
  std::size_t operator()(const int& k) const noexcept {
    return static_cast<std::size_t>(static_cast<std::uint32_t>(k) * 0x9E3779B9u);
  }
};

struct MixHashU64 {
  MixHashU64() noexcept = default;
  explicit MixHashU64(std::size_t) noexcept {}
  std::size_t operator()(const std::uint64_t& k) const noexcept {
    std::uint64_t x = k;
    x ^= x >> 30;
    x *= 0xBF58476D1CE4E5B9ull;
    x ^= x >> 27;
    x *= 0x94D049BB133111EBull;
    x ^= x >> 31;
    return static_cast<std::size_t>(x);
  }
};

struct MixHashStr {
  MixHashStr() noexcept = default;
  explicit MixHashStr(std::size_t) noexcept {}
  std::size_t operator()(const Str& k) const noexcept {
    return fnv1a_bytes(k.s.data(), k.s.size());
  }
};

struct MixHashPod {
  MixHashPod() noexcept = default;
  explicit MixHashPod(std::size_t) noexcept {}
  std::size_t operator()(const Pod16& k) const noexcept {
    return fnv1a_bytes(&k, sizeof(k));
  }
};

// --- hash: xxh3 ---

struct Xxh3HashI32 {
  Xxh3HashI32() noexcept = default;
  explicit Xxh3HashI32(std::size_t) noexcept {}
  std::size_t operator()(const int& k) const noexcept {
    return static_cast<std::size_t>(XXH3_64bits(&k, sizeof(k)));
  }
};

struct Xxh3HashU64 {
  Xxh3HashU64() noexcept = default;
  explicit Xxh3HashU64(std::size_t) noexcept {}
  std::size_t operator()(const std::uint64_t& k) const noexcept {
    return static_cast<std::size_t>(XXH3_64bits(&k, sizeof(k)));
  }
};

struct Xxh3HashStr {
  Xxh3HashStr() noexcept = default;
  explicit Xxh3HashStr(std::size_t) noexcept {}
  std::size_t operator()(const Str& k) const noexcept {
    return static_cast<std::size_t>(XXH3_64bits(k.s.data(), k.s.size()));
  }
};

struct Xxh3HashPod {
  Xxh3HashPod() noexcept = default;
  explicit Xxh3HashPod(std::size_t) noexcept {}
  std::size_t operator()(const Pod16& k) const noexcept {
    return static_cast<std::size_t>(XXH3_64bits(&k, sizeof(k)));
  }
};

// --- xer insert helpers ---

// xer insert requires nothrow copy or we must move; std::string is move-only here.
template <typename Table, typename Key>
void xer_set_insert(Table& t, const Key& k) {
  Key tmp(k);
  xer::ret r = t.insert(static_cast<Key&&>(tmp));
  if (r == xer::ret::out_of_space) {
    (void)t.flush();
    Key tmp2(k);
    r = t.insert(static_cast<Key&&>(tmp2));
  }
  escape_u64(static_cast<std::uint64_t>(r == xer::ret::success));
}

template <typename Table, typename Key, typename Val>
void xer_map_insert(Table& t, const Key& k, const Val& v) {
  Key tk(k);
  Val tv(v);
  xer::ret r = t.insert(static_cast<Key&&>(tk), static_cast<Val&&>(tv));
  if (r == xer::ret::out_of_space) {
    (void)t.flush();
    Key tk2(k);
    Val tv2(v);
    r = t.insert(static_cast<Key&&>(tk2), static_cast<Val&&>(tv2));
  }
  escape_u64(static_cast<std::uint64_t>(r == xer::ret::success));
}

template <typename Key, typename Hash, typename MakeKey>
void bench_set(const char* type_tag, const char* hash_tag, std::uint32_t n,
               MakeKey make_key) {
  char tag[64];
  std::snprintf(tag, sizeof(tag), "%s/%s", type_tag, hash_tag);

  using StdSet = std::unordered_set<Key, Hash>;
  using XerSet = xer::unordered_flat_set<Key, Hash>;
  using BoostSet = boost::unordered::unordered_flat_set<Key, Hash>;
  using AbslSet = absl::flat_hash_set<Key, Hash>;

  std::vector<Key> keys;
  keys.reserve(n);
  for (std::uint32_t i = 0; i < n; ++i) {
    keys.push_back(make_key(i));
  }
  std::vector<Key> miss;
  miss.reserve(n);
  for (std::uint32_t i = 0; i < n; ++i) {
    miss.push_back(make_key(n + i));
  }

  const std::uint64_t ops = n;

  // insert_empty
  {
    Stats s[4];
    measure4(
        [&] {
          return ns_total([&] {
            StdSet t;
            for (std::uint32_t i = 0; i < n; ++i) {
              t.insert(keys[i]);
            }
            escape_u64(t.size());
          });
        },
        [&] {
          return ns_total([&] {
            XerSet t;
            for (std::uint32_t i = 0; i < n; ++i) {
              xer_set_insert(t, keys[i]);
            }
          });
        },
        [&] {
          return ns_total([&] {
            BoostSet t;
            for (std::uint32_t i = 0; i < n; ++i) {
              t.insert(keys[i]);
            }
            escape_u64(t.size());
          });
        },
        [&] {
          return ns_total([&] {
            AbslSet t;
            for (std::uint32_t i = 0; i < n; ++i) {
              t.insert(keys[i]);
            }
            escape_u64(t.size());
          });
        },
        s);
    report4(tag, "insert", ops, s);
  }

  StdSet std_t;
  XerSet xer_t;
  BoostSet boost_t;
  AbslSet absl_t;
  for (std::uint32_t i = 0; i < n; ++i) {
    std_t.insert(keys[i]);
    xer_set_insert(xer_t, keys[i]);
    boost_t.insert(keys[i]);
    absl_t.insert(keys[i]);
  }

  // find_hit
  {
    Stats s[4];
    measure4(
        [&] {
          return ns_total([&] {
            std::uint64_t acc = 0;
            for (std::uint32_t i = 0; i < n; ++i) {
              acc += static_cast<std::uint64_t>(std_t.find(keys[i]) != std_t.end());
            }
            escape_u64(acc);
          });
        },
        [&] {
          return ns_total([&] {
            std::uint64_t acc = 0;
            for (std::uint32_t i = 0; i < n; ++i) {
              acc += static_cast<std::uint64_t>(xer_t.find(keys[i]) != nullptr);
            }
            escape_u64(acc);
          });
        },
        [&] {
          return ns_total([&] {
            std::uint64_t acc = 0;
            for (std::uint32_t i = 0; i < n; ++i) {
              acc += static_cast<std::uint64_t>(boost_t.find(keys[i]) != boost_t.end());
            }
            escape_u64(acc);
          });
        },
        [&] {
          return ns_total([&] {
            std::uint64_t acc = 0;
            for (std::uint32_t i = 0; i < n; ++i) {
              acc += static_cast<std::uint64_t>(absl_t.find(keys[i]) != absl_t.end());
            }
            escape_u64(acc);
          });
        },
        s);
    report4(tag, "find_hit", ops, s);
  }

  // find_miss
  {
    Stats s[4];
    measure4(
        [&] {
          return ns_total([&] {
            std::uint64_t acc = 0;
            for (std::uint32_t i = 0; i < n; ++i) {
              acc += static_cast<std::uint64_t>(std_t.find(miss[i]) != std_t.end());
            }
            escape_u64(acc);
          });
        },
        [&] {
          return ns_total([&] {
            std::uint64_t acc = 0;
            for (std::uint32_t i = 0; i < n; ++i) {
              acc += static_cast<std::uint64_t>(xer_t.find(miss[i]) != nullptr);
            }
            escape_u64(acc);
          });
        },
        [&] {
          return ns_total([&] {
            std::uint64_t acc = 0;
            for (std::uint32_t i = 0; i < n; ++i) {
              acc += static_cast<std::uint64_t>(boost_t.find(miss[i]) != boost_t.end());
            }
            escape_u64(acc);
          });
        },
        [&] {
          return ns_total([&] {
            std::uint64_t acc = 0;
            for (std::uint32_t i = 0; i < n; ++i) {
              acc += static_cast<std::uint64_t>(absl_t.find(miss[i]) != absl_t.end());
            }
            escape_u64(acc);
          });
        },
        s);
    report4(tag, "find_miss", ops, s);
  }

  // erase_half / reinsert: all libs rebuild by insert (xer is move-only).
  {
    Stats s[4];
    measure4(
        [&] {
          return ns_total([&] {
            StdSet t;
            for (std::uint32_t i = 0; i < n; ++i) {
              t.insert(keys[i]);
            }
            for (std::uint32_t i = 0; i < n; i += 2) {
              t.erase(keys[i]);
            }
            escape_u64(t.size());
          });
        },
        [&] {
          return ns_total([&] {
            XerSet t;
            for (std::uint32_t i = 0; i < n; ++i) {
              xer_set_insert(t, keys[i]);
            }
            for (std::uint32_t i = 0; i < n; i += 2) {
              (void)t.erase(keys[i]);
            }
          });
        },
        [&] {
          return ns_total([&] {
            BoostSet t;
            for (std::uint32_t i = 0; i < n; ++i) {
              t.insert(keys[i]);
            }
            for (std::uint32_t i = 0; i < n; i += 2) {
              t.erase(keys[i]);
            }
            escape_u64(t.size());
          });
        },
        [&] {
          return ns_total([&] {
            AbslSet t;
            for (std::uint32_t i = 0; i < n; ++i) {
              t.insert(keys[i]);
            }
            for (std::uint32_t i = 0; i < n; i += 2) {
              t.erase(keys[i]);
            }
            escape_u64(t.size());
          });
        },
        s);
    report4(tag, "erase_half", ops / 2, s);
  }

  {
    Stats s[4];
    measure4(
        [&] {
          return ns_total([&] {
            StdSet t;
            for (std::uint32_t i = 0; i < n; ++i) {
              t.insert(keys[i]);
            }
            for (std::uint32_t i = 0; i < n; i += 2) {
              t.erase(keys[i]);
            }
            for (std::uint32_t i = 0; i < n; i += 2) {
              t.insert(keys[i]);
            }
            escape_u64(t.size());
          });
        },
        [&] {
          return ns_total([&] {
            XerSet t;
            for (std::uint32_t i = 0; i < n; ++i) {
              xer_set_insert(t, keys[i]);
            }
            for (std::uint32_t i = 0; i < n; i += 2) {
              (void)t.erase(keys[i]);
            }
            for (std::uint32_t i = 0; i < n; i += 2) {
              xer_set_insert(t, keys[i]);
            }
          });
        },
        [&] {
          return ns_total([&] {
            BoostSet t;
            for (std::uint32_t i = 0; i < n; ++i) {
              t.insert(keys[i]);
            }
            for (std::uint32_t i = 0; i < n; i += 2) {
              t.erase(keys[i]);
            }
            for (std::uint32_t i = 0; i < n; i += 2) {
              t.insert(keys[i]);
            }
            escape_u64(t.size());
          });
        },
        [&] {
          return ns_total([&] {
            AbslSet t;
            for (std::uint32_t i = 0; i < n; ++i) {
              t.insert(keys[i]);
            }
            for (std::uint32_t i = 0; i < n; i += 2) {
              t.erase(keys[i]);
            }
            for (std::uint32_t i = 0; i < n; i += 2) {
              t.insert(keys[i]);
            }
            escape_u64(t.size());
          });
        },
        s);
    report4(tag, "reinsert", ops / 2, s);
  }
}

template <typename Key, typename Val, typename Hash, typename MakeKey, typename MakeVal>
void bench_map(const char* type_tag, const char* hash_tag, std::uint32_t n,
               MakeKey make_key, MakeVal make_val) {
  char tag[64];
  std::snprintf(tag, sizeof(tag), "%s/%s", type_tag, hash_tag);

  using StdMap = std::unordered_map<Key, Val, Hash>;
  using XerMap = xer::unordered_flat_map<Key, Val, Hash>;
  using BoostMap = boost::unordered::unordered_flat_map<Key, Val, Hash>;
  using AbslMap = absl::flat_hash_map<Key, Val, Hash>;

  std::vector<Key> keys;
  std::vector<Val> vals;
  keys.reserve(n);
  vals.reserve(n);
  for (std::uint32_t i = 0; i < n; ++i) {
    keys.push_back(make_key(i));
    vals.push_back(make_val(i));
  }
  std::vector<Key> miss;
  miss.reserve(n);
  for (std::uint32_t i = 0; i < n; ++i) {
    miss.push_back(make_key(n + i));
  }

  const std::uint64_t ops = n;

  {
    Stats s[4];
    measure4(
        [&] {
          return ns_total([&] {
            StdMap t;
            for (std::uint32_t i = 0; i < n; ++i) {
              t.emplace(keys[i], vals[i]);
            }
            escape_u64(t.size());
          });
        },
        [&] {
          return ns_total([&] {
            XerMap t;
            for (std::uint32_t i = 0; i < n; ++i) {
              xer_map_insert(t, keys[i], vals[i]);
            }
          });
        },
        [&] {
          return ns_total([&] {
            BoostMap t;
            for (std::uint32_t i = 0; i < n; ++i) {
              t.emplace(keys[i], vals[i]);
            }
            escape_u64(t.size());
          });
        },
        [&] {
          return ns_total([&] {
            AbslMap t;
            for (std::uint32_t i = 0; i < n; ++i) {
              t.emplace(keys[i], vals[i]);
            }
            escape_u64(t.size());
          });
        },
        s);
    report4(tag, "insert", ops, s);
  }

  StdMap std_t;
  XerMap xer_t;
  BoostMap boost_t;
  AbslMap absl_t;
  for (std::uint32_t i = 0; i < n; ++i) {
    std_t.emplace(keys[i], vals[i]);
    xer_map_insert(xer_t, keys[i], vals[i]);
    boost_t.emplace(keys[i], vals[i]);
    absl_t.emplace(keys[i], vals[i]);
  }

  {
    Stats s[4];
    measure4(
        [&] {
          return ns_total([&] {
            std::uint64_t acc = 0;
            for (std::uint32_t i = 0; i < n; ++i) {
              auto it = std_t.find(keys[i]);
              acc += static_cast<std::uint64_t>(it != std_t.end());
            }
            escape_u64(acc);
          });
        },
        [&] {
          return ns_total([&] {
            std::uint64_t acc = 0;
            for (std::uint32_t i = 0; i < n; ++i) {
              acc += static_cast<std::uint64_t>(xer_t.find(keys[i]) != nullptr);
            }
            escape_u64(acc);
          });
        },
        [&] {
          return ns_total([&] {
            std::uint64_t acc = 0;
            for (std::uint32_t i = 0; i < n; ++i) {
              auto it = boost_t.find(keys[i]);
              acc += static_cast<std::uint64_t>(it != boost_t.end());
            }
            escape_u64(acc);
          });
        },
        [&] {
          return ns_total([&] {
            std::uint64_t acc = 0;
            for (std::uint32_t i = 0; i < n; ++i) {
              auto it = absl_t.find(keys[i]);
              acc += static_cast<std::uint64_t>(it != absl_t.end());
            }
            escape_u64(acc);
          });
        },
        s);
    report4(tag, "find_hit", ops, s);
  }

  {
    Stats s[4];
    measure4(
        [&] {
          return ns_total([&] {
            std::uint64_t acc = 0;
            for (std::uint32_t i = 0; i < n; ++i) {
              acc += static_cast<std::uint64_t>(std_t.find(miss[i]) != std_t.end());
            }
            escape_u64(acc);
          });
        },
        [&] {
          return ns_total([&] {
            std::uint64_t acc = 0;
            for (std::uint32_t i = 0; i < n; ++i) {
              acc += static_cast<std::uint64_t>(xer_t.find(miss[i]) != nullptr);
            }
            escape_u64(acc);
          });
        },
        [&] {
          return ns_total([&] {
            std::uint64_t acc = 0;
            for (std::uint32_t i = 0; i < n; ++i) {
              acc += static_cast<std::uint64_t>(boost_t.find(miss[i]) != boost_t.end());
            }
            escape_u64(acc);
          });
        },
        [&] {
          return ns_total([&] {
            std::uint64_t acc = 0;
            for (std::uint32_t i = 0; i < n; ++i) {
              acc += static_cast<std::uint64_t>(absl_t.find(miss[i]) != absl_t.end());
            }
            escape_u64(acc);
          });
        },
        s);
    report4(tag, "find_miss", ops, s);
  }

  {
    Stats s[4];
    measure4(
        [&] {
          return ns_total([&] {
            StdMap t;
            for (std::uint32_t i = 0; i < n; ++i) {
              t.emplace(keys[i], vals[i]);
            }
            for (std::uint32_t i = 0; i < n; i += 2) {
              t.erase(keys[i]);
            }
            escape_u64(t.size());
          });
        },
        [&] {
          return ns_total([&] {
            XerMap t;
            for (std::uint32_t i = 0; i < n; ++i) {
              xer_map_insert(t, keys[i], vals[i]);
            }
            for (std::uint32_t i = 0; i < n; i += 2) {
              (void)t.erase(keys[i]);
            }
          });
        },
        [&] {
          return ns_total([&] {
            BoostMap t;
            for (std::uint32_t i = 0; i < n; ++i) {
              t.emplace(keys[i], vals[i]);
            }
            for (std::uint32_t i = 0; i < n; i += 2) {
              t.erase(keys[i]);
            }
            escape_u64(t.size());
          });
        },
        [&] {
          return ns_total([&] {
            AbslMap t;
            for (std::uint32_t i = 0; i < n; ++i) {
              t.emplace(keys[i], vals[i]);
            }
            for (std::uint32_t i = 0; i < n; i += 2) {
              t.erase(keys[i]);
            }
            escape_u64(t.size());
          });
        },
        s);
    report4(tag, "erase_half", ops / 2, s);
  }

  {
    Stats s[4];
    measure4(
        [&] {
          return ns_total([&] {
            StdMap t;
            for (std::uint32_t i = 0; i < n; ++i) {
              t.emplace(keys[i], vals[i]);
            }
            for (std::uint32_t i = 0; i < n; i += 2) {
              t.erase(keys[i]);
            }
            for (std::uint32_t i = 0; i < n; i += 2) {
              t.emplace(keys[i], vals[i]);
            }
            escape_u64(t.size());
          });
        },
        [&] {
          return ns_total([&] {
            XerMap t;
            for (std::uint32_t i = 0; i < n; ++i) {
              xer_map_insert(t, keys[i], vals[i]);
            }
            for (std::uint32_t i = 0; i < n; i += 2) {
              (void)t.erase(keys[i]);
            }
            for (std::uint32_t i = 0; i < n; i += 2) {
              xer_map_insert(t, keys[i], vals[i]);
            }
          });
        },
        [&] {
          return ns_total([&] {
            BoostMap t;
            for (std::uint32_t i = 0; i < n; ++i) {
              t.emplace(keys[i], vals[i]);
            }
            for (std::uint32_t i = 0; i < n; i += 2) {
              t.erase(keys[i]);
            }
            for (std::uint32_t i = 0; i < n; i += 2) {
              t.emplace(keys[i], vals[i]);
            }
            escape_u64(t.size());
          });
        },
        [&] {
          return ns_total([&] {
            AbslMap t;
            for (std::uint32_t i = 0; i < n; ++i) {
              t.emplace(keys[i], vals[i]);
            }
            for (std::uint32_t i = 0; i < n; i += 2) {
              t.erase(keys[i]);
            }
            for (std::uint32_t i = 0; i < n; i += 2) {
              t.emplace(keys[i], vals[i]);
            }
            escape_u64(t.size());
          });
        },
        s);
    report4(tag, "reinsert", ops / 2, s);
  }
}

template <typename MixH, typename XxH, typename MakeKey>
void run_set_both(const char* type_tag, std::uint32_t n, MakeKey mk) {
  bench_set<typename std::invoke_result_t<MakeKey, std::uint32_t>, MixH>(
      type_tag, "hash_mix", n, mk);
  bench_set<typename std::invoke_result_t<MakeKey, std::uint32_t>, XxH>(
      type_tag, "hash_xxh3", n, mk);
}

template <typename MixH, typename XxH, typename MakeKey, typename MakeVal>
void run_map_both(const char* type_tag, std::uint32_t n, MakeKey mk, MakeVal mv) {
  using K = typename std::invoke_result_t<MakeKey, std::uint32_t>;
  using V = typename std::invoke_result_t<MakeVal, std::uint32_t>;
  bench_map<K, V, MixH>(type_tag, "hash_mix", n, mk, mv);
  bench_map<K, V, XxH>(type_tag, "hash_xxh3", n, mk, mv);
}

} // namespace

int main() {
  pin_thread();
  std::printf("xer_unordered_flat_table perf (std|xer|boost|absl) ns/op med/iqr\n");
  std::printf("compiler=");
#if XER_COMPILER(MSVC)
  std::printf("msvc");
#elif XER_COMPILER(GCC)
  std::printf("gcc");
#elif XER_COMPILER(CLANG)
  std::printf("clang");
#else
  std::printf("other");
#endif
  std::printf(" sse2=%d sink=%llu\n", (int)XER_SSE2_AVAILABLE,
              static_cast<unsigned long long>(g_sink));
  std::printf("%-22s %-12s  %8s/%-6s  %8s/%-6s  %8s/%-6s  %8s/%-6s\n", "tag",
              "op", "std", "iqr", "xer", "iqr", "boost", "iqr", "absl", "iqr");

  // N per plan (slightly tuned for wall time).
  constexpr std::uint32_t N_I32 = 500000;
  constexpr std::uint32_t N_U64 = 500000;
  constexpr std::uint32_t N_STR = 100000;
  constexpr std::uint32_t N_POD = 300000;
  constexpr std::uint32_t N_I32STR = 100000;
  constexpr std::uint32_t N_STRSTR = 80000;

  run_set_both<MixHashI32, Xxh3HashI32>("set_i32", N_I32, make_i32);
  run_set_both<MixHashU64, Xxh3HashU64>("set_u64", N_U64, make_u64);
  run_set_both<MixHashStr, Xxh3HashStr>("set_str", N_STR, make_str);
  run_set_both<MixHashPod, Xxh3HashPod>("set_pod", N_POD, make_pod);

  run_map_both<MixHashI32, Xxh3HashI32>("map_i32_i32", N_I32, make_i32, make_i32);
  run_map_both<MixHashI32, Xxh3HashI32>("map_i32_str", N_I32STR, make_i32, make_str_val);
  run_map_both<MixHashStr, Xxh3HashStr>("map_str_i32", N_STR, make_str, make_i32);
  run_map_both<MixHashStr, Xxh3HashStr>("map_str_str", N_STRSTR, make_str, make_str_val);
  run_map_both<MixHashPod, Xxh3HashPod>("map_pod_u64", N_POD, make_pod, make_u64);

  std::printf("sink=%llu\n", static_cast<unsigned long long>(g_sink));
  return 0;
}
