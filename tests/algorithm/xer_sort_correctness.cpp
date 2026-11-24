// Correctness for include/xer/algorithm/xer_sort.h
// (insert_sort, heap_sort, sort)
// Build:
//   MSVC:  cl /std:c++latest /EHsc /Iinclude /Fe:tests\algorithm\xer_sort_correctness_msvc.exe tests\algorithm\xer_sort_correctness.cpp
//   GCC:   g++ -std=c++23 -Iinclude -o tests/algorithm/xer_sort_correctness_gcc tests/algorithm/xer_sort_correctness.cpp
//   Clang: clang++ -std=c++23 -Iinclude -o tests/algorithm/xer_sort_correctness_clang tests/algorithm/xer_sort_correctness.cpp

#include "xer/algorithm/xer_sort.h"

#include <algorithm>
#include <cstdio>
#include <cstdint>
#include <random>
#include <utility>
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

bool is_nondecreasing(const std::vector<int>& v) {
  for (std::size_t i = 1; i < v.size(); ++i) {
    if (v[i - 1] > v[i]) return false;
  }
  return true;
}

template <typename SortFn>
void check_against_std(SortFn sort_fn, std::vector<int> in, const char* tag) {
  auto expected = in;
  std::sort(expected.begin(), expected.end());
  sort_fn(in.data(), in.size());
  if (in != expected) {
    std::printf("FAIL %s: mismatch size=%zu\n", tag, in.size());
    ++g_failures;
  }
}

struct MoveOnly {
  int v = 0;
  MoveOnly() noexcept = default;
  explicit MoveOnly(int x) noexcept : v(x) {}
  MoveOnly(MoveOnly&& o) noexcept : v(o.v) { o.v = -1; }
  MoveOnly& operator=(MoveOnly&& o) noexcept {
    v = o.v;
    o.v = -1;
    return *this;
  }
  MoveOnly(const MoveOnly&) = delete;
  MoveOnly& operator=(const MoveOnly&) = delete;
  friend bool operator>(const MoveOnly& a, const MoveOnly& b) noexcept {
    return a.v > b.v;
  }
  friend bool operator<(const MoveOnly& a, const MoveOnly& b) noexcept {
    return a.v < b.v;
  }
};

template <typename SortFn>
void test_basic_cases(SortFn sort_fn, const char* name) {
  {
    std::vector<int> v;
    sort_fn(v.data(), v.size());
    CHECK(v.empty());
  }
  {
    std::vector<int> v{7};
    sort_fn(v.data(), v.size());
    CHECK(v.size() == 1 && v[0] == 7);
  }
  {
    std::vector<int> v{2, 1};
    sort_fn(v.data(), v.size());
    CHECK(v[0] == 1 && v[1] == 2);
  }
  {
    std::vector<int> v{1, 2, 3, 4, 5};
    sort_fn(v.data(), v.size());
    CHECK(is_nondecreasing(v));
  }
  {
    std::vector<int> v{5, 4, 3, 2, 1};
    sort_fn(v.data(), v.size());
    if (v != std::vector<int>{1, 2, 3, 4, 5}) {
      std::printf("FAIL %s reverse-5\n", name);
      ++g_failures;
    }
  }
  {
    std::vector<int> v{3, 1, 2, 1, 3, 2};
    sort_fn(v.data(), v.size());
    CHECK(is_nondecreasing(v));
  }
}

