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
#include "filesys/file.h"
#include "filesys/filesys.h"

static void syscall_handler (struct intr_frame *);
static int allocate_fd (struct file *file);
static struct file *get_file (int fd);

// 파일 디스크립터 범위 상수
#define FD_MIN 2
#define FD_MAX 128
#define STDIN 0
#define STDOUT 1

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

static int allocate_fd (struct file *file) {
  struct thread *t = thread_current();
  if(t->next_fd >= FD_MAX) {
    return -1;
  }
  int fd = t->next_fd;
  t->fd_table[fd] = file;
  t->next_fd++;
  return fd;
}

static struct file *get_file (int fd) {
  struct thread *t = thread_current();
  if (fd < FD_MIN || fd >= FD_MAX) {
    return NULL;
  }
  return t->fd_table[fd];
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

    case SYS_FIBONACCI:
      check_valid_uaddr (esp + 1);
      f->eax = fibonacci(*(esp + 1));
      break;

    case SYS_MAX_OF_FOUR_INT:
      check_valid_uaddr (esp + 1);
      check_valid_uaddr (esp + 2);
      check_valid_uaddr (esp + 3);
      check_valid_uaddr (esp + 4);
      f->eax = max_of_four_int(*(esp + 1), *(esp + 2), *(esp + 3), *(esp + 4));
      break;

    case SYS_CREATE:
      check_valid_uaddr (esp + 1);
      check_valid_uaddr (esp + 2);
      check_valid_uaddr ((void *) *(esp + 1));
      f->eax = create ((const char *) *(esp + 1), *(esp + 2));
      break;

    case SYS_REMOVE:
      check_valid_uaddr (esp + 1);
      check_valid_uaddr ((void *) *(esp + 1));
      f->eax = remove ((const char *) *(esp + 1));
      break;

    case SYS_OPEN:
      check_valid_uaddr (esp + 1);
      check_valid_uaddr ((void *) *(esp + 1));
      f->eax = open ((const char *) *(esp + 1));
      break;

    case SYS_CLOSE:
      check_valid_uaddr (esp + 1);
      close (*(esp + 1));
      break;

    case SYS_FILESIZE:
      check_valid_uaddr (esp + 1);
      f->eax = filesize (*(esp + 1));
      break;

    case SYS_SEEK:
      check_valid_uaddr (esp + 1);
      check_valid_uaddr (esp + 2);
      seek (*(esp + 1), *(esp + 2));
      break;

    case SYS_TELL:
      check_valid_uaddr (esp + 1);
      f->eax = tell (*(esp + 1));
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
  if (fd == STDIN) {
    uint8_t *buf = (uint8_t *) buffer;
    for (unsigned i = 0; i < size; i++) {
      buf[i] = input_getc ();
    }
    return size;
  }
  if (fd >= 2) {
    struct file *f = get_file(fd);
    if (f == NULL) {
      exit(-1);
    }
    int result = file_read (f, buffer, size);
    return result;
  }
  return -1;
}

int write (int fd, const void *buffer, unsigned size) {
  if(fd == STDOUT) {
    putbuf(buffer, size);
    return size;
  }
  if (fd >= 2) {
    struct file *f = get_file(fd);
    if (f == NULL) {
      exit(-1);
    }
    int result = file_write (f, buffer, size);
    return result;
  }
  return -1;
}

int fibonacci (int n) {
  if (n <= 1) return n;
  int prev = 0, cur = 1;
  for(int i = 2; i<= n; i++) {
    int next = prev + cur;
    prev = cur;
    cur = next;
  }
  return cur;
}

int max_of_four_int (int a, int b, int c, int d) {
  int ans = a;
  if(b > ans) ans = b;
  if(c > ans) ans = c;
  if(d > ans) ans = d;
  return ans;
}

bool create (const char *file, unsigned initial_size) {
  bool result = filesys_create (file, initial_size);
  return result;
}

bool remove (const char *file) {
  bool result = filesys_remove (file);
  return result;
}

int open (const char *file) {
  struct file *f = filesys_open (file);
  if (f == NULL) {
    return -1;
  }
  int fd = allocate_fd (f);
  return fd;
}

void close (int fd) {
  struct file *f = get_file (fd);
  if (f == NULL) {
    exit(-1);
  }
  file_close (f);
  thread_current()->fd_table[fd] = NULL;
}

int filesize (int fd) {
  struct file *f = get_file (fd);
  if (f == NULL) {
    exit(-1);
  }
  int result = file_length (f);
  return result;
}

void seek (int fd, unsigned position) {
  struct file *f = get_file (fd);
  if (f == NULL) {
    exit(-1);
  }
  file_seek (f, position);
}

unsigned tell (int fd) {
  struct file *f = get_file (fd);
  if (f == NULL) {
    lock_release(&filesys_lock);
    exit(-1);
  }
  unsigned result = file_tell (f);
  return result;
}