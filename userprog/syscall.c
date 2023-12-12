#include "userprog/syscall.h"
#include <stdio.h>
#include <string.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"
#include "filesys/inode.h"
#include "filesys/file.h"
#include "filesys/filesys.h"


void syscall_entry (void);
void syscall_handler (struct intr_frame *);

bool check_address(void * address){
	struct thread * t = thread_current();
	if(!pml4_get_page(t->pml4, address)){
		t->exit_status = -1;
		thread_exit();
	}
	return true;
}

/* Projects 2 and later. */
void halt (void){
	power_off();
}

void exit (int status){
	thread_current()->exit_status = status;
	thread_exit();
}

// pid_t fork (const char *thread_name);
// int exec (const char *file);
// int wait (pid_t);

bool create (const char *file, unsigned int initial_size){
	check_address(file);
	if (strlen(file) <= 0 && strlen(file) > 15)
		return 0;
	
	bool success = filesys_create(file,initial_size);
	return success;
}

bool remove (const char *file){
	bool success = filesys_remove(file);
	return success;
}

int open (const char *file){
	check_address(file);
	int fd = 0;
	struct file * open_file = filesys_open(file);
	if (open_file == NULL)
		return -1;
	lock_acquire(&filesys_lock);
	fd = process_add_file(open_file);
	lock_release(&filesys_lock);

	return fd;
}

int filesize (int fd){
	struct file *f = thread_current()->fdt[fd];
	return file_length(f);
}

int read (int fd, void *buffer, unsigned length){
	check_address(buffer);
	if(fd < 3 || fd > 63) {
		thread_current()->exit_status = -1;
		thread_exit();
	}
	struct file *f = thread_current()->fdt[fd];
	return file_read(f, buffer, length);
}

int write (int fd, const void *buffer, unsigned length){
	if(fd == 1){
		printf("%s", buffer);
		return 1;
	}
	check_address(buffer);
	if(fd < 3 || fd > 63) {
		thread_current()->exit_status = -1;
		thread_exit();
	}
	struct file *f = thread_current()->fdt[fd];
	off_t i = file_write(f, buffer, length);
	return i;
}

void seek (int fd, unsigned position){
	if(fd < 3 || fd > 63) {
		thread_current()->exit_status = -1;
		thread_exit();
	}
	file_seek(thread_current()->fdt[fd],position);
}

unsigned tell (int fd){
	if(fd < 3 || fd > 63) {
		thread_current()->exit_status = -1;
		thread_exit();
	}
	return file_tell(thread_current()->fdt[fd]);
}

void close (int fd) {
	if(fd < 3 || fd > 63) {
		thread_current()->exit_status = -1;
		thread_exit();
	}
	struct file *f = thread_current()->fdt[fd];
	if(f != NULL && file_get_deny_write(f) == 0)
		file_close(f);
}

/* System call.
 *
 * Previously system call services was handled by the interrupt handler
 * (e.g. int 0x80 in linux). However, in x86-64, the manufacturer supplies
 * efficient path for requesting the system call, the `syscall` instruction.
 *
 * The syscall instruction works by reading the values from the the Model
 * Specific Register (MSR). For the details, see the manual. */

#define MSR_STAR 0xc0000081         /* Segment selector msr */
#define MSR_LSTAR 0xc0000082        /* Long mode SYSCALL target */
#define MSR_SYSCALL_MASK 0xc0000084 /* Mask for the eflags */

void
syscall_init (void) {
	write_msr(MSR_STAR, ((uint64_t)SEL_UCSEG - 0x10) << 48  |
			((uint64_t)SEL_KCSEG) << 32);
	write_msr(MSR_LSTAR, (uint64_t) syscall_entry);

	lock_init(&filesys_lock);

	/* The interrupt service rountine should not serve any interrupts
	 * until the syscall_entry swaps the userland stack to the kernel
	 * mode stack. Therefore, we masked the FLAG_FL. */
	write_msr(MSR_SYSCALL_MASK,
			FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);
}

/* The main system call interface */
void
syscall_handler (struct intr_frame *f UNUSED) {
	// TODO: Your implementation goes here.
	// printf("\n%s\n", f->R.rax);
	switch (f->R.rax)
	{
		case SYS_HALT:                   /* Halt the operating system. */
			halt();
			break;
		case SYS_EXIT:                   /* Terminate this process. */
			exit(f->R.rdi);
			break;
		// case SYS_FORK:                   /* Clone current process. */
		// 	fork(f->R.rdi);
		// 	break;
		// case SYS_EXEC:                   /* Switch current process. */
		// 	exec(f->R.rdi);
		// 	break;
		// case SYS_WAIT:                   /* Wait for a child process to die. */
		// 	wait(f->R.rdi);
		// 	break;
		case SYS_CREATE:                 /* Create a file. */
			f->R.rax = create(f->R.rdi, f->R.rsi);
			break;
		case SYS_REMOVE:                 /* Delete a file. */
			f->R.rax = remove(f->R.rdi);
			break;
		case SYS_OPEN:                   /* Open a file. */
			f->R.rax = open(f->R.rdi);
			break;
		case SYS_FILESIZE:               /* Obtain a file's size. */
			f->R.rax = filesize(f->R.rdi);
			break;
		case SYS_READ:                   /* Read from a file. */
			f->R.rax = read(f->R.rdi, f->R.rsi, f->R.rdx);
			break;
		case SYS_WRITE:                  /* Write to a file. */
			f->R.rax = write(f->R.rdi, f->R.rsi, f->R.rdx);
			break;
		// case SYS_SEEK:                   /* Change position in a file. */
		// 	seek(f->R.rdi, f->R.rsi);
		// 	break;
		case SYS_TELL:                   /* Report current position in a file. */
			f->R.rax = tell(f->R.rdi);
			break;
		case SYS_CLOSE:                  /* Close a file. */
			close(f->R.rdi);
			break;
		default:
			break;
	}
}
