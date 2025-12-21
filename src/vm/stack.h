#ifndef VM_STACK_H
#define VM_STACK_H

#include <stdbool.h>

#define STACK_MAX_SIZE (8 * 1024 * 1024) // 8MB

bool is_valid_stack_access(void *fault_addr, void *esp);
bool grow_stack(void *upage);

#endif /* vm/stack.h */