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
  bool pinned;
};

void ft_init (void);
void buffer_set_pin (void *buffer, unsigned size, bool pin);
void ft_set_pin (void *paddr, bool status);
void * ft_allocate (enum palloc_flags flags, void *vaddr);
bool ft_evict (void);
void fte_remove (void *paddr);

#endif
