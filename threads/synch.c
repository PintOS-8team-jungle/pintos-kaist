/* This file is derived from source code for the Nachos
   instructional operating system.  The Nachos copyright notice
   is reproduced in full below. */

/* Copyright (c) 1992-1996 The Regents of the University of California.
   All rights reserved.

   Permission to use, copy, modify, and distribute this software
   and its documentation for any purpose, without fee, and
   without written agreement is hereby granted, provided that the
   above copyright notice and the following two paragraphs appear
   in all copies of this software.

   IN NO EVENT SHALL THE UNIVERSITY OF CALIFORNIA BE LIABLE TO
   ANY PARTY FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR
   CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OF THIS SOFTWARE
   AND ITS DOCUMENTATION, EVEN IF THE UNIVERSITY OF CALIFORNIA
   HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

   THE UNIVERSITY OF CALIFORNIA SPECIFICALLY DISCLAIMS ANY
   WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
   WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
   PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS ON AN "AS IS"
   BASIS, AND THE UNIVERSITY OF CALIFORNIA HAS NO OBLIGATION TO
   PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR
   MODIFICATIONS.
   */

#include "threads/synch.h"
#include <stdio.h>
#include <string.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

/* Initializes semaphore SEMA to VALUE.  A semaphore is a
   nonnegative integer along with two atomic operators for
   manipulating it:

   - down or "P": wait for the value to become positive, then
   decrement it.

   - up or "V": increment the value (and wake up one waiting
   thread, if any). */
void
sema_init (struct semaphore *sema, unsigned value) {
	ASSERT (sema != NULL);

	sema->value = value;
	list_init (&sema->waiters);
}

/* Down or "P" operation on a semaphore.  Waits for SEMA's value
   to become positive and then atomically decrements it.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but if it sleeps then the next scheduled
   thread will probably turn interrupts back on. This is
   sema_down function. */
void
sema_down (struct semaphore *sema) {
	enum intr_level old_level;

	ASSERT (sema != NULL);
	ASSERT (!intr_context ());

	old_level = intr_disable ();
	while (sema->value == 0) {
		/* priority에 따라 semapore의 waiters리스트에 내림차순으로 삽입한다 
		   - donate sema를 하기 위해서 sema를 sort하기에 insert_ordered를 할 필요가 없다 */
		//list_insert_ordered(&sema->waiters, &thread_current()->elem, thread_sort_option, (int *) 1);
		list_push_back (&sema->waiters, &thread_current ()->elem);
		list_sort(&sema->waiters, thread_sort_option, (int *) 1);
		thread_block ();
	}
	sema->value--;
	intr_set_level (old_level);
}

/* Down or "P" operation on a semaphore, but only if the
   semaphore is not already 0.  Returns true if the semaphore is
   decremented, false otherwise.

   This function may be called from an interrupt handler. */
bool
sema_try_down (struct semaphore *sema) {
	enum intr_level old_level;
	bool success;

	ASSERT (sema != NULL);

	old_level = intr_disable ();
	if (sema->value > 0)
	{
		sema->value--;
		success = true;
	}
	else
		success = false;
	intr_set_level (old_level);

	return success;
}

/* Up or "V" operation on a semaphore.  Increments SEMA's value
   and wakes up one thread of those waiting for SEMA, if any.

   This function may be called from an interrupt handler. */
void
sema_up (struct semaphore *sema) {
	enum intr_level old_level;

	ASSERT (sema != NULL);

	old_level = intr_disable ();
	if (!list_empty (&sema->waiters)){
		// sema waiter에 있는 요소를 unblock하기 전에 우선도가 바뀌었을 수 있으므로 정렬해준다.
		list_sort(&sema->waiters, thread_sort_option, (int *) 1);
		thread_unblock (list_entry (list_pop_front (&sema->waiters), struct thread, elem));
	}
	sema->value++;
	thread_preemption(); // 준비리스트에 쓰레드가 들어갔으니 우선도를 체크한다
	intr_set_level (old_level);
}

static void sema_test_helper (void *sema_);

/* Self-test for semaphores that makes control "ping-pong"
   between a pair of threads.  Insert calls to printf() to see
   what's going on. */
void
sema_self_test (void) {
	struct semaphore sema[2];
	int i;

	printf ("Testing semaphores...");
	sema_init (&sema[0], 0);
	sema_init (&sema[1], 0);
	thread_create ("sema-test", PRI_DEFAULT, sema_test_helper, &sema);
	for (i = 0; i < 10; i++)
	{
		sema_up (&sema[0]);
		sema_down (&sema[1]);
	}
	printf ("done.\n");
}

/* Thread function used by sema_self_test(). */
static void
sema_test_helper (void *sema_) {
	struct semaphore *sema = sema_;
	int i;

	for (i = 0; i < 10; i++)
	{
		sema_down (&sema[0]);
		sema_up (&sema[1]);
	}
}

