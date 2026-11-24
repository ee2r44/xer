#include "xer/container/xer_arena.h"
int main() {
  xer::arena a;
  int* p = a.alloc<int>(4);
  void* q = a.align_alloc(16, 32);
  (void)p; (void)q;
  xer::arena b = static_cast<xer::arena&&>(a);
  return 0;
}
