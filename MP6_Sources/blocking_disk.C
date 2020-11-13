/*
     File        : blocking_disk.c

     Author      : 
     Modified    : 

     Description : 

*/

/*--------------------------------------------------------------------------*/
/* DEFINES */
/*--------------------------------------------------------------------------*/

    /* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* INCLUDES */
/*--------------------------------------------------------------------------*/

#include "assert.H"
#include "utils.H"
#include "console.H"
#include "blocking_disk.H"
#include "machine.H"

/*--------------------------------------------------------------------------*/
/* EXTERNS */
/*--------------------------------------------------------------------------*/

extern Scheduler* SYSTEM_SCHEDULER;

Queue BlockingDisk::block_queue;

Queue MirroredDisk::master_block_queue;
Queue MirroredDisk::slave_block_queue;

/*--------------------------------------------------------------------------*/
/* CONSTRUCTOR */
/*--------------------------------------------------------------------------*/

BlockingDisk::BlockingDisk(DISK_ID _disk_id, unsigned int _size) : SimpleDisk(_disk_id, _size) {
}

/*--------------------------------------------------------------------------*/
/* SIMPLE_DISK FUNCTIONS */
/*--------------------------------------------------------------------------*/

void BlockingDisk::read(unsigned long _block_no, unsigned char * _buf) {

	// Issue read disk operation
	issue_operation(READ, _block_no);
	
	bool cont = true; // Boolean variable to continue the loop

	// Get the current disk accessing thread 
	Thread* currThread = Thread::CurrentThread();	

	// Register a new thread to the block_thread queue
	block_queue.enqueue(currThread);

	// If disk thread gains CPU access, check if the disk is ready
	// If the disk is not ready, yield the CPU to other threads
	while (!is_ready() && cont) {
		currThread = Thread::CurrentThread();

		if (block_queue.front() != NULL) {
			// If current thread is in front of queue, stop the loop
			if (currThread->get_thread_id() == block_queue.front()->get_thread_id()) {
				cont = false;
			}
			// If the disk is ready but the thread is not in front queue, continue to yeild
			else {
				cont = true;
				SYSTEM_SCHEDULER->resume(currThread);
				SYSTEM_SCHEDULER->yield();	
			}
		}		
	}
	
	// Delete the current thread from the block_thread queue
	block_queue.dequeue();
	
	// If the disk is ready, process I/O reading operation;
	int i;
	unsigned short tmpw;
	for (i = 0; i < 256; i++) {
		tmpw = Machine::inportw(0x1F0);
		_buf[i*2]   = (unsigned char)tmpw;
		_buf[i*2+1] = (unsigned char)(tmpw >> 8);
	}
}


void BlockingDisk::write(unsigned long _block_no, unsigned char * _buf) {

	// Issue write disk operation
	issue_operation(WRITE, _block_no);
	
	bool cont = true; // Boolean variable to continue the loop

	// Get the current disk accessing thread 
	Thread* currThread = Thread::CurrentThread();	

	// Register a new thread to the block_thread queue
	block_queue.enqueue(currThread);

	// If disk thread gains CPU access, check if the disk is ready
	// If the disk is not ready, yield the CPU to other threads
	while (!is_ready() && cont) {
		currThread = Thread::CurrentThread();

		if (block_queue.front() != NULL) {
			// If current thread is in front of queue, stop the loop
			if (currThread->get_thread_id() == block_queue.front()->get_thread_id()) {
				cont = false;
			}
			// If the disk is ready but the thread is not in front queue, continue to yeild
			else {
				cont = true;
				SYSTEM_SCHEDULER->resume(currThread);
				SYSTEM_SCHEDULER->yield();	
			}
		}		
	}
	
	// Delete the current thread from the block_thread queue
	block_queue.dequeue();

	// If the disk is ready, process I/O writing operation;
	int i; 
	unsigned short tmpw;
	for (i = 0; i < 256; i++) {
		tmpw = _buf[2*i] | (_buf[2*i+1] << 8);
		Machine::outportw(0x1F0, tmpw);
	}
}

/*--------------------------------------------------------------------------*/
/* MIRRORED DISK CODE */
/*--------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------*/
/* CONSTRUCTOR */
/*--------------------------------------------------------------------------*/

MirroredDisk::MirroredDisk(DISK_ID _disk_id_master, DISK_ID _disk_id_slave, unsigned int _size) 
  :SimpleDisk(master_disk_id, _size){
	master_disk_id = _disk_id_master;
	slave_disk_id = _disk_id_slave;
 
	SLAVE_DISK = new SimpleDisk(slave_disk_id, _size);
}
/*--------------------------------------------------------------------------*/
/* MIRRORED DISK FUNCTIONS */
/*--------------------------------------------------------------------------*/

