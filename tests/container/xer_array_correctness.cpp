// Correctness for include/xer/container/xer_array.h
// Build:
//   MSVC:  cl /std:c++latest /EHsc /Iinclude /Fe:tests\container\xer_array_correctness_msvc.exe tests\container\xer_array_correctness.cpp
//   GCC:   g++ -std=c++23 -Iinclude -o tests/container/xer_array_correctness_gcc tests/container/xer_array_correctness.cpp
//   Clang: clang++ -std=c++23 -Iinclude -o tests/container/xer_array_correctness_clang tests/container/xer_array_correctness.cpp

#include "xer/container/xer_array.h"

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

void check_aggregate_and_access() {
  xer::array<int, 3> a{10, 20, 30};
  CHECK(a.size() == 3);
  CHECK(a[0] == 10 && a[1] == 20 && a[2] == 30);
  CHECK(a.data() == &a[0]);
  CHECK(a.begin() == a.data());
  CHECK(a.end() == a.data() + 3);

  a[1] = 21;
  CHECK(a[1] == 21);

  const xer::array<int, 3>& ca = a;
  CHECK(ca[0] == 10);
  CHECK(ca.data() == a.data());
  CHECK(ca.begin() == ca.data());
  CHECK(ca.end() == ca.data() + 3);
}

void check_range_for() {
  xer::array<int, 4> a{1, 2, 3, 4};
  int sum = 0;
  for (int& x : a) {
    sum += x;
    x += 10;
  }
  CHECK(sum == 10);
  CHECK(a[0] == 11 && a[3] == 14);

  int csum = 0;
  for (const int& x : a) {
    csum += x;
  }
  CHECK(csum == 50);
}

void check_integral_index_types() {
  xer::array<int, 2> a{7, 8};
  const signed char i0 = 0;
  const unsigned i1 = 1u;
  CHECK(a[i0] == 7);
  CHECK(a[i1] == 8);
}

void check_constexpr() {
  constexpr xer::array<int, 2> a{4, 5};
  static_assert(a.size() == 2);
  static_assert(a[0] == 4);
  static_assert(a[1] == 5);
  static_assert(a.begin() + 2 == a.end());
  CHECK(a[0] == 4);
}

void check_type_traits() {
  using A = xer::array<int, 4>;
  static_assert(std::is_trivially_copyable_v<A>);
  static_assert(std::is_standard_layout_v<A>);
  static_assert(std::is_aggregate_v<A>);
  static_assert(sizeof(A) == sizeof(int) * 4);
  static_assert(alignof(A) == alignof(int));

  struct NonTrivial {
    int v;
    NonTrivial() : v(0) {}
    ~NonTrivial() {}
  };
  static_assert(!std::is_trivial_v<xer::array<NonTrivial, 2>>);
  CHECK(true);
}

void check_const_element_type() {
  xer::array<const int, 2> a{1, 2};
  CHECK(a[0] == 1);
  CHECK(a.size() == 2);
}

void check_nested_array_element() {
  using Row = int[2];
  xer::array<Row, 2> m{{{1, 2}, {3, 4}}};
  CHECK(m[0][0] == 1);
  CHECK(m[1][1] == 4);
}

} // namespace

int main() {
  std::printf("xer_array correctness\n");
  check_aggregate_and_access();
  check_range_for();
  check_integral_index_types();
  check_constexpr();
  check_type_traits();
  check_const_element_type();
  check_nested_array_element();

  if (g_failures != 0) {
    std::printf("RESULT: FAILED (%d checks)\n", g_failures);
    return 1;
  }
  std::printf("RESULT: PASSED\n");
  return 0;
}
