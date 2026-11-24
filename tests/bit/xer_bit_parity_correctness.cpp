// Correctness for include/xer/bit/xer_bit_parity.h
// Build:
//   MSVC:  cl /std:c++latest /EHsc /Iinclude /Fe:tests\bit\xer_bit_parity_correctness_msvc.exe tests\bit\xer_bit_parity_correctness.cpp include\xer\env\xer_env.cpp
//   GCC:   g++ -std=c++23 -Iinclude -o tests/bit/xer_bit_parity_correctness_gcc tests/bit/xer_bit_parity_correctness.cpp include/xer/env/xer_env.cpp
//   Clang: clang++ -std=c++23 -Iinclude -o tests/bit/xer_bit_parity_correctness_clang tests/bit/xer_bit_parity_correctness.cpp include/xer/env/xer_env.cpp

#include "xer/bit/xer_bit_parity.h"
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
constexpr std::uint32_t expect_parity(T v) noexcept {
  return static_cast<std::uint32_t>(std::popcount(v) & 1);
}

template <typename T>
void check_val(T v, const char* tag) {
  const auto got = xer::bit_parity(v);
  const auto fb = xer::details_::bit_parity_fallback(v);
  const auto exp = expect_parity(v);
  if (got != exp || fb != exp) {
    std::printf("FAIL %s sizeof=%zu v=%llu got=%u fb=%u expect=%u\n", tag,
                sizeof(T), static_cast<unsigned long long>(v), got, fb, exp);
    ++g_failures;
  }
}

template <typename T>
void check_type(const char* name) {
  std::printf("check %s\n", name);
  constexpr std::uint32_t bits = static_cast<std::uint32_t>(sizeof(T) * 8u);
  check_val(T{0}, name);
  check_val(T{1}, name);
  check_val(static_cast<T>(~T{0}), name);
  check_val(static_cast<T>(0xAAu), name);
  check_val(static_cast<T>(0x55u), name);

  for (std::uint32_t i = 0; i < bits; ++i) {
    check_val(static_cast<T>(T{1} << i), name);
    if (i + 1u < bits) {
      check_val(static_cast<T>((T{1} << i) | (T{1} << (i + 1u))), name);
    }
  }
  for (std::uint32_t n = 0; n <= bits && n <= 32u; ++n) {
    T v{0};
    for (std::uint32_t i = 0; i < n; ++i) v = static_cast<T>(v | (T{1} << i));
    check_val(v, name);
  }
}

#if XER_HAS_BUILTIN_INT128
void check_u128() {
  std::printf("check u128\n");
  using U = unsigned __int128;
  auto expect128 = [](U v) -> std::uint32_t {
    return xer::details_::bit_parity_fallback(v);
  };
  auto check128 = [&](U v) {
    const auto got = xer::bit_parity(v);
    const auto fb = expect128(v);
    // Reference: xor of low/high 64-bit popcount parities.
    const auto lo = static_cast<std::uint64_t>(v);
    const auto hi = static_cast<std::uint64_t>(v >> 64);
    const auto exp = static_cast<std::uint32_t>((std::popcount(lo) ^ std::popcount(hi)) & 1);
    if (got != exp || fb != exp) {
      std::printf("FAIL u128 got=%u fb=%u expect=%u\n", got, fb, exp);
      ++g_failures;
    }
  };
  check128(U{0});
  check128(U{1});
  check128(U{1} << 64);
  check128(U{1} << 127);
  check128(~U{0});
  check128((U{1} << 64) | U{1});
  check128(U{0xF} | (U{0x1} << 80));
}
#endif

void check_constexpr() {
  constexpr auto a = xer::bit_parity(std::uint32_t{0});
  constexpr auto b = xer::bit_parity(std::uint32_t{1});
  constexpr auto c = xer::bit_parity(std::uint32_t{0xF0F0F0F0u});
  constexpr auto d = xer::bit_parity(std::uint64_t{0xFFFFFFFFFFFFFFFFull});
  CHECK(a == 0u);
  CHECK(b == 1u);
  CHECK(c == 0u);  // 16 ones
  CHECK(d == 0u);  // 64 ones
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
