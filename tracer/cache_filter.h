#ifndef __CACHE_FILTER_H__
#define __CACHE_FILTER_H__

#include "qemu-common.h"

typedef union {
    uint64_t value;
    struct {
        uint32_t flags;
        uint16_t size;
        uint16_t count;
    };
} type_t;

typedef struct {
    union {
        struct {
            target_ulong vaddr;
            target_ulong paddr;
        };
        void *pointer;
    };
    type_t type;
} request_t;

typedef struct {
    void   *head;
    void   *tail;
} batch_t;

void     cache_filter_init(void);
batch_t *cache_filter_first_batch(void);
batch_t *cache_filter_next_batch(void);
void     cache_filter_flush(void);

#endif // __CACHE_FILTER_H__
