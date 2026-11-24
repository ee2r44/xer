// Correctness for include/xer/container/xer_bitset.h
// Build:
//   MSVC:  cl /std:c++latest /EHsc /Iinclude /Fe:tests\container\xer_bitset_correctness_msvc.exe tests\container\xer_bitset_correctness.cpp
//   GCC:   g++ -std=c++23 -Iinclude -o tests/container/xer_bitset_correctness_gcc tests/container/xer_bitset_correctness.cpp
//   Clang: clang++ -std=c++23 -Iinclude -o tests/container/xer_bitset_correctness_clang tests/container/xer_bitset_correctness.cpp

#include "xer/container/xer_bitset.h"
#include "xer/macro/xer_macro.h"

#include <cstddef>
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

template <std::size_t N>
void check_size() {
  xer::bitset<N> bs{};
  CHECK(bs.size() == N);
}

template <std::size_t N>
void check_set_get_reset_flip() {
  xer::bitset<N> bs{};
  for (std::size_t i = 0; i < N; ++i) {
    CHECK(bs.get_unchecked(i) == false);
  }

  for (std::size_t i = 0; i < N; ++i) {
    bs.set_unchecked(i);
    CHECK(bs.get_unchecked(i) == true);
    for (std::size_t j = 0; j < N; ++j) {
      CHECK(bs.get_unchecked(j) == (j <= i));
    }
  }

  for (std::size_t i = 0; i < N; ++i) {
    bs.reset_unchecked(i);
    CHECK(bs.get_unchecked(i) == false);
  }

  for (std::size_t i = 0; i < N; ++i) {
    bs.set_unchecked(i, (i & 1u) != 0u);
  }
  for (std::size_t i = 0; i < N; ++i) {
    CHECK(bs.get_unchecked(i) == ((i & 1u) != 0u));
  }

  for (std::size_t i = 0; i < N; ++i) {
    const bool before = bs.get_unchecked(i);
    bs.flip_unchecked(i);
    CHECK(bs.get_unchecked(i) == !before);
  }
}

template <std::size_t N>
void check_all_ops() {
  xer::bitset<N> bs{};
  bs.set_all();
  for (std::size_t i = 0; i < N; ++i) {
    CHECK(bs.get_unchecked(i) == true);
  }

  bs.reset_all();
  for (std::size_t i = 0; i < N; ++i) {
    CHECK(bs.get_unchecked(i) == false);
  }

  bs.set_all(true);
  for (std::size_t i = 0; i < N; ++i) {
    CHECK(bs.get_unchecked(i) == true);
  }
  bs.set_all(false);
  for (std::size_t i = 0; i < N; ++i) {
    CHECK(bs.get_unchecked(i) == false);
  }

  // Pattern then flip_all twice restores.
  for (std::size_t i = 0; i < N; ++i) {
    bs.set_unchecked(i, (i % 3u) == 0u);
  }
  xer::bitset<N> copy = bs;
  bs.flip_all();
  for (std::size_t i = 0; i < N; ++i) {
    CHECK(bs.get_unchecked(i) == !copy.get_unchecked(i));
  }
  bs.flip_all();
  for (std::size_t i = 0; i < N; ++i) {
    CHECK(bs.get_unchecked(i) == copy.get_unchecked(i));
  }
}

template <std::size_t N>
constexpr bool constexpr_smoke() {
  xer::bitset<N> bs{};
  bs.set_unchecked(0);
  if (!bs.get_unchecked(0)) {
    return false;
  }
  if constexpr (N > 1) {
    bs.set_unchecked(N - 1, true);
    if (!bs.get_unchecked(N - 1)) {
      return false;
    }
    bs.flip_unchecked(N - 1);
    if (bs.get_unchecked(N - 1)) {
      return false;
    }
  }
  bs.set_all();
  if (!bs.get_unchecked(0)) {
    return false;
  }
  bs.reset_all();
  if (bs.get_unchecked(0)) {
    return false;
  }
  bs.flip_all();
  if (!bs.get_unchecked(0)) {
    return false;
  }
  return bs.size() == N;
}

template <std::size_t N>
void check_n(const char* name) {
  std::printf("check N=%s (%zu)\n", name, N);
  check_size<N>();
  check_set_get_reset_flip<N>();
  check_all_ops<N>();
  static_assert(constexpr_smoke<N>());
  CHECK(constexpr_smoke<N>());
  CHECK(std::is_trivial_v<xer::bitset<N>>);
}

} // namespace

int main() {
  std::printf("xer_bitset_correctness compiler=");
#if XER_COMPILER(MSVC)
  std::printf("msvc\n");
#elif XER_COMPILER(GCC)
  std::printf("gcc\n");
#elif XER_COMPILER(CLANG)
  std::printf("clang\n");
#else
  std::printf("other\n");
#endif
  std::printf("arch=%s\n", XER_ARCH(64) ? "64" : "32");

  check_n<1>("1");
  check_n<7>("7");
  check_n<8>("8");
  check_n<9>("9");
  check_n<15>("15");
  check_n<16>("16");
  check_n<31>("31");
  check_n<32>("32");
  check_n<33>("33");
  check_n<63>("63");
  check_n<64>("64");
  check_n<65>("65");
  check_n<96>("96");
  check_n<127>("127");
  check_n<128>("128");
  check_n<255>("255");
  check_n<256>("256");

  if (g_failures != 0) {
    std::printf("FAILED %d\n", g_failures);
    return 1;
  }
  std::printf("ALL_PASSED\n");
  return 0;
}
