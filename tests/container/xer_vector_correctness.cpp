// Correctness for include/xer/container/xer_vector.h
// Build:
//   MSVC:  cl /std:c++latest /EHsc /Iinclude /Fe:tests\container\xer_vector_correctness_msvc.exe tests\container\xer_vector_correctness.cpp
//   GCC:   g++ -std=c++23 -Iinclude -o tests/container/xer_vector_correctness_gcc tests/container/xer_vector_correctness.cpp
//   Clang: clang++ -std=c++23 -Iinclude -o tests/container/xer_vector_correctness_clang tests/container/xer_vector_correctness.cpp

#include "xer/container/xer_vector.h"

#include <cstdio>

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
};

struct MoveOnly {
  int v;
  explicit MoveOnly(int x = 0) noexcept : v(x) {}
  MoveOnly(const MoveOnly&) = delete;
  MoveOnly(MoveOnly&& o) noexcept : v(o.v) { o.v = -1; }
  MoveOnly& operator=(const MoveOnly&) = delete;
  MoveOnly& operator=(MoveOnly&& o) noexcept {
    v = o.v;
    o.v = -1;
    return *this;
  }
  ~MoveOnly() = default;
};

void check_basic_int() {
  xer::vector<int> v;
  CHECK(v.is_empty());
  CHECK(v.size() == 0);
  CHECK(v.capacity() == 0);
  CHECK(v.data() == nullptr);

  CHECK(v.reserve(4));
  v.push_back_unchecked(1);
  v.push_back_unchecked(2);
  v.push_back_unchecked(3);
  CHECK(v.size() == 3);
  CHECK(v[0] == 1 && v[1] == 2 && v[2] == 3);
  CHECK(v.begin() + 3 == v.end());

  CHECK(v.erase_last());
  CHECK(v.size() == 2);
  CHECK(v[1] == 2);

  CHECK(v.erase(0));
  CHECK(v.size() == 1);
  CHECK(v[0] == 2);

  v.clear();
  CHECK(v.is_empty());
  CHECK(v.capacity() >= 1);
}

void check_growth_via_reserve() {
  xer::vector<int> v;
  CHECK(v.reserve(10));
  CHECK(v.capacity() >= 10);
  CHECK(v.size() == 0);
  for (int i = 0; i < 10; ++i) {
    v.push_back_unchecked(i);
  }
  CHECK(v.size() == 10);
  for (int i = 0; i < 10; ++i) {
    CHECK(v[i] == i);
  }

  // Geometric growth through reserve beyond current capacity.
  CHECK(v.reserve(11));
  CHECK(v.capacity() >= 20); // max(11, 10<<1)
  v.push_back_unchecked(10);
  CHECK(v.size() == 11);
  CHECK(v[10] == 10);
}

void check_move() {
  xer::vector<int> a;
  CHECK(a.reserve(2));
  a.push_back_unchecked(7);
  a.push_back_unchecked(8);
  xer::vector<int> b(static_cast<xer::vector<int>&&>(a));
  CHECK(a.is_empty());
  CHECK(a.data() == nullptr);
  CHECK(b.size() == 2);
  CHECK(b[0] == 7 && b[1] == 8);

  xer::vector<int> c;
  CHECK(c.reserve(1));
  c.push_back_unchecked(1);
  c = static_cast<xer::vector<int>&&>(b);
  CHECK(b.is_empty());
  CHECK(c.size() == 2);
  CHECK(c[1] == 8);
}

void check_tracker_lifetime() {
  g_alive = 0;
  {
    xer::vector<Tracker> v;
    CHECK(v.reserve(4));
    v.emplace_back_unchecked(1);
    v.emplace_back_unchecked(2);
    v.emplace_back_unchecked(3);
    CHECK(g_alive == 3);
    CHECK(v.erase(1));
    CHECK(v.size() == 2);
    CHECK(v[0].v == 1 && v[1].v == 3);
    CHECK(g_alive == 2);
    CHECK(v.unordered_erase(0));
    CHECK(v.size() == 1);
    CHECK(g_alive == 1);
  }
  CHECK(g_alive == 0);
}

void check_move_only() {
  xer::vector<MoveOnly> v;
  CHECK(v.reserve(4));
  v.push_back_unchecked(MoveOnly{10});
  v.emplace_back_unchecked(20);
  v.push_back_unchecked(MoveOnly{30});
  CHECK(v.size() == 3);
  CHECK(v[0].v == 10 && v[1].v == 20 && v[2].v == 30);

  CHECK(v.erase(1));
  CHECK(v.size() == 2);
  CHECK(v[0].v == 10 && v[1].v == 30);

  MoveOnly out = v.pop_out_unchecked();
  CHECK(out.v == 30);
  CHECK(v.size() == 1);
}

