#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include "stdbool.h"
#include <stddef.h>
#include "threads/interrupt.h"
#include "threads/synch.h"
#include "threads/mmu.h"
#include "process.h"

/* Process identifier. */
typedef int pid_t;
#define PID_ERROR ((pid_t) -1)

/* Map region identifier. */
typedef int off_t;
#define MAP_FAILED ((void *) NULL)

struct lock filesys_lock;

void syscall_init (void);

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
int write (int fd, const void *buffer, unsigned length);
void seek (int fd, unsigned position);
unsigned tell (int fd);
void close (int fd);

#endif /* userprog/syscall.h */
