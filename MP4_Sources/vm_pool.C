/*
 File: vm_pool.C
 
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

#include "vm_pool.H"
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

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* METHODS FOR CLASS   V M P o o l */
/*--------------------------------------------------------------------------*/

VMPool::VMPool(unsigned long  _base_address,
               unsigned long  _size,
               ContFramePool *_frame_pool,
               PageTable     *_page_table) {
    baseAddress = _base_address;
    size = _size;
    framePool = _frame_pool;
    pageTable = _page_table;
    

    listOfRegionDescriptors = (RegionDescriptors*)baseAddress;
    listOfRegionDescriptors[0].addressOfRegion = baseAddress;
    listOfRegionDescriptors[0].length = 4096;
    totalRegions = 1;
    totalRegionsSize = 4096;

    pageTable->register_pool(this);
    Console::puts("Constructed VMPool object.\n");
}

unsigned long VMPool::allocate(unsigned long _size) {

    if (_size == 0) {
    	return 0;
    }

    if (totalRegions >= MAX_REGIONS) {
    	Console::puts("Reach Maximum Number of Regions. Cannot Allocate Any More!!!\n");
    	for(;;);
    }
	
	if ((totalRegionsSize + _size) > size) {
		Console::puts("No More Space in Virtual Memory Pool. Cannot Allocate Any More!!!\n");
    	for(;;);
	}
    unsigned long currentAddressOfRegion;
    if (totalRegions == 0) {
    	currentAddressOfRegion = baseAddress;
    }
    else {
    	unsigned long last_index = totalRegions - 1;
    	currentAddressOfRegion = listOfRegionDescriptors[last_index].addressOfRegion + listOfRegionDescriptors[last_index].length;
    }

    unsigned long current_index = totalRegions;
    listOfRegionDescriptors[current_index].addressOfRegion = currentAddressOfRegion;
    listOfRegionDescriptors[current_index].length = _size;
    totalRegions++;
    totalRegionsSize += _size;

    Console::puts("Allocated region of memory.\n");
    return currentAddressOfRegion;
}

void VMPool::release(unsigned long _start_address) {
    unsigned long region_index;
    for (unsigned long i; i < totalRegions; ++i) {
    	if (listOfRegionDescriptors[i].addressOfRegion == _start_address) {
    		region_index = i;
    		break;
    	}
    }

    // Release all the pages that the region located in
    unsigned long end_address = _start_address + listOfRegionDescriptors[region_index].length;
    unsigned long release_address = _start_address;
    while (release_address < end_address) {
    	pageTable->free_page(release_address);
    	release_address += PageTable::PAGE_SIZE;
    }

    // Decrease the total region size
    totalRegionsSize -= listOfRegionDescriptors[region_index].length;

    for (unsigned long i = region_index; i < totalRegions - 1; ++i) {
    	listOfRegionDescriptors[i].addressOfRegion = listOfRegionDescriptors[i+1].addressOfRegion;
    	listOfRegionDescriptors[i].length = listOfRegionDescriptors[i+1].length;
    }
    listOfRegionDescriptors[totalRegions - 1].addressOfRegion = 0;
    listOfRegionDescriptors[totalRegions - 1].length = 0;
    totalRegions--;


    pageTable->load();
    Console::puts("Released region of memory.\n");
}

bool VMPool::is_legitimate(unsigned long _address) {
    unsigned long min_address, max_address;

    for (unsigned long i = 0; i < totalRegions; i++) {
    	min_address = listOfRegionDescriptors[i].addressOfRegion;
    	max_address = min_address + listOfRegionDescriptors[i].length - 1;
	    if ((_address >= (min_address)) && (_address <= max_address)) {
	    	return true;
	    }
	}
    return false;
    Console::puts("Checked whether address is part of an allocated region.\n");
}

