#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <hash.h>
#include <debug.h>

struct fte {
  struct hash_elem hash_elem;
  void* pa;
};

void ft_init (struct hash *ft);
unsigned ft_hash_func (const struct hash_elem *e, void *aux UNUSED);
bool ft_less_func (const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED);
void ft_insert (void *vaddr);
void ft_delete (void *vaddr);
void ft_destroy (void);

#endif
