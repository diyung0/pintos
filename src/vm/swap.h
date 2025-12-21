#ifndef VM_SWAP_H
#define VM_SWAP_H

#include <stddef.h>
#include <bitmap.h>
#include "threads/synch.h"
#include "devices/block.h"

struct swap_table {
  struct block *swap_block;
  struct bitmap *swap_bitmap;
  struct lock swap_lock;
  size_t swap_size;
};

void swap_init(void);
size_t swap_out(void *frame);
void swap_in(size_t slot, void *frame);
void swap_free(size_t slot);

#endif /* vm/swap.h */