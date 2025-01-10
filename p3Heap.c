/*
 * Dhruv Butani - Heap Allocator - UW Madison CS354
 * Copyright 2020-2024 Deb Deppeler based on work by Jim Skrentny
 */

#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <string.h>
#include "p3Heap.h"

/*
 * This structure serves as the header for each allocated and free block.
 * It also serves as the footer for each free block.
 */
typedef struct blockHeader {           

    /*
     * 1) The size of each heap block must be a multiple of 8
     * 2) heap blocks have blockHeaders that contain size and status bits
     * 3) free heap block contain a footer, but we can use the blockHeader 
     *.
     * All heap blocks have a blockHeader with size and status
     * Free heap blocks have a blockHeader as its footer with size only
     *
     * Status is stored using the two least significant bits.
     *   Bit0 => least significant bit, last bit
     *   Bit0 == 0 => free block
     *   Bit0 == 1 => allocated block
     *
     *   Bit1 => second last bit 
     *   Bit1 == 0 => previous block is free
     *   Bit1 == 1 => previous block is allocated
     * 
     * Start Heap: 
     *  The blockHeader for the first block of the heap is after skip 4 bytes.
     *  This ensures alignment requirements can be met.
     * 
     * End Mark: 
     *  The end of the available memory is indicated using a size_status of 1.
     * 
     */
    int size_status;

} blockHeader;         

/* Global variable
 * It must point to the first block in the heap and is set by init_heap()
 * i.e., the block at the lowest address.
 */
blockHeader *heap_start = NULL;     

/* Size of heap allocation padded to round to nearest page size. */
int alloc_size;

unsigned int aBit = 1; 
unsigned int pBit = 2; 
unsigned int sMask = ~7; 




/* 
 * Function for allocating 'size' bytes of heap memory.
 * Argument size: requested size for the payload
 * Returns address of allocated block (payload) on success.
 * Returns NULL on failure.
 *
 * - BEST-FIT PLACEMENT POLICY to chose a free block
 *
 * - If the BEST-FIT block that is found is exact size match
 *   - 1. Update all heap blocks as needed for any affected blocks
 *   - 2. Return the address of the allocated block payload
 *
 * - If the BEST-FIT block that is found is large enough to split 
 *   - 1. SPLIT the free block into two valid heap blocks:
 *         1. an allocated block
 *         2. a free block
 *         NOTE: both blocks must meet heap block requirements 
 *       - Update all heap block header(s) and footer(s) 
 *              as needed for any affected blocks.
 *   - 2. Return the address of the allocated block payload
 *
 *   Return if NULL unable to find and allocate block for required size
 *
 */
void* alloc(int size) {     
    
  
    int blockSize = size + 4;
    
    if (size < 1 || blockSize > alloc_size) {
        return NULL;
    }

    if (blockSize % 8 != 0) {
        blockSize = (blockSize + 7) & ~7;
    }

    blockHeader *temp = heap_start;
    int tempSize = (temp->size_status) & sMask;
    int tempLoc = (temp->size_status) % 8; 

    
    blockHeader *bf = heap_start;
    int bfs = 0;

    blockHeader *nextHeader = (blockHeader*)((char*)temp + tempSize);

    int exact = 0;
    int partial = 0;

    while(exact == 0 && temp->size_status != 1) {

        tempLoc = (temp->size_status) % 8; 
        tempSize = (temp->size_status) & sMask;
        
        nextHeader = (blockHeader*)((char*)temp + tempSize);

        

        if (tempLoc == 0 || tempLoc == 2) {

            if (tempSize > blockSize && (tempSize <= bfs || bfs == 0)) {
                bf = temp;
                bfs = (bf->size_status) & sMask;

                partial = 1;
            } 

            else if (tempSize == blockSize) {
                exact = 1;
                bf = temp;
                bfs = (bf->size_status) & sMask;
                
                if (nextHeader->size_status != 1) {
                    nextHeader->size_status += 2;        
                }
                bf->size_status += 1;
            }
        }

        temp = (blockHeader*)((char*)temp + tempSize);
    }

    if (exact == 0 && partial == 0) {
        return NULL;
    }

    if (blockSize < bfs && (bfs - blockSize >= 8)) {
        blockHeader *splitBlock = (blockHeader*)((char*)bf + blockSize);

        int remainder = bfs - blockSize;
        
		splitBlock->size_status = remainder + 2; 
        
        if (bf == heap_start) { 
            bf->size_status = blockSize + 3;
        } 
        else { 
            tempLoc = (temp->size_status) % 8 + 1;
            bf->size_status = blockSize + tempLoc + 1;
        }
    }

    return bf+1;

} 

/* 
 * Function for freeing up a previously allocated block.
 * Argument ptr: address of the block to be freed up.
 * Returns 0 on success.
 * Returns -1 on failure.
 * This function Will:
 * - Return -1 if ptr is NULL.
 * - Return -1 if ptr is not a multiple of 8.
 * - Return -1 if ptr is outside of the heap space.
 * - Return -1 if ptr block is already freed.
 * - Update header(s) and footer as needed.
 *
 * If free results in two or more adjacent free blocks,
 * they will be immediately coalesced into one larger free block.
 * so free blocks require a footer (blockHeader works) to store the size
 */                    
