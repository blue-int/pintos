#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <hash.h>
#include <debug.h>
#include "threads/malloc.h"
#include "threads/vaddr.h"
#include "threads/palloc.h"
#include "threads/synch.h"
#include "userprog/pagedir.h"

struct lock ft_lock;

struct fte {
  struct hash_elem hash_elem;
  struct list_elem list_elem;
  void *paddr;
  void *vaddr;
  struct thread *t;
};

void ft_init (void);
void * ft_allocate (enum palloc_flags flags);
void ft_evict (void);
void ft_insert (void *paddr);
void ft_add_vaddr (void *vaddr, void *paddr);
void ft_delete (struct fte * fte);
void ft_destroy (void);

#endif
