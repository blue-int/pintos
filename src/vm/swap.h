#ifndef VM_SWAP_H
#define VM_SWAP_H
#include "vm/frame.h"

struct ste {
  void * vaddr;
};

void swap_init (void);
bool swap_out (struct fte * fte);
void swap_in (void *vaddr);

#endif
