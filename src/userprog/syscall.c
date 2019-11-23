#include "userprog/syscall.h"
#include "userprog/pagedir.h"
#include "userprog/process.h"
#include <stdio.h>
#include <syscall-nr.h>
#include <string.h>
#include <bitmap.h>
#include <round.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "devices/shutdown.h"
#include "devices/input.h"
#include "vm/frame.h"
#include "vm/page.h"
#include "filesys/filesys.h"
#include "filesys/file.h"

static void syscall_handler (struct intr_frame *);
static struct bitmap *mmap_index;

void
syscall_init (void) 
{
  mmap_index = bitmap_create (128);
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f) 
{
  uint32_t *esp = (uint32_t *)(f->esp);
  check_valid_addr (esp);
  check_valid_addr (esp + 1);
  int sysnum = *(int *)f->esp;
  uint32_t *arg0 = (uint32_t *)(f->esp + 4);
  uint32_t *arg1 = (uint32_t *)(f->esp + 8);
  uint32_t *arg2 = (uint32_t *)(f->esp + 12);

  switch (sysnum)
  {
  case SYS_HALT: // 0
    halt ();
    break;
  
  case SYS_EXIT: // 1
    check_valid_addr (arg0);
    exit ((int)*arg0);
    break;
  
  case SYS_EXEC: // 1
    check_valid_addr (arg0);
    char *p = f->esp + 4;
    check_valid_addr (p);
    while (*p != '\0')
      check_valid_addr (++p);
    f->eax = exec (*(const char **)(f->esp + 4));
    break;
  
  case SYS_WAIT: // 1
    check_valid_addr (arg0);
    f->eax = wait ((pid_t)*arg0);
    break;
  
  case SYS_CREATE: // 2
    check_valid_addr (arg1);
    f->eax = create ((const char *)*arg0, (unsigned)*arg1);
    break;
  
  case SYS_REMOVE: // 1
    check_valid_addr (arg0);
    f->eax = remove ((const char *)*arg0);
    break;

  case SYS_OPEN: // 1
    check_valid_addr (arg0);
    f->eax = open ((const char *)*arg0);
    break;

  case SYS_FILESIZE: // 1
    check_valid_addr (arg0);
    f->eax = filesize ((int)*arg0);
    break;

  case SYS_READ: // 3
    check_valid_addr (arg2);
    f->eax = read ((int)*arg0, (void *)*arg1, (unsigned)*arg2);
    break;

  case SYS_WRITE: // 3
    check_valid_addr (arg2);
    f->eax = write ((int)*arg0, (void *)*arg1, (unsigned)*arg2);
    break;

  case SYS_SEEK: // 2
    check_valid_addr (arg1);
    seek ((int)*arg0, (unsigned)*arg1);
    break;

  case SYS_TELL: // 1
    check_valid_addr (arg0);
    f->eax = tell ((int)*arg0);
    break;

  case SYS_CLOSE: // 1
    check_valid_addr (arg0);
    close ((int)*arg0);
    break;
  
  case SYS_MMAP:
    check_valid_addr (arg0);
    f->eax = mmap ((int)*arg0, (void *)*arg1);
    break;
  
  case SYS_MUNMAP:
    check_valid_addr (arg1);
    munmap ((mapid_t)*arg0);
    break;
  
  default:
    break;
  }
}

void check_valid_addr (const void *vaddr) {
  struct hash *spt = &thread_current() ->spt;
  struct spte *spte = spt_find (spt, (void *) pg_round_down (vaddr));
  if (is_user_vaddr (vaddr) == false || (pagedir_get_page (thread_current ()->pagedir, vaddr) == NULL && spte == NULL)) {
    exit (-1);
  }
}

void halt (void) {
  shutdown_power_off ();
}

void exit (int status) {
  printf ("%s: exit(%d)\n", thread_name (), status);
  thread_current ()->exit_status = status;
  thread_exit ();
}

