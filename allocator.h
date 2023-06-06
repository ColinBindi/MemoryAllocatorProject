/**
 * @file
 *
 * Function prototypes and structures for our memory allocator implementation.
 */

#ifndef ALLOCATOR_H
#define ALLOCATOR_H

#include <pthread.h>
#include <stddef.h>
#include <stdbool.h>

/* -- Helper functions -- */
struct mem_block *split_block(struct mem_block *block, size_t size);
struct mem_block *merge_block(struct mem_block *block);
void *reuse(size_t size);
void *first_fit(size_t size);
void *worst_fit(size_t size);
void *best_fit(size_t size);
void print_memory(void);
size_t align(size_t, size_t);

/* -- C Memory API functions -- */
void *malloc_impl(size_t size, char *name);
void free_impl(void *ptr);
void *calloc_impl(size_t nmemb, size_t size, char *name);
void *realloc_impl(void *ptr, size_t size, char *name);

/* -- Data Structures -- */

/**
 * Defines metadata structure for memory 'regions.' This
 * structure is prefixed before each region's data area.
 */
struct mem_region {
    /** Region serial number */
    unsigned long region_number;

    /** Next region in the region list */
    struct mem_region *next_region;

    /** Mutex for protecting the region */
    pthread_mutex_t lock;

    /**
     * "Padding" to make the total size of this struct 64 bytes. This serves no
     * purpose other than to make memory address calculations easier.
     */
    char padding[8];
} __attribute__((packed));

/**
 * Defines metadata structure for both memory 'regions' and 'blocks.' This
 * structure is prefixed before each allocation's data area.
 */
struct mem_block {
    /** Block serial number */
    unsigned long block_number;

    /**
     * The name of this memory block. If the user doesn't specify a name for the
     * block, it should be left empty (a single null byte).
     */
    char name[20];

    /** Size of the block */
    size_t size;

    /** Whether or not this block is free */
    bool free;

    /** Next block in the region */
    struct mem_block *next_block;

    /** Previous block in the region */
    struct mem_block *prev_block;

    /** Next **free** block (could be in another region) */
    struct mem_block *next_free;

    /**
     * "Padding" to make the total size of this struct 64 bytes. This serves no
     * purpose other than to make memory address calculations easier.
     */
    char padding[3];
} __attribute__((packed));

#endif
