/* #include <hash.h>
#include "vm/frame.h"
#include "threads/thread.h"

void ft_init (struct hash *ft) {
  hash_init (ft, ft_hash_func, ft_less_func, NULL);
}

unsigned ft_hash_func (const struct hash_elem *e, void *aux UNUSED) {
  struct fte *fte = hash_entry (e, struct fte, hash_elem);
  return hash_int((int)(fte->pa));
}

bool ft_less_func (const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED) {
  struct fte *fte_a = hash_entry (a, struct fte, hash_elem);
  struct fte *fte_b = hash_entry (b, struct fte, hash_elem);
  return fte_a->pa < fte_b->pa;
}

void ft_insert (void *paddr) {
  struct thread *cur = thread_current ();
  struct fte fte;
  fte.pa = paddr;
  hash_insert (&cur->ft, &fte.hash_elem);
}

void ft_delete (void *paddr) {
  struct thread *cur = thread_current ();
  struct fte fte;
  fte.pa = paddr;
  hash_delete (&cur->ft, &fte.hash_elem);
}

void ft_destroy (void) {
  struct thread *cur = thread_current ();
  if (ft != NULL) {
    hash_destroy (&cur->ft, NULL);
  }
}
 */