pid_t exec (const char *cmd_line) {
  const char *p = cmd_line;
  check_valid_addr (p);
  while (*p != '\0')
    check_valid_addr (++p);
  lock_acquire (&filesys_lock);
  pid_t pid = process_execute (cmd_line);
  lock_release (&filesys_lock);
  return pid;
}

int wait (pid_t pid) {
  pid_t child = process_wait (pid);
  return child;
}

bool create (const char *file, unsigned initial_size) {
  check_valid_addr (file);
  lock_acquire (&filesys_lock);
  bool success = filesys_create (file, initial_size);
  lock_release (&filesys_lock);
  return success;
}

bool remove (const char *file) {
  check_valid_addr (file);
  lock_acquire (&filesys_lock);
  bool success = filesys_remove (file);
  lock_release (&filesys_lock);
  return success;
}

int open (const char *file) {
  check_valid_addr (file);
  lock_acquire (&filesys_lock);
  struct file *fp = filesys_open (file);
  struct thread *cur = thread_current ();
  if (fp) {
    for (int i = 2; i < 128; i++) {
      if (!(cur->fd[i])) {
        if (strcmp (thread_name (), file) == 0)
          file_deny_write (fp);
        cur->fd[i] = fp;
        lock_release (&filesys_lock);
        return i;
      }
    }
  }
  lock_release (&filesys_lock);
  return -1;
}

int filesize (int fd) {
  lock_acquire (&filesys_lock);
  int length = file_length (thread_current ()->fd[fd]);
  lock_release (&filesys_lock);
  return length;
}

int read (int fd, void *buffer, unsigned size) {
  check_valid_addr (buffer);
  struct thread *cur = thread_current ();
  int result = -1;
  int len;
  lock_acquire (&filesys_lock);

  if (fd == 0) {
    input_getc ();
    result = size;
  } else if (fd > 1 && fd < 128) {
    struct file* fp = cur->fd[fd];
    if (fp) {
      len = file_length (fp);
      buffer_set_pin (buffer, size, true);
      len = file_read (fp, buffer, size);
      buffer_set_pin (buffer, size, false);
      result = len;
    }
  }
  lock_release (&filesys_lock);
  return result;
}

int write (int fd, const void *buffer, unsigned size) {
  check_valid_addr (buffer);
  struct thread *cur = thread_current ();
  int result = -1;

  lock_acquire (&filesys_lock);
  if (fd == 1 && size <= 512) {
    putbuf(buffer, size);
    result = size;
  } else if (fd > 1 && fd < 128) {
    struct file *fp = cur->fd[fd];
    if (fp) {
      buffer_set_pin ((void *)buffer, size, true);
      result = file_write (fp, buffer, size);
      buffer_set_pin ((void *)buffer, size, false);
    }
  }
  lock_release (&filesys_lock);
  return result;
}

void seek (int fd, unsigned position) {
  lock_acquire (&filesys_lock);
  struct thread *cur = thread_current ();
  if (fd > 1 && fd < 128) {
    struct file *fp = cur->fd[fd];
    if (fp)
      file_seek (fp, position);
  }
  lock_release (&filesys_lock);
}

unsigned tell (int fd) {
  lock_acquire (&filesys_lock);
  off_t pos = file_tell (thread_current ()->fd[fd]);
  lock_release (&filesys_lock);
  return pos;
}

void close (int fd) {
  lock_acquire (&filesys_lock);
  struct thread *cur = thread_current ();
  if (fd > 1 && fd < 128) {
    struct file *fp = cur->fd[fd];
    cur->fd[fd] = NULL;
    if (fp)
      file_close (fp);
  }
  lock_release (&filesys_lock);
}