void test_random_vs_std() {
  std::mt19937 rng(0xC0FFEEu);
  std::uniform_int_distribution<int> dist(-1000, 1000);
  const std::size_t sizes[] = {0, 1, 2, 3, 7, 8, 15, 16, 31, 32, 33, 64, 100, 257, 1024};

  for (std::size_t n : sizes) {
    for (int trial = 0; trial < 40; ++trial) {
      std::vector<int> v(n);
      for (std::size_t i = 0; i < n; ++i) v[i] = dist(rng);

      check_against_std(
          [](int* p, std::size_t n) { xer::insert_sort(p, n); }, v,
          "insert_sort random");
      check_against_std(
          [](int* p, std::size_t n) { xer::heap_sort(p, n); }, v,
          "heap_sort random");
      check_against_std(
          [](int* p, std::size_t n) {
            CHECK(xer::sort(p, n));
          },
          v, "sort random");
    }
  }

  for (std::size_t n : {0u, 1u, 2u, 50u, 200u, 1000u}) {
    std::vector<int> equal(n, 42);
    check_against_std(
        [](int* p, std::size_t n) { CHECK(xer::sort(p, n)); }, equal,
        "sort equal");

    std::vector<int> asc(n);
    for (std::size_t i = 0; i < n; ++i) asc[i] = static_cast<int>(i);
    check_against_std(
        [](int* p, std::size_t n) { CHECK(xer::sort(p, n)); }, asc, "sort asc");

    std::vector<int> desc(n);
    for (std::size_t i = 0; i < n; ++i) desc[i] = static_cast<int>(n - i);
    check_against_std(
        [](int* p, std::size_t n) { CHECK(xer::sort(p, n)); }, desc,
        "sort desc");

    std::vector<int> saw(n);
    for (std::size_t i = 0; i < n; ++i) saw[i] = static_cast<int>(i % 7);
    check_against_std(
        [](int* p, std::size_t n) { CHECK(xer::sort(p, n)); }, saw,
        "sort sawtooth");
  }
}

void test_move_only() {
  {
    std::vector<MoveOnly> v;
    v.emplace_back(5);
    v.emplace_back(1);
    v.emplace_back(4);
    v.emplace_back(2);
    v.emplace_back(3);
    xer::insert_sort(v.data(), v.size());
    for (int i = 0; i < 5; ++i) CHECK(v[static_cast<std::size_t>(i)].v == i + 1);
  }
  {
    std::vector<MoveOnly> v;
    for (int i = 0; i < 64; ++i) v.emplace_back(64 - i);
    xer::heap_sort(v.data(), v.size());
    for (int i = 0; i < 64; ++i)
      CHECK(v[static_cast<std::size_t>(i)].v == i + 1);
  }
  // xer::sort currently requires nothrow copy constructible (pivot by value).
}

void test_adversarial_patterns() {
  // Many duplicates + a few extremes (stresses pivot/partition).
  for (int trial = 0; trial < 20; ++trial) {
    std::vector<int> v(500, 0);
    v[0] = 1000000;
    v[1] = -1000000;
    v[250] = 1;
    v[499] = -1;
    for (int i = 2; i < 50; ++i) v[static_cast<std::size_t>(i)] = trial;
    check_against_std(
        [](int* p, std::size_t n) { CHECK(xer::sort(p, n)); }, v,
        "sort adversarial");
  }

  // Already partitioned-looking / organ-pipe.
  {
    std::vector<int> v;
    for (int i = 0; i < 256; ++i) v.push_back(i);
    for (int i = 255; i >= 0; --i) v.push_back(i);
    check_against_std(
        [](int* p, std::size_t n) { CHECK(xer::sort(p, n)); }, v,
        "sort organ-pipe");
  }
}

}  // namespace

int main() {
  test_basic_cases([](int* p, std::size_t n) { xer::insert_sort(p, n); },
                   "insert_sort");
  test_basic_cases([](int* p, std::size_t n) { xer::heap_sort(p, n); },
                   "heap_sort");
  test_basic_cases(
      [](int* p, std::size_t n) { CHECK(xer::sort(p, n)); }, "sort");
  test_random_vs_std();
  test_move_only();
  test_adversarial_patterns();

  if (g_failures == 0) {
    std::printf("ok\n");
    return 0;
  }
  std::printf("%d failure(s)\n", g_failures);
  return 1;
}
