#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include <string.h>
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
#include "filesys/directory.h"
#include "filesys/inode.h"

/* Virtual memory address limit. */
#define VADD_LIMIT 0x08048000

static void syscall_handler (struct intr_frame *);

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
      const void *vcmd_line = (const void *)get_ith_arg(f, 0);
      check_valid(vcmd_line);
      check_valid(vcmd_line+4);
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
    {
      const void *file_ptr = (const void *)get_ith_arg(f, 0);
      check_valid (file_ptr);
      const char *file = (const char *)pagedir_get_page (thread_current ()->pagedir,
                                                      file_ptr);
      f->eax = remove (file);
    }
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
      f->eax = read (fd, (void *)buffer, size);
    }
    break;

  case SYS_CHDIR:
    {
      void *name_ptr = get_ith_arg (f, 0);
      check_valid (name_ptr);
      const char *name = (const char *)pagedir_get_page (thread_current ()->pagedir,
                                                         name_ptr);
      f->eax = chdir (name);
    }
  
  case SYS_MKDIR:
    {
      void *name_ptr = get_ith_arg (f, 0);
      check_valid (name_ptr);
      const char *name = (const char *)pagedir_get_page (thread_current ()->pagedir,
                                                         name_ptr);
      f->eax = mkdir (name);
    }

  case SYS_READDIR:
    {
      int fd = get_ith_arg (f, 0);
      void *name_ptr = get_ith_arg (f, 1);
      check_valid (name_ptr);
      const char *name = (const char *)pagedir_get_page (thread_current ()->pagedir,
                                                         name_ptr);
      f->eax = readdir (fd, name);
    }

  case SYS_ISDIR:
    {
      int fd = get_ith_arg (f, 0);
      f->eax = isdir (fd);
    }

  case SYS_INUMBER:
    {
      int fd = get_ith_arg (f, 0);
      f->eax = inumber (fd);
    }
  
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
  enum entry_type type;
  
  struct file *f = filesys_open (thread_current()->current_dir, file, &type);
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
  if (type == DIRECTORY){
    thread_f->is_dir = true;
    thread_f->dir = dir_open(inode_open(file_get_inode(f)));
  }
  else
    thread_f->is_dir = false;
  thread_f->f = f;
  thread_f->fd = cur->fd;
  cur->fd++;
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
  bool ret = filesys_create(thread_current()->current_dir, file, initial_size, FILE);
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
  bool ret = filesys_remove(thread_current()->current_dir, file);
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
  if (!f->is_dir)
    file_close (f->f);
  else{
    dir_close (f->dir);
  }
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

bool chdir (const char *dir)
{
  lock_acquire(&filesys_lock);
  struct inode *dir_inode = NULL;
  enum entry_type *type;
  if (! dir_lookup(thread_current()->current_dir, dir, &dir_inode, type) || (*type != DIRECTORY))
  {
    lock_release(&filesys_lock);
    return false;
  }
  dir_close(thread_current()->current_dir);
  thread_current()->current_dir = dir_open(dir_inode);
  lock_release(&filesys_lock);
  return true;
  
}

bool mkdir (const char *dir)
{
  lock_acquire(&filesys_lock);
  char *parent_dir = NULL;
  struct inode *dir_inode = NULL;
  struct inode *par_inode = NULL;
  enum entry_type *par_type;
  enum entry_type *dir_type;

  int len = strlen(dir);
  parent_dir = (char *)malloc(len +1);
  strncpy(parent_dir, dir, len + 1);
  for (size_t i = len -1; i >= 0 && parent_dir[i] != '/'; i--)
  {
    parent_dir[i] = '\0';
  }
  if (!dir_lookup(thread_current()->current_dir, parent_dir, par_inode, par_type) || *par_type == FILE || 
       dir_lookup(thread_current()->current_dir, dir, dir_inode, dir_type))
  {
    free(parent_dir);
    lock_release(&filesys_lock);
    return false;
  }
  if (!filesys_create(thread_current()->current_dir, dir, 5*sizeof(struct dir_entry), DIRECTORY))
  {
    free(parent_dir);
    lock_release(&filesys_lock);
    return false;
  }
  free(parent_dir);
  lock_release(&filesys_lock);
  return true;
}

/* Reads a directory entry from file descriptor fd, 
   which must represent a directory. If successful, 
   stores the null-terminated file name in name, 
   which must have room for READDIR_MAX_LEN + 1 bytes, 
   and returns true. If no entries are left in the 
   directory, returns false. */
bool
readdir (int fd, char *name)
{
  struct thread_file *f = find_file_by_fd (fd);
  if (f == NULL || f->is_dir == false)
    return false;
  struct dir *f_dir = f->dir;
  return dir_readdir (f_dir, name);
}

/* Returns true if fd represents a directory, 
   false if it represents an ordinary file. */
bool 
isdir (int fd)
{
  struct thread_file *f = find_file_by_fd (fd);
  if (f == NULL)
    return false;
  return f->is_dir;
}

/* Returns the inode number of the inode 
   associated with fd, which may represent an 
   ordinary file or a directory. */
int 
inumber (int fd)
{
  struct thread_file *f = find_file_by_fd (fd);
  if (f==NULL)
    return false;
  if (f->is_dir)
    return (int)f->dir->inode->sector;
  return (int)f->f->inode->sector;
}

/* check an address is valid or not, if not valid exit with -1.*/
void 
check_valid (const void *add)
{
  if (!is_user_vaddr(add) || add == NULL || 
      pagedir_get_page(thread_current()->pagedir, add) == NULL || add < (void *)VADD_LIMIT)
      exit(-1);
}

/* Get the ith arg for the given f */
int 
get_ith_arg (struct intr_frame *f, int i)
{
  int *argv = (int *) f->esp + i + 1;
  check_valid(argv);
  check_valid((void *)argv + 3);
  return *argv;
}
