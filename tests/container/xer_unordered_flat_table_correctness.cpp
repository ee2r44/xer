// Correctness for include/xer/container/xer_unordered_flat_table.h
// Build (C++23):
//   MSVC:  call D:\develop\c++\vs\VC\Auxiliary\Build\vcvars64.bat
//          cl /std:c++latest /EHsc /Iinclude /Fe:tests\container\xer_unordered_flat_table_correctness_msvc.exe tests\container\xer_unordered_flat_table_correctness.cpp include\xer\env\xer_env.cpp
//   GCC:   g++ -std=c++23 -Iinclude -o tests/container/xer_unordered_flat_table_correctness_gcc tests/container/xer_unordered_flat_table_correctness.cpp include/xer/env/xer_env.cpp
//   Clang: clang++ -std=c++23 -Iinclude -o tests/container/xer_unordered_flat_table_correctness_clang tests/container/xer_unordered_flat_table_correctness.cpp include/xer/env/xer_env.cpp

#include "xer/container/xer_unordered_flat_table.h"
#include "xer/error/xer_ret.h"
#include "xer/macro/xer_macro.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <utility>

namespace {

int g_failures = 0;
int g_alive = 0;

#define CHECK(cond)                                                            \
  do {                                                                         \
    if (!(cond)) {                                                             \
      std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);              \
      ++g_failures;                                                            \
    }                                                                          \
  } while (0)

struct IntHash {
  IntHash() noexcept = default;
  explicit IntHash(std::size_t) noexcept {}
  std::size_t operator()(const int& k) const noexcept {
    return static_cast<std::size_t>(static_cast<std::uint32_t>(k) * 0x9E3779B9u);
  }
};

struct Tracker {
  int v;
  explicit Tracker(int x = 0) noexcept : v(x) { ++g_alive; }
  Tracker(const Tracker& o) noexcept : v(o.v) { ++g_alive; }
  Tracker(Tracker&& o) noexcept : v(o.v) {
    o.v = -1;
    ++g_alive;
  }
  Tracker& operator=(const Tracker& o) noexcept {
    v = o.v;
    return *this;
  }
  Tracker& operator=(Tracker&& o) noexcept {
    v = o.v;
    o.v = -1;
    return *this;
  }
  ~Tracker() { --g_alive; }
  friend bool operator==(const Tracker& a, const Tracker& b) noexcept {
    return a.v == b.v;
  }
};

struct TrackerHash {
  TrackerHash() noexcept = default;
  explicit TrackerHash(std::size_t) noexcept {}
  std::size_t operator()(const Tracker& k) const noexcept {
    return IntHash{}(k.v);
  }
};

using Set = xer::unordered_flat_set<int, IntHash>;
using Map = xer::unordered_flat_map<int, int, IntHash>;
using TrackerSet = xer::unordered_flat_set<Tracker, TrackerHash>;

template <typename Table, typename... Args>
xer::ret insert_grow(Table& t, Args&&... args) noexcept {
  xer::ret r = t.insert(static_cast<Args&&>(args)...);
  if (r == xer::ret::out_of_space) {
    r = t.flush();
    if (r != xer::ret::success) {
      return r;
    }
    r = t.insert(static_cast<Args&&>(args)...);
  }
  return r;
}

void check_set_basic() {
  Set s;
  CHECK(s.find(1) == nullptr);
  CHECK(insert_grow(s, 1) == xer::ret::success);
  CHECK(s.find(1) != nullptr);
  CHECK(*s.find(1) == 1);

  CHECK(s.insert(1) == xer::ret::value_collison);
  CHECK(insert_grow(s, 2) == xer::ret::success);
  CHECK(s.find(2) != nullptr);

  CHECK(s.erase(3) == xer::ret::value_no_found);
  CHECK(s.erase(1) == xer::ret::success);
  CHECK(s.find(1) == nullptr);
  CHECK(s.find(2) != nullptr);
}

void check_map_basic() {
  Map m;
  CHECK(m.find(10) == nullptr);
  CHECK(insert_grow(m, 10, 100) == xer::ret::success);
  CHECK(m.find(10) != nullptr);
  CHECK(*m.find(10) == 100);

  CHECK(m.insert(10, 999) == xer::ret::value_collison);
  CHECK(*m.find(10) == 100);

  CHECK(insert_grow(m, 20, 200) == xer::ret::success);
  CHECK(*m.find(20) == 200);
  CHECK(m.erase(10) == xer::ret::success);
  CHECK(m.find(10) == nullptr);
}

