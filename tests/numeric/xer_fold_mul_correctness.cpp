// Correctness for xer::fold_mulu64 (low64 ^ high64 of a*b).
// Build (from repo root):
//   MSVC:  call D:\develop\c++\vs\VC\Auxiliary\Build\vcvars64.bat && cl /std:c++latest /O2 /EHsc /Iinclude /Fe:tests\numeric\xer_fold_mul_correctness_msvc.exe tests\numeric\xer_fold_mul_correctness.cpp
//   GCC:   g++ -std=c++23 -O2 -Iinclude -o tests/numeric/xer_fold_mul_correctness_gcc tests/numeric/xer_fold_mul_correctness.cpp
//   Clang: clang++ -std=c++23 -O2 -Iinclude -o tests/numeric/xer_fold_mul_correctness_clang tests/numeric/xer_fold_mul_correctness.cpp

#include <cstdint>
#include <cstdio>
#include <cstdlib>

#if defined(_MSC_VER)
#	include <intrin.h>
#endif

#include "xer/numeric/xer_fold_mul.h"

namespace {

std::uint64_t ref_fold(const std::uint64_t a, const std::uint64_t b) noexcept {
#if defined(_MSC_VER) && defined(_M_X64)
	unsigned long long hi = 0;
	const std::uint64_t lo = _umul128(a, b, &hi);
	return lo ^ hi;
#elif defined(_MSC_VER) && defined(_M_ARM64)
	return (a * b) ^ __umulh(a, b);
#elif defined(__SIZEOF_INT128__)
	const __uint128_t p = static_cast<__uint128_t>(a) * b;
	return static_cast<std::uint64_t>(p) ^ static_cast<std::uint64_t>(p >> 64);
#else
#error "need wide multiply for reference"
#endif
}

int fails = 0;

void check(const char* name, const std::uint64_t a, const std::uint64_t b) {
	const std::uint64_t got = xer::fold_mulu64(a, b);
	const std::uint64_t exp = ref_fold(a, b);
	const std::uint64_t fb = xer::details_::fold_mulu64_fallback(a, b);
	if (got != exp) {
		std::printf("FAIL api %s a=%016llx b=%016llx got=%016llx exp=%016llx\n",
			name,
			static_cast<unsigned long long>(a),
			static_cast<unsigned long long>(b),
			static_cast<unsigned long long>(got),
			static_cast<unsigned long long>(exp));
		++fails;
	}
	if (fb != exp) {
		std::printf("FAIL fb  %s a=%016llx b=%016llx got=%016llx exp=%016llx\n",
			name,
			static_cast<unsigned long long>(a),
			static_cast<unsigned long long>(b),
			static_cast<unsigned long long>(fb),
			static_cast<unsigned long long>(exp));
		++fails;
	}
}

}  // namespace

int main() {
	check("0", 0, 0);
	check("1*1", 1, 1);
	check("max*1", ~0ull, 1);
	check("max*max", ~0ull, ~0ull);
	check("2^32", 1ull << 32, 1ull << 32);
	check("2^32-1^2", 0xffffffffull, 0xffffffffull);
	check("hi*hi", 0xffffffff00000000ull, 0xffffffff00000000ull);
	check("carry_low", 0xffffffffffffffffull, 0x100000001ull);  // forces mid<<32 + a0b0 carry
	check("lo_sum_ov", 0xffffffffull, 0x100000001ull);

	// precedence / middle edge: both cross lo parts near max
	check("cross_lo", 0x00000000ffffffffull, 0xffffffff00000001ull);
	check("mix1", 0x0123456789abcdefull, 0xfedcba9876543210ull);
	check("mix2", 0x8000000000000001ull, 0x8000000000000001ull);

	std::uint64_t x = 0x9e3779b97f4a7c15ull;
	std::uint64_t y = 0xbf58476d1ce4e5b9ull;
	for (int i = 0; i < 256; ++i) {
		check("walk", x, y);
		x = x * 0xd1b54a32d192ed03ull + 1;
		y = y * 0x9e3779b97f4a7c15ull + i;
	}

	if (fails) {
		std::printf("%d failure(s)\n", fails);
		return 1;
	}
	std::printf("ok\n");
	return 0;
}
