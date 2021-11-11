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

#define VADD_LIMIT 0x08048000

static void syscall_handler (struct intr_frame *);

struct lock filesys_lock;

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
      f->eax = exec((char *)get_ith_arg(f, 0));

    }
    break;

  case SYS_WAIT:
    f->eax = wait((pid_t)get_ith_arg(f, 0));

  case SYS_WRITE:
    {
      enum intr_level old_level = intr_disable ();
      int fd = get_ith_arg(f, 0);
      const void *buffer_head = (const void *) get_ith_arg(f, 1);
      unsigned size = (unsigned) get_ith_arg(f, 2);
      const void *buffer_tail = (const void *) buffer_head + (size-1)*4;
      
      check_valid(buffer_head);
      check_valid(buffer_tail);

      const void *buffer = (const void *)pagedir_get_page (thread_current ()->pagedir, 
                                                           buffer_head);
      f->eax = write(fd, buffer, size);
      intr_set_level (old_level);
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