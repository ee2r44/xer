// Correctness for include/xer/algorithm/xer_min.h and xer_max.h
// Build:
//   MSVC:  cl /std:c++latest /EHsc /Iinclude /Fe:tests\algorithm\xer_min_max_correctness_msvc.exe tests\algorithm\xer_min_max_correctness.cpp
//   GCC:   g++ -std=c++23 -Iinclude -o tests/algorithm/xer_min_max_correctness_gcc tests/algorithm/xer_min_max_correctness.cpp
//   Clang: clang++ -std=c++23 -Iinclude -o tests/algorithm/xer_min_max_correctness_clang tests/algorithm/xer_min_max_correctness.cpp

#include "xer/algorithm/xer_max.h"
#include "xer/algorithm/xer_min.h"

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

struct OnlyLess {
  int v;
  friend constexpr bool operator<(const OnlyLess& a, const OnlyLess& b) noexcept {
    return a.v < b.v;
  }
};

struct OnlyGreater {
  int v;
  friend constexpr bool operator>(const OnlyGreater& a,
                                  const OnlyGreater& b) noexcept {
    return a.v > b.v;
  }
};

struct Both {
  int v;
  int id;
  friend constexpr bool operator<(const Both& a, const Both& b) noexcept {
    return a.v < b.v;
  }
  friend constexpr bool operator>(const Both& a, const Both& b) noexcept {
    return a.v > b.v;
  }
};

struct IntCompare {
  int v;
  friend constexpr int operator<(const IntCompare& a,
                                 const IntCompare& b) noexcept {
    return a.v < b.v;
  }
  friend constexpr int operator>(const IntCompare& a,
                                 const IntCompare& b) noexcept {
    return a.v > b.v;
  }
};

void check_integers() {
  CHECK(xer::max(1, 2) == 2);
  CHECK(xer::max(2, 1) == 2);
  CHECK(xer::min(1, 2) == 1);
  CHECK(xer::min(2, 1) == 1);
  CHECK(xer::max(5, 5) == 5);
  CHECK(xer::min(5, 5) == 5);
}

void check_tie_prefers_second() {
  const int a = 5;
  const int b = 5;
  CHECK(&xer::max(a, b) == &b);
  CHECK(&xer::min(a, b) == &b);

  Both x{1, 10};
  Both y{1, 20};
  CHECK(&xer::max(x, y) == &y);
  CHECK(&xer::min(x, y) == &y);
}

void check_identity() {
  const int x = 3;
  const int y = 7;
  CHECK(&xer::max(x, y) == &y);
  CHECK(&xer::min(x, y) == &x);
  CHECK(&xer::max(y, x) == &y);
  CHECK(&xer::min(y, x) == &x);
}

void check_concepts() {
  static_assert(xer::details_::max_judgable<int>);
  static_assert(xer::details_::min_judgable<int>);
  static_assert(xer::details_::max_judgable<Both>);
  static_assert(xer::details_::min_judgable<Both>);

  static_assert(!xer::details_::max_judgable<OnlyLess>);
  static_assert(xer::details_::min_judgable<OnlyLess>);
  static_assert(xer::details_::max_judgable<OnlyGreater>);
  static_assert(!xer::details_::min_judgable<OnlyGreater>);

  // Nested requires now rejects non-bool comparison results.
  static_assert(!xer::details_::max_judgable<IntCompare>);
  static_assert(!xer::details_::min_judgable<IntCompare>);

  OnlyLess l1{1};
  OnlyLess l2{2};
  CHECK(xer::min(l1, l2).v == 1);

  OnlyGreater g1{1};
  OnlyGreater g2{2};
  CHECK(xer::max(g1, g2).v == 2);
}

void check_constexpr() {
  constexpr int a = xer::max(1, 3);
  constexpr int b = xer::min(1, 3);
  constexpr int c = xer::max(xer::min(4, 2), 3);
  static_assert(a == 3);
  static_assert(b == 1);
  static_assert(c == 3);
  CHECK(a == 3 && b == 1 && c == 3);

  static constexpr Both x{1, 10};
  static constexpr Both y{2, 20};
  static_assert(xer::max(x, y).id == 20);
  static_assert(xer::min(x, y).id == 10);
}

} // namespace

int main() {
  std::printf("xer_min / xer_max correctness\n");
  check_integers();
  check_tie_prefers_second();
  check_identity();
  check_concepts();
  check_constexpr();

  if (g_failures != 0) {
    std::printf("RESULT: FAILED (%d checks)\n", g_failures);
    return 1;
  }
  std::printf("RESULT: PASSED\n");
  return 0;
}
