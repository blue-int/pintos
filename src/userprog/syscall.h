#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H
#include <stdio.h>
#include <user/syscall.h>
#include "vm/frame.h"
#include "filesys/directory.h"
#include "filesys/inode.h"

void syscall_init (void);
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
mapid_t mmap (int fd, void *addr);
void munmap (mapid_t);
bool chdir (const char *dir);
bool mkdir (const char *dir);
bool readdir (int fd, char *name);
bool isdir (int fd);
int inumber (int fd);
void parse_path (const char *dir, char *file_name, struct dir **dir_ptr);

#endif /* userprog/syscall.h */
