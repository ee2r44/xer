// Correctness for include/xer/error/xer_valid_chain.h
// Build:
//   MSVC:  cl /std:c++latest /EHsc /Iinclude /Fe:tests\error\xer_valid_chain_correctness_msvc.exe tests\error\xer_valid_chain_correctness.cpp
//   GCC:   g++ -std=c++23 -Iinclude -o tests/error/xer_valid_chain_correctness_gcc tests/error/xer_valid_chain_correctness.cpp
//   Clang: clang++ -std=c++23 -Iinclude -o tests/error/xer_valid_chain_correctness_clang tests/error/xer_valid_chain_correctness.cpp

#include "xer/error/xer_valid_chain.h"
#include "xer/macro/xer_macro.h"

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

bool ret_true() noexcept { return true; }
bool ret_false() noexcept { return false; }

bool bump(int& n, bool v) noexcept {
  ++n;
  return v;
}

void check_basic() {
  xer::valid_chain c;
  CHECK(static_cast<bool>(c));
  c = true;
  CHECK(static_cast<bool>(c));
  c = false;
  CHECK(!static_cast<bool>(c));
  c = true; // AND with false stays false
  CHECK(!static_cast<bool>(c));
}

void check_try_call_chain() {
  xer::valid_chain c;
  c.try_call(ret_true).try_call(ret_true);
  CHECK(static_cast<bool>(c));

  c.try_call(ret_false).try_call(ret_true);
  CHECK(!static_cast<bool>(c));
}

void check_fuse_skips_later_calls() {
  int n = 0;
  xer::valid_chain c;
  c.try_call(bump, n, true);
  CHECK(n == 1);
  CHECK(static_cast<bool>(c));

  c.try_call(bump, n, false);
  CHECK(n == 2);
  CHECK(!static_cast<bool>(c));

  c.try_call(bump, n, true);
  CHECK(n == 2); // fused: not called
  CHECK(!static_cast<bool>(c));

  c.try_call(bump, n, false);
  CHECK(n == 2);
}

void check_assign_bool_rhs_eager() {
  // operator=(bool) receives an already-computed value; RHS is not deferred.
  int n = 0;
  xer::valid_chain c;
  c = bump(n, false);
  CHECK(n == 1);
  CHECK(!static_cast<bool>(c));
  c = bump(n, true); // bump still runs; chain stays false
  CHECK(n == 2);
  CHECK(!static_cast<bool>(c));
}

void check_copy_can_revive() {
  xer::valid_chain ok;
  xer::valid_chain dead;
  dead = false;
  CHECK(!static_cast<bool>(dead));
  dead = ok; // copy-assign replaces judge
  CHECK(static_cast<bool>(dead));
}

void check_reset() {
  int n = 0;
  xer::valid_chain c;
  c.try_call(bump, n, false);
  CHECK(n == 1);
  CHECK(!static_cast<bool>(c));
  c.reset();
  CHECK(static_cast<bool>(c));
  c.try_call(bump, n, true);
  CHECK(n == 2);
  CHECK(static_cast<bool>(c));
}

constexpr bool constexpr_smoke() {
  xer::valid_chain c;
  if (!static_cast<bool>(c)) {
    return false;
  }
  c.try_call([]() noexcept { return true; });
  if (!static_cast<bool>(c)) {
    return false;
  }
  c.try_call([]() noexcept { return false; });
  if (static_cast<bool>(c)) {
    return false;
  }
  c.try_call([]() noexcept { return true; });
  if (static_cast<bool>(c)) {
    return false;
  }
  c.reset();
  return static_cast<bool>(c);
}

} // namespace

int main() {
  std::printf("xer_valid_chain_correctness compiler=");
#if XER_COMPILER(MSVC)
  std::printf("msvc\n");
#elif XER_COMPILER(GCC)
  std::printf("gcc\n");
#elif XER_COMPILER(CLANG)
  std::printf("clang\n");
#else
  std::printf("other\n");
#endif

  check_basic();
  check_try_call_chain();
  check_fuse_skips_later_calls();
  check_assign_bool_rhs_eager();
  check_copy_can_revive();
  check_reset();
  static_assert(constexpr_smoke());
  CHECK(constexpr_smoke());

  if (g_failures != 0) {
    std::printf("FAILED %d\n", g_failures);
    return 1;
  }
  std::printf("ALL_PASSED\n");
  return 0;
}
