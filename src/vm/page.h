#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <hash.h>
#include <stdlib.h>
#include "filesys/filesys.h"
#include "threads/vaddr.h"

typedef int mapid_t;

enum page_type {
  PAGE_BINARY,
  PAGE_SWAP,
  PAGE_MMAP,
  PAGE_STACK
};

struct page_table_entry {
  void *upage;
  void *kpage;

  bool is_loaded;

  enum page_type type;
  enum page_type original_type;

  struct hash_elem elem;

  struct file *file;
  off_t file_offset;
  uint32_t read_bytes;
  uint32_t zero_bytes;
  bool writable;

  size_t swap_slot;

  mapid_t mapid;
};


void spt_init(struct hash *spt);
bool spt_insert(struct hash *spt, struct page_table_entry *pte);
struct page_table_entry *spt_create_page(struct hash *spt, void *upage);
struct page_table_entry *spt_find(struct hash *spt, void *upage);
void spt_remove_page(struct hash *spt, void *upage);
void spt_remove(struct hash *spt, struct page_table_entry *pte);
void spt_destroy(struct hash *spt);

unsigned page_hash(const struct hash_elem *e, void *aux UNUSED);
bool page_less(const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED);

#endif /* vm/page.h */