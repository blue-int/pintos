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

void * ft_allocate (enum palloc_flags flags) {
  // printf("flags are : %d\n", flags);
  // printf("ft_allocate \n");
  void *kpage = palloc_get_page (flags);
  if (kpage == NULL) {
    // Find victim frame and SWAP_OUT
    ft_evict ();
    kpage = palloc_get_page (flags);
  }
  if(flags != (PAL_USER | PAL_ZERO)){
    ft_insert (kpage);
  }
  return kpage;
}

void ft_evict (void) {
  // printf("ft_evict \n");

  struct list_elem *e;
  for (e = list_begin (&ft_list); e != list_end (&ft_list);
       e = list_next (e))
    {
      struct fte *fte = list_entry (e, struct fte, list_elem);
      uint32_t *pd = fte->t->pagedir;
      struct hash *spt = &fte->t->spt;
      void *vaddr = fte->vaddr;
      if (pagedir_is_accessed (pd, vaddr)) {
        pagedir_set_accessed (pd, vaddr, false);
      } 
      else {
        printf ("evicted vaddr: %p\n", vaddr);
        if (swap_out (spt, fte)) {
          pagedir_clear_page (pd, vaddr);
          palloc_free_page (fte->paddr);
          ft_delete (fte);
          break;
        }
        else {
          PANIC ("not enough swap slots");
        }
      }
    }
}

void ft_insert (void *paddr) {
  // printf("ft_insert \n");
  struct fte *fte = (struct fte *) malloc (sizeof (struct fte));
  fte->paddr = paddr;
  fte->t = thread_current ();
  hash_insert (&ft_hash, &fte->hash_elem);
  list_push_back (&ft_list, &fte->list_elem);
}

void ft_add_vaddr (void *vaddr, void *paddr) {
  // printf("ft_add_vaddr \n");
  struct fte sample;
  sample.paddr = paddr;
  struct hash_elem *e = hash_find (&ft_hash, &sample.hash_elem);
  if (e == NULL){
    // printf ("hash_elem not found\n");
    return;
  }
  struct fte *fte = hash_entry (e, struct fte, hash_elem);
  fte->vaddr = vaddr;
  printf ("vaddr is : %p\n",vaddr );
}

void ft_delete (struct fte * fte) {
  // printf("ft_delete \n");
  hash_delete (&ft_hash, &fte->hash_elem);
  list_remove (&fte->list_elem);
}

void ft_destroy (void) {
  hash_destroy (&ft_hash, NULL);
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
