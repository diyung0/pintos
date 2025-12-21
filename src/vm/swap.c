#include "vm/swap.h"
#include <bitmap.h>
#include <stdio.h>
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "devices/block.h"
#include "threads/thread.h"

#define SECTORS_PER_PAGE (PGSIZE / BLOCK_SECTOR_SIZE)

static struct swap_table swap_table;

void swap_init(void) {
  swap_table.swap_block = block_get_role(BLOCK_SWAP);
  block_sector_t swap_sectors = block_size(swap_table.swap_block);
  swap_table.swap_size = swap_sectors / SECTORS_PER_PAGE;
  swap_table.swap_bitmap = bitmap_create(swap_table.swap_size);
  bitmap_set_all(swap_table.swap_bitmap, false);
  lock_init(&swap_table.swap_lock);
}

size_t swap_out(void *frame) {
  ASSERT(frame != NULL);
  ASSERT(pg_ofs(frame) == 0);
  
  if (swap_table.swap_bitmap == NULL || swap_table.swap_block == NULL) {
    return BITMAP_ERROR;
  }
  
  lock_acquire(&swap_table.swap_lock);
  
  // 빈 swap 슬롯 찾기, 0 대신 1부터 스캔 시작 (슬롯 0을 예약)
  size_t slot = bitmap_scan_and_flip(swap_table.swap_bitmap, 1, 1, false);

  block_sector_t sector = slot * SECTORS_PER_PAGE;
  // 페이지를 섹터 단위로 swap 디스크에 쓰기
  for (int i = 0; i < SECTORS_PER_PAGE; i++) {
    block_write(swap_table.swap_block, 
                sector + i,
                frame + (i * BLOCK_SECTOR_SIZE));
  }
  
  lock_release(&swap_table.swap_lock);
  return slot;
}

void swap_in(size_t slot, void *frame) {
  ASSERT(frame != NULL);
  ASSERT(pg_ofs(frame) == 0);
  ASSERT(slot < swap_table.swap_size);
  
  lock_acquire(&swap_table.swap_lock);
  
  // 슬롯이 사용 중인지 확인
  if (!bitmap_test(swap_table.swap_bitmap, slot)) {
    lock_release(&swap_table.swap_lock);
    return;
  }
  
  block_sector_t sector = slot * SECTORS_PER_PAGE;
  
  // swap 디스크에서 페이지 읽기
  for (int i = 0; i < SECTORS_PER_PAGE; i++) {
    block_read(swap_table.swap_block,
               sector + i, 
               frame + (i * BLOCK_SECTOR_SIZE));
  }
  
  bitmap_set(swap_table.swap_bitmap, slot, false);
  
  lock_release(&swap_table.swap_lock);
}

void swap_free(size_t slot) {
  ASSERT(slot < swap_table.swap_size);
  lock_acquire(&swap_table.swap_lock);
  if (!bitmap_test(swap_table.swap_bitmap, slot)) {
    lock_release(&swap_table.swap_lock);
    return;
  }
  bitmap_set(swap_table.swap_bitmap, slot, false);
  lock_release(&swap_table.swap_lock);
}
