#include "vm/stack.h"
#include "vm/page.h"
#include "vm/frame.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include <string.h>
#include <stdio.h>

bool is_valid_stack_access(void *fault_addr, void *esp) {
  // 1. 사용자 주소 공간인지 확인
  if (!is_user_vaddr(fault_addr)) {
    return false;
  }

  // 2. 최대 스택 크기 제한 확인 (8MB)
  if (PHYS_BASE - pg_round_down(fault_addr) > STACK_MAX_SIZE) {
    return false;
  }
  
  // 3. 스택 포인터 기준 유효성 검사 (PUSHA 명령어는 esp - 32까지 접근 가능)
  if ((char *)fault_addr >= (char *)esp - 32) {
    return true;
  }
  
  return false;
}

bool grow_stack(void *upage) {
  struct thread *t = thread_current();

  upage = pg_round_down(upage);

  if (spt_find(&t->spt, upage) != NULL) {
    return false;
  }

  struct page_table_entry *pte = spt_create_page(&t->spt, upage);
  if (pte == NULL) {
    return false;
  }

  pte->type = PAGE_STACK;
  pte->original_type = PAGE_STACK;
  pte->writable = true;

  void *frame = get_frame(PAL_USER | PAL_ZERO, upage);
  if (frame == NULL) {
    spt_remove_page(&t->spt, upage);
    return false;
  }
  
  if (!pagedir_set_page(t->pagedir, upage, frame, true)) {
    free_frame(frame);
    spt_remove_page(&t->spt, upage);
    return false;
  }
  
  pte->kpage = frame;
  pte->is_loaded = true;
  
  return true;
}