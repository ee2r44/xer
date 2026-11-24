// Correctness checks for include/xer/memory/xer_raw_alloc.h
// Build (C++23):
//   MSVC:  call D:\develop\c++\vs\VC\Auxiliary\Build\vcvars64.bat
//          cl /std:c++latest /EHsc /Iinclude /Fe:tests\memory\xer_raw_alloc_correctness_msvc.exe tests\memory\xer_raw_alloc_correctness.cpp
//   GCC:   g++ -std=c++23 -Iinclude -o tests/memory/xer_raw_alloc_correctness_gcc tests/memory/xer_raw_alloc_correctness.cpp
//   Clang: clang++ -std=c++23 -Iinclude -o tests/memory/xer_raw_alloc_correctness_clang tests/memory/xer_raw_alloc_correctness.cpp

#include "xer/memory/xer_raw_alloc.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
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

bool is_aligned(const void* p, std::size_t alignment) noexcept {
  return (reinterpret_cast<std::uintptr_t>(p) % alignment) == 0;
}

struct alignas(64) OverAlign64 {
  char bytes[64];
};

struct alignas(128) OverAlign128 {
  int v;
};

static_assert(alignof(OverAlign64) > xer::details_::raw_alloc_basic_alignment ||
                  alignof(OverAlign64) <= xer::details_::raw_alloc_basic_alignment,
              "alignment relation is well-formed");

void check_basic_alignment_constant() noexcept {
  CHECK(xer::details_::raw_alloc_basic_alignment > 0);
  CHECK((xer::details_::raw_alloc_basic_alignment &
         (xer::details_::raw_alloc_basic_alignment - 1)) == 0);
  std::printf("  raw_alloc_basic_alignment=%zu alignof(max_align_t)=%zu\n",
              xer::details_::raw_alloc_basic_alignment,
              alignof(std::max_align_t));
}

void check_single_alloc_free() noexcept {
  int* p = xer::raw_alloc<int>();
  CHECK(p != nullptr);
  if (p) {
    CHECK(is_aligned(p, alignof(int)));
    *p = 42;
    CHECK(*p == 42);
    xer::raw_free(p);
  }

  xer::raw_free<int>(nullptr); // documented: nullptr is valid
}

void check_count_alloc_free() noexcept {
  constexpr std::size_t n = 17;
  int* p = xer::raw_alloc<int>(n);
  CHECK(p != nullptr);
  if (p) {
    CHECK(is_aligned(p, alignof(int)));
    for (std::size_t i = 0; i < n; ++i) {
      p[i] = static_cast<int>(i * 3);
    }
    for (std::size_t i = 0; i < n; ++i) {
      CHECK(p[i] == static_cast<int>(i * 3));
    }
    xer::raw_free(p);
  }
}

void check_zero_and_overflow() noexcept {
  CHECK(xer::raw_alloc<int>(0) == nullptr);
  CHECK(xer::raw_alloc<char>(0) == nullptr);

  constexpr std::size_t lim = (~static_cast<std::size_t>(0)) / sizeof(int);
  CHECK(xer::raw_alloc<int>(lim + 1) == nullptr);
  // size==1 path: only rejects count==0; max count is allowed to attempt alloc.
  // Do not request SIZE_MAX bytes here (may thrash / OOM). Just confirm API
  // accepts the type path by allocating a tiny char buffer.
  char* c = xer::raw_alloc<char>(8);
  CHECK(c != nullptr);
  if (c) {
    std::memset(c, 0xA5, 8);
    CHECK(static_cast<unsigned char>(c[0]) == 0xA5);
    xer::raw_free(c);
  }
}

void check_overaligned() noexcept {
  OverAlign64* a = xer::raw_alloc<OverAlign64>();
  CHECK(a != nullptr);
  if (a) {
    CHECK(is_aligned(a, alignof(OverAlign64)));
    a->bytes[0] = 1;
    a->bytes[63] = 2;
    CHECK(a->bytes[0] == 1);
    CHECK(a->bytes[63] == 2);
    xer::raw_free(a);
  }

  OverAlign128* b = xer::raw_alloc<OverAlign128>(4);
  CHECK(b != nullptr);
  if (b) {
    CHECK(is_aligned(b, alignof(OverAlign128)));
    for (int i = 0; i < 4; ++i) {
      b[i].v = i + 10;
    }
    for (int i = 0; i < 4; ++i) {
      CHECK(b[i].v == i + 10);
    }
    xer::raw_free(b);
  }

  CHECK(xer::raw_alloc<OverAlign64>(0) == nullptr);
  xer::raw_free<OverAlign64>(nullptr);
}

void check_concept_constraints() noexcept {
  static_assert(xer::details_::raw_allocable<int>);
  static_assert(xer::details_::raw_allocable<OverAlign64>);
  static_assert(!xer::details_::raw_allocable<void>);
  static_assert(!xer::details_::raw_allocable<int&>);
  static_assert(!xer::details_::raw_allocable<int()>);
  static_assert(!xer::details_::raw_allocable<int[4]>);
  static_assert(!xer::details_::raw_allocable<int[]>);
}

void check_nothrow_failure_contract() noexcept {
  // Failure is nullptr, not an exception.
  static_assert(noexcept(xer::raw_alloc<int>()));
  static_assert(noexcept(xer::raw_alloc<int>(1)));
  static_assert(noexcept(xer::raw_free<int>(nullptr)));
}

void check_align16_path() noexcept {
  // On MSVC, max_align_t is often 8, so alignas(16) takes the over-aligned path.
  struct alignas(16) A16 {
    char b[16];
  };
  A16* p = xer::raw_alloc<A16>();
  CHECK(p != nullptr);
  if (p) {
    CHECK(is_aligned(p, 16));
    p->b[0] = 1;
    p->b[15] = 2;
    CHECK(p->b[0] == 1);
    CHECK(p->b[15] == 2);
    xer::raw_free(p);
  }
  A16* q = xer::raw_alloc<A16>(3);
  CHECK(q != nullptr);
  if (q) {
    CHECK(is_aligned(q, 16));
    xer::raw_free(q);
  }
}

} // namespace

int main() {
  std::printf("xer_raw_alloc correctness\n");
  std::printf("  basic_alignment=%zu\n",
              xer::details_::raw_alloc_basic_alignment);

  check_basic_alignment_constant();
  check_single_alloc_free();
  check_count_alloc_free();
  check_zero_and_overflow();
  check_overaligned();
  check_align16_path();
  check_concept_constraints();
  check_nothrow_failure_contract();

  if (g_failures != 0) {
    std::printf("RESULT: FAILED (%d checks)\n", g_failures);
    return 1;
  }
  std::printf("RESULT: PASSED\n");
  return 0;
}
