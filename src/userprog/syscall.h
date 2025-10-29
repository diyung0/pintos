#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include "threads/thread.h"

void syscall_init (void);

void check_valid_uaddr (const void *uaddr);

void halt (void);
void exit (int status);
tid_t exec (const char *cmd_line);
int wait (tid_t pid);
int read (int fd, void *buffer, unsigned size);
int write (int fd, const void *buffer, unsigned size);

int fibonacci (int n);
int max_of_four_int (int a, int b, int c, int d);

bool create (const char *file, unsigned initial_size);
bool remove (const char *file);
int open (const char *file);
void close (int fd);
int filesize (int fd);
void seek (int fd, unsigned position);
unsigned tell (int fd);

#endif /* userprog/syscall.h */
