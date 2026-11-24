// Correctness for include/xer/container/xer_span.h
// Build:
//   MSVC:  cl /std:c++latest /EHsc /Iinclude /Fe:tests\container\xer_span_correctness_msvc.exe tests\container\xer_span_correctness.cpp
//   GCC:   g++ -std=c++23 -Iinclude -o tests/container/xer_span_correctness_gcc tests/container/xer_span_correctness.cpp
//   Clang: clang++ -std=c++23 -Iinclude -o tests/container/xer_span_correctness_clang tests/container/xer_span_correctness.cpp

#include "xer/container/xer_span.h"
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

void check_empty() {
  xer::span<int> s;
  CHECK(s.is_empty());
  CHECK(s.size() == 0);
  CHECK(s.data() == nullptr);
  CHECK(s.begin() == s.end());
}

void check_const_span_over_array() {
  int buf[4] = {1, 2, 3, 4};
  xer::span<int, true> s(buf, 4); // default is_const=true
  CHECK(!s.is_empty());
  CHECK(s.size() == 4);
  CHECK(s.data() == buf);
  CHECK(s[0] == 1 && s[3] == 4);
  CHECK(s.begin() + 4 == s.end());

  int sum = 0;
  for (const int& x : s) {
    sum += x;
  }
  CHECK(sum == 10);
}

void check_mutable_span() {
  int buf[3] = {10, 20, 30};
  xer::span<int, false> s(buf, 3);
  s[1] = 21;
  CHECK(buf[1] == 21);

  // const span object can still mutate elements when is_const=false (view semantics)
  const xer::span<int, false> cs = s;
  cs[0] = 11;
  CHECK(buf[0] == 11);
}

void check_from_xer_array() {
  xer::array<int, 3> a{5, 6, 7};
  xer::span<int, false> s(a.data(), a.size());
  CHECK(s[2] == 7);
  s[2] = 70;
  CHECK(a[2] == 70);

  xer::span<int, true> cs(a.data(), a.size());
  CHECK(cs[0] == 5);
}

void check_copy_assign() {
  int buf[2] = {1, 2};
  xer::span<int, false> a(buf, 2);
  xer::span<int, false> b;
  b = a;
  CHECK(b.size() == 2);
  CHECK(b.data() == buf);
  xer::span<int, false> c(a);
  CHECK(c[1] == 2);
}

void check_type_traits() {
  using C = xer::span<int, true>;
  using M = xer::span<int, false>;
  static_assert(std::is_trivially_copyable_v<C>);
  static_assert(std::is_trivially_copyable_v<M>);
  static_assert(std::is_nothrow_default_constructible_v<C>);
  static_assert(std::is_same_v<decltype(std::declval<C>().data()), const int*>);
  static_assert(std::is_same_v<decltype(std::declval<M>().data()), int*>);
  static_assert(std::is_same_v<decltype(std::declval<C>()[0]), const int&>);
  static_assert(std::is_same_v<decltype(std::declval<M>()[0]), int&>);
  CHECK(true);
}

void check_mutable_to_const_conversion() {
  int buf[2] = {3, 4};
  xer::span<int, false> m(buf, 2);
  xer::span<int, true> c(m);
  CHECK(c.data() == buf);
  CHECK(c.size() == 2);
  CHECK(c[0] == 3 && c[1] == 4);

  xer::span<int, true> c2{};
  c2 = xer::span<int, true>(m); // convert then copy-assign
  CHECK(c2[1] == 4);

  // mutable <- const must not compile; runtime path only checks mutable->const
  static_assert(std::is_constructible_v<xer::span<int, true>, xer::span<int, false>>);
  static_assert(!std::is_constructible_v<xer::span<int, false>, xer::span<int, true>>);
}

void check_constexpr_full() {
  constexpr xer::span<int> empty{};
  static_assert(empty.size() == 0);
  static_assert(empty.is_empty());
  static_assert(empty.data() == nullptr);
  static_assert(empty.begin() == empty.end());

  static constexpr int buf[2] = {9, 8};
  constexpr xer::span<int, true> s(buf, 2);
  static_assert(s.size() == 2);
  static_assert(!s.is_empty());
  static_assert(s.data() == buf);
  static_assert(s[0] == 9);
  static_assert(s[1] == 8);
  static_assert(s.begin() + 2 == s.end());

  constexpr auto conv_ok = []() constexpr {
    int local[2] = {9, 8};
    xer::span<int, false> m(local, 2);
    xer::span<int, true> c(m);
    return c.size() == 2 && c[0] == 9 && c[1] == 8;
  }();
  static_assert(conv_ok);
  CHECK(conv_ok);
}

} // namespace

int main() {
  std::printf("xer_span correctness\n");
  check_empty();
  check_const_span_over_array();
  check_mutable_span();
  check_from_xer_array();
  check_copy_assign();
  check_type_traits();
  check_mutable_to_const_conversion();
  check_constexpr_full();

  if (g_failures != 0) {
    std::printf("RESULT: FAILED (%d checks)\n", g_failures);
    return 1;
  }
  std::printf("RESULT: PASSED\n");
  return 0;
}
