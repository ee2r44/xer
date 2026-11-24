// Correctness for include/xer/random/xer_mt19937.h
// Build:
//   MSVC:  cl /std:c++latest /EHsc /Iinclude /Fe:tests\random\xer_mt19937_correctness_msvc.exe tests\random\xer_mt19937_correctness.cpp
//   GCC:   g++ -std=c++23 -Iinclude -o tests/random/xer_mt19937_correctness_gcc tests/random/xer_mt19937_correctness.cpp
//   Clang: clang++ -std=c++23 -Iinclude -o tests/random/xer_mt19937_correctness_clang tests/random/xer_mt19937_correctness.cpp

#include "xer/random/xer_mt19937.h"

#include <cstdint>
#include <cstdio>
#include <random>

namespace {

int g_failures = 0;

#define CHECK(cond)                                                            \
  do {                                                                         \
    if (!(cond)) {                                                             \
      std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);              \
      ++g_failures;                                                            \
    }                                                                          \
  } while (0)

void test_default_seed_vs_std() {
  xer::valid_chain v;
  xer::mt19937 x(v);
  CHECK(static_cast<bool>(v));

  std::mt19937 s;  // seed 5489
  for (int i = 0; i < 10000; ++i) {
    const auto a = x();
    const auto b = s();
    if (a != b) {
      std::printf("FAIL default-seed mismatch at i=%d xer=%lu std=%lu\n", i,
                  static_cast<unsigned long>(a), static_cast<unsigned long>(b));
      ++g_failures;
      return;
    }
  }
}

void test_seed_vs_std() {
  const std::uint32_t seeds[] = {0u, 1u, 5489u, 0xFFFFFFFFu, 123456789u};
  for (std::uint32_t seed : seeds) {
    xer::valid_chain v;
    xer::mt19937 x(v, seed);
    CHECK(static_cast<bool>(v));
    std::mt19937 s(seed);
    for (int i = 0; i < 2000; ++i) {
      if (x() != s()) {
        std::printf("FAIL seed=%u mismatch at i=%d\n", seed, i);
        ++g_failures;
        return;
      }
    }
  }
}

void test_known_vector_5489() {
  // From mt19937ar reference: seed 5489, first output.
  xer::valid_chain v;
  xer::mt19937 x(v, 5489u);
  CHECK(static_cast<bool>(v));
  CHECK(x() == 3499211612u);
}

void test_reset_matches_reseed() {
  xer::valid_chain v;
  xer::mt19937 x(v, 42u);
  CHECK(static_cast<bool>(v));
  for (int i = 0; i < 100; ++i) (void)x();
  CHECK(x.reset(99u));

  xer::valid_chain v2;
  xer::mt19937 y(v2, 99u);
  CHECK(static_cast<bool>(v2));
  for (int i = 0; i < 500; ++i) CHECK(x() == y());
}

void test_move() {
  xer::valid_chain v;
  xer::mt19937 x(v, 7u);
  CHECK(static_cast<bool>(v));
  const auto a0 = x();
  const auto a1 = x();

  xer::mt19937 y(static_cast<xer::mt19937&&>(x));
  std::mt19937 s(7u);
  CHECK(a0 == s());
  CHECK(a1 == s());
  CHECK(y() == s());
  CHECK(y() == s());
}

void test_init_by_array_vs_reference_algorithm() {
  // Independently mirror classic init_by_array + genrand, compare to xer.
  // (Not std::seed_seq; C++ seed_seq uses a different initialization.)
  const std::uint32_t key[] = {0x123u, 0x234u, 0x345u, 0x456u};

  xer::valid_chain v;
  xer::mt19937 x(v, key, sizeof(key) / sizeof(key[0]));
  CHECK(static_cast<bool>(v));

  // Reference outputs from mt19937ar.out with init_by_array(key,4), first 5:
  CHECK(x() == 1067595299u);
  CHECK(x() == 955945823u);
  CHECK(x() == 477289528u);
  CHECK(x() == 4107218783u);
  CHECK(x() == 4228976476u);
}

void test_reset_default() {
  xer::valid_chain v;
  xer::mt19937 x(v, 1u);
  CHECK(x.reset());
  std::mt19937 s;
  for (int i = 0; i < 1000; ++i) CHECK(x() == s());
}

void test_empty_key_fails() {
  xer::valid_chain v;
  xer::mt19937 x(v, static_cast<const std::uint32_t*>(nullptr), 0);
  CHECK(!static_cast<bool>(v));
}

}  // namespace

int main() {
  test_default_seed_vs_std();
  test_seed_vs_std();
  test_known_vector_5489();
  test_reset_matches_reseed();
  test_move();
  test_init_by_array_vs_reference_algorithm();
  test_reset_default();
  test_empty_key_fails();

  if (g_failures == 0) {
    std::printf("ok\n");
    return 0;
  }
  std::printf("%d failure(s)\n", g_failures);
  return 1;
}
