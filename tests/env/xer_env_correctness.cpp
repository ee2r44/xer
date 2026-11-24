// Correctness for include/xer/env/xer_env.h (+ xer_env.cpp)
// Build (C++23):
//   MSVC:  call D:\develop\c++\vs\VC\Auxiliary\Build\vcvars64.bat
//          cl /std:c++latest /EHsc /Iinclude /Fe:tests\env\xer_env_correctness_msvc.exe tests\env\xer_env_correctness.cpp include\xer\env\xer_env.cpp
//   GCC:   g++ -std=c++23 -Iinclude -o tests/env/xer_env_correctness_gcc tests/env/xer_env_correctness.cpp include/xer/env/xer_env.cpp
//   Clang: clang++ -std=c++23 -Iinclude -o tests/env/xer_env_correctness_clang tests/env/xer_env_correctness.cpp include/xer/env/xer_env.cpp
//
// Contract: do not call is_* from other dynamic initializers (SIOF).

#include "xer/env/xer_env.h"
#include "xer/macro/xer_macro.h"

#include <cstddef>
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

void check_noexcept_surface() {
#if XER_CPU(X86_64) || XER_CPU(X86_32)
  static_assert(noexcept(xer::is_ssse3_supported()));
  static_assert(noexcept(xer::is_sse4_2_supported()));
  static_assert(noexcept(xer::is_popcnt_supported()));
  static_assert(noexcept(xer::is_bmi1_supported()));
  static_assert(noexcept(xer::is_bmi2_supported()));
  static_assert(noexcept(xer::is_lzcnt_supported()));
  static_assert(noexcept(xer::is_tzcnt_supported()));
  static_assert(noexcept(xer::is_avx_supported()));
  static_assert(noexcept(xer::is_avx2_supported()));
#endif
}

void check_stable_reads() {
#if XER_CPU(X86_64) || XER_CPU(X86_32)
  const bool ssse3 = xer::is_ssse3_supported();
  const bool sse42 = xer::is_sse4_2_supported();
  const bool popcnt = xer::is_popcnt_supported();
  const bool bmi1 = xer::is_bmi1_supported();
  const bool bmi2 = xer::is_bmi2_supported();
  const bool lzcnt = xer::is_lzcnt_supported();
  const bool tzcnt = xer::is_tzcnt_supported();
  const bool avx = xer::is_avx_supported();
  const bool avx2 = xer::is_avx2_supported();

  // Repeated calls must be stable (single global_env).
  for (int i = 0; i < 3; ++i) {
    CHECK(xer::is_ssse3_supported() == ssse3);
    CHECK(xer::is_sse4_2_supported() == sse42);
    CHECK(xer::is_popcnt_supported() == popcnt);
    CHECK(xer::is_bmi1_supported() == bmi1);
    CHECK(xer::is_bmi2_supported() == bmi2);
    CHECK(xer::is_lzcnt_supported() == lzcnt);
    CHECK(xer::is_tzcnt_supported() == tzcnt);
    CHECK(xer::is_avx_supported() == avx);
    CHECK(xer::is_avx2_supported() == avx2);
  }
#else
  // Non-x86: feature queries are not declared; nothing to probe.
  CHECK(true);
#endif
}

void check_implication_rules() {
#if XER_CPU(X86_64) || XER_CPU(X86_32)
  // TZCNT is provided by BMI1.
  CHECK(xer::is_tzcnt_supported() == xer::is_bmi1_supported());

  // AVX2 requires OS-enabled AVX.
  if (xer::is_avx2_supported()) {
    CHECK(xer::is_avx_supported());
  }
#endif
}

void check_table_bounds() {
  using xer::details_::env_::COUNT_;
  using xer::details_::env_::pad_;
  CHECK(static_cast<std::size_t>(pad_) == 0);
  CHECK(static_cast<std::size_t>(COUNT_) >= 1);

#if XER_CPU(X86_64) || XER_CPU(X86_32)
  using xer::details_::env_::avx2_support_;
  CHECK(static_cast<std::size_t>(avx2_support_) <
        static_cast<std::size_t>(COUNT_));
  CHECK(static_cast<std::size_t>(COUNT_) == 9); // pad_ + 8 features
#else
  CHECK(static_cast<std::size_t>(COUNT_) == 1); // pad_ only
#endif

  // global_env is readable for every slot.
  for (std::size_t i = 0; i < static_cast<std::size_t>(COUNT_); ++i) {
    const bool v = xer::details_::env_::get_env(
        static_cast<xer::details_::env_::support_enum>(i));
    CHECK(v == true || v == false);
  }
}

#if XER_CPU(X86_64) && (XER_COMPILER(GCC) || XER_COMPILER(CLANG))
void check_vs_builtin_cpu_supports() {
  __builtin_cpu_init();
  CHECK(xer::is_ssse3_supported() ==
        static_cast<bool>(__builtin_cpu_supports("ssse3")));
  CHECK(xer::is_sse4_2_supported() ==
        static_cast<bool>(__builtin_cpu_supports("sse4.2")));
  CHECK(xer::is_popcnt_supported() ==
        static_cast<bool>(__builtin_cpu_supports("popcnt")));
  CHECK(xer::is_avx_supported() ==
        static_cast<bool>(__builtin_cpu_supports("avx")));
  CHECK(xer::is_avx2_supported() ==
        static_cast<bool>(__builtin_cpu_supports("avx2")));
  CHECK(xer::is_bmi1_supported() ==
        static_cast<bool>(__builtin_cpu_supports("bmi")));
  CHECK(xer::is_bmi2_supported() ==
        static_cast<bool>(__builtin_cpu_supports("bmi2")));
  CHECK(xer::is_lzcnt_supported() ==
        static_cast<bool>(__builtin_cpu_supports("lzcnt")));
}
#endif

void print_features() {
#if XER_CPU(X86_64) || XER_CPU(X86_32)
  std::printf(
      "features ssse3=%d sse4.2=%d popcnt=%d bmi1=%d bmi2=%d "
      "lzcnt=%d tzcnt=%d avx=%d avx2=%d\n",
      (int)xer::is_ssse3_supported(), (int)xer::is_sse4_2_supported(),
      (int)xer::is_popcnt_supported(), (int)xer::is_bmi1_supported(),
      (int)xer::is_bmi2_supported(), (int)xer::is_lzcnt_supported(),
      (int)xer::is_tzcnt_supported(), (int)xer::is_avx_supported(),
      (int)xer::is_avx2_supported());
#else
  std::printf("features (non-x86: no CPUID probes)\n");
#endif
}

} // namespace

int main() {
  std::printf("xer_env_correctness compiler=");
#if XER_COMPILER(MSVC)
  std::printf("msvc\n");
#elif XER_COMPILER(GCC)
  std::printf("gcc\n");
#elif XER_COMPILER(CLANG)
  std::printf("clang\n");
#else
  std::printf("other\n");
#endif
  std::printf("cpu=%s\n",
#if XER_CPU(X86_64)
              "x86_64"
#elif XER_CPU(X86_32)
              "x86_32"
#elif XER_CPU(ARM64)
              "arm64"
#else
              "other"
#endif
  );

  check_noexcept_surface();
  check_stable_reads();
  check_implication_rules();
  check_table_bounds();
#if XER_CPU(X86_64) && (XER_COMPILER(GCC) || XER_COMPILER(CLANG))
  check_vs_builtin_cpu_supports();
#endif
  print_features();

  if (g_failures != 0) {
    std::printf("RESULT: FAILED (%d checks)\n", g_failures);
    return 1;
  }
  std::printf("RESULT: PASSED\n");
  return 0;
}