int free_block(void *ptr) {

    if((ptr == NULL) || (((int) ptr % 8 != 0))) {
        return -1;
    }

    if(((int)ptr > ((int)heap_start + alloc_size)) || ((int)ptr < (int)heap_start)) {
        return -1;
    }

    blockHeader *header = (blockHeader *) ( (char *) ptr - sizeof(blockHeader));
    int headerSize = (header->size_status) & sMask;
    int headerStatus = (header->size_status) & aBit;

    if(headerStatus == 0) {
        return -1;
    }

    blockHeader *footer = (blockHeader *) ((char *) header + headerSize - sizeof(blockHeader));
    footer->size_status = (header->size_status) & sMask;
    

    blockHeader *next = (blockHeader *) ((char *) header + headerSize);


    header->size_status -=1;

    if(next->size_status != 1) {
        (next->size_status) -=2;
    }

    return 0;
    
} 

/* 
 * Initializes the memory allocator.
 * Called once by a program.
 * Argument sizeOfRegion: the size of the heap space to be allocated.
 * Returns 0 on success.
 * Returns -1 on failure.
 */                    
int init_heap(int sizeOfRegion) {    

    static int allocated_once = 0; //prevent multiple myInit calls

    int   pagesize; // page size
    int   padsize;  // size of padding when heap size not a multiple of page size
    void* mmap_ptr; // pointer to memory mapped area
    int   fd;

    blockHeader* end_mark;

    if (0 != allocated_once) {
        fprintf(stderr, 
                "Error: mem.c: InitHeap has allocated space during a previous call\n");
        return -1;
    }

    if (sizeOfRegion <= 0) {
        fprintf(stderr, "Error: mem.c: Requested block size is not positive\n");
        return -1;
    }

    // Get the pagesize from O.S. 
    pagesize = getpagesize();

    // Calculate padsize as the padding required to round up sizeOfRegion 
    // to a multiple of pagesize
    padsize = sizeOfRegion % pagesize;
    padsize = (pagesize - padsize) % pagesize;

    alloc_size = sizeOfRegion + padsize;

    // Using mmap to allocate memory
    fd = open("/dev/zero", O_RDWR);
    if (-1 == fd) {
        fprintf(stderr, "Error:mem.c: Cannot open /dev/zero\n");
        return -1;
    }
    mmap_ptr = mmap(NULL, alloc_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
    if (MAP_FAILED == mmap_ptr) {
        fprintf(stderr, "Error:mem.c: mmap cannot allocate space\n");
        allocated_once = 0;
        return -1;
    }

    allocated_once = 1;

    // for double word alignment and end mark
    alloc_size -= 8;

    // Initially there is only one big free block in the heap.
    // Skip first 4 bytes for double word alignment requirement.
    heap_start = (blockHeader*) mmap_ptr + 1;

    // Set the end mark
    end_mark = (blockHeader*)((void*)heap_start + alloc_size);
    end_mark->size_status = 1;

    // Set size in header
    heap_start->size_status = alloc_size;

    // Set p-bit as allocated in header
    // note a-bit left at 0 for free
    heap_start->size_status += 2;

    // Set the footer
    blockHeader *footer = (blockHeader*) ((void*)heap_start + alloc_size - 4);
    footer->size_status = alloc_size;

    return 0;
} 

/* 
 * 
 * Prints out a list of all the blocks including this information:
 * No.      : serial number of the block 
 * Status   : free/used (allocated)
 * Prev     : status of previous block free/used (allocated)
 * t_Begin  : address of the first byte in the block (where the header starts) 
 * t_End    : address of the last byte in the block 
 * t_Size   : size of the block as stored in the block header
 */                     
void disp_heap() {     

    int    counter;
    char   status[6];
    char   p_status[6];
    char * t_begin = NULL;
    char * t_end   = NULL;
    int    t_size;

    blockHeader *current = heap_start;
    counter = 1;

    int used_size =  0;
    int free_size =  0;
    int is_used   = -1;

    fprintf(stdout, 
            "********************************** HEAP: Block List ****************************\n");
    fprintf(stdout, "No.\tStatus\tPrev\tt_Begin\t\tt_End\t\tt_Size\n");
    fprintf(stdout, 
            "--------------------------------------------------------------------------------\n");

    while (current->size_status != 1) {
        t_begin = (char*)current;
        t_size = current->size_status;

        if (t_size & 1) {
            // LSB = 1 => used block
            strcpy(status, "alloc");
            is_used = 1;
            t_size = t_size - 1;
        } else {
            strcpy(status, "FREE ");
            is_used = 0;
        }

        if (t_size & 2) {
            strcpy(p_status, "alloc");
            t_size = t_size - 2;
        } else {
            strcpy(p_status, "FREE ");
        }

        if (is_used) 
            used_size += t_size;
        else 
            free_size += t_size;

        t_end = t_begin + t_size - 1;

        fprintf(stdout, "%d\t%s\t%s\t0x%08lx\t0x%08lx\t%4i\n", counter, status, 
                p_status, (unsigned long int)t_begin, (unsigned long int)t_end, t_size);

        current = (blockHeader*)((char*)current + t_size);
        counter = counter + 1;
    }

    fprintf(stdout, 
            "--------------------------------------------------------------------------------\n");
    fprintf(stdout, 
            "********************************************************************************\n");
    fprintf(stdout, "Total used size = %4d\n", used_size);
    fprintf(stdout, "Total free size = %4d\n", free_size);
    fprintf(stdout, "Total size      = %4d\n", used_size + free_size);
    fprintf(stdout, 
            "********************************************************************************\n");
    fflush(stdout);

    return;  
}            
                                       
