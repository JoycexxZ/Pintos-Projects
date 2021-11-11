#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H
#include "threads/interrupt.h"
#include "filesys/file.h"
#include <list.h>

typedef int pid_t;

struct thread_file
{
    struct file *f;
    struct list_elem f_listelem;
    int fd;
};

void syscall_init (void);

int get_ith_arg (struct intr_frame *f, int i);

void check_valid (const void *add);

void halt (void);
void exit (int status);
pid_t exec (const char *cmd_line);
int write (int fd, const void *buffer, unsigned size);
int wait (pid_t pid);
int open (const char *file);
void close (int fd);
int read (int fd, void *buffer, unsigned size);
bool create (const char *file, unsigned initial_size);
bool remove (const char *file);

#endif /* userprog/syscall.h */
