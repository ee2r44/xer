// Correctness for include/xer/numeric/xer_sub_needborrow.h
// Build:
//   MSVC:  cl /std:c++latest /EHsc /Iinclude /Fe:tests\numeric\xer_sub_needborrow_correctness_msvc.exe tests\numeric\xer_sub_needborrow_correctness.cpp
//   GCC:   g++ -std=c++23 -Iinclude -o tests/numeric/xer_sub_needborrow_correctness_gcc tests/numeric/xer_sub_needborrow_correctness.cpp
//   Clang: clang++ -std=c++23 -Iinclude -o tests/numeric/xer_sub_needborrow_correctness_clang tests/numeric/xer_sub_needborrow_correctness.cpp

#include "xer/numeric/xer_sub_needborrow.h"
#include "xer/macro/xer_macro.h"

#include <cstdint>
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

// Borrow iff wider-unsigned subtraction wraps: static_cast<B>(l) < static_cast<B>(r)
// with B = larger of L,R (same rule as add_overflow's width choice).
template <typename L, typename R>
constexpr bool ref_needborrow(L l, R r) noexcept {
  using B = std::conditional_t<(sizeof(L) > sizeof(R)), L, R>;
  return static_cast<B>(l) < static_cast<B>(r);
}

template <typename L, typename R>
void check_pair(L l, R r) {
  const bool got = xer::sub_needborrow(l, r);
  const bool expect = ref_needborrow(l, r);
  CHECK(got == expect);

  using B = std::conditional_t<(sizeof(L) > sizeof(R)), L, R>;
  const B diff = static_cast<B>(static_cast<B>(l) - static_cast<B>(r));
  const bool wrapped = static_cast<B>(l) < static_cast<B>(r);
  CHECK(got == wrapped);
  if (!wrapped) {
    CHECK(diff == static_cast<B>(static_cast<B>(l) - static_cast<B>(r)));
  }
  (void)diff;
}

template <typename T>
void check_same_type(const char* name) {
  std::printf("check same %s\n", name);
  check_pair(T{0}, T{0});
  check_pair(T{0}, T{1});
  check_pair(T{1}, T{0});
  check_pair(T{1}, T{1});
  check_pair(static_cast<T>(~T{0}), T{0});
  check_pair(static_cast<T>(~T{0}), T{1});
  check_pair(T{0}, static_cast<T>(~T{0}));
  check_pair(static_cast<T>(~T{0}), static_cast<T>(~T{0}));

  constexpr bool c0 = xer::sub_needborrow(T{0}, T{1});
  constexpr bool c1 = xer::sub_needborrow(T{1}, T{0});
  CHECK(c0 == true);
  CHECK(c1 == false);
}

void check_mixed() {
  std::printf("check mixed\n");
  check_pair(std::uint8_t{0}, std::uint16_t{0});
  check_pair(std::uint8_t{0}, std::uint16_t{1});
  check_pair(std::uint8_t{1}, std::uint16_t{0});
  check_pair(std::uint8_t{0xFF}, std::uint16_t{0x100});
  check_pair(std::uint8_t{0xFF}, std::uint16_t{0xFF});
  check_pair(std::uint16_t{0x100}, std::uint8_t{0xFF});
  check_pair(std::uint32_t{0}, std::uint64_t{1});
  check_pair(std::uint64_t{1}, std::uint32_t{0});
  check_pair(std::uint32_t{1}, std::uint64_t{0});
  check_pair(std::uint8_t{1}, std::uint64_t{1});
}

void check_bool() {
  std::printf("check bool\n");
  check_pair(false, std::uint8_t{0});
  check_pair(false, std::uint8_t{1});
  check_pair(true, std::uint8_t{0});
  check_pair(true, std::uint8_t{1});
  check_pair(true, std::uint8_t{2});
  check_pair(std::uint8_t{0}, false);
  check_pair(std::uint8_t{0}, true);
  check_pair(std::uint8_t{1}, false);
  check_pair(std::uint8_t{1}, true);
  check_pair(false, std::uint32_t{1});
  check_pair(true, std::uint32_t{0});
  check_pair(std::uint64_t{0}, true);
  check_pair(std::uint64_t{1}, true);
}

#if XER_HAS_BUILTIN_INT128
void check_u128() {
  std::printf("check u128\n");
  using U = unsigned __int128;
  check_pair(U{0}, U{1});
  check_pair(U{1}, U{0});
  check_pair(U{0}, std::uint64_t{1});
  check_pair(std::uint64_t{0}, U{1});
  check_pair(static_cast<U>(~U{0}), U{1});
  check_pair(U{0}, static_cast<U>(~U{0}));
}
#endif

static_assert(xer::details_::sub_needborrowable<std::uint32_t, std::uint32_t>);
static_assert(xer::details_::sub_needborrowable<std::uint8_t, std::uint64_t>);
static_assert(xer::details_::sub_needborrowable<bool, std::uint32_t>);
static_assert(xer::details_::sub_needborrowable<std::uint32_t, bool>);
static_assert(!xer::details_::sub_needborrowable<bool, bool>);
static_assert(!xer::details_::sub_needborrowable<int, std::uint32_t>);

} // namespace

int main() {
  std::printf("xer_sub_needborrow_correctness compiler=");
#if XER_COMPILER(MSVC)
  std::printf("msvc\n");
#elif XER_COMPILER(GCC)
  std::printf("gcc\n");
#elif XER_COMPILER(CLANG)
  std::printf("clang\n");
#else
  std::printf("other\n");
#endif

  check_same_type<std::uint8_t>("u8");
  check_same_type<std::uint16_t>("u16");
  check_same_type<std::uint32_t>("u32");
  check_same_type<std::uint64_t>("u64");
  check_mixed();
  check_bool();
#if XER_HAS_BUILTIN_INT128
  check_u128();
#endif

  if (g_failures != 0) {
    std::printf("FAILED %d\n", g_failures);
    return 1;
  }
  std::printf("ALL_PASSED\n");
  return 0;
}
