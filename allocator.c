/**
 * @file
 *
 * Implementations of allocator functions.
 */

#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdlib.h>

#include "allocator.h"
#include "logger.h"

#define ALIGN_SZ 8

static unsigned long g_regions = 0; /*!< Allocation counter */
static unsigned long g_allocations = 0; /*!< Allocation counter */

/** Linked list of memory regions, ending with the most recently-mapped */
static struct mem_region *g_region_head = NULL;
static struct mem_region *g_region_tail = NULL;

/** Linked list of free memory blocks, starting with the most recently-freed */
static struct mem_block *g_free_list = NULL;

/** Mutex for protecting the region list */
pthread_mutex_t rlist_lock = PTHREAD_MUTEX_INITIALIZER;

/** Mutex for protecting the free list */
pthread_mutex_t flist_lock = PTHREAD_MUTEX_INITIALIZER;


/**
 * Given a free block, this function will split it into two blocks (if
 * possible).
 *
 * @param block Block to split
 * @param size Size of the newly-created block. This block should be at the
 * "end" of the original block:
 * 
 *     +----------------------+-----+
 *     | (old block)          | new |
 *     +----------------------+-----+
 *     ^                      ^
 *     |                      |
 *     |                      +-- pointer to beginning of new block
 *     |
 *     +-- original block pointer (unchanged)
 *         update its size: old_block_size - new_block_size
 *
 *
 * @return address of the resulting second block (the original address will be
 * unchanged) or NULL if the block cannot be split.
 */
struct mem_block *split_block(struct mem_block *block, size_t size)
{
    if (block != NULL && size > 0 && block->size > size + sizeof(struct mem_block)) {
        size_t updated_sz = block->size - size;
        if (size < (sizeof(struct mem_block) + ALIGN_SZ)) {
            return NULL;
        }
        if (updated_sz < (sizeof(struct mem_block) + ALIGN_SZ)) {
            return NULL;
        }
        struct mem_block *new_block = (void*) block + (block->size - size);
        new_block->size = size;
        new_block->block_number = g_allocations++;
        new_block->free = true;
        new_block->prev_block = block;
        new_block->next_block = block->next_block;
        new_block->size = size;
        block->next_block = new_block;
        block->size = updated_sz;
        return new_block;
    }
    return NULL;
}

/**
 * Given a free block, this function attempts to merge it with neighboring
 * blocks --- both the previous and next neighbors --- and update the linked
 * list accordingly.
 *
 * For example,
 *
 * merge(b1)
 *   -or-
 * merge(b2)
 *
 *     +-------------+--------------+-----------+
 *     | b1 (free)   | b2 (free)    | b3 (used) |
 *     +-------------+--------------+-----------+
 *
 *                   |
 *                   V
 *
 *     +----------------------------+-----------+
 *     | b1 (free)                  | b3 (used) |
 *     +----------------------------+-----------+
 *
 * merge(b3) = NULL
 *
 * @param block the block to merge
 *
 * @return address of the merged block or NULL if the block cannot be merged.
 */
struct mem_block *merge_block(struct mem_block *block)
{
    if (block->free != true) {
        return NULL;
    }

    struct mem_block *prev_merge = block->prev_block;
    while (prev_merge != NULL && prev_merge->free == true) {
        prev_merge->size += block->size;
        prev_merge->next_block = block->next_block;
        block = prev_merge;
        prev_merge = prev_merge->prev_block;
    }

    struct mem_block *next_merge = block->next_block;
    while (next_merge != NULL && next_merge->free == true) {
        next_merge->size += block->size;
        next_merge->prev_block = block->prev_block;
        block = next_merge;
        next_merge = next_merge->next_block;
    }
    return block;
}

/**
 * Given a block size (header + data), locate a suitable location in the free
 * list using the first fit free space management algorithm.
 *
 * @param size size of the block (header + data)
 * 
 * @return address of the block that can fit the new block size according to the first fit algorithm or NULL otherwise
 */
void *first_fit(size_t size)
{
    struct mem_block *curr = g_free_list;
    while (curr != NULL) {
        if (curr->size >= size) {
            return curr;
        }
        curr = curr->next_free;
    }
    return NULL;
}

/**
 * Given a block size (header + data), locate a suitable location in the free
 * list using the worst fit free space management algorithm. If there are ties
 * (i.e., you find multiple worst fit candidates with the same size), use the
 * first candidate found in the list.
 *
 * @param size size of the block (header + data)
 * 
 * @return address of the block that can fit the new block size according to the worst fit algorithm or NULL otherwise
 */
