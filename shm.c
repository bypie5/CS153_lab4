#include "param.h"
#include "types.h"
#include "defs.h"
#include "x86.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "spinlock.h"

struct {
  struct spinlock lock;
  struct shm_page {
    uint id;
    char *frame;
    int refcnt;
  } shm_pages[64];
} shm_table;

void shminit() {
  int i;
  initlock(&(shm_table.lock), "SHM lock");
  acquire(&(shm_table.lock));
  for (i = 0; i< 64; i++) {
    shm_table.shm_pages[i].id =0;
    shm_table.shm_pages[i].frame =0;
    shm_table.shm_pages[i].refcnt =0;
  }
  release(&(shm_table.lock));
}

int shm_open(int id, char **pointer) {
	acquire(&(shm_table.lock));	

	int id_exists = 0;
	int entry_index = -1;
	int i = 0;	
	for (i = 0; i < 64; i++) {
		if (id == shm_table.shm_pages[i].id) {
			id_exists = 1;
			entry_index = i;
			break;
		}
	}	

	struct proc *curproc = myproc();

	if (id_exists && entry_index != -1) {
		// Case 1
		uint va = PGROUNDUP(curproc->sz);
		mappages(curproc->pgdir, (void*) va, PGSIZE, V2P(shm_table.shm_pages[entry_index].frame), PTE_W|PTE_U);
		shm_table.shm_pages[entry_index].refcnt++;

		// Return the pointer to the virtual address
		*pointer=(char *)va;

		// Update sz
		curproc->sz += PGSIZE;
	} else {
		// Case 2
		// Shared memory segment does not exist, so we need to allocate	
		
		// Find an empty entry
		int i = 0;
		int found_index = -1;
		for (i = 0; i < 64; i++) {
			// This is an empty entry (newly initalized)
			if (shm_table.shm_pages[i].id == 0 && shm_table.shm_pages[i].refcnt == 0) {
				found_index = i;
				break;
			}
		}
		// Set values based on the arugments passed in
		shm_table.shm_pages[found_index].id = id;
		shm_table.shm_pages[found_index].frame = kalloc();
		memset(shm_table.shm_pages[found_index].frame, 0, PGSIZE);
		
		// memmap and return pointer through parameter
		uint va = PGROUNDUP(curproc->sz);
		mappages(curproc->pgdir, (void*) va, PGSIZE, (uint) P2V(shm_table.shm_pages[entry_index].frame), PTE_W|PTE_U);
		shm_table.shm_pages[entry_index].refcnt++;

	}

	release(&(shm_table.lock));
	return 0; 
}

int shm_close(int id) {
	// Find shm_page with matching id
	acquire(&(shm_table.lock));
	int i = 0;
	int found_index = -1;
	for (i = 0; i < 64; i++) {
		if(shm_table.shm_pages[i].id == id) {
			found_index = i;
			break;
		}
	}

	// Decrease refcount
	shm_table.shm_pages[found_index].refcnt--;
	
	// Nobody is referencing this shm_page -> clear it!
	if (shm_table.shm_pages[found_index].refcnt == 0) {
		shm_table.shm_pages[found_index].id = 0;
    shm_table.shm_pages[found_index].refcnt = 0;
	}

	release(&(shm_table.lock));
	return 0; 
}

