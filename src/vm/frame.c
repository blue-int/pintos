#include "threads/thread.h"
#include "vm/frame.h"
#include "vm/swap.h"

static struct hash ft_hash;
static struct list ft_list;
static unsigned ft_hash_func (const struct hash_elem *e, void *aux UNUSED);
static bool ft_less_func (const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED);

void ft_init (void) {
  hash_init (&ft_hash, ft_hash_func, ft_less_func, NULL);
  list_init (&ft_list);
}

void *ft_allocate (void) {
  void *kpage = palloc_get_page (PAL_USER);
  if (kpage == NULL) {
    // Find victim frame and SWAP_OUT
    ft_evict ();
    kpage = palloc_get_page (PAL_USER);
  }
  ft_insert (kpage);
  return kpage;
}

void ft_evict (void) {
  struct list_elem *e;
  for (e = list_begin (&ft_list); e != list_end (&ft_list);
       e = list_next (e))
    {
      struct fte *fte = list_entry (e, struct fte, list_elem);
      struct thread *cur = thread_current ();
      bool chance = pagedir_is_accessed (cur->pagedir, fte->vaddr);
      printf("test point 1\n");
      if (chance) 
      {
        printf("test point 2\n");
        pagedir_set_accessed (cur->pagedir, fte->vaddr, false);
      } 
      else 
      { 
        printf("test point 3\n");
        if (swap_out (fte)) {
          printf("test point 4\n");
          // ft_delete (fte);
          // spt_delete (fte->vaddr);
          break;
        }
        else{
          printf("test point 5\n");
          PANIC ("not enough swap slots");
        }
      }
    }
}

void ft_insert (void *paddr) {
  struct fte *fte = (struct fte *) malloc (sizeof (struct fte));
  fte->paddr = paddr;
  fte->t = thread_current ();
  hash_insert (&ft_hash, &fte->hash_elem);
  list_push_back (&ft_list, &fte->list_elem);
}

void ft_add_vaddr (void *vaddr, void *paddr) {
  struct fte *fte = (struct fte *) malloc (sizeof (struct fte));
  fte->paddr = paddr;
  fte->vaddr = vaddr;
  hash_replace (&ft_hash, &fte->hash_elem);
}

void ft_delete (struct fte * fte) {
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
