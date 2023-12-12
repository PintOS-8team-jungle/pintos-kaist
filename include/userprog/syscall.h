#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include "stdbool.h"
#include <stddef.h>
#include "threads/synch.h"

/* Process identifier. */
typedef int pid_t;
#define PID_ERROR ((pid_t) -1)

/* Map region identifier. */
typedef int off_t;
#define MAP_FAILED ((void *) NULL)

void syscall_init (void);

struct lock filesys_lock;

#endif /* userprog/syscall.h */