void check_many_insert_find() {
  Set s;
  constexpr int N = 200;
  for (int i = 0; i < N; ++i) {
    CHECK(insert_grow(s, i) == xer::ret::success);
  }
  for (int i = 0; i < N; ++i) {
    const int* p = s.find(i);
    CHECK(p != nullptr);
    if (p) {
      CHECK(*p == i);
    }
  }
  for (int i = 0; i < N; i += 2) {
    CHECK(s.erase(i) == xer::ret::success);
  }
  for (int i = 0; i < N; ++i) {
    if ((i % 2) == 0) {
      CHECK(s.find(i) == nullptr);
    } else {
      CHECK(s.find(i) != nullptr);
    }
  }
  // Re-insert evens after holes / possible ofw dirt.
  for (int i = 0; i < N; i += 2) {
    CHECK(insert_grow(s, i) == xer::ret::success);
  }
  for (int i = 0; i < N; ++i) {
    CHECK(s.find(i) != nullptr);
  }
}

void check_reserve() {
  // Empty table: reserve then insert without out_of_space.
  {
    Set s;
    CHECK(s.reserve(500) == xer::ret::success);
    for (int i = 0; i < 500; ++i) {
      const xer::ret r = s.insert(i);
      CHECK(r != xer::ret::out_of_space);
      CHECK(r == xer::ret::success);
    }
    for (int i = 0; i < 500; ++i) {
      CHECK(s.find(i) != nullptr);
    }
    CHECK(s.reserve(100) == xer::ret::success);
    CHECK(s.insert(0) == xer::ret::value_collison);
  }
  // Non-empty grow path.
  {
    Set s;
    CHECK(insert_grow(s, 0) == xer::ret::success);
    CHECK(s.reserve(300) == xer::ret::success);
    for (int i = 1; i < 300; ++i) {
      const xer::ret r = s.insert(i);
      CHECK(r != xer::ret::out_of_space);
      CHECK(r == xer::ret::success);
    }
  }
}

void check_flush_and_insert_grow() {
  Set s;
  // Post-insert flush usually grows before out_of_space is visible; insert_grow
  // must still take a large table through any out_of_space + flush retries.
  constexpr int N = 500;
  for (int i = 0; i < N; ++i) {
    CHECK(insert_grow(s, i) == xer::ret::success);
  }
  CHECK(s.flush() == xer::ret::success);
  for (int i = 0; i < N; ++i) {
    CHECK(s.find(i) != nullptr);
  }
  CHECK(s.insert(0) == xer::ret::value_collison);

  // Anti-drift erases then re-insert: may surface out_of_space; handle via flush.
  for (int i = 0; i < N; i += 2) {
    CHECK(s.erase(i) == xer::ret::success);
  }
  for (int i = 0; i < N; i += 2) {
    const xer::ret r0 = s.insert(i);
    if (r0 == xer::ret::out_of_space) {
      CHECK(s.flush() == xer::ret::success);
      CHECK(s.insert(i) == xer::ret::success);
    } else {
      CHECK(r0 == xer::ret::success);
    }
    CHECK(s.find(i) != nullptr);
  }
}

void check_erase_if_set() {
  Set s;
  for (int i = 0; i < 40; ++i) {
    CHECK(insert_grow(s, i) == xer::ret::success);
  }
  s.erase_if_k([](const int& k) noexcept { return (k % 3) == 0; });
  for (int i = 0; i < 40; ++i) {
    if ((i % 3) == 0) {
      CHECK(s.find(i) == nullptr);
    } else {
      CHECK(s.find(i) != nullptr);
    }
  }
}

struct Box {
  int x;
  explicit Box(int v = 0) noexcept : x(v) {}
  Box(const Box&) noexcept = default;
  Box(Box&&) noexcept = default;
  Box& operator=(const Box&) noexcept = default;
  Box& operator=(Box&&) noexcept = default;
};

void check_erase_if_map_by_value() {
  Map m;
  for (int i = 0; i < 30; ++i) {
    CHECK(insert_grow(m, i, i * 10) == xer::ret::success);
  }
  m.erase_if_kv([](const int&, const int& v) noexcept { return v >= 200; });
  for (int i = 0; i < 30; ++i) {
    if (i * 10 >= 200) {
      CHECK(m.find(i) == nullptr);
    } else {
      CHECK(m.find(i) != nullptr);
    }
  }

  using MapIB = xer::unordered_flat_map<int, Box, IntHash>;
  MapIB mb;
  for (int i = 0; i < 30; ++i) {
    CHECK(insert_grow(mb, i, Box{i * 10}) == xer::ret::success);
  }
  mb.erase_if_v([](const Box& v) noexcept { return v.x >= 200; });
  for (int i = 0; i < 30; ++i) {
    if (i * 10 >= 200) {
      CHECK(mb.find(i) == nullptr);
    } else {
      CHECK(mb.find(i) != nullptr);
    }
  }

  // erase_if_v must use Value even when F is also invocable with Key (int→long).
  using MapIL = xer::unordered_flat_map<int, long, IntHash>;
  MapIL ml;
  for (int i = 0; i < 30; ++i) {
    CHECK(insert_grow(ml, i, static_cast<long>(i * 10)) == xer::ret::success);
  }
  ml.erase_if_v([](const long& v) noexcept { return v >= 200; });
  for (int i = 0; i < 30; ++i) {
    if (i * 10 >= 200) {
      CHECK(ml.find(i) == nullptr);
    } else {
      CHECK(ml.find(i) != nullptr);
    }
  }
}

