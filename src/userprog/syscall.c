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
#include "vm/page.h"
#include "userprog/mmap.h"

/* Virtual memory address limit. */
#define VADD_LIMIT 0x08048000

static void syscall_handler (struct intr_frame *);
static int FD = 2;

/* Lock of the file system. */
static struct lock filesys_lock;

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
  ASSERT (f != NULL);
  check_valid((const void *)f->esp, false);
  check_valid((const void *)f->esp +4, false);
  
  switch (*(int *)f->esp)
  {
  case SYS_HALT:
    halt();
    break;

  case SYS_EXIT:
    exit(get_ith_arg(f, 0, true));
    break;

  case SYS_EXEC:
    {
      const void *vcmd_line = (const void *)get_ith_arg(f, 0, true);
      check_valid(vcmd_line, true);
      check_valid(vcmd_line+4, true);
      const void *cmd_line = (const void *)pagedir_get_page (thread_current ()->pagedir, 
                                                           vcmd_line);
      f->eax = exec((char *)cmd_line);

    }
    break;

  case SYS_WAIT:
    f->eax = wait((pid_t)get_ith_arg(f, 0, true));
    break;

  case SYS_WRITE:
    {
      int fd = get_ith_arg(f, 0, true);
      const void *buffer_head = (const void *) get_ith_arg(f, 1, false);
      unsigned size = (unsigned) get_ith_arg(f, 2, true);
      //const void *buffer_tail = ((const void *) buffer_head) + (size-1)*4;
      
      for (size_t i = 0; i < size ; i++)
      {
        check_valid_rw(buffer_head + i, f, false);  
      }
      
      //check_valid_rw(buffer_tail, f);

      const void *buffer = (const void *)pagedir_get_page (thread_current ()->pagedir, 
                                                           buffer_head);
      f->eax = write(fd, buffer, size);
    }
    break;
    
  case SYS_OPEN:
    {
      const void *file_ptr = (const void *)get_ith_arg (f, 0, false);
      check_valid (file_ptr, false);
      const char *file = (const char *)pagedir_get_page (thread_current ()->pagedir,
                                                         file_ptr);
      f->eax = open (file);
    }
    break;

  case SYS_CREATE:
    {
      const void * file_head = (const void *)get_ith_arg(f, 0, false);
      unsigned size = (unsigned) get_ith_arg(f, 1, true);
      //const void * file_tail = file_head + size *4;

      check_valid(file_head, false);
      //check_valid(file_tail);

      const void *file = (const void *)pagedir_get_page (thread_current ()->pagedir, 
                                                           file_head);
      f->eax = create( (const char *) file, size);
    }
    break;

  case SYS_REMOVE:
    {
      const void *file_ptr = (const void *)get_ith_arg(f, 0, false);
      check_valid (file_ptr, false);
      const char *file = (const char *)pagedir_get_page (thread_current ()->pagedir,
                                                      file_ptr);
      f->eax = remove (file);
    }
    break;

  case SYS_CLOSE:
    {
      int fd = get_ith_arg (f, 0, true);
      close (fd);
    }
    break;

  case SYS_FILESIZE:
    f->eax = filesize(get_ith_arg(f, 0, true));
    break;

  case SYS_SEEK:
    {
      int fd = get_ith_arg(f, 0, true);
      unsigned position = (unsigned) get_ith_arg(f, 1, true);
      seek(fd, position);
    }
    break;
  case SYS_TELL:
    f->eax = tell(get_ith_arg(f, 0, true));
    break;

  case SYS_READ:
    {
      int fd = get_ith_arg (f, 0, true);
      const void *buffer_head = (const void *) get_ith_arg(f, 1, false);
      unsigned size = (unsigned) get_ith_arg(f, 2, true);
      //const void *buffer_tail = ((const void *) buffer_head) + (size - 1)*4;
      
      for (size_t i = 0; i < size ; i++)
      {
        check_valid_rw(buffer_head + i, f, false);  
      }
      //check_valid_rw(buffer_head, f);
      //check_valid_rw(buffer_tail, f);
      struct sup_page_table_entry *entry = sup_page_table_look_up(thread_current()->sup_page_table, buffer_head);
      if (!entry->writable) 
        exit(-1);
      const void *buffer = (const void *)pagedir_get_page (thread_current ()->pagedir, 
                                                           buffer_head);
      f->eax = read (fd, (void *)buffer, size);
    }
    break;

  case SYS_MMAP:
    {
      int fd = get_ith_arg(f, 0, true);
      void *vadd = get_ith_arg(f, 1, true);
      f->eax = mmap(fd, vadd);
    }
    break;
  case SYS_MUNMAP:
    munmap(get_ith_arg(f, 0, true));
    break;
  
  default:
    exit(-1);
    break;
  }
}

/*  Terminates Pintos by calling shutdown_power_off() (declared in threads/init.h). 
    This should be seldom used, because you lose some information about possible deadlock situations, etc.*/
void 
halt (void)
{
	shutdown_power_off();
}

/*Terminates the current user program, returning status to the kernel. 
If the process's parent waits for it (see below), this is the status that will be returned. 
Conventionally, a status of 0 indicates success and nonzero values indicate errors.*/

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

  while (!list_empty(&cur->child_list))
  {
    struct list_elem *i = list_begin(&cur->child_list);
    struct child_thread * this_child = list_entry(i, struct child_thread, child_elem);
    this_child->t->parent = NULL;
    free(this_child);
  }
  cur->exit_status = status;
  printf("%s: exit(%d)\n",cur->name, cur->exit_status);
  cur->has_exit = 1;
  thread_exit();
}

