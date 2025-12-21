#include "vm/frame.h"
#include <list.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include "threads/thread.h"
#include "threads/synch.h"
#include "threads/palloc.h"
#include "threads/malloc.h"
#include "userprog/pagedir.h"
#include "vm/page.h"
#include "vm/swap.h"
#include "filesys/file.h"
#include "userprog/syscall.h"

static struct list frame_table;
static struct list_elem *clock_hand;
static struct lock frame_lock;

static void *evict_page(void);
static void *handle_eviction(struct frame_table_entry *victim);
static void cleanup_invalid_frames(void);
static void update_clock_hand_if_needed(struct list_elem *removed_elem);

void frame_init (void) {
  list_init(&frame_table);
  clock_hand = NULL;
  lock_init(&frame_lock);
}

void *get_frame (enum palloc_flags flags, void *upage) {
  void *frame = palloc_get_page(PAL_USER | flags);
  if (frame == NULL) {
    lock_acquire(&frame_lock); 
    frame = evict_page();
    lock_release(&frame_lock); 
    if (frame == NULL) {
      return NULL;
    }
  }

  struct frame_table_entry *fte = malloc(sizeof(struct frame_table_entry));
  if (fte == NULL) {
    palloc_free_page(frame);
    return NULL;
  }

  fte->frame = frame;
  fte->upage = upage;
  fte->owner = thread_current();
  fte->pinned = false;

  lock_acquire(&frame_lock);
  list_push_back(&frame_table, &fte->elem);
  lock_release(&frame_lock);

  return frame;
}

static void cleanup_invalid_frames(void) {
  struct list_elem *e = list_begin(&frame_table);
  
  while (e != list_end(&frame_table)) {
    struct frame_table_entry *fte = list_entry(e, struct frame_table_entry, elem);
    struct list_elem *next = list_next(e);
    
    if (fte->owner == NULL || fte->owner->pagedir == NULL) {
      update_clock_hand_if_needed(e);
      list_remove(e);
      palloc_free_page(fte->frame);
      free(fte);
    }
    
    e = next;
  }
}

static void *evict_page (void) {
  struct frame_table_entry *victim = NULL;
  
  if (list_empty(&frame_table)) {
    return NULL;
  }
  
  cleanup_invalid_frames();
  
  if (list_empty(&frame_table)) {
    return NULL;
  }
  
  if (clock_hand == NULL || clock_hand == list_end(&frame_table))
    clock_hand = list_begin(&frame_table);

  while (true) {
    struct frame_table_entry *fte = list_entry(clock_hand, struct frame_table_entry, elem);

    if (fte->owner == NULL || fte->owner->pagedir == NULL) {
      clock_hand = list_next(clock_hand);
      if (clock_hand == list_end(&frame_table))
        clock_hand = list_begin(&frame_table);
      continue;
    }

    if (fte->pinned) {
      clock_hand = list_next(clock_hand);
      if (clock_hand == list_end(&frame_table))
        clock_hand = list_begin(&frame_table);
      continue;
    }

    struct page_table_entry *pte = spt_find(&fte->owner->spt, fte->upage);
    
    if (pte == NULL || !pte->is_loaded) {
      victim = fte;
      clock_hand = list_next(clock_hand);
      if (clock_hand == list_end(&frame_table))
        clock_hand = list_begin(&frame_table);
      
      list_remove(&victim->elem);
      void *frame = victim->frame;
      pagedir_clear_page(victim->owner->pagedir, victim->upage);
      free(victim);
      return frame;
    }

    // Second chance algorithm
    if (pagedir_is_accessed(fte->owner->pagedir, fte->upage)) {
      pagedir_set_accessed(fte->owner->pagedir, fte->upage, false);
      clock_hand = list_next(clock_hand);
      if (clock_hand == list_end(&frame_table)) {
        clock_hand = list_begin(&frame_table);
      }
    } else {
      victim = fte;
      clock_hand = list_next(clock_hand);
      if (clock_hand == list_end(&frame_table)) {
        clock_hand = list_begin(&frame_table);
      }
      break;
    }
  }
  
  void *result = handle_eviction(victim);
  return result;
}

