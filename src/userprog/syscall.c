#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "devices/shutdown.h"
#include "userprog/process.h"
#include "devices/input.h"
#include "lib/kernel/console.h"

static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

void
check_valid_uaddr (const void *uaddr) 
{
  if (uaddr == NULL || 
      !is_user_vaddr (uaddr) ||
      pagedir_get_page (thread_current ()->pagedir, uaddr) == NULL) 
  {
    exit (-1);
  }
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  check_valid_uaddr (f->esp);

  uint32_t *esp = (uint32_t *) f->esp;
  uint32_t syscall_num = *esp;

  switch (syscall_num) {
    case SYS_HALT:
      halt ();
      break;

    case SYS_EXIT:
      check_valid_uaddr (esp + 1);
      exit (*(esp + 1));
      break;
    
    case SYS_EXEC:
      check_valid_uaddr (esp + 1);
      check_valid_uaddr ((void *) *(esp + 1));
      f->eax = exec ((const char*) *(esp + 1));
      break;

    case SYS_WAIT:
      check_valid_uaddr (esp + 1);
      f->eax = wait(*(esp + 1));
      break;

    case SYS_READ:
      check_valid_uaddr (esp + 1);
      check_valid_uaddr (esp + 2);
      check_valid_uaddr (esp + 3);
      {
        int fd = *(esp + 1);
        void *buffer = (void *) *(esp + 2);
        unsigned size = *(esp + 3);

        check_valid_uaddr (buffer);
        if (size > 0) check_valid_uaddr (buffer + size - 1);

        f->eax = read (fd, buffer, size);
      }
      break;

    case SYS_WRITE:
      check_valid_uaddr (esp + 1);
      check_valid_uaddr (esp + 2);
      check_valid_uaddr (esp + 3);
      {
        int fd = *(esp + 1);
        const void *buffer = (const void *) *(esp + 2);
        unsigned size = *(esp + 3);

        check_valid_uaddr (buffer);
        if (size > 0) check_valid_uaddr (buffer + size - 1);

        f->eax = write (fd, buffer, size);
      }
      break;
    
    default:
      exit (-1);
      break;
  }
}

void halt() {
  shutdown_power_off();
}

void exit(int status) {
  thread_current()->exit_status = status;
  thread_exit();
}

tid_t exec(const char *cmd_line) {
  return process_execute(cmd_line);
}

int wait(tid_t tid) {
  return process_wait(tid);
}

int read (int fd, void *buffer, unsigned size) {
  if (fd == 0) {
    uint8_t *buf = (uint8_t *) buffer;
    for (unsigned i = 0; i < size; i++) {
      buf[i] = input_getc ();
    }
    return size;
  }
  return -1;
}

int write (int fd, const void *buffer, unsigned size) {
  if(fd == 1) {
    putbuf(buffer, size);
    return size;
  }
  return -1;
}