/* Initializes LOCK.  A lock can be held by at most a single
   thread at any given time.  Our locks are not "recursive", that
   is, it is an error for the thread currently holding a lock to
   try to acquire that lock.

   A lock is a specialization of a semaphore with an initial
   value of 1.  The difference between a lock and such a
   semaphore is twofold.  First, a semaphore can have a value
   greater than 1, but a lock can only be owned by a single
   thread at a time.  Second, a semaphore does not have an owner,
   meaning that one thread can "down" the semaphore and then
   another one "up" it, but with a lock the same thread must both
   acquire and release it.  When these restrictions prove
   onerous, it's a good sign that a semaphore should be used,
   instead of a lock. */
void
lock_init (struct lock *lock) {
	ASSERT (lock != NULL);

	lock->holder = NULL;
	sema_init (&lock->semaphore, 1);
}

/* Acquires LOCK, sleeping until it becomes available if
   necessary.  The lock must not already be held by the current
   thread.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but interrupts will be turned back on if
   we need to sleep. */
void
lock_acquire (struct lock *lock) {
	// 지정된 lock이 존재하고, 문맥 전환이 일어나지 않는 상태고, 현재 쓰레드가 lock을 가지고 있지 않음.
	ASSERT (lock != NULL);
	ASSERT (!intr_context ());
	ASSERT (!lock_held_by_current_thread (lock));

	if (!thread_mlfqs){ // mlfqs 스케줄러 활성화 시 priority donation 관련 코드 비활성화

		if(lock->holder != NULL){		// 다른 어떤 쓰레드가 lock을 가지고 있는 상태
			struct thread * lock_holder = lock->holder;
			
			list_push_back(&lock_holder->donate_list, &thread_current()->d_elem);	// lock_holder를 donate_list에 넣어줌
			thread_current()->wait_on_lock = lock; // 쓰레드가 기다리는 lock을 명시해준다
			
			// donate_list에서 priority가 가장 높은 쓰레드의 d_elem을 가져와줌
			struct list_elem * donor_elem = list_max(&lock_holder->donate_list, donate_max_option, NULL);
			// priority donation이 발생하는 부분
			lock_holder->priority = list_entry(donor_elem, struct thread, d_elem)->priority;

			// donate_priority 선언하고, 이는 lock을 가지고 있는 쓰레드의 priority를 의미한다
			int donated_priority = lock_holder->priority;
			// lock을 가지고 있는 쓰레드가 또 다른 lock을 기다리고 있다면
			while (lock_holder->wait_on_lock != NULL)
			{
				// lock을 가지고 있는 쓰레드가 기다리는 lock의 소유자로 lock_holder를 갱신한다
				lock_holder = lock_holder->wait_on_lock->holder;
				// lock을 가지고 있는 쓰레드의 priority가 donated_priority보다 작다면, priority 갱신
				if(lock_holder->priority < donated_priority)
					lock_holder->priority = donated_priority;
				else
					break;
			}
		}
	}
	//현재 쓰레드가 락을 획득할 수 있을 때까지 현재 lock의 semaphore의 waiter에 들어가고 block됨 
	sema_down (&lock->semaphore);
	// 현재 쓰레드가 락을 획득하고 wait on lock을 갱신함
	lock->holder = thread_current();

	if (!thread_mlfqs) // mlfqs 스케줄러 활성화 시 priority donation 관련 코드 비활성화
		thread_current()->wait_on_lock = NULL;
}

/* Tries to acquires LOCK and returns true if successful or false
   on failure.  The lock must not already be held by the current
   thread.

   This function will not sleep, so it may be called within an
   interrupt handler. */
bool
lock_try_acquire (struct lock *lock) {
	bool success;

	ASSERT (lock != NULL);
	ASSERT (!lock_held_by_current_thread (lock));

	success = sema_try_down (&lock->semaphore);
	if (success)
		lock->holder = thread_current ();
	return success;
}

/* Releases LOCK, which must be owned by the current thread.
   This is lock_release function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to release a lock within an interrupt
   handler. */
void
lock_release (struct lock *lock) {
	// 락 자체가 존재하고, 현재 쓰레드는 lock을 가지고 있음.
	ASSERT (lock != NULL);
	ASSERT (lock_held_by_current_thread (lock));

	if (!thread_mlfqs){ // mlfqs 스케줄러 활성화 시 priority donation 관련 코드 비활성화
		// 현재 쓰레드의 donate_list가 비어있지 않음.
		if(!list_empty(&thread_current()->donate_list)){
			// donate_list의 첫 번째 d_elem을 받아옴.
			struct list_elem * donor_elem = list_begin(&thread_current()->donate_list);

			// donor_elem이 현재 쓰레드의 donate_list의 tail이 될 때까지
			while (donor_elem != &thread_current()->donate_list.tail)
			{
				// donor_elem을 가진 쓰레드가 해당 lock을 기다리고 있을 때 donate_list에서 제거
				if(list_entry(donor_elem, struct thread, d_elem)->wait_on_lock == lock)
					donor_elem = list_remove(donor_elem);
				else
					donor_elem = donor_elem->next;
			}

			// 현재 쓰레드의 donate_list가 비어있지 않음.
			if(!list_empty(&thread_current()->donate_list)){ 
				// donate_list에서 priority가 가장 높은 쓰레드의 d_elem을 가져옴
				donor_elem = list_max(&thread_current()->donate_list, donate_max_option, NULL);
				// 현재 쓰레드의 priority를 donor_elem을 가지고 있는 쓰레드의 priority값으로 갱신해줌
				thread_current()->priority = list_entry(donor_elem, struct thread, d_elem)->priority;
			}
			/* 현재쓰레드의 donate_list가 비어있다면 = 현재쓰레드가 보유중인 락을 요구하는 쓰레드 중
				현재쓰레드의 우선순위가 가장 높다면 현재쓰레드의 priority가 original_priority로 돌아옴*/
			else
				thread_current()->priority = thread_current()->original_priority;
		}
	}
	// holder의 lock 소유권을 해제하고 Sema_up을 해줌
	lock->holder = NULL;
	sema_up (&lock->semaphore);
}

