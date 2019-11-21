#include "vm/page.h"
#include <stdio.h>

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

void spt_insert (void *vaddr, void *paddr, bool writable) {
  struct thread *cur = thread_current ();
  struct spte *spte = (struct spte *) malloc (sizeof(struct spte));
  spte->vaddr = vaddr;
  spte->paddr = paddr;
  spte->status = FRAME;
  spte->writable = writable;
  hash_insert (&cur->spt, &spte->hash_elem);
}

void spt_remove (struct hash_elem *e, void *aux UNUSED) {
  struct spte *spte = hash_entry (e, struct spte, hash_elem);
  if (spte->paddr != NULL)
    fte_remove (spte->paddr);
  else if (spte->status == SWAP){
    // printf("swap_remove happens\n");
    swap_remove (spte->block_index);
  }
  free (spte);
}

void spt_destroy (struct hash *spt) {
  if (spt != NULL) {
    hash_destroy (spt, spt_remove);
  }
}

bool grow_stack (void *fault_addr) {
  bool success = false;
  void *new_page = pg_round_down (fault_addr);
  if (new_page >= PHYS_BASE - PGSIZE) {
    return false;
  }
  struct spte sample;
  sample.vaddr = new_page;
  struct hash_elem *e = hash_find (&thread_current() -> spt, &sample.hash_elem);
  if (e != NULL) {
    return true;
  }
  void *kpage = ft_allocate (PAL_USER | PAL_ZERO, new_page);
  if (kpage != NULL) {
      struct thread *t = thread_current ();

      success = pagedir_get_page (t->pagedir, new_page) == NULL
                  && pagedir_set_page (t->pagedir, new_page, kpage, true);
      if (success) {
        spt_insert (new_page, kpage, true);
        ft_set_pin (kpage, false);
      } else
        palloc_free_page (kpage);
    }
  return grow_stack(new_page + PGSIZE);
}
