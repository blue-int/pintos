#ifndef VM_SWAP_H
#define VM_SWAP_H

#include <hash.h>
#include "threads/malloc.h"
#include "threads/vaddr.h"
#include "vm/frame.h"
#include "vm/page.h"

bool swap_out (struct hash *spt, struct fte * fte);
void swap_in (struct hash *spt, void *vaddr);
bool swap_check (struct hash *spt, void *vaddr);
void swap_insert (struct hash *spt, void *vaddr, block_sector_t block_index);

#endif
