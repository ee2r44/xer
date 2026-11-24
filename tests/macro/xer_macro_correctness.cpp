// Correctness checks for include/xer/macro/xer_macro.h
// Build (C++23):
//   MSVC:  cl /std:c++latest /EHsc /Iinclude /Fe:tests\macro\xer_macro_correctness.exe tests\macro\xer_macro_correctness.cpp
//   GCC:   g++ -std=c++23 -Iinclude -o tests/macro/xer_macro_correctness_gcc tests/macro/xer_macro_correctness.cpp
//   Clang: clang++ -std=c++23 -Iinclude -o tests/macro/xer_macro_correctness_clang tests/macro/xer_macro_correctness.cpp

#include "xer/macro/xer_macro.h"

#include <cassert>
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

#define CHECK_MSG(cond, msg)                                                   \
  do {                                                                         \
    if (!(cond)) {                                                             \
      std::printf("FAIL %s:%d: %s (%s)\n", __FILE__, __LINE__, #cond, (msg));  \
      ++g_failures;                                                            \
    }                                                                          \
  } while (0)

void check_cplusplus() noexcept {
  CHECK(XER_CPLUSPLUS >= 202302L);
  std::printf("  XER_CPLUSPLUS=%ld\n", static_cast<long>(XER_CPLUSPLUS));
}

void check_arch() noexcept {
  const int hits =
      static_cast<int>(XER_ARCH(32)) + static_cast<int>(XER_ARCH(64)) +
      static_cast<int>(XER_ARCH(UNKNOWN));
  CHECK_MSG(hits == 1, "exactly one XER_ARCH_* must be true");
  CHECK(sizeof(void*) == 8 ? XER_ARCH(64) : (sizeof(void*) == 4 ? XER_ARCH(32)
                                                                : XER_ARCH(UNKNOWN)));
  std::printf("  XER_ARCH_THIS=%d sizeof(void*)=%zu\n", XER_ARCH_THIS,
              sizeof(void*));
}

void check_platform() noexcept {
  const int hits = static_cast<int>(XER_PLATFORM(WINDOWS)) +
                   static_cast<int>(XER_PLATFORM(POSIX)) +
                   static_cast<int>(XER_PLATFORM(UNKNOWN));
  CHECK_MSG(hits == 1, "exactly one XER_PLATFORM_* must be true");

#if defined(_WIN32) && !defined(__CYGWIN__) && !defined(__MSYS__)
  CHECK(XER_PLATFORM(WINDOWS));
  CHECK(!XER_PLATFORM(POSIX));
#elif defined(__linux__) || defined(__unix__) || defined(__APPLE__)
  CHECK(XER_PLATFORM(POSIX));
  CHECK(!XER_PLATFORM(WINDOWS));
#endif
  std::printf("  XER_PLATFORM_THIS=%d\n", XER_PLATFORM_THIS);
}

void check_compiler() noexcept {
  const int hits = static_cast<int>(XER_COMPILER(MSVC)) +
                   static_cast<int>(XER_COMPILER(CLANG)) +
                   static_cast<int>(XER_COMPILER(GCC)) +
                   static_cast<int>(XER_COMPILER(UNKNOWN));
  CHECK_MSG(hits == 1, "exactly one XER_COMPILER_* must be true");

  // clang also defines __GNUC__; detection must prefer clang.
#if defined(__clang__)
  CHECK(XER_COMPILER(CLANG));
  CHECK(!XER_COMPILER(GCC));
  CHECK(!XER_COMPILER(MSVC));
#elif defined(_MSC_VER)
  CHECK(XER_COMPILER(MSVC));
  CHECK(!XER_COMPILER(CLANG));
  CHECK(!XER_COMPILER(GCC));
#elif defined(__GNUC__)
  CHECK(XER_COMPILER(GCC));
  CHECK(!XER_COMPILER(CLANG));
  CHECK(!XER_COMPILER(MSVC));
#endif
  std::printf("  XER_COMPILER_THIS=%d\n", XER_COMPILER_THIS);
}

void check_cpu() noexcept {
  const int hits = static_cast<int>(XER_CPU(X86_64)) +
                   static_cast<int>(XER_CPU(X86_32)) +
                   static_cast<int>(XER_CPU(ARM64)) +
                   static_cast<int>(XER_CPU(ARM32)) +
                   static_cast<int>(XER_CPU(RISCV64)) +
                   static_cast<int>(XER_CPU(RISCV32)) +
                   static_cast<int>(XER_CPU(UNKNOWN));
  CHECK_MSG(hits == 1, "exactly one XER_CPU_* must be true");

#if defined(_M_X64) || defined(_M_AMD64) || defined(__x86_64__) ||             \
    defined(__amd64__)
  CHECK(XER_CPU(X86_64));
#elif defined(_M_IX86) || defined(__i386__) || defined(__i686__)
  CHECK(XER_CPU(X86_32));
#elif defined(_M_ARM64) || defined(__aarch64__) || defined(__arm64__)
  CHECK(XER_CPU(ARM64));
#elif defined(_M_ARM) || defined(__arm__)
  CHECK(XER_CPU(ARM32));
#elif defined(__riscv)
#  if __riscv_xlen == 64
  CHECK(XER_CPU(RISCV64));
#  else
  CHECK(XER_CPU(RISCV32));
#  endif
#endif

  // Pointer width and CPU family should agree on common hosts.
  if (XER_CPU(X86_64) || XER_CPU(ARM64) || XER_CPU(RISCV64)) {
    CHECK(XER_ARCH(64));
  }
  if (XER_CPU(X86_32) || XER_CPU(ARM32) || XER_CPU(RISCV32)) {
    CHECK(XER_ARCH(32));
  }
  std::printf("  XER_CPU_THIS=%d\n", XER_CPU_THIS);
}

void check_byte_order() noexcept {
  const int hits = static_cast<int>(XER_BYTE_ORDER(LITTLE)) +
                   static_cast<int>(XER_BYTE_ORDER(BIG));
  CHECK_MSG(hits == 1, "exactly one XER_BYTE_ORDER_* must be true");

  constexpr std::uint16_t probe = 0x0102;
  const auto* bytes = reinterpret_cast<const unsigned char*>(&probe);
  const bool host_little = bytes[0] == 0x02 && bytes[1] == 0x01;
  const bool host_big = bytes[0] == 0x01 && bytes[1] == 0x02;
  CHECK(host_little || host_big);
  if (host_little) {
    CHECK(XER_BYTE_ORDER(LITTLE));
    CHECK(!XER_BYTE_ORDER(BIG));
  }
  if (host_big) {
    CHECK(XER_BYTE_ORDER(BIG));
    CHECK(!XER_BYTE_ORDER(LITTLE));
  }
  std::printf("  XER_BYTE_ORDER_THIS=%d host_little=%d\n", XER_BYTE_ORDER_THIS,
              static_cast<int>(host_little));
}

void check_feature_flags() noexcept {
  // Boolean feature macros must be safe in C++ expression context.
  CHECK(XER_HAS_BUILTIN_ADDRESSOF == true ||
        XER_HAS_BUILTIN_ADDRESSOF == false);
  CHECK(XER_HAS_BUILTIN_MAKE_INDEX_SEQUENCE == true ||
        XER_HAS_BUILTIN_MAKE_INDEX_SEQUENCE == false);
  CHECK(XER_HAS_BUILTIN_BIT_CAST == true || XER_HAS_BUILTIN_BIT_CAST == false);
  CHECK(XER_HAS_MSVC_WIDE_MUL == true || XER_HAS_MSVC_WIDE_MUL == false);
  CHECK(XER_X86_INTRIN_AVAILABLE == true ||
        XER_X86_INTRIN_AVAILABLE == false);
  CHECK(XER_HAS_BUILTIN_INT128 == true || XER_HAS_BUILTIN_INT128 == false);
  CHECK(XER_SSE2_AVAILABLE == true || XER_SSE2_AVAILABLE == false);
  CHECK(XER_BMI1_AVAILABLE == true || XER_BMI1_AVAILABLE == false);
#if defined(__BMI__) ||                                                       \
    (defined(_MSC_VER) && _MSC_VER >= 1900 && defined(__AVX2__) &&             \
     !defined(__clang__))
  CHECK(XER_BMI1_AVAILABLE);
#else
  // Default x64 MSVC/GCC/Clang builds without -mbmi /arch:AVX2 report false.
  CHECK(!XER_BMI1_AVAILABLE);
#endif

#if XER_HAS_BUILTIN_BIT_CAST
  constexpr bool has_bit_cast = true;
#else
  constexpr bool has_bit_cast = false;
#endif
  CHECK(static_cast<bool>(XER_HAS_BUILTIN_BIT_CAST) == has_bit_cast);

#if XER_COMPILER(MSVC)
  CHECK(XER_HAS_BUILTIN_ADDRESSOF);
  CHECK(XER_HAS_BUILTIN_MAKE_INDEX_SEQUENCE);
  CHECK(has_bit_cast); // recent MSVC; project uses stdcpplatest
  CHECK(!XER_HAS_BUILTIN_INT128);
  if (XER_ARCH(64) && (XER_CPU(X86_64) || XER_CPU(ARM64))) {
    CHECK(XER_HAS_MSVC_WIDE_MUL);
  } else {
    CHECK(!XER_HAS_MSVC_WIDE_MUL);
  }
#else
  CHECK(!XER_HAS_MSVC_WIDE_MUL);
  CHECK(has_bit_cast); // __builtin_bit_cast expected on our GCC/Clang hosts
#endif

  if (XER_CPU(X86_64) || XER_CPU(X86_32)) {
    CHECK(XER_X86_INTRIN_AVAILABLE);
  } else {
    CHECK(!XER_X86_INTRIN_AVAILABLE);
  }

  if (XER_CPU(X86_64)) {
    CHECK(XER_SSE2_AVAILABLE);
  }

#if defined(__SIZEOF_INT128__)
  CHECK(XER_HAS_BUILTIN_INT128);
#else
  CHECK(!XER_HAS_BUILTIN_INT128);
#endif

  std::printf(
      "  ADDRESSOF=%d INDEX_SEQ=%d BIT_CAST=%d MSVC_WIDE_MUL=%d "
      "X86_INTRIN=%d INT128=%d SSE2=%d BMI1=%d\n",
      static_cast<int>(XER_HAS_BUILTIN_ADDRESSOF),
      static_cast<int>(XER_HAS_BUILTIN_MAKE_INDEX_SEQUENCE),
      static_cast<int>(has_bit_cast),
      static_cast<int>(XER_HAS_MSVC_WIDE_MUL),
      static_cast<int>(XER_X86_INTRIN_AVAILABLE),
      static_cast<int>(XER_HAS_BUILTIN_INT128),
      static_cast<int>(XER_SSE2_AVAILABLE),
      static_cast<int>(XER_BMI1_AVAILABLE));
}

// Compile-time usability of attribute / qualifier macros.
XER_NOINLINE int noinline_add(int a, int b) noexcept { return a + b; }

XER_FORCE_INLINE int force_inline_mul(int a, int b) noexcept { return a * b; }

void restrict_sink(int* XER_RESTRICT a, int* XER_RESTRICT b) noexcept {
  *a += 1;
  *b += 2;
}

void check_codegen_macros() noexcept {
  CHECK(noinline_add(1, 2) == 3);
  CHECK(force_inline_mul(3, 4) == 12);
  int x = 10;
  int y = 20;
  restrict_sink(&x, &y);
  CHECK(x == 11);
  CHECK(y == 22);
}

void check_likely_unlikely() noexcept {
#if !defined(XER_LIKELY)
  CHECK_MSG(false, "XER_LIKELY must be defined under __cplusplus");
#endif
#if !defined(XER_UNLIKELY)
  CHECK_MSG(false, "XER_UNLIKELY must be defined under __cplusplus");
#endif

  int v = 0;
  if (true) XER_LIKELY { v = 1; }
  if (false) XER_UNLIKELY { v = 2; }
  CHECK(v == 1);

  int w = 0;
  if (false) XER_LIKELY { w = 1; }
  if (true) XER_UNLIKELY { w = 2; }
  CHECK(w == 2);
}

void check_has_cpp_attribute_and_builtin_wrappers() noexcept {
  // Wrappers must be usable in #if.
#if XER_HAS_CPP_ATTRIBUTE(nodiscard)
  constexpr bool has_nodiscard = true;
#else
  constexpr bool has_nodiscard = false;
#endif
  // C++17 nodiscard is widely available on our three toolchains under C++23.
  CHECK(has_nodiscard);

  // Unknown attribute / builtin should be safe (0 / false), not a hard error.
#if XER_HAS_CPP_ATTRIBUTE(xer_not_a_real_attribute_xyz)
  CHECK_MSG(false, "unknown attribute must not report present");
#endif
#if XER_HAS_BUILTIN(__builtin_xer_not_a_real_builtin_xyz)
  CHECK_MSG(false, "unknown builtin must not report present");
#endif

  std::printf("  XER_HAS_CPP_ATTRIBUTE(nodiscard)=%d\n",
              static_cast<int>(has_nodiscard));
}

} // namespace

int main() {
  std::printf("xer_macro correctness\n");

  check_cplusplus();
  check_arch();
  check_platform();
  check_compiler();
  check_cpu();
  check_byte_order();
  check_feature_flags();
  check_codegen_macros();
  check_likely_unlikely();
  check_has_cpp_attribute_and_builtin_wrappers();

  if (g_failures != 0) {
    std::printf("RESULT: FAILED (%d checks)\n", g_failures);
    return 1;
  }
  std::printf("RESULT: PASSED\n");
  return 0;
}
