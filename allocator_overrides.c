/**
 * @file
 *
 * Contains stubs that call into our custom allocator library. This is necessary
 * during testing to allow us to call both our own allocator and the system
 * allocator.
 *
 * To use (one specific command):
 * LD_PRELOAD=$(pwd)/allocator.so command
 * ('command' will run with your allocator)
 *
 * To use (all following commands):
 * export LD_PRELOAD=$(pwd)/allocator.so
 * (Everything after this point will use your custom allocator -- be careful!)
 */

#include "allocator.h"

/**
 * Given a block size (header + data) and a block name,
 * allocate memory for a block with that size and name.
 *
 * @param size size of the block (header + data)
 * @param name name of the block
 * 
 * @return address of the block (header + data)
 */
void *malloc(size_t size)
{
    return malloc_impl(size, "");
}

/**
 * Given a block address (header + data),
 * remove memory for the block at that address.
 *
 * @param ptr address of the block (header + data)
 */
void free(void *ptr)
{
    free_impl(ptr);
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
void *calloc(size_t nmemb, size_t size)
{
    return calloc_impl(nmemb, size, "");
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
void *realloc(void *ptr, size_t size)
{
    return realloc_impl(ptr, size, "");
}
