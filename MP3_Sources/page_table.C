#include "assert.H"
#include "exceptions.H"
#include "console.H"
#include "paging_low.H"
#include "page_table.H"

PageTable * PageTable::current_page_table = NULL;
unsigned int PageTable::paging_enabled = 0;
ContFramePool * PageTable::kernel_mem_pool = NULL;
ContFramePool * PageTable::process_mem_pool = NULL;
unsigned long PageTable::shared_size = 0;

void PageTable::init_paging(ContFramePool * _kernel_mem_pool,
                            ContFramePool * _process_mem_pool,
                            const unsigned long _shared_size) 
{
   PageTable::kernel_mem_pool = _kernel_mem_pool;
   PageTable::process_mem_pool = _process_mem_pool;
   PageTable::shared_size = _shared_size;
   Console::puts("Initialized Paging System\n");
}

PageTable::PageTable() {
//initialize if page directory null
   if (page_directory == NULL){
      //Put the page_directory in kernel_mem_pool
      page_directory = ( unsigned long*)(kernel_mem_pool->get_frames(1)*PAGE_SIZE); 
      
      //Direct map page_table
      unsigned long *kernelPageTable = (unsigned long*)(kernel_mem_pool->get_frames(1)*PAGE_SIZE); 
      unsigned long  addr = 0;

      for (unsigned int i = 0; i < ENTRIES_PER_PAGE; i++){
      kernelPageTable[i] = addr| 0x3;
      addr += PAGE_SIZE;
      }
      page_directory[0] = (unsigned long) kernelPageTable| 0x3;
   }
   for(unsigned int i = 1; i < ENTRIES_PER_PAGE; i++)
      page_directory[i] = 0 | 0x2; 
  
   Console::puts("Constructed Page Table object\n");
}

void PageTable::load() {
    current_page_table = this;
    write_cr3((unsigned long) this->page_directory);
    Console::puts("Loaded page table\n");
}

void PageTable::enable_paging() {
    paging_enabled = 1;
    write_cr0(read_cr0() | 0x80000000);
    Console::puts("Enabled paging\n");
}

void PageTable::handle_fault(REGS * _r) {
    unsigned long errorCode = _r->err_code;

    if((errorCode & 0x1) == 1)
      Console::puts("Protection Fault!\n");
      
    else { 

      unsigned long *pageDir = (unsigned long *)read_cr3();      
      unsigned long addr = read_cr2();

      unsigned long *pageTable;

      unsigned long page_tab_index = (addr / PAGE_SIZE) & 0x3FF;
      unsigned long page_dir_index = addr / (ENTRIES_PER_PAGE*PAGE_SIZE);

      if(pageDir[page_dir_index] & 0x1 == 0x1) {  
        pageTable = (unsigned long *) ((pageDir[page_dir_index]) & 0xFFFFF000);
      } 
      else {  
        pageDir[page_dir_index] = (unsigned long)((kernel_mem_pool->get_frames(1)*PAGE_SIZE) | 0x3); 
        pageTable = (unsigned long *)((pageDir[page_dir_index]) & 0xFFFFF000);
        for(unsigned int i = 0; i < ENTRIES_PER_PAGE; i++){
            pageTable[i] = 0;
        }
      }

      pageTable[page_tab_index] = (PageTable::process_mem_pool->get_frames(1)*PAGE_SIZE) | 0x3;
    }

    Console::puts("handled page fault\n");
}

