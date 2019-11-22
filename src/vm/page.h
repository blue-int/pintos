#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <hash.h>
#include <debug.h>
#include "devices/block.h"
#include "threads/thread.h"
#include "threads/malloc.h"
#include "vm/frame.h"
#include "vm/swap.h"
#include "filesys/file.h"
#include "filesys/filesys.h"

enum status {
  ON_FRAME,
  ON_SWAP,
  ON_DISK,
  ZERO
};

struct spte {
  enum status status;
  struct hash_elem hash_elem;
  void *vaddr;
  void *paddr;
  block_sector_t block_index;
  bool writable;
  bool dirty;
  struct file *fp;
  size_t ofs;
  size_t read_bytes;
  size_t zero_bytes;
};

void spt_init (struct hash *spt);
unsigned spt_hash_func (const struct hash_elem *e, void *aux UNUSED);
bool spt_less_func (const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED);
void spt_insert (void *vaddr, void *paddr, bool writable, bool dirty);
void spt_remove (struct hash_elem *e, void *aux UNUSED);
struct spte *spt_find (struct hash *spt, void *vaddr);
void spt_destroy (struct hash *spt);
bool page_check (struct hash *spt, void *fault_addr);
bool grow_stack (void *fault_addr);
bool load_file (struct hash *spt, void *fault_addr);

#endif
