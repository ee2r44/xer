// Correctness for xer::arena.
//   MSVC:  call D:\develop\c++\vs\VC\Auxiliary\Build\vcvars64.bat && cl /std:c++latest /O2 /EHsc /Iinclude /Fe:tests\container\xer_arena_correctness_msvc.exe tests\container\xer_arena_correctness.cpp
//   GCC:   g++ -std=c++23 -O2 -Iinclude -o tests/container/xer_arena_correctness_gcc tests/container/xer_arena_correctness.cpp
//   Clang: clang++ -std=c++23 -O2 -Iinclude -o tests/container/xer_arena_correctness_clang tests/container/xer_arena_correctness.cpp

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "xer/container/xer_arena.h"

namespace {

int fails = 0;

void expect(bool ok, const char* msg) {
	if (!ok) {
		std::printf("FAIL %s\n", msg);
		++fails;
	}
}

bool is_aligned(const void* p, std::size_t align) {
	return (reinterpret_cast<std::uintptr_t>(p) & (align - 1)) == 0;
}

}  // namespace

int main() {
	{
		xer::arena a;
		expect(a.alloc<int>(0) == nullptr, "alloc count0");
		expect(a.align_alloc(16, 0) == nullptr, "align size0");
		expect(a.align_alloc(3, 8) == nullptr, "align not pow2");
	}

	{
		xer::arena a;
		int* p = a.alloc<int>(4);
		expect(p != nullptr, "alloc int4 nonnull");
		expect(is_aligned(p, alignof(int)), "alloc int4 align");
		std::memset(p, 0x5a, sizeof(int) * 4);
	}

	{
		xer::arena a;
		struct alignas(64) B { char x[64]; };
		B* p = a.alloc<B>(1);
		expect(p != nullptr, "overalign nonnull");
		expect(is_aligned(p, 64), "overalign 64");
	}

	{
		xer::arena a;
		// Force many small bump allocs to cross pages (DFSize ~3KiB).
		for (int i = 0; i < 4000; ++i) {
			void* p = a.align_alloc(8, 16);
			expect(p != nullptr, "bump loop nonnull");
			if (!p) break;
			expect(is_aligned(p, 8), "bump loop align");
			*static_cast<std::uint64_t*>(p) = static_cast<std::uint64_t>(i);
		}
	}

	{
		xer::arena a;
		constexpr std::size_t big = (static_cast<std::size_t>(1) << 18) + 64;
		void* p = a.align_alloc(32, big);
		expect(p != nullptr, "big page nonnull");
		expect(is_aligned(p, 32), "big page align");
		std::memset(p, 0, 64);
	}

	{
		xer::arena a;
		int* p = a.alloc<int>();
		expect(p != nullptr, "before reset");
		a.reset();
		int* q = a.alloc<int>();
		expect(q != nullptr, "after reset");
	}

	{
		xer::arena a;
		int* p = a.alloc<int>();
		expect(p != nullptr, "move src alloc");
		xer::arena b(static_cast<xer::arena&&>(a));
		int* q = b.alloc<int>();
		expect(q != nullptr, "move dst alloc");
		int* r = a.alloc<int>();
		expect(r != nullptr, "moved-from still usable");
	}

	{
		xer::arena a;
		xer::arena b;
		(void)a.alloc<int>();
		b = static_cast<xer::arena&&>(a);
		expect(b.alloc<char>(32) != nullptr, "move-assign dst");
	}

	if (fails) {
		std::printf("%d failure(s)\n", fails);
		return 1;
	}
	std::printf("ok\n");
	return 0;
}
