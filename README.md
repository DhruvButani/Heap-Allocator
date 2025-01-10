# Heap-Allocator

This project implements a custom heap allocator in C, designed to simulate dynamic memory management. The allocator supports allocating and freeing blocks of memory, as well as managing fragmentation. It uses a best-fit placement policy to allocate memory and ensures proper block splitting and coalescing of free blocks.

Key features of the allocator include:

Best-fit allocation policy: Finds the best-fit free block for the requested size.

Block splitting: If a free block is larger than the requested size, it splits into two blocks: one allocated and the other remaining free.

Coalescing free blocks: Automatically merges adjacent free blocks into one larger free block when a block is freed.

Memory management: Uses a header and footer to store block size and allocation status, ensuring efficient memory management and alignment.

This project is implemented as part of the University of Wisconsin-Madison’s CS 354 course on Computer Organization and Programming.

