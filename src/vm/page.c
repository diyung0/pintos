#include <stdlib.h>
#include <string.h>
#include "threads/malloc.h"
#include "threads/thread.h"
#include "filesys/file.h"
#include "userprog/pagedir.h"
#include "vm/page.h"
#include "vm/frame.h"
#include "vm/swap.h"
#include "vm/mmap.h"
#include "userprog/syscall.h"

static void cleanup_pte_resources(struct page_table_entry *pte);
static void spt_destroy_func(struct hash_elem *e, void *aux UNUSED);

void spt_init(struct hash *spt) {
  hash_init(spt, page_hash, page_less, NULL);
}

unsigned page_hash(const struct hash_elem *e, void *aux UNUSED) {
  const struct page_table_entry *pte = hash_entry(e, struct page_table_entry, elem);
  return hash_bytes(&pte->upage, sizeof(pte->upage));
}

bool page_less(const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED) {
  const struct page_table_entry *pte_a = hash_entry(a, struct page_table_entry, elem);
  const struct page_table_entry *pte_b = hash_entry(b, struct page_table_entry, elem);
  return pte_a->upage < pte_b->upage;
}

bool spt_insert(struct hash *spt, struct page_table_entry *pte) {
  struct hash_elem *old_elem = hash_insert(spt, &pte->elem);
  return old_elem == NULL;
}

struct page_table_entry *spt_create_page(struct hash *spt, void *upage) {
  struct page_table_entry *pte = malloc(sizeof(struct page_table_entry));
  if (pte == NULL) {
    return NULL;
  }

  memset(pte, 0, sizeof(*pte));
  pte->upage = pg_round_down(upage);
  pte->kpage = NULL;
  pte->is_loaded = false;
  pte->type = PAGE_STACK;
  pte->original_type = PAGE_STACK;
  pte->writable = true;
  pte->file = NULL;
  pte->file_offset = 0;
  pte->read_bytes = 0;
  pte->zero_bytes = 0;
  pte->swap_slot = 0;
  pte->mapid = -1;
  
  if(!spt_insert(spt, pte)) {
    free(pte);
    return NULL;
  }
  return pte;
}

struct page_table_entry *spt_find(struct hash *spt, void *upage) {
  struct page_table_entry pte_temp;
  struct hash_elem *e;
  
  pte_temp.upage = pg_round_down(upage);
  e = hash_find(spt, &pte_temp.elem);
    
  return e != NULL ? hash_entry(e, struct page_table_entry, elem) : NULL;
}

static void cleanup_pte_resources(struct page_table_entry *pte) {
  if (pte == NULL) {
    return;
  }

  switch (pte->type) {
    case PAGE_BINARY:
      if (pte->file != NULL) {
        lock_acquire(&filesys_lock);
        file_close(pte->file);
        lock_release(&filesys_lock);
        pte->file = NULL;
      }
      break;
      
    case PAGE_SWAP:
      if (pte->swap_slot != 0) {
        swap_free(pte->swap_slot);
        pte->swap_slot = 0;
      }
      break;
      
    case PAGE_MMAP:
      break;

    case PAGE_STACK:
      break;
      
    default:
      break;
  }
}

void spt_remove(struct hash *spt, struct page_table_entry *pte) {
  if (pte == NULL) {
    return;
  }
  
  hash_delete(spt, &pte->elem);
  
  struct thread *t = thread_current();
  if (t->pagedir != NULL && pte->upage != NULL) {
    pagedir_clear_page(t->pagedir, pte->upage);
  }
  
  cleanup_pte_resources(pte);
  
  free(pte);
}

void spt_remove_page(struct hash *spt, void *upage) {
  struct page_table_entry *pte = spt_find(spt, upage);
  if (pte != NULL) {
    spt_remove(spt, pte);
  }
}

static void spt_destroy_func(struct hash_elem *e, void *aux UNUSED) {
  struct page_table_entry *pte = hash_entry(e, struct page_table_entry, elem);
  cleanup_pte_resources(pte);
  free(pte);
}

void spt_destroy(struct hash *spt) {
  hash_destroy(spt, spt_destroy_func);
}
