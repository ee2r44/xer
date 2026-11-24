// Correctness for include/xer/bit/xer_popcount.h
// Build:
//   MSVC:  cl /std:c++latest /EHsc /Iinclude /Fe:tests\bit\xer_popcount_correctness_msvc.exe tests\bit\xer_popcount_correctness.cpp include\xer\env\xer_env.cpp
//   GCC:   g++ -std=c++23 -Iinclude -o tests/bit/xer_popcount_correctness_gcc tests/bit/xer_popcount_correctness.cpp include/xer/env/xer_env.cpp
//   Clang: clang++ -std=c++23 -Iinclude -o tests/bit/xer_popcount_correctness_clang tests/bit/xer_popcount_correctness.cpp include/xer/env/xer_env.cpp
// Optional: add -mpopcnt / -march=native to exercise compile-time POPCNT path.

#include "xer/bit/xer_popcount.h"
#include "xer/macro/xer_macro.h"

#include <bit>
#include <cstdint>
#include <cstdio>
#include <limits>

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
void check_val(T v, std::uint32_t expect, const char* tag) {
  const auto got = xer::popcount(v);
  const auto fb = xer::details_::popcount_fallback(v);
  if (got != expect || fb != expect) {
    std::printf("FAIL %s sizeof=%zu v=%llu got=%u fb=%u expect=%u\n", tag,
                sizeof(T), static_cast<unsigned long long>(v), got, fb, expect);
    ++g_failures;
  }
#if defined(__cpp_lib_bitops)
  if constexpr (sizeof(T) <= 8) {
    CHECK(got == static_cast<std::uint32_t>(std::popcount(v)));
    CHECK(fb == static_cast<std::uint32_t>(std::popcount(v)));
  }
#endif
}

template <typename T>
void check_type(const char* name) {
  std::printf("check %s\n", name);
  constexpr std::uint32_t bits = static_cast<std::uint32_t>(sizeof(T) * 8u);
  check_val(T{0}, 0u, name);
  check_val(T{1}, 1u, name);
  check_val(static_cast<T>(~T{0}), bits, name);

  for (std::uint32_t i = 0; i < bits; ++i) {
    check_val(static_cast<T>(T{1} << i), 1u, name);
  }
  // Dense patterns that stress SWAR masks.
  check_val(static_cast<T>(0xFFu), bits < 8 ? bits : 8u, name);
  if (bits >= 16) check_val(static_cast<T>(0xFFFFu), 16u, name);
  if (bits >= 32) check_val(static_cast<T>(0xFFFFFFFFu), 32u, name);
  if (bits >= 64) {
    check_val(static_cast<T>(0xFFFFFFFFFFFFFFFFull), 64u, name);
    check_val(static_cast<T>(0x0123456789ABCDEFull), 32u, name);
  }

  for (std::uint32_t n = 0; n <= bits; ++n) {
    T v{0};
    for (std::uint32_t i = 0; i < n; ++i) v = static_cast<T>(v | (T{1} << i));
    check_val(v, n, name);
  }
}

#if XER_HAS_BUILTIN_INT128
void check_u128() {
  std::printf("check u128\n");
  using U = unsigned __int128;
  check_val(U{0}, 0u, "u128");
  check_val(U{1}, 1u, "u128");
  check_val(U{1} << 64, 1u, "u128");
  check_val(U{1} << 127, 1u, "u128");
  const U all = ~U{0};
  check_val(all, 128u, "u128");
  const U v = (U{0xFF} << 60) | U{0x0F};
  check_val(v, 8u + 4u, "u128");

  constexpr auto c = xer::details_::popcount_fallback(U{0xF});
  CHECK(c == 4u);
}
#endif

void check_constexpr() {
  constexpr auto a = xer::popcount(std::uint32_t{0});
  constexpr auto b = xer::popcount(std::uint32_t{0xF0F0F0F0u});
  constexpr auto c = xer::popcount(std::uint64_t{0xFFFFFFFFFFFFFFFFull});
  CHECK(a == 0u);
  CHECK(b == 16u);
  CHECK(c == 64u);
}

}  // namespace

int main() {
  std::printf("XER_POPCNT_AVAILABLE=%d popcnt_cpu=%d\n",
              (int)XER_POPCNT_AVAILABLE, (int)xer::is_popcnt_supported());

  check_type<std::uint8_t>("u8");
  check_type<std::uint16_t>("u16");
  check_type<std::uint32_t>("u32");
  check_type<std::uint64_t>("u64");
#if XER_HAS_BUILTIN_INT128
  check_u128();
#endif
  check_constexpr();

  if (g_failures == 0) {
    std::printf("ok\n");
    return 0;
  }
  std::printf("%d failure(s)\n", g_failures);
  return 1;
}
