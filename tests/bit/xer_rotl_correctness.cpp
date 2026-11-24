// Correctness for include/xer/bit/xer_rotl.h
// Build:
//   MSVC:  cl /std:c++latest /EHsc /Iinclude /Fe:tests\bit\xer_rotl_correctness_msvc.exe tests\bit\xer_rotl_correctness.cpp
//   GCC:   g++ -std=c++23 -Iinclude -o tests/bit/xer_rotl_correctness_gcc tests/bit/xer_rotl_correctness.cpp
//   Clang: clang++ -std=c++23 -Iinclude -o tests/bit/xer_rotl_correctness_clang tests/bit/xer_rotl_correctness.cpp

#include "xer/bit/xer_rotl.h"
#include "xer/macro/xer_macro.h"

#include <cstdint>
#include <cstdio>
#include <type_traits>

#if defined(__cpp_lib_bitops) && __cpp_lib_bitops >= 201907L
#include <bit>
#define XER_TEST_HAS_STD_ROTL 1
#else
#define XER_TEST_HAS_STD_ROTL 0
#endif

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
constexpr T ref_rotl(T t, std::uint32_t count) noexcept {
  constexpr std::uint32_t bit_count = static_cast<std::uint32_t>(sizeof(T) * 8u);
  constexpr std::uint32_t mask = bit_count - 1u;
  const std::uint32_t c = count & mask;
  return static_cast<T>((t << c) | (t >> ((0u - c) & mask)));
}

template <typename T>
void check_one(T v, std::uint32_t count) {
  const T got = xer::rotl(v, count);
  const T expect = ref_rotl(v, count);
  CHECK(got == expect);
#if XER_TEST_HAS_STD_ROTL
  if constexpr (!std::is_same_v<T, bool> && sizeof(T) <= 8) {
    CHECK(got == std::rotl(v, static_cast<int>(count)));
  }
#endif
  CHECK(xer::details_::rotl_fallback(v, count) == expect);
}

template <typename T>
void check_type(const char* name) {
  std::printf("check %s\n", name);
  check_one(T{0}, 0u);
  check_one(T{1}, 0u);
  check_one(T{1}, 1u);
  check_one(static_cast<T>(~T{0}), 1u);
  check_one(static_cast<T>(T{1} << (sizeof(T) * 4u)), static_cast<std::uint32_t>(sizeof(T) * 4u));

  constexpr std::uint32_t bits = static_cast<std::uint32_t>(sizeof(T) * 8u);
  for (std::uint32_t c = 0; c < bits * 3u + 7u; ++c) {
    check_one(static_cast<T>(0xA5A5A5A5A5A5A5A5ull), c);
    check_one(static_cast<T>(0x0123456789ABCDEFull), c);
  }

  // large / unmasked counts
  check_one(T{0x11}, bits);
  check_one(T{0x11}, bits + 3u);
  check_one(T{0x11}, 0xFFFFFFFFu);
  check_one(T{0x11}, 0x80000000u);

  // constexpr path
  constexpr T cv = xer::rotl(T{1}, 1u);
  CHECK(cv == ref_rotl(T{1}, 1u));
}

#if XER_HAS_BUILTIN_INT128
void check_u128() {
  std::printf("check u128\n");
  using U = unsigned __int128;
  check_one(U{0}, 0u);
  check_one(U{1}, 1u);
  check_one(U{1}, 64u);
  check_one(U{1}, 127u);
  check_one(U{1}, 128u);
  check_one(U{1}, 129u);
  const U v = (U{0x0123456789ABCDEFull} << 64) | U{0xFEDCBA9876543210ull};
  for (std::uint32_t c = 0; c < 256u; c += 3u) {
    check_one(v, c);
  }
  constexpr U cv = xer::rotl(U{1}, 1u);
  CHECK(cv == ref_rotl(U{1}, 1u));
}
#endif

} // namespace

int main() {
  std::printf("xer_rotl_correctness compiler=");
#if XER_COMPILER(MSVC)
  std::printf("msvc\n");
#elif XER_COMPILER(GCC)
  std::printf("gcc\n");
#elif XER_COMPILER(CLANG)
  std::printf("clang\n");
#else
  std::printf("other\n");
#endif

  check_type<std::uint8_t>("u8");
  check_type<std::uint16_t>("u16");
  check_type<std::uint32_t>("u32");
  check_type<std::uint64_t>("u64");
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
