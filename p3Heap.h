
#ifndef __p3Heap_h
#define __p3Heap_h

int   init_heap(int sizeOfRegion);
void  disp_heap();

void* alloc(int size);
int   free_block(void *ptr);

void* malloc(size_t size) {
    return NULL;
}

#endif