/* Returns true if the current thread holds LOCK, false
   otherwise.  (Note that testing whether some other thread holds
   a lock would be racy.) */
bool
lock_held_by_current_thread (const struct lock *lock) {
	ASSERT (lock != NULL);

	return lock->holder == thread_current ();
}

/* One semaphore in a list. */
struct semaphore_elem {
	struct list_elem elem;              /* List element. */
	struct semaphore semaphore;         /* This semaphore. */
};

/* Initializes condition variable COND.  A condition variable
   allows one piece of code to signal a condition and cooperating
   code to receive the signal and act upon it. */
void
cond_init (struct condition *cond) {
	ASSERT (cond != NULL);

	list_init (&cond->waiters);
}

/* 비교함수: condition.waiters 리스트에서 semaphore_elem을 가져오고
	각 semaphore_elem 안의 semaphore에서 waters의 첫 쓰레드의 prority를 비교한다 */
bool
compare_priority(const struct list_elem *a, const struct list_elem *b, void *aux){
	const struct semaphore_elem *sema_a = list_entry(a, struct semaphore_elem, elem);
    const struct semaphore_elem *sema_b = list_entry(b, struct semaphore_elem, elem);

	struct list_elem *elem_a = list_begin(&sema_a->semaphore.waiters);
	struct list_elem *elem_b = list_begin(&sema_b->semaphore.waiters);

	return thread_sort_option(elem_a, elem_b, (int*) 1);
}

/* Atomically releases LOCK and waits for COND to be signaled by
   some other piece of code.  After COND is signaled, LOCK is
   reacquired before returning.  LOCK must be held before calling
   this function.

   The monitor implemented by this function is "Mesa" style, not
   "Hoare" style, that is, sending and receiving a signal are not
   an atomic operation.  Thus, typically the caller must recheck
   the condition after the wait completes and, if necessary, wait
   again.

   A given condition variable is associated with only a single
   lock, but one lock may be associated with any number of
   condition variables.  That is, there is a one-to-many mapping
   from locks to condition variables.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but interrupts will be turned back on if
   we need to sleep. */
void
cond_wait (struct condition *cond, struct lock *lock) {
	struct semaphore_elem waiter;
	enum intr_level old_level;

	ASSERT (cond != NULL);
	ASSERT (lock != NULL);
	ASSERT (!intr_context ());
	ASSERT (lock_held_by_current_thread (lock));

	old_level = intr_disable ();
	sema_init (&waiter.semaphore, 0);
	/* semaphor의 첫 쓰레드의 우선도를 비교하여 semaphore_elem의 리스트에 내림차순으로 넣는다 */
	// list_insert_ordered(&cond->waiters, &waiter.elem, compare_priority, NULL);
	list_push_back (&cond->waiters, &waiter.elem);
	list_sort(&cond->waiters, compare_priority, NULL);
	lock_release (lock);
	sema_down (&waiter.semaphore);
	intr_set_level(old_level);
	lock_acquire (lock);
}

/* If any threads are waiting on COND (protected by LOCK), then
   this function signals one of them to wake up from its wait.
   LOCK must be held before calling this function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to signal a condition variable within an
   interrupt handler. */
void
cond_signal (struct condition *cond, struct lock *lock UNUSED) {
	enum intr_level old_level;
	ASSERT (cond != NULL);
	ASSERT (lock != NULL);
	ASSERT (!intr_context ());
	ASSERT (lock_held_by_current_thread (lock));

	if (!list_empty (&cond->waiters)){
	    old_level = intr_disable ();
	    list_sort(&cond->waiters, compare_priority, NULL);
		sema_up (&list_entry (list_pop_front (&cond->waiters),
					struct semaphore_elem, elem)->semaphore);
		intr_set_level(old_level);
	}
}

/* Wakes up all threads, if any, waiting on COND (protected by
   LOCK).  LOCK must be held before calling this function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to signal a condition variable within an
   interrupt handler. */
void
cond_broadcast (struct condition *cond, struct lock *lock) {
	ASSERT (cond != NULL);
	ASSERT (lock != NULL);

	while (!list_empty (&cond->waiters))
		cond_signal (cond, lock);
}