static void *handle_eviction(struct frame_table_entry *victim) {
  struct page_table_entry *pte;
  void *frame = victim->frame;
  void *upage = victim->upage;
  struct thread *owner = victim->owner;
  
  // frame_table에서 제거
  list_remove(&victim->elem);
  
  // PTE 찾기
  pte = spt_find(&owner->spt, upage);
  if (pte == NULL) {
    pagedir_clear_page(owner->pagedir, upage);
    free(victim);
    return frame;
  }

  if (pte->type == PAGE_SWAP) {
    pagedir_clear_page(owner->pagedir, upage);
    pte->is_loaded = false;
    pte->kpage = NULL;
    free(victim);
    return frame;
  }

  if (!pte->is_loaded) {
    pagedir_clear_page(owner->pagedir, upage);
    pte->is_loaded = false;
    pte->kpage = NULL;
    free(victim);
    return frame;
  }

  bool dirty = pagedir_is_dirty(owner->pagedir, upage);
  
  pte->is_loaded = false;
  
  size_t swap_slot = 0;
  bool need_swap = false;

  switch (pte->type) {
    case PAGE_BINARY:
      if (pte->writable || dirty) {
        swap_slot = swap_out(frame);        
        if (swap_slot == BITMAP_ERROR) {
          pte->is_loaded = true;
          list_push_back(&frame_table, &victim->elem);
          return NULL;
        }
        need_swap = true;
      } else {

      }
      break;
    
    case PAGE_STACK:
      {
        swap_slot = swap_out(frame);
        if (swap_slot == BITMAP_ERROR) {
          pte->is_loaded = true;
          list_push_back(&frame_table, &victim->elem);
          return NULL;
        }
        need_swap = true;
      }
      break;
      
    case PAGE_MMAP:
      if (dirty) {
        lock_acquire(&filesys_lock);
        file_write_at(pte->file, frame, pte->read_bytes, pte->file_offset);
        lock_release(&filesys_lock);
      }
      break;

    default:
      pagedir_clear_page(owner->pagedir, upage);
      pte->kpage = NULL;
      free(victim);
      return frame;
  }
  
  // page directory에서 매핑 제거
  pagedir_clear_page(owner->pagedir, upage);
  pte->kpage = NULL;
  
  if (need_swap) {
    pte->swap_slot = swap_slot;
    pte->original_type = pte->type;
    pte->type = PAGE_SWAP;
  }
  
  free(victim);
  
  return frame;
}

static void update_clock_hand_if_needed(struct list_elem *removed_elem) {
  if (clock_hand == removed_elem) {
    clock_hand = list_next(clock_hand);
    if (clock_hand == list_end(&frame_table)) {
      clock_hand = list_begin(&frame_table);
    }
  }
}

void free_frame (void *frame) {
  if (frame == NULL) {
    return;
  }

  lock_acquire(&frame_lock);

  struct list_elem *e;
  bool found = false;

  for (e = list_begin(&frame_table); e != list_end(&frame_table); e = list_next(e)) {
    struct frame_table_entry *fte = list_entry(e, struct frame_table_entry, elem);
    if (fte->frame == frame) {
      update_clock_hand_if_needed(e);
      
      list_remove(e);
      found = true;
      free(fte);
      break;
    }
  }

  lock_release(&frame_lock);

  if (found) {
    palloc_free_page(frame);
  }
}

void frame_clear_owner(struct thread *t) {
  lock_acquire(&frame_lock); 

  struct list_elem *e = list_begin(&frame_table);
  
  while (e != list_end(&frame_table)) {
    struct frame_table_entry *fte = list_entry(e, struct frame_table_entry, elem);
    struct list_elem *next = list_next(e);
    
    if (fte->owner == t) {
      if (is_kernel_vaddr(fte->upage)) {
        e = next;
        continue;
      }
      
      void *frame = fte->frame;
      void *upage = fte->upage;
      
      if (t->pagedir != NULL) {
        pagedir_clear_page(t->pagedir, upage);
      }
      
      update_clock_hand_if_needed(e);
      
      list_remove(e);
      free(fte);

      palloc_free_page(frame);
    }
    
    e = next;
  }
  
  lock_release(&frame_lock);  
}