void *worst_fit(size_t size)
{
    struct mem_block *curr = g_free_list;
    struct mem_block *largest_free_region = NULL;
    while(curr != NULL) {
        if (curr->size >= size) {
            if (largest_free_region == NULL || (largest_free_region->size <= curr->size)) {
                largest_free_region = curr;
            }
        }
        curr = curr->next_free;
    }
    return largest_free_region;
}

/**
 * Given a block size (header + data), locate a suitable location in the free
 * list using the best fit free space management algorithm. If there are ties
 * (i.e., you find multiple best fit candidates with the same size), use the
 * first candidate found in the list.
 *
 * @param size size of the block (header + data)
 * 
 * @return address of the block that can fit the new block size according to the best fit algorithm or NULL otherwise
 */
void *best_fit(size_t size)
{
    struct mem_block *curr = g_free_list;
    struct mem_block *perfect_free_region = NULL;
    while(curr != NULL) {
        if (curr->size >= size) {
            if (perfect_free_region == NULL || (perfect_free_region->size >= curr->size)) {
                perfect_free_region = curr;
            }
        }
        curr = curr->next_free;
    }
    return perfect_free_region;
}

/**
 * Given a block size (header + data), locate a suitable location in the free
 * list using one of the free space management algorithms. If a suitable spot
 * is found add the new block to the region.
 *
 * @param size size of the block (header + data)
 * 
 * @return address of the block that is being reused
 */
void *reuse(size_t size)
{
    char *algo = getenv("ALLOCATOR_ALGORITHM");
    if (algo == NULL) {
        algo = "first_fit";
    }

    struct mem_block *reused_block = NULL;
    if (strcmp(algo, "first_fit") == 0) {
        reused_block = first_fit(size);
    } else if (strcmp(algo, "best_fit") == 0) {
        reused_block = best_fit(size);
    } else if (strcmp(algo, "worst_fit") == 0) {
        reused_block = worst_fit(size);
    }

    if (reused_block == NULL) {
        return NULL;
    }

    struct mem_block *split_reused_block = split_block(reused_block, reused_block->size - size);
    struct mem_block *curr = g_free_list;

    if (curr == reused_block) {
        g_free_list = curr->next_free;
    } else {
        while (curr != NULL) {
            if (curr->next_free == reused_block) {
                curr->next_free = reused_block->next_free;
            }
            curr = curr->next_free;
        }
    }

    if (g_free_list == NULL) {
        g_free_list = split_reused_block;
    } else {
        if (split_reused_block != NULL) {
            split_reused_block->next_free = g_free_list;
            g_free_list = split_reused_block;
        }
    }

    reused_block->free = false;

    return reused_block;
}

/**
 * Given the actual size of a block and the alignment size,
 * calculate the aligned size of the block
 *
 * @param orig_sz the actual size of the block (header + data)
 * @param alignment the aligned size variable
 * 
 * @return aligned size of the block (header + data)
 */
size_t align(size_t orig_sz, size_t alignment) 
{
    size_t new_sz = (orig_sz / alignment) * alignment;
    if (orig_sz % alignment != 0) {
        new_sz += alignment;
    }
    return new_sz;
}

/**
 * Given a block size (header + data) and a block name,
 * allocate memory for a block with that size and name.
 *
 * @param size size of the block (header + data)
 * @param name name of the block
 * 
 * @return address of the block (header + data)
 */
