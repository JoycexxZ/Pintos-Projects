#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H
#include "threads/interrupt.h"
#include "filesys/file.h"
#include "filesys/directory.h"
#include <list.h>
#include "filesys/filesys.h"

/* Process id number pid for convenience. */
typedef int pid_t;

/* A file possessed by a thread (user program). */
struct thread_file
{
    struct file *f;                   /* The file. */
    struct list_elem f_listelem;      /* List element for file list of thread. */
    int fd;                           /* The file descriptor. */
    bool is_dir;
    struct dir *dir;
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
int filesize (int fd);
void seek (int fd, unsigned position);
unsigned tell (int fd);

bool chdir (const char *dir);
bool mkdir (const char *dir);
bool readdir (int fd, char *name);

#endif /* userprog/syscall.h */