void MirroredDisk::read(unsigned long _block_no, unsigned char * _buf) {

	// Issue read disk operation
	disk_id = master_disk_id;
	issue_operation(READ, _block_no);

	bool cont = true; // Boolean variable to continue the loop

	// Get the current disk accessing thread 
	Thread* currThread = Thread::CurrentThread();	

	// Register a new thread to the block_thread queue
	master_block_queue.enqueue(currThread);
	slave_block_queue.enqueue(currThread);

	// If disk thread gains CPU access, check if the disk is ready
	// If the disk is not ready, yield the CPU to other threads
	while ((!is_ready() && !SLAVE_DISK->is_ready()) && cont) {
		currThread = Thread::CurrentThread();

		if (master_block_queue.front() != NULL && slave_block_queue.front() != NULL) {

			int currThread_id = currThread->get_thread_id();
			int master_thread_id = master_block_queue.front()->get_thread_id();
			int slave_thread_id = slave_block_queue.front()->get_thread_id();
			
			// If current thread is in front of queue, stop the loop
			if (currThread_id == master_thread_id || currThread_id == slave_thread_id) {
				cont = false;
			}
			// If the disk is ready but the thread is not in front queue, continue to yeild
			else {
				cont = true;
				SYSTEM_SCHEDULER->resume(currThread);
				SYSTEM_SCHEDULER->yield();	
			}
		}		
	}
	
	// Delete the current thread from the block_thread queue
	master_block_queue.dequeue();
	slave_block_queue.dequeue();
	
	// If the disk is ready, process I/O reading operation;
	disk_id = master_disk_id;
	int i;
	unsigned short tmpw;
	for (i = 0; i < 256; i++) {
		tmpw = Machine::inportw(0x1F0);
		_buf[i*2]   = (unsigned char)tmpw;
		_buf[i*2+1] = (unsigned char)(tmpw >> 8);
	}

	disk_id = slave_disk_id;
	issue_operation(READ, _block_no);

	for (i = 0; i < 256; i++) {
		tmpw = Machine::inportw(0x1F0);
		_buf[i*2]   = (unsigned char)tmpw;
		_buf[i*2+1] = (unsigned char)(tmpw >> 8);
	}
}


void MirroredDisk::write(unsigned long _block_no, unsigned char * _buf) {

	// Issue read disk operation
	disk_id = master_disk_id;
	issue_operation(WRITE, _block_no);
	
	bool cont = true; // Boolean variable to continue the loop

	// Get the current disk accessing thread 
	Thread* currThread = Thread::CurrentThread();	

	// Register a new thread to the block_thread queue
	master_block_queue.enqueue(currThread);
	slave_block_queue.enqueue(currThread);

	// If disk thread gains CPU access, check if the disk is ready
	// If the disk is not ready, yield the CPU to other threads
	while ((!is_ready() || !SLAVE_DISK->is_ready()) && cont) {
		currThread = Thread::CurrentThread();

		if (master_block_queue.front() != NULL && slave_block_queue.front() != NULL) {

			int currThread_id = currThread->get_thread_id();
			int master_thread_id = master_block_queue.front()->get_thread_id();
			int slave_thread_id = slave_block_queue.front()->get_thread_id();
			
			// If current thread is in front of queue, stop the loop
			if (currThread_id == master_thread_id && currThread_id == slave_thread_id) {
				cont = false;
			}
			// If the disk is ready but the thread is not in front queue, continue to yeild
			else {
				cont = true;
				SYSTEM_SCHEDULER->resume(currThread);
				SYSTEM_SCHEDULER->yield();	
			}
		}		
	}
	
	// Delete the current thread from the block_thread queue
	master_block_queue.dequeue();
	slave_block_queue.dequeue();

	// If the disk is ready, process I/O writing operation;
	disk_id = master_disk_id;
	int i; 
	unsigned short tmpw;
	for (i = 0; i < 256; i++) {
		tmpw = _buf[2*i] | (_buf[2*i+1] << 8);
		Machine::outportw(0x1F0, tmpw);
	}

	disk_id = slave_disk_id;
	issue_operation(WRITE, _block_no);

	for (i = 0; i < 256; i++) {
		tmpw = _buf[2*i] | (_buf[2*i+1] << 8);
		Machine::outportw(0x1F0, tmpw);
	}
}