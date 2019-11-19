#include "userprog/syscall.h"
#include "userprog/pagedir.h"
#include "userprog/process.h"
#include <stdio.h>
#include <syscall-nr.h>
#include <string.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "devices/shutdown.h"
#include "devices/input.h"
#include "vm/frame.h"
#include "filesys/filesys.h"
#include "filesys/file.h"

static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
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
  
  default:
    break;
  }
}

void check_valid_addr (const void *vaddr) {
  if (is_user_vaddr (vaddr) == false || !pagedir_get_page (thread_current ()->pagedir, vaddr))
    exit (-1);
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
  return process_execute (cmd_line);
}

int wait (pid_t pid) {
  pid_t child = process_wait (pid);
  return child;
}

bool create (const char *file, unsigned initial_size) {
  check_valid_addr (file);
  return filesys_create (file, initial_size);
}

bool remove (const char *file) {
  check_valid_addr (file);
  return filesys_remove (file);
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
  return file_length (thread_current ()->fd[fd]);
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
  }
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
  struct thread *cur = thread_current ();
  if (fd > 1 && fd < 128) {
    struct file *fp = cur->fd[fd];
    if (fp)
      file_seek (fp, position);
  }
}

unsigned tell (int fd) {
  return file_tell (thread_current ()->fd[fd]);
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