void check_resize() {
  xer::vector<int> v;
  CHECK(v.resize(5));
  CHECK(v.size() == 5);
  for (std::size_t i = 0; i < 5; ++i) {
    CHECK(v[i] == 0);
  }
  v[2] = 42;
  CHECK(v.resize(3));
  CHECK(v.size() == 3);
  CHECK(v[2] == 42);
  CHECK(v.resize(0));
  CHECK(v.is_empty());
}

void check_resize_tracker() {
  g_alive = 0;
  xer::vector<Tracker> v;
  CHECK(v.resize(4));
  CHECK(g_alive == 4);
  CHECK(v.resize(1));
  CHECK(g_alive == 1);
  CHECK(v.resize(0));
  CHECK(g_alive == 0);
}

void check_shrink_to_fit() {
  xer::vector<int> v;
  CHECK(v.reserve(32));
  v.push_back_unchecked(1);
  v.push_back_unchecked(2);
  CHECK(v.shrink_to_fit());
  CHECK(v.capacity() == 2);
  CHECK(v.size() == 2);
  CHECK(v[0] == 1 && v[1] == 2);

  // size_ == capacity_: no-op, still succeeds
  int* const p = v.data();
  CHECK(v.shrink_to_fit());
  CHECK(v.data() == p);
  CHECK(v.capacity() == 2);

  v.clear();
  CHECK(v.shrink_to_fit());
  CHECK(v.capacity() == 0);
  CHECK(v.data() == nullptr);

  // already empty with zero capacity
  CHECK(v.shrink_to_fit());
  CHECK(v.capacity() == 0);
  CHECK(v.data() == nullptr);
}

void check_shrink_to_fit_tracker() {
  // Tracker is not reallocable; must use transfer path and keep values/lifetime.
  g_alive = 0;
  {
    xer::vector<Tracker> v;
    CHECK(v.reserve(16));
    v.emplace_back_unchecked(1);
    v.emplace_back_unchecked(2);
    v.emplace_back_unchecked(3);
    CHECK(g_alive == 3);
    CHECK(v.shrink_to_fit());
    CHECK(v.capacity() == 3);
    CHECK(v[0].v == 1 && v[1].v == 2 && v[2].v == 3);
    CHECK(g_alive == 3);
  }
  CHECK(g_alive == 0);
}

struct Align64 {
  alignas(64) int v;
  Align64() noexcept : v(0) {}
  explicit Align64(int x) noexcept : v(x) {}
};

void check_shrink_overaligned() {
  static_assert(!xer::reallocable<Align64>);
  xer::vector<Align64> v;
  CHECK(v.reserve(8));
  v.emplace_back_unchecked(10);
  v.emplace_back_unchecked(20);
  CHECK(v.shrink_to_fit());
  CHECK(v.capacity() == 2);
  CHECK(v[0].v == 10 && v[1].v == 20);
}

void check_erase_edges() {
  xer::vector<int> v;
  CHECK(v.reserve(5));
  for (int i = 0; i < 5; ++i) {
    v.push_back_unchecked(i);
  }
  CHECK(!v.erase(5));
  CHECK(v.erase(4));
  CHECK(v.size() == 4);
  CHECK(v[3] == 3);
  CHECK(v.erase(0));
  CHECK(v[0] == 1);

  xer::vector<int> u;
  CHECK(u.reserve(1));
  u.push_back_unchecked(9);
  CHECK(u.unordered_erase(0));
  CHECK(u.is_empty());
}

void check_range_for() {
  xer::vector<int> v;
  CHECK(v.reserve(4));
  for (int i = 1; i <= 4; ++i) {
    v.push_back_unchecked(i);
  }
  int sum = 0;
  for (int x : v) {
    sum += x;
  }
  CHECK(sum == 10);
}

} // namespace

int main() {
  std::printf("xer_vector correctness\n");
  check_basic_int();
  check_growth_via_reserve();
  check_move();
  check_tracker_lifetime();
  check_move_only();
  check_resize();
  check_resize_tracker();
  check_shrink_to_fit();
  check_shrink_to_fit_tracker();
  check_shrink_overaligned();
  check_erase_edges();
  check_range_for();

  if (g_failures != 0) {
    std::printf("RESULT: FAILED (%d checks)\n", g_failures);
    return 1;
  }
  std::printf("RESULT: PASSED\n");
  return 0;
}
