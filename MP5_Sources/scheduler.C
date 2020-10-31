/*
 File: scheduler.C
 
 Author:
 Date  :
 
 */

/*--------------------------------------------------------------------------*/
/* DEFINES */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* INCLUDES */
/*--------------------------------------------------------------------------*/

#include "scheduler.H"
#include "thread.H"
#include "console.H"
#include "utils.H"
#include "assert.H"
#include "simple_keyboard.H"

/*--------------------------------------------------------------------------*/
/* DATA STRUCTURES */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* CONSTANTS */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* FORWARDS */
/*--------------------------------------------------------------------------*/

extern Thread* current_thread;

/*--------------------------------------------------------------------------*/
/* METHODS FOR CLASS   S c h e d u l e r  */
/*--------------------------------------------------------------------------*/

Scheduler::Scheduler() {
	thread_list->thread = NULL;
	thread_list->next = NULL;	

  Console::puts("Constructed Scheduler.\n");
}

void Scheduler::yield() {
	Node* curr = thread_list;
	if (curr->thread == NULL){
		Console::puts("Yielded thread did not exist.\n");
	}
  else {
		// Had an item, check if it has a next item in ll
		if (thread_list->next != NULL) {
			thread_list = thread_list->next;
		}
    else {
			Console::puts("Yielded thread had no next thread.\n");
		}
		Thread::dispatch_to(curr->thread);
	//	Console::puts("Scheduler yielded thread.\n");
	}
}

void Scheduler::resume(Thread * _thread) {
	Node* curr = new Node;
	curr->thread = _thread;
	curr->next = NULL;
	// Item in list, traverse to end in FIFO fashion
	if(thread_list->thread != NULL) {
		Node* traversal = thread_list;
		while (traversal->next != NULL){
			traversal = traversal->next;
		}
		traversal->next = curr;
	}
  else {
	// No item, set curr to first item in list
		thread_list = curr;
	}
}

void Scheduler::add(Thread * _thread) {
	Node* curr = new Node;
	curr->thread = _thread;
	curr->next = NULL;
	// Item in list, traverse to end in FIFO fashion
	if(thread_list->thread != NULL) {
		Node* traversal = thread_list;
		while (traversal->next != NULL){
			Console::puts("Adding thread AFTER ");
			Console::puti(traversal->thread->ThreadId());
			Console::puts("\n");
			traversal = traversal->next;
		}
		traversal->next = curr;
	}
  else {
	// No item, set curr to first item in list
		thread_list = curr;
	}
}

void Scheduler::terminate(Thread * _thread) {
	if(thread_list->thread != NULL) {
		Node* traversal = thread_list;
		
		while(traversal->next != NULL) {
			if(traversal->thread->ThreadId() == _thread->ThreadId()) {
				traversal->thread = NULL;
			}
			traversal = traversal->next;
		}
		if (traversal->thread->ThreadId() == _thread->ThreadId()){
			traversal->thread = NULL;
		}	
	} 
	Console::puts("Terminated\n");
}