void *malloc_impl(size_t size, char *name)
{
    size_t actual_sz = size + sizeof(struct mem_region);
    size_t aligned_sz = align(actual_sz, ALIGN_SZ);

    LOG("Allocation request: %zu bytes; actual_sz = %zu bytes; aligned = %zu bytes\n", size, actual_sz, aligned_sz);
    
    char *algo = getenv("ALLOCATOR_SCRIBBLE");
    int scribble_check = 0;
    if (algo != NULL) {
        scribble_check = atoi(algo);
    }

    struct mem_block *reused_block = reuse(aligned_sz);
    if (reused_block != NULL) {
        strcpy(reused_block->name, name);
        if (scribble_check == 1) {
            memset((void*) reused_block+1, 0xAA, size);
        }
        return reused_block + 1;
    }

    pthread_mutex_lock(&rlist_lock);
    pthread_mutex_lock(&flist_lock);

    size_t map_sz = align(aligned_sz + sizeof(struct mem_region), getpagesize());

    struct mem_region *region = mmap(NULL, map_sz, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (region == MAP_FAILED) {
        perror("mmap");
        pthread_mutex_unlock(&rlist_lock);
        pthread_mutex_unlock(&flist_lock);
        return NULL;
    }

    LOG("-> %p\n", region);
    region->region_number = g_regions++;

    if (g_region_head == NULL && g_region_tail == NULL) {
        g_region_head = region;
        g_region_tail = region;
    } else {
        g_region_tail->next_region = region;
        g_region_tail = region;
    }

    struct mem_block *block = (struct mem_block *) region + 1;
    block->free = false;
    block->size = map_sz - sizeof(struct mem_region);
    block->block_number = g_allocations++;
    if (name != NULL) {
        strcpy(block->name, name);
    }

    struct mem_block *new_block = split_block(block, block->size - actual_sz);

    if (g_free_list == NULL) {
        g_free_list = new_block;
    } else {
        new_block->next_free = g_free_list;
        g_free_list = new_block;
    }

    if (scribble_check == 1) {
        memset((void*) block+1, 0xAA, aligned_sz);
    }

    pthread_mutex_unlock(&rlist_lock);
    pthread_mutex_unlock(&flist_lock);

    return block + 1;
}

/**
 * Given a block address (header + data),
 * remove memory for the block at that address.
 *
 * @param ptr address of the block (header + data)
 */
void free_impl(void *ptr)
{
    LOG("Free request at memory address: %p\n", ptr);

    if (ptr == NULL) {
        return;
    }

    struct mem_block *block = (struct mem_block *) ptr - 1;
    block->free = true;

    if (g_free_list == NULL) {
        g_free_list = block;
    } else {
        block->next_free = g_free_list;
        g_free_list = block;
    }

    pthread_mutex_lock(&rlist_lock);
    pthread_mutex_lock(&flist_lock);

    struct mem_region *region = (void *) block - 1;

    if (munmap(region, block->size + sizeof(struct mem_region)) == -1) {
        perror("munmap");
        pthread_mutex_unlock(&rlist_lock);
        pthread_mutex_unlock(&flist_lock);
        return;
    }

    pthread_mutex_unlock(&rlist_lock);
    pthread_mutex_unlock(&flist_lock);
}

/**
 * Given the number of blocks, the size of each block,
 * and a block name, allocate memory.
 *
 * @param nmemb number of blocks to be allocated memory
 * @param size size of the block (header + data)
 * @param name name of the block
 * 
 * @return address of the block (header + data)
 */
void *calloc_impl(size_t nmemb, size_t size, char *name)
{
    void *ptr = malloc_impl(nmemb * size, name);
    memset(ptr, 0, nmemb * size);
    return ptr;
}

/**
 * Given an address of a block, a block size (header + data),
 * and a block name, reallocate memory for that block with a
 * new size.
 *
 * @param ptr address of the block (header + data)
 * @param size size of the block (header + data)
 * @param name name of the block
 * 
 * @return address of the block (header + data)
 */
void *realloc_impl(void *ptr, size_t size, char *name)
{
    if (ptr == NULL) {
        return malloc_impl(size, name);
    }
    if (size == 0) {
        free_impl(ptr);
        return NULL;
    }
    return NULL;
}

/**
 * print_memory
 *
 * Prints out the current memory state, including both the regions and blocks.
 * Entries are printed in order, so there is an implied link from the topmost
 * entry to the next, and so on.
 *
 * Format:
 *
 *    -- Current Memory State --
 *    [REGION <region no>] <start addr>
 *      [BLOCK] <block no> <start addr>-<end addr> '<name>' <block-size> [FREE]
 *      [BLOCK] <block no> <start addr>-<end addr> '<name>' <block-size> [USED]
 *      [BLOCK] <block no> <start addr>-<end addr> '<name>' <block-size> [FREE]
 *    [REGION <region no>] <start addr>
 *      [BLOCK] <block no> <start addr>-<end addr> '<name>' <block-size> [USED]
 */
void print_memory(void)
{
    struct mem_region *region = g_region_head;
    while (region != NULL) {
        printf("[REGION %lu] <%p>\n", region->region_number, region);
        struct mem_block* curr_block = (struct mem_block*) (region + 1);
        int i = 1;
        while (curr_block != NULL) {
            char *free_str = NULL;
            if (curr_block->free == true) {
                free_str = "FREE";
            } else {
                free_str = "USED";
            }
            printf("[BLOCK] <%lu> <%p>-<%p> <'%s'> <%zu> [%s]\n", curr_block->block_number, curr_block, (void*) curr_block + curr_block->size, curr_block->name, curr_block->size, free_str);
            i++;
            curr_block = curr_block->next_block;
        }
        region = region->next_region;
    }
}
