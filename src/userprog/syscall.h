#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H
#include "threads/interrupt.h"

typedef int pid_t;

void syscall_init (void);

int get_ith_arg (struct intr_frame *f, int i);

void check_valid (const void *add);

void halt (void);
void exit (int status);
pid_t exec (const char *cmd_line);
int write (int fd, const void *buffer, unsigned size);
int wait (pid_t pid);


#endif /* userprog/syscall.h */
