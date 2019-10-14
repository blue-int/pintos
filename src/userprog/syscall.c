#include "userprog/syscall.h"
#include "userprog/pagedir.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "devices/shutdown.h"
#include "devices/input.h"

static void syscall_handler (struct intr_frame *);
void check_valid_addr (const void *vaddr);
void exit (int status);
int read (int fd, void *buffer, unsigned size);
int write (int fd, const void *buffer, unsigned size);
void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f) 
{
  check_valid_addr ((uint32_t *)(f->esp));
  int sysnum = *(int *)f->esp;
  // printf ("system call %d!\n", sysnum);

  switch (sysnum)
  {
  case SYS_HALT: // 0
    shutdown_power_off ();
    break;
  
  case SYS_EXIT: // 1
    check_valid_addr ((uint32_t *)(f->esp + 4));
    exit (*(int *)(f->esp + 4));
    break;
  
  case SYS_EXEC: // 1
    check_valid_addr ((uint32_t *)(f->esp + 4));
    break;
  
  case SYS_WAIT: // 1
    check_valid_addr ((uint32_t *)(f->esp + 4));
    break;
  
  case SYS_CREATE: // 2
    check_valid_addr ((uint32_t *)(f->esp + 8));
    break;
  
  case SYS_REMOVE: // 1
    check_valid_addr ((uint32_t *)(f->esp + 4));
    break;

  case SYS_OPEN: // 1
    check_valid_addr ((uint32_t *)(f->esp + 4));
    break;

  case SYS_FILESIZE: // 1
    check_valid_addr ((uint32_t *)(f->esp + 4));
    break;

  case SYS_READ: // 3
    check_valid_addr ((uint32_t *)(f->esp + 12));
    f->eax = read ((int)*(uint32_t *)(f->esp + 4), (void *)*(uint32_t *)(f->esp + 8), (unsigned)*(uint32_t *)(f->esp + 12));
    break;

  case SYS_WRITE: // 3
    check_valid_addr ((uint32_t *)(f->esp + 12));
    f->eax = write ((int)*(uint32_t *)(f->esp + 4), (void *)*(uint32_t *)(f->esp + 8), (unsigned)*(uint32_t *)(f->esp + 12));
    break;

  case SYS_SEEK: // 2
    check_valid_addr ((uint32_t *)(f->esp + 8));
    break;

  case SYS_TELL: // 1
    check_valid_addr ((uint32_t *)(f->esp + 4));
    break;

  case SYS_CLOSE: // 1
    check_valid_addr ((uint32_t *)(f->esp + 4));
    break;
  
  default:
    break;
  }

  // thread_exit ();
}

void check_valid_addr (const void *vaddr) {
  if (is_user_vaddr (vaddr) == false || !pagedir_get_page (thread_current ()->pagedir, vaddr))
    exit (-1);
}

void exit (int status) {
  printf ("%s: exit(%d)\n", thread_name (), status);
  thread_exit ();
}

int read (int fd, void *buffer UNUSED, unsigned size) {
  if (fd == 0) {
    // size should be the actual size read
    input_getc ();
    return size;
  }
  else return -1;
}

int write (int fd, const void *buffer, unsigned size) {
  if (fd == 1 && size <= 512) {
    // size should be the actual size written
    putbuf(buffer, size);
    return size;
  } else return -1;
}

