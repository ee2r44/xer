// Correctness for include/xer/memory/xer_construct.h
// Build:
//   MSVC:  cl /std:c++latest /EHsc /Iinclude /Fe:tests\memory\xer_construct_correctness_msvc.exe tests\memory\xer_construct_correctness.cpp
//   GCC:   g++ -std=c++23 -Iinclude -o tests/memory/xer_construct_correctness_gcc tests/memory/xer_construct_correctness.cpp
//   Clang: clang++ -std=c++23 -Iinclude -o tests/memory/xer_construct_correctness_clang tests/memory/xer_construct_correctness.cpp

#include "xer/memory/xer_construct.h"

#include <cstddef>
#include <cstdio>
#include <type_traits>

namespace {

int g_failures = 0;

#define CHECK(cond)                                                            \
  do {                                                                         \
    if (!(cond)) {                                                             \
      std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);              \
      ++g_failures;                                                            \
    }                                                                          \
  } while (0)

int g_alive = 0;

struct Tracker {
  int v;
  explicit Tracker(int x = 0) noexcept : v(x) { ++g_alive; }
  Tracker(const Tracker& o) noexcept : v(o.v) { ++g_alive; }
  Tracker(Tracker&& o) noexcept : v(o.v) {
    o.v = -1;
    ++g_alive;
  }
  Tracker& operator=(const Tracker&) = delete;
  Tracker& operator=(Tracker&&) = delete;
  ~Tracker() { --g_alive; }
};

struct MoveOnly {
  int v;
  explicit MoveOnly(int x = 0) noexcept : v(x) {}
  MoveOnly(const MoveOnly&) = delete;
  MoveOnly(MoveOnly&& o) noexcept : v(o.v) { o.v = -1; }
  MoveOnly& operator=(const MoveOnly&) = delete;
  MoveOnly& operator=(MoveOnly&&) = delete;
  ~MoveOnly() = default;
};

struct CopyOnly {
  int v;
  explicit CopyOnly(int x = 0) noexcept : v(x) {}
  CopyOnly(const CopyOnly& o) noexcept : v(o.v) {}
  CopyOnly(CopyOnly&&) = delete;
  CopyOnly& operator=(const CopyOnly&) = delete;
  CopyOnly& operator=(CopyOnly&&) = delete;
};

void check_construct_destroy_scalar() {
  alignas(Tracker) unsigned char buf[sizeof(Tracker)];
  auto* p = reinterpret_cast<Tracker*>(buf);
  g_alive = 0;
  xer::construct_at(p, 42);
  CHECK(g_alive == 1);
  CHECK(p->v == 42);
  xer::destroy_at(p);
  CHECK(g_alive == 0);
}

void check_destroy_n() {
  constexpr std::size_t n = 5;
  alignas(Tracker) unsigned char buf[sizeof(Tracker) * n];
  auto* p = reinterpret_cast<Tracker*>(buf);
  g_alive = 0;
  for (std::size_t i = 0; i < n; ++i) {
    xer::construct_at(p + i, static_cast<int>(i + 1));
  }
  CHECK(g_alive == static_cast<int>(n));
  xer::destroy_n(p, n);
  CHECK(g_alive == 0);
  xer::destroy_n(p, 0); // no-op
}

void check_destroy_at_array_type() {
  using A = Tracker[3];
  alignas(A) unsigned char buf[sizeof(A)];
  auto* p = reinterpret_cast<A*>(buf);
  g_alive = 0;
  // Manually construct elements; construct_at rejects array T.
  for (int i = 0; i < 3; ++i) {
    xer::construct_at((*p) + i, i + 10);
  }
  CHECK(g_alive == 3);
  xer::destroy_at(p);
  CHECK(g_alive == 0);
}

void check_transfer_trivially_copyable() {
  int src[4] = {1, 2, 3, 4};
  int dst[4] = {};
  xer::transfer_construct_n(dst, src, 4);
  CHECK(dst[0] == 1 && dst[1] == 2 && dst[2] == 3 && dst[3] == 4);
  xer::transfer_construct_n(dst, src, 0);
}

