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
  lock_init (&alloc_lock);
}

void ft_set_pin (void *buffer, unsigned size, bool status) {
  // printf ("thread %s buffer: %p size of : %d\n", thread_name (), buffer, size);
  lock_acquire (&ft_lock);
  struct spte sample;
  sample.vaddr = pg_round_down (buffer);
  struct hash_elem *e = hash_find (&thread_current ()->spt, &sample.hash_elem);
  if (e != NULL) {
    struct spte *spte = hash_entry (e, struct spte, hash_elem);
    if (spte != NULL && spte->status == SWAP) {
      printf ("spte not null and paddr points to %p\n",spte->paddr);
      struct fte f_sample;
      f_sample.paddr = spte->paddr;
      struct hash_elem *elem = hash_find (&ft_hash, &f_sample.hash_elem);
      if (elem != NULL) {
        printf ("hash elem not null\n");
        struct fte *fte = hash_entry (elem, struct fte, hash_elem);
        if (fte != NULL) {
          printf ("fte not null\n");
          fte->accessed = status;
        }
      }
    }
  }
  lock_release (&ft_lock);
}

void * ft_allocate (enum palloc_flags flags) {
  lock_acquire (&alloc_lock);
  void *kpage = palloc_get_page (flags);
  if (kpage == NULL) {
    ft_evict ();
    kpage = palloc_get_page (flags);
  }
  ft_insert (kpage);
  lock_release (&alloc_lock);
  return kpage;
}

void ft_evict (void) {
  struct list_elem *e;
  for (e = list_begin (&ft_list); e != list_end (&ft_list);
       e = list_next (e))
    {
      struct fte *fte = list_entry (e, struct fte, list_elem);
      uint32_t *pd = fte->t->pagedir;
      struct hash *spt = &fte->t->spt;
      void *vaddr = fte->vaddr;
      // printf ("%p evict candidate\n", vaddr);
      if (vaddr == NULL) {
        printf ("NULL\n");
        continue;
      }
      if (fte->accessed) {
        printf ("accessed!\n");
        continue;
      }
      if (pagedir_is_accessed (pd, vaddr)) {
        pagedir_set_accessed (pd, vaddr, false);
      } 
      else {
        // printf ("%p evicted vaddr\n", vaddr);
        if (swap_out (spt, fte)) {
          pagedir_clear_page (pd, vaddr);
          palloc_free_page (fte->paddr);
          ft_delete (fte);
          return;
        }
        else {
          PANIC ("not enough swap slots");
        }
      }
    }
  ft_evict ();
}

void ft_insert (void *paddr) {
  lock_acquire (&ft_lock);
  struct fte *fte = (struct fte *) malloc (sizeof (struct fte));
  fte->paddr = paddr;
  fte->vaddr = NULL;
  fte->accessed = false;
  fte->t = thread_current ();
  hash_insert (&ft_hash, &fte->hash_elem);
  list_push_back (&ft_list, &fte->list_elem);
  lock_release (&ft_lock);
}

void ft_add_vaddr (void *vaddr, void *paddr) {
  lock_acquire (&ft_lock);
  struct fte sample;
  sample.paddr = paddr;
  struct hash_elem *e = hash_find (&ft_hash, &sample.hash_elem);
  if (e == NULL){
    printf ("hash_elem not found\n");
    return;
  }
  struct fte *fte = hash_entry (e, struct fte, hash_elem);
  fte->vaddr = vaddr;
  lock_release (&ft_lock);
}

void ft_delete (struct fte *fte) {
  lock_acquire (&ft_lock);
  hash_delete (&ft_hash, &fte->hash_elem);
  list_remove (&fte->list_elem);
  free (fte);
  lock_release (&ft_lock);
}

void fte_remove (void *paddr) {
  lock_acquire (&ft_lock);
  struct fte sample;
  sample.paddr = paddr;
  struct hash_elem *elem = hash_delete (&ft_hash, &sample.hash_elem);
  if (elem == NULL) {
    return;
  }
  struct fte *fte = hash_entry (elem, struct fte, hash_elem);
  if (fte != NULL) {
    list_remove (&fte->list_elem);
    palloc_free_page (fte->paddr);
    free (fte);
  }
  lock_release (&ft_lock);
}

void ft_destroy (void) {
  lock_acquire (&ft_lock);
  hash_destroy (&ft_hash, NULL);
  lock_release (&ft_lock);
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
