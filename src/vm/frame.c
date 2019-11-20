#include "threads/thread.h"
#include "vm/frame.h"
#include "vm/swap.h"
#include <stdio.h>

static struct hash ft_hash;
static struct list ft_list;
static unsigned ft_hash_func (const struct hash_elem *e, void *aux UNUSED);
static bool ft_less_func (const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED);

void ft_init (void) {
  hash_init (&ft_hash, ft_hash_func, ft_less_func, NULL);
  list_init (&ft_list);
  lock_init (&ft_lock);
}

void buffer_set_pin (void *buffer, unsigned size, bool pin) {
  struct hash *spt = &thread_current()->spt;
  for (void *vaddr = pg_round_down(buffer); vaddr < buffer + size; vaddr += PGSIZE) {
    struct spte sample;
    sample.vaddr = vaddr;
    struct hash_elem *e = hash_find (spt, &sample.hash_elem);
    if (e == NULL)
      PANIC ("Hash elem null\n");
    struct spte *spte = hash_entry (e, struct spte, hash_elem);
    if (spte == NULL) PANIC ("no spte");
    if (spte->status == FRAME) {
      ft_set_pin (spte->paddr, pin);
    }
    else {
      if (pin) {
        swap_in (spt, vaddr);
      }
      ft_set_pin (spte->paddr, pin);
    }
  }
}

void ft_set_pin (void *paddr, bool status) {
  lock_acquire (&ft_lock);

  struct fte sample;
  sample.paddr = paddr;
  struct hash_elem *e = hash_find (&ft_hash, &sample.hash_elem);
  if (e == NULL)
    PANIC ("hash_elem is null");
  struct fte *fte = hash_entry (e, struct fte, hash_elem);
  fte->pinned = status;

  lock_release (&ft_lock);
}

void * ft_allocate (enum palloc_flags flags, void *vaddr) {
  lock_acquire (&ft_lock);
  void *kpage = palloc_get_page (flags);
  if (kpage == NULL) {
    bool evict_flag = ft_evict();
    if(!evict_flag)
      ft_evict();
    kpage = palloc_get_page (flags);
  }
  ft_insert (kpage, vaddr);
  lock_release (&ft_lock);
  return kpage;
}

bool ft_evict (void) {
  struct list_elem *e;
  for (e = list_begin (&ft_list); e != list_end (&ft_list);
       e = list_next (e))
    {
      struct fte *fte = list_entry (e, struct fte, list_elem);
      uint32_t *pd = fte->t->pagedir;
      struct hash *spt = &fte->t->spt;
      void *vaddr = fte->vaddr;
      if (fte->pinned){
        continue;
      }
      if (pagedir_is_accessed (pd, vaddr)) {
        pagedir_set_accessed (pd, vaddr, false);
      } 
      else {
        pagedir_clear_page (pd, vaddr);
        swap_out (spt, fte);
        palloc_free_page (fte->paddr);
        // printf("ft_delete start\n");
        ft_delete (fte);
        return true;
      }
    }
  return false;
}

void ft_insert (void *paddr, void *vaddr) {
  struct fte *fte = (struct fte *) malloc (sizeof (struct fte));
  fte->paddr = paddr;
  fte->vaddr = vaddr;
  fte->pinned = true;
  fte->t = thread_current ();
  hash_insert (&ft_hash, &fte->hash_elem);
  list_push_back (&ft_list, &fte->list_elem);
}

void ft_delete (struct fte *fte) {
  hash_delete (&ft_hash, &fte->hash_elem);
  list_remove (&fte->list_elem);
  free (fte);
}

void fte_remove (void *paddr) {
  lock_acquire (&ft_lock);
  struct fte sample;
  sample.paddr = paddr;
  struct hash_elem *elem = hash_delete (&ft_hash, &sample.hash_elem);
  struct fte *fte = hash_entry (elem, struct fte, hash_elem);
  ASSERT (fte != NULL);
  list_remove (&fte->list_elem);
  free (fte);
  lock_release (&ft_lock);
}

void buffer_set_pin (void *buffer, unsigned size, bool status) {
  struct hash *spt = &thread_current()->spt;
  for (void *vaddr = pg_round_down(buffer); vaddr < buffer + size; vaddr += PGSIZE) {
    struct spte sample;
    sample.vaddr = vaddr;
    struct hash_elem *e = hash_find (spt, &sample.hash_elem);
    struct spte *spte = hash_entry (e, struct spte, hash_elem);
    if (spte == NULL) PANIC ("no spte");
    if (spte->status == FRAME)
      ft_set_pin (spte->paddr, status);
    else if (spte->status == SWAP && status) {
      swap_in (spt, spte->vaddr);
      ft_set_pin (spte->paddr, status);
    } else if (spte->status == SWAP && !status) {
      ft_set_pin (spte->vaddr, status);
    }
  }
}

static unsigned ft_hash_func (const struct hash_elem *e, void *aux UNUSED) {
  struct fte *fte = hash_entry (e, struct fte, hash_elem);
  return hash_int((int)(fte->paddr));
}

static bool ft_less_func (const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED) {
  struct fte *fte_a = hash_entry (a, struct fte, hash_elem);
  struct fte *fte_b = hash_entry (b, struct fte, hash_elem);
  return fte_a->paddr < fte_b->paddr;
}
