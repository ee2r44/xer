#include "xer/container/xer_arena.h"

#include <cstdio>

int main() {
	std::printf("step1\n");
	xer::arena a;
	std::printf("step2\n");
	int* p = a.alloc<int>(4);
	std::printf("p=%p\n", static_cast<void*>(p));
	for (int i = 0; i < 4000; ++i) {
		void* q = a.align_alloc(8, 16);
		if (!q) {
			std::printf("null at %d\n", i);
			return 1;
		}
	}
	std::printf("ok bump\n");

	xer::arena b;
	constexpr std::size_t big = (static_cast<std::size_t>(1) << 18) + 64;
	void* r = b.align_alloc(32, big);
	std::printf("big=%p\n", r);

	std::printf("ok\n");
	return 0;
}
