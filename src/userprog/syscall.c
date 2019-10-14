#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "devices/shutdown.h"
#include "devices/input.h"

static void syscall_handler (struct intr_frame *);
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
  int sysnum = *(int *)f->esp;
  // printf ("system call %d!\n", sysnum);

  switch (sysnum)
  {
  case SYS_HALT:
    shutdown_power_off ();
    break;
  
  case SYS_EXIT:
    printf ("%s: exit(%d)\n", thread_name (), *(int *)(f->esp + 4));
    thread_exit ();
    break;
  
  case SYS_EXEC: // 1
    break;
  
  case SYS_WAIT: // 1
    break;
  
  case SYS_CREATE: // 2
    break;
  
  case SYS_REMOVE: // 1
    break;

  case SYS_OPEN: // 1
    break;

  case SYS_FILESIZE: // 1
    break;

  case SYS_READ: // 3
    f->eax = read ((int)*(uint32_t *)(f->esp + 4), (void *)*(uint32_t *)(f->esp + 8), (unsigned)*(uint32_t *)(f->esp + 12));
    break;

  case SYS_WRITE: // 3
    f->eax = write ((int)*(uint32_t *)(f->esp + 4), (void *)*(uint32_t *)(f->esp + 8), (unsigned)*(uint32_t *)(f->esp + 12));
    break;

  case SYS_SEEK: // 2
    break;

  case SYS_TELL: // 1
    break;

  case SYS_CLOSE: // 1
    break;
  
  default:
    break;
  }

  // thread_exit ();
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