void check_transfer_move_only() {
  constexpr std::size_t n = 4;
  alignas(MoveOnly) unsigned char sb[sizeof(MoveOnly) * n];
  alignas(MoveOnly) unsigned char db[sizeof(MoveOnly) * n];
  auto* src = reinterpret_cast<MoveOnly*>(sb);
  auto* dst = reinterpret_cast<MoveOnly*>(db);
  for (std::size_t i = 0; i < n; ++i) {
    xer::construct_at(src + i, static_cast<int>(i + 1));
  }
  xer::transfer_construct_n(dst, src, n);
  for (std::size_t i = 0; i < n; ++i) {
    CHECK(dst[i].v == static_cast<int>(i + 1));
  }
  // Source objects were destroyed after move; reconstruct to free buffer cleanly
  // is unnecessary for raw buffer end-of-test.
  xer::destroy_n(dst, n);
}

void check_transfer_tracker_alive() {
  constexpr std::size_t n = 6;
  alignas(Tracker) unsigned char sb[sizeof(Tracker) * n];
  alignas(Tracker) unsigned char db[sizeof(Tracker) * n];
  auto* src = reinterpret_cast<Tracker*>(sb);
  auto* dst = reinterpret_cast<Tracker*>(db);
  g_alive = 0;
  for (std::size_t i = 0; i < n; ++i) {
    xer::construct_at(src + i, static_cast<int>(i));
  }
  CHECK(g_alive == static_cast<int>(n));
  xer::transfer_construct_n(dst, src, n);
  // After relocate: dst alive, src destroyed.
  CHECK(g_alive == static_cast<int>(n));
  for (std::size_t i = 0; i < n; ++i) {
    CHECK(dst[i].v == static_cast<int>(i));
  }
  xer::destroy_n(dst, n);
  CHECK(g_alive == 0);
}

void check_transfer_copy_only() {
  constexpr std::size_t n = 3;
  alignas(CopyOnly) unsigned char sb[sizeof(CopyOnly) * n];
  alignas(CopyOnly) unsigned char db[sizeof(CopyOnly) * n];
  auto* src = reinterpret_cast<CopyOnly*>(sb);
  auto* dst = reinterpret_cast<CopyOnly*>(db);
  for (std::size_t i = 0; i < n; ++i) {
    xer::construct_at(src + i, static_cast<int>(i + 7));
  }
  xer::transfer_construct_n(dst, src, n);
  for (std::size_t i = 0; i < n; ++i) {
    CHECK(dst[i].v == static_cast<int>(i + 7));
  }
  xer::destroy_n(dst, n);
}

void check_transfer_ints_runtime() {
  int src[2] = {9, 8};
  int dst[2] = {};
  xer::transfer_construct_n(dst, src, 2);
  CHECK(dst[0] == 9 && dst[1] == 8);
}

void check_noexcept_surface() {
  static_assert(noexcept(xer::construct_at(static_cast<int*>(nullptr), 1)));
  static_assert(noexcept(xer::destroy_at(static_cast<int*>(nullptr))));
  static_assert(noexcept(xer::destroy_n(static_cast<int*>(nullptr), 0)));
  static_assert(
      noexcept(xer::transfer_construct_n(static_cast<int*>(nullptr),
                                         static_cast<int*>(nullptr), 0)));
}

} // namespace

int main() {
  std::printf("xer_construct correctness\n");
  check_construct_destroy_scalar();
  check_destroy_n();
  check_destroy_at_array_type();
  check_transfer_trivially_copyable();
  check_transfer_move_only();
  check_transfer_tracker_alive();
  check_transfer_copy_only();
  check_transfer_ints_runtime();
  check_noexcept_surface();

  if (g_failures != 0) {
    std::printf("RESULT: FAILED (%d checks)\n", g_failures);
    return 1;
  }
  std::printf("RESULT: PASSED\n");
  return 0;
}
