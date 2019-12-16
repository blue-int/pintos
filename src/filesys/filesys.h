#ifndef FILESYS_FILESYS_H
#define FILESYS_FILESYS_H

#include <stdbool.h>
#include "filesys/off_t.h"
#include "filesys/cache.h"
#include "threads/synch.h"

/* Sectors of system file inodes. */
#define FREE_MAP_SECTOR 0       /* Free map file inode sector. */
#define ROOT_DIR_SECTOR 1       /* Root directory file inode sector. */

/* Block device that contains the file system. */
struct block *fs_device;
struct lock filesys_lock;

void filesys_init (bool format);
void filesys_done (void);
bool filesys_create (const char *name, off_t initial_size);
bool filesys_create_dir (const char *name, void *dir, off_t initial_size, bool is_dir);
struct file *filesys_open (const char *name);
struct file *filesys_open_dir (const char *name, void *dir_ptr);
bool filesys_remove (const char *name);
bool filesys_remove_dir (const char *name, void *dir_ptr);

#endif /* filesys/filesys.h */