void check_tracker_lifetime() {
  g_alive = 0;
  {
    TrackerSet s;
    for (int i = 0; i < 50; ++i) {
      CHECK(insert_grow(s, Tracker{i}) == xer::ret::success);
    }
    CHECK(g_alive == 50);
    CHECK(s.erase(Tracker{7}) == xer::ret::success);
    CHECK(g_alive == 49);
    s.erase_if_k([](const Tracker& k) noexcept { return (k.v % 2) == 0; });
    int expect = 0;
    for (int i = 0; i < 50; ++i) {
      if (i != 7 && (i % 2) != 0) {
        ++expect;
        CHECK(s.find(Tracker{i}) != nullptr);
      } else {
        CHECK(s.find(Tracker{i}) == nullptr);
      }
    }
    CHECK(g_alive == expect);
  }
  CHECK(g_alive == 0);
}

void check_move() {
  Set a;
  for (int i = 0; i < 20; ++i) {
    CHECK(insert_grow(a, i) == xer::ret::success);
  }
  Set b(static_cast<Set&&>(a));
  CHECK(a.find(0) == nullptr);
  CHECK(b.find(0) != nullptr);
  CHECK(b.find(19) != nullptr);

  Set c;
  CHECK(insert_grow(c, 100) == xer::ret::success);
  c = static_cast<Set&&>(b);
  CHECK(b.find(0) == nullptr);
  CHECK(c.find(0) != nullptr);
  CHECK(c.find(100) == nullptr);
}

void check_for_each() {
  Set s;
  for (int i = 1; i <= 5; ++i) {
    CHECK(insert_grow(s, i) == xer::ret::success);
  }
  int sum = 0;
  s.for_each_k([&](const int& k) noexcept { sum += k; });
  CHECK(sum == 15);

  Map m;
  for (int i = 1; i <= 4; ++i) {
    CHECK(insert_grow(m, i, i * 10) == xer::ret::success);
  }
  m.for_each_v([](int& v) noexcept { v += 1; });
  for (int i = 1; i <= 4; ++i) {
    const int* p = m.find(i);
    CHECK(p != nullptr);
    if (p) {
      CHECK(*p == i * 10 + 1);
    }
  }
  m.for_each_kv([](const int& k, int& v) noexcept { v = k * 100; });
  for (int i = 1; i <= 4; ++i) {
    const int* p = m.find(i);
    CHECK(p != nullptr);
    if (p) {
      CHECK(*p == i * 100);
    }
  }

  const Map& cm = m;
  int ksum = 0;
  int vsum = 0;
  cm.for_each_k([&](const int& k) noexcept { ksum += k; });
  cm.for_each_v([&](const int& v) noexcept { vsum += v; });
  cm.for_each_kv([&](const int& k, const int& v) noexcept {
    CHECK(v == k * 100);
  });
  CHECK(ksum == 10);
  CHECK(vsum == 100 + 200 + 300 + 400);
}

void check_same_group_labels() {
  // Keys that share low hash byte stress fine_label / ofw.
  Set s;
  for (int i = 0; i < 64; ++i) {
    const int k = i * 256; // low byte 0 -> fine_label 1
    CHECK(insert_grow(s, k) == xer::ret::success);
  }
  for (int i = 0; i < 64; ++i) {
    const int k = i * 256;
    CHECK(s.find(k) != nullptr);
  }
  for (int i = 0; i < 64; i += 3) {
    CHECK(s.erase(i * 256) == xer::ret::success);
  }
  for (int i = 0; i < 64; ++i) {
    const int k = i * 256;
    if ((i % 3) == 0) {
      CHECK(s.find(k) == nullptr);
    } else {
      CHECK(s.find(k) != nullptr);
    }
  }
}

} // namespace

int main() {
  std::printf("xer_unordered_flat_table correctness\n");
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
  std::printf(" sse2=%d\n", (int)XER_SSE2_AVAILABLE);

  check_set_basic();
  check_map_basic();
  check_many_insert_find();
  check_reserve();
  check_flush_and_insert_grow();
  check_erase_if_set();
  check_erase_if_map_by_value();
  check_tracker_lifetime();
  check_move();
  check_for_each();
  check_same_group_labels();

  if (g_failures != 0) {
    std::printf("RESULT: FAILED (%d checks)\n", g_failures);
    return 1;
  }
  std::printf("RESULT: PASSED\n");
  return 0;
}