mapid_t mmap (int fd, void *addr) {
  if (fd == 0 || fd == 1) {
    exit (-1);
  }
  if (addr != pg_round_down (addr) || addr == NULL || addr == 0) {
    return -1;
  }

  lock_acquire (&filesys_lock);

  struct thread *cur = thread_current ();
  struct file *fp = file_reopen (cur->fd[fd]);
  struct hash *spt = &cur->spt;
  struct list *map_list = &cur->map_list;
  uint32_t size = file_length (fp);
  for (int i = 0; i < (ROUND_UP (size, PGSIZE) / PGSIZE); i++) {
    struct spte *overlap = spt_find (spt, addr);
    if (overlap != NULL){
      lock_release (&filesys_lock);
      return -1;
    }
  }
  if (size <= 0) {
    exit (-1);
  }
  struct mape *mape = (struct mape *) malloc (sizeof(struct mape));
  mape->fp = fp;
  mape->addr = addr;
  mape->size = size;
  mapid_t mapid = (mapid_t) bitmap_scan_and_flip (mmap_index, 0, 1, false);
  mape->mapid = mapid;
  list_push_back (map_list, &mape->list_elem);
  off_t ofs = 0;
  uint32_t read_bytes = size;
  uint32_t zero_bytes = PGSIZE - (size - ((size/PGSIZE) * PGSIZE));
  while (read_bytes > 0 || zero_bytes > 0) 
    {
      /* Calculate how to fill this page.
         We will read PAGE_READ_BYTES bytes from FILE
         and zero the final PAGE_ZERO_BYTES bytes. */
      size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
      size_t page_zero_bytes = PGSIZE - page_read_bytes;
    
      struct spte *spte = (struct spte *) malloc (sizeof(struct spte));
      spte->vaddr = addr;
      spte->paddr = NULL;
      spte->writable = true;
      spte->dirty = false;
      spte->ofs = ofs;
      spte->read_bytes = page_read_bytes;
      spte->zero_bytes = page_zero_bytes;
      if (page_zero_bytes == PGSIZE) {
        spte->fp = NULL;
        spte->status = ZERO;
      }
      else {
        spte->fp = fp;
        spte->status = ON_DISK;
      }
      hash_insert (spt, &spte->hash_elem);
      /* Advance. */
      read_bytes -= page_read_bytes;
      zero_bytes -= page_zero_bytes;
      addr += PGSIZE;
      ofs += PGSIZE;
    }
  lock_release (&filesys_lock);
  return mapid;
}

void munmap (mapid_t mapping) {
  lock_acquire (&filesys_lock);
  
  struct thread *cur = thread_current ();
  struct hash *spt = &cur->spt;
  struct list *map_list = &cur->map_list;
  struct list_elem *e;
  struct mape *mape;
  for (e = list_begin (map_list); e != list_end (map_list); e = list_next (e)) {
    mape = list_entry (e, struct mape, list_elem);
    if (mape->mapid == mapping) {
      size_t index = bitmap_scan_and_flip (mmap_index, mapping, 1, true);
      if (index == BITMAP_ERROR) {
        PANIC ("munmap BITMAP ERROR");
      }
      int size = mape->size;
      void *vaddr = mape->addr;
      int free_count = (size%PGSIZE == 0) ? size/PGSIZE : size/PGSIZE+1;
      for (int i = 0; i < free_count; i++) {
        struct spte *spte = spt_find (spt, vaddr + (i*PGSIZE));
        if (spte == NULL) {
          PANIC ("munmap spte cannot be found\n");
        }
        void *pd = cur->pagedir;
        bool dirty = pagedir_is_dirty (pd, spte->vaddr) || pagedir_is_dirty (pd, spte->paddr) || spte->dirty;
        if (dirty) {
          if (spte->status == ON_SWAP) {
            swap_in (spt, spte->vaddr);
          }
          buffer_set_pin (spte->vaddr, spte->read_bytes, true);
          file_write_at (spte->fp, spte->vaddr, spte->read_bytes, spte->ofs);
          buffer_set_pin (spte->vaddr, spte->read_bytes, false);
        }
        pagedir_clear_page (pd, spte->vaddr);
        hash_delete (spt, &spte->hash_elem);
        if (spte->paddr != NULL) {
          fte_remove (spte->paddr);
        }
        free (spte);
      }
      list_remove (&mape->list_elem);
      file_close (mape->fp);
      free (mape);
      lock_release (&filesys_lock);
      return;
    }
  }
  PANIC ("Unreachable point reached\n");
  file_close (mape->fp);
  lock_release (&filesys_lock);

}