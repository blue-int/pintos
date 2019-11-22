#include "vm/page.h"
#include <stdio.h>
#include <string.h>

void spt_init (struct hash *spt) {
  hash_init (spt, spt_hash_func, spt_less_func, NULL);
}

unsigned spt_hash_func (const struct hash_elem *e, void *aux UNUSED) {
  struct spte *spte = hash_entry (e, struct spte, hash_elem);
  return hash_int((int)(spte->vaddr));
}

bool spt_less_func (const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED) {
  struct spte *spte_a = hash_entry (a, struct spte, hash_elem);
  struct spte *spte_b = hash_entry (b, struct spte, hash_elem);
  return spte_a->vaddr < spte_b->vaddr;
}

void spt_insert (void *vaddr, void *paddr, bool writable, bool dirty) {
  struct thread *cur = thread_current ();
  struct hash *spt = &cur->spt;
  struct spte *spte = spt_find (spt, vaddr);
  bool new = false;
  if (spte == NULL) {
    new = true;
    spte = (struct spte *) malloc (sizeof(struct spte));
  }
  spte->vaddr = vaddr;
  spte->paddr = paddr;
  spte->writable = writable;
  spte->dirty = dirty;
  spte->status = ON_FRAME;
  pagedir_set_dirty (cur->pagedir, vaddr, dirty);
  if (new)
    hash_insert (spt, &spte->hash_elem);
}

void spt_remove (struct hash_elem *e, void *aux UNUSED) {
  struct spte *spte = hash_entry (e, struct spte, hash_elem);
  if (spte->paddr != NULL)
    fte_remove (spte->paddr);
  else if (spte->status == ON_SWAP){
    swap_remove (spte->block_index);
  }
  free (spte);
}

struct spte *spt_find (struct hash *spt, void *vaddr) {
  struct spte sample;
  sample.vaddr = vaddr;
  struct hash_elem *e = hash_find (spt, &sample.hash_elem);
  if (e == NULL) return NULL;
  struct spte *spte = hash_entry (e, struct spte, hash_elem);
  return spte;
}

void spt_destroy (struct hash *spt) {
  if (spt != NULL)
    hash_destroy (spt, spt_remove);
}

bool page_check (struct hash *spt, void *fault_addr) {
  struct spte *spte = spt_find (spt, pg_round_down (fault_addr));
  if (spte == NULL)
    return false;
  if (spte->status == ON_SWAP) {
    swap_in (spt, fault_addr);
    return true;
  }
  else {
    if (spte->status == ON_DISK || spte->status == ZERO) {
      return load_file (spt, fault_addr);
    } else {
      return true;
    }
  }
}

bool grow_stack (void *fault_addr) {
  bool success = false;
  void *new_page = pg_round_down (fault_addr);
  if (new_page >= PHYS_BASE - PGSIZE)
    return false;

  struct thread *cur = thread_current ();
  struct spte *spte = spt_find (&cur->spt, new_page);
  if (spte != NULL)
    return true;
  void *kpage = ft_allocate (PAL_USER | PAL_ZERO, new_page);
  if (kpage != NULL) {
    success = pagedir_get_page (cur->pagedir, new_page) == NULL
                && pagedir_set_page (cur->pagedir, new_page, kpage, true);
    if (success) {
      spt_insert (new_page, kpage, true, true);
      ft_set_pin (kpage, false);
    } else
      palloc_free_page (kpage);
  }
  return grow_stack (new_page + PGSIZE);
}

bool load_file (struct hash *spt, void *fault_addr) {
  struct thread *cur = thread_current ();
  void *new_page = pg_round_down (fault_addr);
  struct spte *spte = spt_find (spt, new_page);
  size_t read_bytes = spte->read_bytes;
  size_t zero_bytes = spte->zero_bytes;
  bool writable = spte->writable;

  // if (pagedir_get_page (cur->pagedir, new_page))
    // return true;

  void *kpage = ft_allocate (PAL_USER, new_page);
  if (kpage == NULL)
    return false;

  if (file_read_at (spte->fp, kpage, read_bytes, spte->ofs) != (off_t)read_bytes)
    {
      palloc_free_page (kpage);
      return false;
    }
  memset (kpage + read_bytes, 0, zero_bytes);
  bool success = pagedir_get_page (cur->pagedir, new_page) == NULL
              && pagedir_set_page (cur->pagedir, new_page, kpage, writable);
  if (success) {
    spt_insert (new_page, kpage, writable, false);
    ft_set_pin (kpage, false);
  } else {
    palloc_free_page (kpage);
  }
  return success;
}
