#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/synch.h"
#include "userprog/process.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "threads/malloc.h"
#include "devices/input.h"


#define VADD_LIMIT 0x08048000

static void syscall_handler (struct intr_frame *);

struct lock filesys_lock;

static struct thread_file*
find_file_by_fd (int fd)
{
  struct thread* cur = thread_current ();
  struct list_elem* it = list_begin (&cur->files);
  for (;it != list_end (&cur->files); it = list_next (it))
    {
      struct thread_file *f = list_entry (it, struct thread_file, f_listelem);
      if (f->fd == fd)
        return f;
    }
  return NULL;
}

void
syscall_init (void) 
{
  lock_init(&filesys_lock);
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  check_valid((const void *)f->esp);
  check_valid((const void *)f->esp +4);
  
  switch (*(int *)f->esp)
  {
  case SYS_HALT:
    halt();
    break;

  case SYS_EXIT:
    exit(get_ith_arg(f, 0));
    break;

  case SYS_EXEC:
    {

      check_valid((const void *)get_ith_arg(f, 0));
      const void *vcmd_line = (const void *)get_ith_arg(f, 0);
      const void *cmd_line = (const void *)pagedir_get_page (thread_current ()->pagedir, 
                                                           vcmd_line);
      f->eax = exec((char *)cmd_line);

    }
    break;

  case SYS_WAIT:
    f->eax = wait((pid_t)get_ith_arg(f, 0));
    break;

  case SYS_WRITE:
    {
      int fd = get_ith_arg(f, 0);
      const void *buffer_head = (const void *) get_ith_arg(f, 1);
      unsigned size = (unsigned) get_ith_arg(f, 2);
      //const void *buffer_tail = ((const void *) buffer_head) + (size-1)*4;
      
      check_valid(buffer_head);
      //check_valid(buffer_tail);

      const void *buffer = (const void *)pagedir_get_page (thread_current ()->pagedir, 
                                                           buffer_head);
      f->eax = write(fd, buffer, size);
    }
    break;
    
  case SYS_OPEN:
    {
      const void *file_ptr = (const void *)get_ith_arg (f, 0);
      check_valid (file_ptr);
      const char *file = (const char *)pagedir_get_page (thread_current ()->pagedir,
                                                     file_ptr);
      f->eax = open (file);
    }
    break;

  case SYS_CREATE:
    {
      const void * file_head = (const void *)get_ith_arg(f, 0);
      unsigned size = (unsigned) get_ith_arg(f, 1);
      //const void * file_tail = file_head + size *4;

      check_valid(file_head);
      //check_valid(file_tail);

      const void *file = (const void *)pagedir_get_page (thread_current ()->pagedir, 
                                                           file_head);
      f->eax = create( (const char *) file, size);
    }
    break;

  case SYS_REMOVE:
    check_valid(get_ith_arg(f, 0));
    f->eax = remove( (const char *) get_ith_arg(f, 0));
    break;

  case SYS_CLOSE:
    {
      int fd = get_ith_arg (f, 0);
      close (fd);
    }
    break;

  case SYS_FILESIZE:
    f->eax = filesize(get_ith_arg(f, 0));
    break;

  case SYS_SEEK:
    {
      int fd = get_ith_arg(f, 0);
      unsigned position = (unsigned) get_ith_arg(f, 1);
      seek(fd, position);
    }
    break;
  case SYS_TELL:
    f->eax = tell(get_ith_arg(f, 0));
    break;

  case SYS_READ:
    {
      int fd = get_ith_arg (f, 0);
      const void *buffer_head = (const void *) get_ith_arg(f, 1);
      unsigned size = (unsigned) get_ith_arg(f, 2);
      //const void *buffer_tail = ((const void *) buffer_head) + (size - 1)*4;
      
      check_valid(buffer_head);
      //check_valid(buffer_tail);

      const void *buffer = (const void *)pagedir_get_page (thread_current ()->pagedir, 
                                                           buffer_head);
      f->eax = read (fd, buffer, size);
    }
    break;
  
  default:
    exit(-1);
    break;
  }
}

void 
halt (void)
{
	shutdown_power_off();
}

void 
exit (int status)
{
  struct thread *cur = thread_current ();
  while (!list_empty(&cur->files))
  {
    struct list_elem* e = list_begin (&cur->files);
    int fd = list_entry (e, struct thread_file, f_listelem)->fd;
    close (fd);
  }
  thread_current()->exit_status = status;
  thread_exit();
}

