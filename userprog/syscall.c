#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"

#include "filesys/filesys.h"
#include "filesys/file.h"


/* Projects 2 and later. */
void halt (void);
void exit (int status);
pid_t fork (const char *thread_name);
int exec (const char *file);
int wait (pid_t);
bool create (const char *file, unsigned initial_size);
bool remove (const char *file);
int open (const char *file);
int filesize (int fd);
int read (int fd, void *buffer, unsigned length);
void seek (int fd, unsigned position);
unsigned tell (int fd);
void close (int fd);

void syscall_entry (void);
void syscall_handler (struct intr_frame *);

int write2 (struct intr_frame *);

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

	lock_init(&filesys_lock);

	write_msr(MSR_STAR, ((uint64_t)SEL_UCSEG - 0x10) << 48  |
			((uint64_t)SEL_KCSEG) << 32);
	write_msr(MSR_LSTAR, (uint64_t) syscall_entry);

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
	int syscall_number = f->R.rax;
	// printf("\nsyscall_number : %d\n", f->R.rax);
	switch (syscall_number){
		case SYS_HALT:			/* Halt the operating system. */
			halt();
			break;
		case SYS_EXIT:			/* Terminate this process. */
			// exit(57);
			exit(f->R.rdi); // status
			break;
		case SYS_FORK:			/* Clone current process. */
			break;
		case SYS_EXEC:			/* Switch current process. */
			break;
		case SYS_WAIT:			/* Wait for a child process to die. */
			break;
		case SYS_CREATE:		/* Create a file. */
			create(f->R.rdi, f->R.rsi);
			break;                 
		case SYS_REMOVE:		/* Delete a file. */
			break;                 
		case SYS_OPEN:			/* Open a file. */
			open(f->R.rdi);
			break;                   
		case SYS_FILESIZE:	/* Obtain a file's size. */
			break;             
		case SYS_READ:			/* Read from a file. */
			break;                   
		case SYS_WRITE:			/* Write to a file. */
			// printf("\nwrite test\n");
			write2(f);
			break;                  
		case SYS_SEEK:			/* Change position in a file. */
			break;                   
		case SYS_TELL:			/* Report current position in a file. */
			break;
		case SYS_CLOSE:			/* Close a file. */
			break;
		default:
			break;
	}
	// char * argv_addr = f->R.rsi;
	// printf("%%rdi | argc : %d\n", f->R.rdi);
	// printf("%%rsi | argv 주소 : %p, argv : %s\n", f->R.rsi, f->R.rsi);
	// printf("%%rsi | size of rsi : %d, argv_addr[] : %s\n", f->R.rsi, f->R.rsi);
	// printf("%%rax | system call number : %d\n", syscall_number);
	// printf("%%rdx | %d\n", f->R.rdx);
	// printf("%%r10 | %d\n", f->R.r10);
	// printf("%%r8 | %d\n", f->R.r8);
	// printf("%%r9 | %d\n", f->R.r9);
	// printf("%%rip | %p\n", f->rip);


	
	// 유저 메모리 접근 시
	// 1. 커널 메모리 주소 이상일 때
	// 2. 사용자 메모리 주소에서 유저 스택 영역이 아닐 때??
	// 유저 프로세스 종료

	// syscall 명령어로 시스템 콜을 불러온다.
	// %rax는 시스템 콜 번호

	// 시스템 콜 번호를 받아오고
	// 어떤 시스템 콜 인자들을 받아오고
	// 그에 알맞은 액션을 취해야 한다.

	// thread_exit();
	
}


void halt (void){
	power_off();
}

void exit (int status){
	thread_current()->exit_status = status;
	thread_exit();
}

pid_t fork (const char *thread_name);
int exec (const char *file);
int wait (pid_t);

bool create (const char *file, unsigned initial_size){

	bool success = filesys_create(file, initial_size);
	return success;

}

bool remove (const char *file);

int open (const char *file){

	struct file *f = filesys_open(file); // Returns the new file if successful or a null pointer otherwise.
	if (f == NULL){
		exit(-1);
	}
	// f->inode->open_cnt++;
	// if (f->inode->removed) return -1;

	/* First Fit. TODO: Next fit. */
	int fd = FDT_MIN;
	struct thread *t = thread_current();

	while (t->fdt[fd++] != NULL && fd < FDT_SIZE){
		// t->next_fd;
	}

	if(fd >= FDT_SIZE){
		return -1;
	}

	t->fdt [fd] = f;
	return fd;
	// t->next_fd = fd;
}

int filesize (int fd);
int read (int fd, void *buffer, unsigned length);
// int write (int fd, const void *buffer, unsigned length)
int write2 (struct intr_frame *f){

	int fd = f->R.rdi;
	void *buffer = f->R.rsi;
	unsigned length = f->R.rdx;

	printf("%s", buffer);
	return length;
	// %rdi, %rsi, %rdx, %r10, %r8, %r9
}
void seek (int fd, unsigned position);
unsigned tell (int fd);
void close (int fd);