// Correctness for include/xer/bit/xer_countr_zero.h
// Build:
//   MSVC:  cl /std:c++latest /EHsc /Iinclude /Fe:tests\bit\xer_countr_zero_correctness_msvc.exe tests\bit\xer_countr_zero_correctness.cpp include\xer\env\xer_env.cpp
//   GCC:   g++ -std=c++23 -Iinclude -o tests/bit/xer_countr_zero_correctness_gcc tests/bit/xer_countr_zero_correctness.cpp include/xer/env/xer_env.cpp
//   Clang: clang++ -std=c++23 -Iinclude -o tests/bit/xer_countr_zero_correctness_clang tests/bit/xer_countr_zero_correctness.cpp include/xer/env/xer_env.cpp

#include "xer/bit/xer_countr_zero.h"
#include "xer/env/xer_env.h"
#include "xer/macro/xer_macro.h"

#include <bit>
#include <cstdint>
#include <cstdio>

namespace {

int g_failures = 0;

#define CHECK(cond)                                                            \
  do {                                                                         \
    if (!(cond)) {                                                             \
      std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);              \
      ++g_failures;                                                            \
    }                                                                          \
  } while (0)

template <typename T>
constexpr T bit(std::uint32_t i) noexcept {
  return static_cast<T>(T{1} << i);
}

template <typename T>
void check_one(const xer::countr_zero& cz, T v, std::uint32_t expect) {
  const std::uint32_t got = cz(v);
  const std::uint32_t fb = xer::details_::countr_zero_fallback(v);
  if (got != expect || fb != expect) {
    std::printf("FAIL type_size=%zu v=%llu got=%u fb=%u expect=%u\n", sizeof(T),
                static_cast<unsigned long long>(v), got, fb, expect);
    ++g_failures;
  }
#if defined(__cpp_lib_bitops)
  if constexpr (sizeof(T) <= 8) {
    CHECK(got == static_cast<std::uint32_t>(std::countr_zero(v)));
  }
#endif
}

template <typename T>
void check_type(const xer::countr_zero& cz, const char* name) {
  std::printf("check %s\n", name);
  constexpr std::uint32_t bits = static_cast<std::uint32_t>(sizeof(T) * 8u);
  check_one(cz, T{0}, bits);
  for (std::uint32_t i = 0; i < bits; ++i) {
    check_one(cz, bit<T>(i), i);
    // OR in a higher bit so the lowest set bit stays at i.
    if (i + 3u < bits) {
      check_one(cz, static_cast<T>(bit<T>(i) | bit<T>(i + 3u)), i);
    }
  }
  check_one(cz, static_cast<T>(~T{0}), 0u);

  constexpr auto c0 = xer::details_::countr_zero_fallback(T{0});
  constexpr auto c1 = xer::details_::countr_zero_fallback(T{1});
  CHECK(c0 == bits);
  CHECK(c1 == 0u);
}

#if XER_HAS_BUILTIN_INT128
void check_u128(const xer::countr_zero& cz) {
  std::printf("check u128\n");
  using U = unsigned __int128;
  check_one(cz, U{0}, 128u);
  check_one(cz, U{1}, 0u);
  check_one(cz, U{1} << 63, 63u);
  check_one(cz, U{1} << 64, 64u);
  check_one(cz, U{1} << 127, 127u);
  const U v = (U{1} << 80) | (U{1} << 10);
  check_one(cz, v, 10u);
  const U v2 = (U{1} << 80);
  check_one(cz, v2, 80u);
}
#endif

} // namespace

int main() {
  std::printf("xer_countr_zero_correctness compiler=");
#if XER_COMPILER(MSVC)
  std::printf("msvc\n");
#elif XER_COMPILER(GCC)
  std::printf("gcc\n");
#elif XER_COMPILER(CLANG)
  std::printf("clang\n");
#else
  std::printf("other\n");
#endif
  std::printf("bmi1_macro=%d\n", (int)XER_BMI1_AVAILABLE);

  xer::env e;
  e.try_set();
  xer::countr_zero cz(e);
  cz.reset_env(e);

  check_type<std::uint8_t>(cz, "u8");
  check_type<std::uint16_t>(cz, "u16");
  check_type<std::uint32_t>(cz, "u32");
  check_type<std::uint64_t>(cz, "u64");
#if XER_HAS_BUILTIN_INT128
  check_u128(cz);
#endif

  // Default-constructed functor (may lack tzcnt flag).
  xer::countr_zero cz0;
  CHECK(cz0(std::uint32_t{8}) == 3u);
  CHECK(cz0(std::uint8_t{0}) == 8u);
  CHECK(cz0(std::uint16_t{0}) == 16u);

  if (g_failures != 0) {
    std::printf("FAILED %d\n", g_failures);
    return 1;
  }
  std::printf("ALL_PASSED\n");
  return 0;
}
