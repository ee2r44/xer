// Correctness for include/xer/bit/xer_byteswap.h
// Build:
//   MSVC:  cl /std:c++latest /EHsc /Iinclude /Fe:tests\bit\xer_byteswap_correctness_msvc.exe tests\bit\xer_byteswap_correctness.cpp
//   GCC:   g++ -std=c++23 -Iinclude -o tests/bit/xer_byteswap_correctness_gcc tests/bit/xer_byteswap_correctness.cpp
//   Clang: clang++ -std=c++23 -Iinclude -o tests/bit/xer_byteswap_correctness_clang tests/bit/xer_byteswap_correctness.cpp

#include "xer/bit/xer_byteswap.h"
#include "xer/macro/xer_macro.h"

#include <cstdint>
#include <cstdio>
#include <type_traits>

#if __cplusplus >= 202302L && defined(__cpp_lib_byteswap)
#include <bit>
#define XER_TEST_HAS_STD_BYTESWAP 1
#else
#define XER_TEST_HAS_STD_BYTESWAP 0
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
void check_roundtrip(T v) {
  const T once = xer::byteswap(v);
  const T twice = xer::byteswap(once);
  CHECK(twice == v);
}

void check_u8() {
  CHECK(xer::byteswap(std::uint8_t{0xAB}) == std::uint8_t{0xAB});
  check_roundtrip(std::uint8_t{0x00});
  check_roundtrip(std::uint8_t{0xFF});
}

void check_u16() {
  CHECK(xer::byteswap(std::uint16_t{0x1234}) == std::uint16_t{0x3412});
  CHECK(xer::byteswap(std::uint16_t{0x0000}) == std::uint16_t{0x0000});
  CHECK(xer::byteswap(std::uint16_t{0xFFFF}) == std::uint16_t{0xFFFF});
  check_roundtrip(std::uint16_t{0xA1B2});
}

void check_u32() {
  CHECK(xer::byteswap(std::uint32_t{0x12345678u}) == std::uint32_t{0x78563412u});
  CHECK(xer::byteswap(std::uint32_t{0x00000000u}) == std::uint32_t{0x00000000u});
  CHECK(xer::byteswap(std::uint32_t{0xFFFFFFFFu}) == std::uint32_t{0xFFFFFFFFu});
  check_roundtrip(std::uint32_t{0xA1B2C3D4u});
}

void check_u64() {
  CHECK(xer::byteswap(std::uint64_t{0x0123456789ABCDEFull}) ==
        std::uint64_t{0xEFCDAB8967452301ull});
  CHECK(xer::byteswap(std::uint64_t{0}) == std::uint64_t{0});
  CHECK(xer::byteswap(std::uint64_t{~std::uint64_t{0}}) ==
        std::uint64_t{~std::uint64_t{0}});
  check_roundtrip(std::uint64_t{0xA1B2C3D4E5F60718ull});
}

void check_vs_std() {
#if XER_TEST_HAS_STD_BYTESWAP
  CHECK(xer::byteswap(std::uint16_t{0xBEEF}) ==
        std::byteswap(std::uint16_t{0xBEEF}));
  CHECK(xer::byteswap(std::uint32_t{0xDEADBEEFu}) ==
        std::byteswap(std::uint32_t{0xDEADBEEFu}));
  CHECK(xer::byteswap(std::uint64_t{0x0123456789ABCDEFull}) ==
        std::byteswap(std::uint64_t{0x0123456789ABCDEFull}));
#else
  CHECK(true);
#endif
}

void check_consteval() {
  constexpr auto a = xer::byteswap(std::uint16_t{0x1234});
  constexpr auto b = xer::byteswap(std::uint32_t{0x12345678u});
  constexpr auto c = xer::byteswap(std::uint64_t{0x0123456789ABCDEFull});
  static_assert(a == std::uint16_t{0x3412});
  static_assert(b == std::uint32_t{0x78563412u});
  static_assert(c == std::uint64_t{0xEFCDAB8967452301ull});
  CHECK(a == std::uint16_t{0x3412});
  CHECK(b == std::uint32_t{0x78563412u});
  CHECK(c == std::uint64_t{0xEFCDAB8967452301ull});
}

#if XER_HAS_BUILTIN_INT128
void check_u128() {
  using U128 = unsigned __int128;
  const U128 v = (U128{0x0123456789ABCDEFull} << 64) | U128{0xFEDCBA9876543210ull};
  const U128 expect =
      (U128{0x1032547698BADCFEull} << 64) | U128{0xEFCDAB8967452301ull};
  const U128 got = xer::byteswap(v);
  CHECK(got == expect);
  CHECK(xer::byteswap(got) == v);

  constexpr U128 cv =
      (U128{0x0123456789ABCDEFull} << 64) | U128{0xFEDCBA9876543210ull};
  constexpr U128 cexpect =
      (U128{0x1032547698BADCFEull} << 64) | U128{0xEFCDAB8967452301ull};
  static_assert(xer::byteswap(cv) == cexpect);
}
#endif

} // namespace

int main() {
  std::printf("xer_byteswap correctness\n");
  check_u8();
  check_u16();
  check_u32();
  check_u64();
  check_vs_std();
  check_consteval();
#if XER_HAS_BUILTIN_INT128
  check_u128();
#endif

  if (g_failures != 0) {
    std::printf("RESULT: FAILED (%d checks)\n", g_failures);
    return 1;
  }
  std::printf("RESULT: PASSED\n");
  return 0;
}
