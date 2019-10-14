#include "userprog/syscall.h"
#include "userprog/pagedir.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "devices/shutdown.h"
#include "devices/input.h"
#include "filesys/filesys.h"
#include "filesys/file.h"

static void syscall_handler (struct intr_frame *);
void check_valid_addr (const void *vaddr);

typedef int pid_t;
#define PID_ERROR ((pid_t) -1)

void halt (void);
void exit (int status);
pid_t exec (const char *cmd_line);
int wait (pid_t pid);
bool create (const char *file, unsigned initial_size);
bool remove (const char *file);
int open (const char *file);
int filesize (int fd);
int read (int fd, void *buffer, unsigned size);
int write (int fd, const void *buffer, unsigned size);
void seek (int fd, unsigned position);
unsigned tell (int fd);
void close (int fd);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f) 
{
  // hex_dump ((uintptr_t)f->esp, f->esp, 0xc0000000 - (uintptr_t)f->esp, true);
  uint32_t *esp = (uint32_t *)(f->esp);
  check_valid_addr (esp);
  int sysnum = *(int *)f->esp;
  uint32_t *arg0 = (uint32_t *)(f->esp + 4);
  uint32_t *arg1 = (uint32_t *)(f->esp + 8);
  uint32_t *arg2 = (uint32_t *)(f->esp + 12);
  // printf ("system call %d!\n", sysnum);

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
    break;
  
  case SYS_WAIT: // 1
    check_valid_addr (arg0);
    break;
  
  case SYS_CREATE: // 2
    check_valid_addr (arg1);
    f->eax = create ((const char *)*arg0, (unsigned)*arg1);
    break;
  
  case SYS_REMOVE: // 1
    check_valid_addr (arg0);
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
    break;

  case SYS_TELL: // 1
    check_valid_addr (arg0);
    break;

  case SYS_CLOSE: // 1
    check_valid_addr (arg0);
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

void halt (void) {
  shutdown_power_off ();
};

void exit (int status) {
  printf ("%s: exit(%d)\n", thread_name (), status);
  thread_exit ();
}

bool create (const char *file, unsigned initial_size) {
  check_valid_addr (file);
  return filesys_create (file, initial_size);
};

int open (const char *file) {
  check_valid_addr (file);
  struct file *fp = filesys_open (file);
  struct thread *cur = thread_current ();
  if (fp) {
    for (int i = 2; i < 128; i++) {
      if (!(cur->fd[i])) {
        cur->fd[i] = fp;
        return i;
      }
    }
  }
  return -1;
};

int filesize (int fd) {
  return file_length (thread_current ()->fd[fd]);
};

int read (int fd, void *buffer, unsigned size) {
  check_valid_addr (buffer);
  struct thread *cur = thread_current ();
  int len;
  if (fd == 0) {
    input_getc ();
    return size;
  } else if (fd > 1 && fd < 128) {
    struct file* fp = cur->fd[fd];
    if (fp) {
      len = file_length (fp);
      file_seek (fp ,0);
      len = file_read (fp, buffer, size);
      return len;
    }
  }
  return -1;
}

int write (int fd, const void *buffer, unsigned size) {
  struct thread *cur = thread_current ();
  if (fd == 1 && size <= 512) {
    putbuf(buffer, size);
    return size;
  } else if (fd > 2 && fd < 128)
    return file_write (cur->fd[fd], buffer, size);
  return -1;
}

