#ifndef VM_FRMAE_H
#define VM_FRAME_H

#include <list.h>
#include "threads/thread.h"
#include "threads/palloc.h"

struct frame_table_entry {
  void *frame;
  void *upage;
  struct thread *owner;
  bool pinned;

  struct list_elem elem;
};

void frame_init(void);
void *get_frame(enum palloc_flags flags, void *upage);
void free_frame(void *frame);
void frame_clear_owner(struct thread *t);

#endif