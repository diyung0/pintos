#include "vm/mmap.h"
#include "threads/thread.h"
#include "threads/malloc.h"
#include "threads/vaddr.h"
#include "filesys/file.h"
#include "userprog/pagedir.h"
#include "vm/frame.h"
#include "vm/page.h"
#include "vm/swap.h"
#include "vm/stack.h"
#include <string.h>
#include "userprog/syscall.h"

extern struct lock filesys_lock;

static struct mmap_entry *mmap_find_entry(struct thread *t, mapid_t mapping);
static void mmap_cleanup_on_fail(size_t count, void *addr, struct file *file);

mapid_t mmap_insert(struct file *file_reopen, int fd, void *addr, off_t file_length, bool writable) {
  struct thread *cur = thread_current();
  size_t page_count = (file_length + PGSIZE - 1) / PGSIZE;

  if (pg_ofs(addr) != 0) {
    lock_acquire(&filesys_lock);
    file_close(file_reopen);
    lock_release(&filesys_lock);
    return -1;
  }

  if (!is_user_vaddr(addr) || addr == 0) {
    lock_acquire(&filesys_lock);
    file_close(file_reopen);
    lock_release(&filesys_lock);
    return -1;
  }
  
  for (size_t i = 0; i < page_count; i++) {
    void *upage = addr + (i * PGSIZE);

    if (spt_find(&cur->spt, upage) != NULL) {
      lock_acquire(&filesys_lock);
      file_close(file_reopen);
      lock_release(&filesys_lock);
      return -1;
    }
    
    if (pagedir_get_page(cur->pagedir, upage) != NULL) {
        lock_acquire(&filesys_lock);
        file_close(file_reopen);
        lock_release(&filesys_lock);
        return -1;
    }

    if (upage >= (void *)(PHYS_BASE - STACK_MAX_SIZE) && upage < PHYS_BASE) {
        lock_acquire(&filesys_lock);
        file_close(file_reopen);
        lock_release(&filesys_lock);
        return -1;
    }
  }
  
  struct mmap_entry *me = malloc(sizeof(struct mmap_entry));
  if (me == NULL) {
    lock_acquire(&filesys_lock);
    file_close(file_reopen);
    lock_release(&filesys_lock);
    return -1;
  }
  
  me->mapid = cur->next_mapid++;
  me->fd = fd;
  me->file = file_reopen;
  me->addr = addr;
  me->length = file_length;
  me->page_count = page_count;
  
  // 각 페이지에 대한 PTE 생성
  for (size_t i = 0; i < page_count; i++) {
    void *upage = addr + (i * PGSIZE);
    off_t offset = i * PGSIZE;
    size_t read_bytes = (offset + PGSIZE < file_length) ? PGSIZE : (file_length - offset);
    size_t zero_bytes = PGSIZE - read_bytes;
    
    struct page_table_entry *pte = malloc(sizeof(struct page_table_entry));
    if (pte == NULL) {
      mmap_cleanup_on_fail(i, addr, file_reopen);
      free(me);
      return -1;
    }
    
    memset(pte, 0, sizeof(*pte));
    pte->upage = upage;
    pte->kpage = NULL;
    pte->is_loaded = false;
    pte->type = PAGE_MMAP;
    pte->original_type = PAGE_MMAP;
    pte->file = file_reopen;
    pte->file_offset = offset;
    pte->read_bytes = read_bytes;
    pte->zero_bytes = zero_bytes;
    pte->writable = writable;
    pte->mapid = me->mapid;
    pte->swap_slot = 0;
   
    if (!spt_insert(&cur->spt, pte)) {
      free(pte);
      mmap_cleanup_on_fail(i, addr, file_reopen);
      free(me);
      return -1;
    }
  }
  
  list_push_back(&cur->mmap_list, &me->elem);
  return me->mapid;
}

void mmap_munmap(struct thread *t, mapid_t mapping) {
  struct mmap_entry *me = mmap_find_entry(t, mapping);
  
  if (me == NULL)
    return;
  
  // 각 페이지 정리
  for (size_t i = 0; i < me->page_count; i++) {
    void *upage = me->addr + (i * PGSIZE);
    struct page_table_entry *pte = spt_find(&t->spt, upage);
    
    if (pte == NULL)
      continue;
    
    // dirty 페이지를 파일에 write back
    if (pte->is_loaded && pte->kpage != NULL) {
      if (pagedir_is_dirty(t->pagedir, upage)) {
        lock_acquire(&filesys_lock);
        file_write_at(pte->file, pte->kpage, pte->read_bytes, pte->file_offset);
        lock_release(&filesys_lock);
      }
      pagedir_clear_page(t->pagedir, upage);
      free_frame(pte->kpage);
    }
    
    hash_delete(&t->spt, &pte->elem);
    
    free(pte);
  }
  
  lock_acquire(&filesys_lock);
  file_close(me->file);
  lock_release(&filesys_lock);
  
  list_remove(&me->elem);
  free(me);
}

void mmap_unmap_all(struct thread *t) {
  while (!list_empty(&t->mmap_list)) {
    struct list_elem *e = list_front(&t->mmap_list);
    struct mmap_entry *me = list_entry(e, struct mmap_entry, elem);
    mmap_munmap(t, me->mapid);
  }
}

static struct mmap_entry *mmap_find_entry(struct thread *t, mapid_t mapping) {
  struct list_elem *e;
  for (e = list_begin(&t->mmap_list); e != list_end(&t->mmap_list); e = list_next(e)) {
    struct mmap_entry *temp = list_entry(e, struct mmap_entry, elem);
    if (temp->mapid == mapping) {
      return temp;
    }
  }
  return NULL;
}

static void mmap_cleanup_on_fail(size_t count, void *addr, struct file *file) {
  struct thread *cur = thread_current();
  
  for (size_t i = 0; i < count; i++) {
    void *upage = addr + (i * PGSIZE);
    struct page_table_entry *pte = spt_find(&cur->spt, upage);
    
    if (pte != NULL) {
      spt_remove(&cur->spt, pte);
    }
  }
  
  lock_acquire(&filesys_lock);
  file_close(file);
  lock_release(&filesys_lock);
}

void mmap_write_back(struct page_table_entry *pte) {
  if (pte == NULL || pte->type != PAGE_MMAP) {
    return;
  }
  
  if (pte->is_loaded && pte->kpage != NULL && pte->file != NULL) {
    file_write_at(pte->file, pte->kpage, pte->read_bytes, pte->file_offset);
  }
}

bool check_mmap_overlap(void *addr, off_t length) {
  struct thread *t = thread_current();
  void *end_addr = addr + length;

  for (void *page_addr = addr; page_addr < end_addr; page_addr += PGSIZE) {
    if (spt_find(&t->spt, page_addr) != NULL) {
      return true;
    }
    
    if (pagedir_get_page(t->pagedir, page_addr) != NULL) {
      return true;
    }
    
    if (page_addr >= (void *)(PHYS_BASE - STACK_MAX_SIZE) && page_addr < PHYS_BASE) {
      return true; 
    }
  }
  
  return false;
}