// Correctness for xer::details_::aligned_alloc_fallback /
// aligned_free_fallback.
//
// Those symbols are behind (!WINDOWS && !POSIX). This TU mirrors that path by
// forcing XER_PLATFORM_THIS to unknown before including the real header, so
// the three host toolchains exercise the same implementation.
//
// Build (C++23):
//   MSVC:  cl /std:c++latest /EHsc /Iinclude /Fe:tests\memory\xer_aligned_alloc_fallback_correctness_msvc.exe tests\memory\xer_aligned_alloc_fallback_correctness.cpp
//   GCC:   g++ -std=c++23 -Iinclude -o tests/memory/xer_aligned_alloc_fallback_correctness_gcc tests/memory/xer_aligned_alloc_fallback_correctness.cpp
//   Clang: clang++ -std=c++23 -Iinclude -o tests/memory/xer_aligned_alloc_fallback_correctness_clang tests/memory/xer_aligned_alloc_fallback_correctness.cpp

#define XER_PLATFORM_THIS 0 // XER_PLATFORM_UNKNOWN; must precede xer headers
#include "xer/memory/xer_raw_alloc.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

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

template <std::size_t Align>
void check_one(std::size_t size) {
  void* p = xer::details_::aligned_alloc_fallback<Align>(size);
  CHECK(p != nullptr);
  if (p == nullptr) {
    return;
  }
  CHECK(is_aligned(p, Align));
  // Header slot immediately before the user pointer must be writable and
  // hold a recoverable offset into the malloc block.
  auto* header = reinterpret_cast<std::uintptr_t*>(
      reinterpret_cast<std::uintptr_t>(p) - sizeof(std::uintptr_t));
  CHECK(is_aligned(header, alignof(std::uintptr_t)));
  CHECK(*header >= xer::details_::raw_alloc_basic_alignment);
  CHECK(*header <= Align);

  auto* bytes = static_cast<unsigned char*>(p);
  if (size == 1) {
    bytes[0] = 0xA5;
    CHECK(bytes[0] == 0xA5);
  } else if (size > 1) {
    bytes[0] = 0xA5;
    bytes[size - 1] = 0x5A;
    CHECK(bytes[0] == 0xA5);
    CHECK(bytes[size - 1] == 0x5A);
  }
  if (size > 0) {
    std::memset(bytes, 0x3C, size);
    CHECK(bytes[0] == 0x3C);
    CHECK(bytes[size - 1] == 0x3C);
  }
  xer::details_::aligned_free_fallback(p);
}

void check_basic() {
  CHECK(xer::details_::raw_alloc_basic_alignment > 0);
  CHECK(xer::has_single_bit(xer::details_::raw_alloc_basic_alignment));
  std::printf("  basic_alignment=%zu\n",
              xer::details_::raw_alloc_basic_alignment);
}

void check_sizes_and_alignments() {
  // Alignments must exceed raw_alloc_basic_alignment.
  constexpr std::size_t aligns[] = {32, 64, 128, 256, 512, 1024, 4096};
  constexpr std::size_t sizes[] = {1, 2, 3, 7, 8, 15, 16, 31, 32, 33, 63, 64,
                                   65, 127, 128, 255, 256, 1023, 1024, 4095,
                                   4096, 65536};

  for (std::size_t align : aligns) {
    if (align <= xer::details_::raw_alloc_basic_alignment) {
      continue;
    }
    for (std::size_t size : sizes) {
      switch (align) {
        case 32:
          check_one<32>(size);
          break;
        case 64:
          check_one<64>(size);
          break;
        case 128:
          check_one<128>(size);
          break;
        case 256:
          check_one<256>(size);
          break;
        case 512:
          check_one<512>(size);
          break;
        case 1024:
          check_one<1024>(size);
          break;
        case 4096:
          check_one<4096>(size);
          break;
        default:
          CHECK(false);
          break;
      }
    }
  }
}

void check_many_live() {
  constexpr std::size_t n = 256;
  std::vector<void*> ptrs;
  ptrs.reserve(n);
  for (std::size_t i = 0; i < n; ++i) {
    void* p = xer::details_::aligned_alloc_fallback<128>(17 + (i % 64));
    CHECK(p != nullptr);
    if (p) {
      CHECK(is_aligned(p, 128));
      *static_cast<unsigned char*>(p) = static_cast<unsigned char>(i);
      ptrs.push_back(p);
    }
  }
  for (std::size_t i = 0; i < ptrs.size(); ++i) {
    CHECK(*static_cast<unsigned char*>(ptrs[i]) ==
          static_cast<unsigned char>(i));
  }
  // Free in mixed order.
  for (std::size_t i = 0; i < ptrs.size(); i += 2) {
    xer::details_::aligned_free_fallback(ptrs[i]);
    ptrs[i] = nullptr;
  }
  for (std::size_t i = 1; i < ptrs.size(); i += 2) {
    xer::details_::aligned_free_fallback(ptrs[i]);
  }
}

void check_nullptr_and_overflow() {
  xer::details_::aligned_free_fallback(nullptr);

  // size + align must not wrap.
  constexpr std::size_t align = 64;
  void* bad0 =
      xer::details_::aligned_alloc_fallback<align>(~std::size_t{0});
  CHECK(bad0 == nullptr);

  // size + align == SIZE_MAX + 1 must be rejected.
  void* bad1 = xer::details_::aligned_alloc_fallback<align>(
      (~std::size_t{0}) - (align - 1));
  CHECK(bad1 == nullptr);

  // size + align == SIZE_MAX is representable; implementation may still return
  // nullptr from malloc. Do not request that here (would ask for ~SIZE_MAX).
  void* modest = xer::details_::aligned_alloc_fallback<align>(1024 * 1024);
  if (modest) {
    CHECK(is_aligned(modest, align));
    xer::details_::aligned_free_fallback(modest);
  }
}

void check_requires_reject_small_align() {
  // align must be > basic alignment. 8 and 16 are typically <= basic.
  // We cannot call a constrained template that fails requires; verify the
  // compile-time relation instead.
  constexpr std::size_t basic = xer::details_::raw_alloc_basic_alignment;
  static_assert(basic >= 8);
  CHECK(basic >= sizeof(std::uintptr_t));
}

} // namespace

int main() {
  std::printf("xer_aligned_alloc_fallback correctness\n");
  std::printf("  forced XER_PLATFORM_THIS=%d (unknown)\n", XER_PLATFORM_THIS);

  check_basic();
  check_sizes_and_alignments();
  check_many_live();
  check_nullptr_and_overflow();
  check_requires_reject_small_align();

  if (g_failures != 0) {
    std::printf("RESULT: FAILED (%d checks)\n", g_failures);
    return 1;
  }
  std::printf("RESULT: PASSED\n");
  return 0;
}
