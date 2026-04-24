/*
 *  lib_memory.c
 *
 *  Copyright 2014-2021 Michael Zillgith
 *
 *  This file is part of Platform Abstraction Layer (libpal)
 *  for libiec61850, libmms, and lib60870.
 */

#include <stdint.h>
#include <string.h>
#include "FreeRTOS.h"
#include "lib_memory.h"

static MemoryExceptionHandler exceptionHandler = NULL;
static void* exceptionHandlerParameter = NULL;

static void
noMemoryAvailableHandler(void);

typedef struct
{
    size_t size;
} MemoryBlockHeader;

static void*
allocateBlock(size_t size)
{
    size_t totalSize = sizeof(MemoryBlockHeader) + size;

    MemoryBlockHeader* header = (MemoryBlockHeader*) pvPortMalloc(totalSize);

    if (header == NULL)
    {
        noMemoryAvailableHandler();
        return NULL;
    }

    header->size = size;

    return (void*) (header + 1);
}

static MemoryBlockHeader*
getHeader(void* memory)
{
    if (memory == NULL)
        return NULL;

    return ((MemoryBlockHeader*) memory) - 1;
}

static void
noMemoryAvailableHandler(void)
{
    if (exceptionHandler != NULL)
        exceptionHandler(exceptionHandlerParameter);
}

void Memory_installExceptionHandler(MemoryExceptionHandler handler, void* parameter)
{
    exceptionHandler = handler;
    exceptionHandlerParameter = parameter;
}

void* Memory_malloc(size_t size)
{
    return allocateBlock(size);
}

void* Memory_calloc(size_t nmemb, size_t size)
{
    if ((nmemb > 0) && (size > (SIZE_MAX / nmemb)))
    {
        noMemoryAvailableHandler();
        return NULL;
    }

    size_t totalSize = nmemb * size;

    void* memory = allocateBlock(totalSize);

    if (memory == NULL)
        return NULL;

    (void) memset(memory, 0, totalSize);

    return memory;
}

void* Memory_realloc(void* ptr, size_t size)
{
    if (ptr == NULL)
    {
        return allocateBlock(size);
    }

    if (size == 0)
    {
        Memory_free(ptr);
        return NULL;
    }

    MemoryBlockHeader* oldHeader = getHeader(ptr);
    size_t oldSize = oldHeader->size;

    void* newMemory = allocateBlock(size);

    if (newMemory == NULL)
        return NULL;

    size_t copySize = (oldSize < size) ? oldSize : size;

    (void) memcpy(newMemory, ptr, copySize);

    Memory_free(ptr);

    return newMemory;
}

void Memory_free(void* memb)
{
    MemoryBlockHeader* header = getHeader(memb);

    if (header != NULL)
        vPortFree(header);
}