/* Runs the executable whose name is given in cmd_line, passing any given arguments, 
and returns the new process's program id (pid). Must return pid -1, which otherwise should not be a valid pid, 
if the program cannot load or run for any reason. Thus, the parent process cannot return from the exec 
until it knows whether the child process successfully loaded its executable. 
You must use appropriate synchronization to ensure this.*/
pid_t 
exec (const char *cmd_line)
{
  if (!cmd_line)
    return -1;

  if (check_all_list() > 40)
    return -1;

  lock_acquire(&filesys_lock);
  pid_t new_tid = process_execute(cmd_line);
  lock_release(&filesys_lock);

  return new_tid;
}


/* Waits for a child process pid and retrieves the child's exit status. */
int
wait (pid_t pid)
{
  return process_wait(pid);
}


/* Writes size bytes from buffer to the open file fd. Returns the number of bytes actually written, 
which may be less than size if some bytes could not be written. Writing past end-of-file would normally extend the file, 
but file growth is not implemented by the basic file system. The expected behavior is to write as many bytes as possible up to end-of-file 
and return the actual number written, or 0 if no bytes could be written at all. Fd 1 writes to the console. 
Your code to write to the console should write all of buffer in one call to putbuf(), at least as long as size is not bigger 
than a few hundred bytes. (It is reasonable to break up larger buffers.) Otherwise, lines of text output by different processes may end up interleaved on the console, 
confusing both human readers and our grading scripts.*/
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


/* Opens the file called file. 
Returns a nonnegative integer handle called a "file descriptor" (fd), 
or -1 if the file could not be opened.*/
int
open (const char *file)
{
  lock_acquire (&filesys_lock);
  
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
  thread_f->fd = FD++;
  list_push_back (&cur->files, &thread_f->f_listelem);

  lock_release (&filesys_lock);
  return thread_f->fd;
}


/* Creates a new file called file initially initial_size bytes in size. 
Returns true if successful, false otherwise. Creating a new file does not open it: 
opening the new file is a separate operation which would require a open system call.*/
bool 
create (const char *file, unsigned initial_size)
{
  lock_acquire(&filesys_lock);
  bool ret = filesys_create(file, initial_size);
  lock_release(&filesys_lock);
  return ret;
}

/* Deletes the file called file. Returns true if successful, false otherwise.
 A file may be removed regardless of whether it is open or closed, 
 and removing an open file does not close it*/
bool 
remove (const char *file)
{
  lock_acquire(&filesys_lock);
  bool ret = filesys_remove(file);
  lock_release(&filesys_lock);
  return ret;
}

/* Closes file descriptor fd. Exiting or terminating a process implicitly 
closes all its open file descriptors, as if by calling this function for each one.*/
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
  free (f);
  lock_release (&filesys_lock);
}

/* Returns the size, in bytes, of the file open as fd. */
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

/* Changes the next byte to be read or written in open file fd to position, 
expressed in bytes from the beginning of the file. (Thus, a position of 0 is the file's start.)*/
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


/* Returns the position of the next byte to be read or written in open file fd, 
expressed in bytes from the beginning of the file.*/
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

/* Reads size bytes from the file open as fd into buffer. 
Returns the number of bytes actually read (0 at end of file), 
or -1 if the file could not be read (due to a condition other than end of file). 
Fd 0 reads from the keyboard using input_getc().*/
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

int
reopen(int fd)
{
  lock_acquire(&filesys_lock);
  struct thread_file *f = find_file_by_fd (fd);
  if (f == NULL)
  {
    lock_release (&filesys_lock);
    return -1;
  }else{
    struct file *file = file_reopen(f->f);
    file_seek(file, 0);
    struct thread_file* thread_f = (struct thread_file *)malloc (sizeof(struct thread_file));
    if (thread_f == NULL){
      lock_release(&filesys_lock);
      exit (-1);
    }

    struct thread *cur = thread_current ();
    thread_f->f = file;
    thread_f->fd = FD++;
    list_push_back (&cur->files, &thread_f->f_listelem);

    lock_release (&filesys_lock);
    return thread_f->fd;
  }
}

/* check an address is valid or not, if not valid exit with -1.*/
void 
check_valid (const void *add, bool swap_able)
{
  struct sup_page_table_entry *entry = sup_page_table_look_up(thread_current()->sup_page_table, add);

  if (!is_user_vaddr(add) || add == NULL || 
      entry == NULL || add < (void *)VADD_LIMIT)
      exit(-1);
  
  page_set_swap_able(entry, swap_able);

}

void
check_valid_rw(void *add, struct intr_frame *f, bool swap_able)
{
  if (!is_user_vaddr(add) || add == NULL)
    exit(-1);

  struct sup_page_table_entry *entry = sup_page_table_look_up(thread_current()->sup_page_table, add);

  if (entry == NULL)
    if (f->esp - 33 < add && add < f->esp + PGSIZE*100)
      entry = sup_page_create(pg_round_down(add), PAL_USER|PAL_ZERO, true);
    else
      exit(-1);

  sup_page_activate(entry);
  page_set_swap_able(entry, swap_able);
}

/* Get the ith arg for the given f */
int 
get_ith_arg (struct intr_frame *f, int i, bool swap_able)
{
  int *argv = (int *) f->esp + i + 1;
  check_valid(argv, swap_able);
  check_valid((void *)argv + 3, swap_able);
  return *argv;
}
