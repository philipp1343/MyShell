#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <dlfcn.h>
#include <sys/mman.h>
#include <stdint.h>
#include <stdbool.h>

size_t ALIGN(size_t x, size_t y) {
    return (x + (y - 1)) & ~(y - 1);
}
struct obj_metadata
{
    struct obj_metadata *prev;
    struct obj_metadata *next;
    size_t size;
    int is_free;
};
bool foundBlock= false;
const size_t pageSize = 4096;
const size_t memALignment = 8;
const size_t meta_data_size = 32;
struct obj_metadata *inBand_freeList = NULL;
struct obj_metadata *makePrev = NULL;

void *mymalloc(size_t size)
{
    size = ALIGN(size, memALignment);

    if (inBand_freeList == NULL){
        inBand_freeList = sbrk(pageSize);
        inBand_freeList->size = pageSize - meta_data_size;
        inBand_freeList->next = NULL;
        inBand_freeList->prev = NULL;
        inBand_freeList->is_free = 1;
    }

    struct obj_metadata *lastBlock = NULL;
    struct obj_metadata *metaBlock = inBand_freeList;
    while (metaBlock != NULL){
            if(metaBlock->next == NULL){
                lastBlock = metaBlock;
            }
            if (metaBlock->is_free == 1 && metaBlock->size >= size){
                foundBlock = true;
                break;
            }else{
                metaBlock = metaBlock->next;
            }
        }

    size_t memToAdd = ALIGN(size, pageSize);
    if (memToAdd == 0) memToAdd = pageSize;
    
    if (foundBlock){
        
        if ((metaBlock->size - size) > meta_data_size){
            struct obj_metadata *next = metaBlock->next;
            struct obj_metadata *new_block = (struct obj_metadata *)((void*)(uintptr_t)metaBlock + size + meta_data_size);
            new_block->size = (metaBlock->size - size) - meta_data_size;
            new_block->is_free = 1;
            new_block->prev = metaBlock;
            new_block->next = next;
            metaBlock->next = new_block;
            metaBlock->size = size;
        }
        foundBlock = false;
        metaBlock->is_free = 0;
        void *memPtr = ((void*)(uintptr_t)metaBlock) + meta_data_size;
        return memPtr;
    }else if (lastBlock->is_free && !foundBlock){
        sbrk(memToAdd);
        lastBlock->size += memToAdd;
        return mymalloc(size);
    }else if(!lastBlock->is_free && !foundBlock){
        sbrk(memToAdd);
        struct obj_metadata *new_block = (struct obj_metadata *)((void*)(uintptr_t)lastBlock + lastBlock->size + meta_data_size);
        new_block->size = memToAdd - meta_data_size;
        new_block->next = NULL;
        new_block->is_free = 1;
        new_block->prev = lastBlock;
        lastBlock->next = new_block;
        return mymalloc(size);
    }
    return NULL;
}

void *mycalloc(size_t nmemb, size_t size)
{
    void *memPtr = mymalloc(nmemb*size);
    memset(memPtr, 0, (nmemb*size));
    return memPtr;
}

void myfree(void *ptr)
{ 
    struct obj_metadata *metaBlock = (struct obj_metadata *)((void*)(uintptr_t)ptr - meta_data_size);
    metaBlock->is_free = 1;
     if (metaBlock->prev != NULL && metaBlock->prev->is_free){
        metaBlock->prev->size += metaBlock->size + meta_data_size;
        metaBlock->prev->next = metaBlock->next;
        metaBlock = metaBlock->prev;
        makePrev = metaBlock->next;
        makePrev->prev = metaBlock;
    }
    if (metaBlock->next != NULL && metaBlock->next->is_free){
        makePrev = metaBlock->next->next;
        metaBlock->size += metaBlock->next->size + meta_data_size;
        metaBlock->next = metaBlock->next->next;
        if(makePrev != NULL){
        makePrev->prev = metaBlock;
        }  
    }

    if(metaBlock->size > pageSize && metaBlock->next == NULL){
        int numOfPages = metaBlock->size / pageSize;
        int memToRemove = numOfPages * pageSize;
        int brkResult = brk((void*)((uintptr_t)metaBlock + meta_data_size + metaBlock->size) - memToRemove);
        if (brkResult != 0){
            perror("break failed");
        }
        metaBlock->size -= memToRemove;
    }
}

void *myrealloc(void *ptr, size_t size)
{
    struct obj_metadata *metaBlock = (struct obj_metadata *)((void*)(uintptr_t)ptr - meta_data_size);
    size = ALIGN(size, memALignment);
    if(ptr == NULL) return mymalloc(size);
    if (metaBlock->size >= size) return ptr;
    size_t possible_mem = metaBlock->size + metaBlock->next->size +meta_data_size;        
    
    if ( metaBlock->next->is_free == 1 && size < (possible_mem)){
        struct obj_metadata *blockToAdd = (struct obj_metadata *)((void*)(uintptr_t)metaBlock + size + meta_data_size);
        blockToAdd->is_free = 1;
        blockToAdd->size = metaBlock->next->size - (size - metaBlock->size);
        blockToAdd->next = metaBlock->next->next;
        makePrev = metaBlock->next->next;
        if(makePrev != NULL){
            makePrev->prev = blockToAdd;
        }  
        metaBlock->size = size;
        metaBlock->next = blockToAdd;
        blockToAdd->prev = metaBlock;
        return ptr;
    }else{
        void *memPtr = mymalloc(size);
        memcpy(memPtr, ptr, metaBlock->size);
        myfree(ptr);
        return memPtr;
    }      
   
}

/*
 * Enable the code below to enable system allocator support for your allocator.
 * Doing so will make debugging much harder (e.g., using printf may result in
 * infinite loops).
 */
#if 1
void *malloc(size_t size)
{
    return mymalloc(size);
}
void *calloc(size_t nmemb, size_t size) { return mycalloc(nmemb, size); }
void *realloc(void *ptr, size_t size) { return myrealloc(ptr, size); }
void free(void *ptr) { myfree(ptr); }
#endif
