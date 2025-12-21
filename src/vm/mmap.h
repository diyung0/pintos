#ifndef VM_MMAP_H
#define VM_MMAP_H

#include <list.h>
#include <stddef.h>
#include "threads/thread.h"
#include "filesys/file.h"
#include "vm/page.h"

struct mmap_entry {
  mapid_t mapid;
  int fd;
  struct file *file;
  void *addr;
  size_t length;
  size_t page_count;
  struct list_elem elem;
};

mapid_t mmap_insert(struct file *file_reopen, int fd, void *addr, off_t length, bool writable);
void mmap_munmap(struct thread *t, mapid_t mapping);
void mmap_unmap_all(struct thread *t);
void mmap_write_back(struct page_table_entry *pte);

bool check_mmap_overlap(void *addr, off_t length);

#endif /* vm/mmap.h */