pid_t 
exec (const char *cmd_line)
{
  if (!cmd_line)
    return -1;

  lock_acquire(&filesys_lock);
  pid_t new_tid = process_execute(cmd_line);
  lock_release(&filesys_lock);

  return new_tid;
}

int
wait (pid_t pid)
{
  return process_wait(pid);
}

int 
write (int fd, const void *buffer, unsigned size)
{

  lock_acquire(&filesys_lock);

  if (fd == 1)
  {
    putbuf(buffer, size);
    lock_release(&filesys_lock);
    return size;
  }

  struct thread_file *f = find_file_by_fd (fd);
  if (f == NULL)
  {
    lock_release(&filesys_lock);
    return 0;
  }

  int real_size = (int)file_write(f->f, buffer, (off_t)size);
  lock_release (&filesys_lock);
  return real_size;
}

int
open (const char *file)
{
  lock_acquire (&filesys_lock);
  /*if (file == ""){
    lock_release(&filesys_lock);
    exit (-1);    
  }*/
  struct file *f = filesys_open (file);
  if (f == NULL)
  {
    lock_release(&filesys_lock);
    return -1;
  }

  struct thread_file* thread_f = (struct thread_file *)malloc (sizeof(struct thread_file));
  if (thread_f == NULL){
    lock_release(&filesys_lock);
    exit (-1);
  }

  struct thread *cur = thread_current ();
  thread_f->f = f;
  thread_f->fd = cur->fd;
  cur->fd++;
  list_push_back (&thread_current ()->files, &thread_f->f_listelem);

  lock_release (&filesys_lock);
  return thread_f->fd;
}

bool 
create (const char *file, unsigned initial_size)
{
  lock_acquire(&filesys_lock);
  bool ret = filesys_create(file, initial_size);
  lock_release(&filesys_lock);
  return ret;
}

bool 
remove (const char *file)
{
  lock_acquire(&filesys_lock);
  bool ret = filesys_remove(file);
  lock_release(&filesys_lock);
  return ret;
}

void
close (int fd)
{
  lock_acquire (&filesys_lock);
  struct thread_file *f = find_file_by_fd (fd);
  if (f == NULL){
    lock_release (&filesys_lock);
    exit (-1);
  }
  file_close (f->f);
  list_remove (&f->f_listelem);
  lock_release (&filesys_lock);
}

int 
filesize (int fd)
{
  lock_acquire(&filesys_lock);
  struct thread_file *f = find_file_by_fd(fd);
  if (f == NULL)
  {
    lock_release(&filesys_lock);
    return -1;
  }
  lock_release(&filesys_lock);
  return (int) file_length(f->f);
}

void 
seek (int fd, unsigned position)
{
  lock_acquire(&filesys_lock);
  struct thread_file *f = find_file_by_fd(fd);
  if (f == NULL)
  {
    lock_release(&filesys_lock);
    return;
  }
  file_seek(f->f, position);
  lock_release(&filesys_lock);
  return;
}

unsigned 
tell (int fd)
{
  lock_acquire(&filesys_lock);
  struct thread_file *f = find_file_by_fd(fd);
  if (f == NULL)
  {
    lock_release(&filesys_lock);
    return -1;
  }
  unsigned position = (unsigned) file_tell(f->f);
  lock_release(&filesys_lock);
  return position;
}

int
read (int fd, void *buffer, unsigned size)
{
  lock_acquire (&filesys_lock);
  if (fd == 0){
    lock_release(&filesys_lock);
    return (int)input_getc ();
  }

  struct thread_file *f = find_file_by_fd (fd);
  if (f == NULL){
    lock_release (&filesys_lock);
    return -1;
  }

  int length = (int)file_read (f->f, buffer, size);
  lock_release (&filesys_lock);
  return length;
}

void 
check_valid (const void *add)
{
  if (!is_user_vaddr(add) || add == NULL || 
      pagedir_get_page(thread_current()->pagedir, add) == NULL || add < (void *)VADD_LIMIT)
      exit(-1);
}

int 
get_ith_arg (struct intr_frame *f, int i)
{
  int *argv = (int *) f->esp + i + 1;
  check_valid(argv);
  return *argv;
}
