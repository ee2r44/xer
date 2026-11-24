#include "xer/numeric/xer_fold_mul.h"

static_assert(xer::fold_mulu64(3, 5) == 15);
static_assert(
	xer::fold_mulu64(0xffffffffffffffffull, 0xffffffffffffffffull) ==
	0xffffffffffffffffull);

int main() { return 0; }
