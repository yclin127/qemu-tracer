#include "tracer/cache_filter.h"
#include "tracer/sync_queue.h"
#include "tracer/config.h"

#include "qemu/thread.h"

#include "tracer/lru_algorithm.h"

int tracer_capture_count = 0;

typedef struct {
    target_ulong vtag;
    uint32_t flags;
} data_t;

static inline void cache_create(cache_t *cache)
{
    lru_create(cache, cache_line_bits, cache_set_bits, cache_way_count, sizeof(data_t));
}

static inline void cache_access(cache_t *cache, const request_t *request, uint64_t icount)
{
    line_t *line;
    data_t *data;
    
    assert(!((request->vaddr ^ request->paddr) & 0xfff));
                         
    // lru_access don't handle unaligned requests, so make them aligned here.
    target_ulong vaddr = request->vaddr & cache->tag_mask;
    target_ulong paddr = request->paddr & cache->tag_mask;
    target_ulong limit = (request->paddr+request->type.size-1) & cache->tag_mask;
    
    uint8_t counter = 0;
    uint8_t miss = 0;
    for (;;) {
        line = lru_access(cache, paddr);
        data = (data_t *)line->data;
        if (unlikely(line->tag != paddr)) {
            if (data->flags & TRACER_TYPE_WRITE) {
#ifdef CONFIG_FILE_LOGGER
                // write back
                trace_file_log(data->vtag, line->tag, data->flags | TRACER_TYPE_MEM_WRITE, icount);
#endif
            }
#ifdef CONFIG_FILE_LOGGER
            // miss
            trace_file_log(vaddr, paddr, request->type.flags | TRACER_TYPE_MEM_READ, icount);
#endif
            line->tag = paddr;
            data->vtag = vaddr;
            data->flags = 0;
            miss = 1;
        }
        data->flags |= request->type.flags;
        
        assert(counter++ < 3);
        
        // chop up the requests if they cross cachelines
        if (likely(paddr == limit)) break;
        paddr += __size(cache->line_bits);
    }
    
    if (unlikely(tracer_capture_count && miss)) {
        fprintf(stderr, "%12lx %12lx %08x %2d %ld\n", 
                request->vaddr&0xffffffffffff, request->paddr&0xffffffffffff, 
                request->type.flags, request->type.size, icount);
        tracer_capture_count -= 1;
    }
}

/*
 * concurreny fifo
 */

#ifdef CONFIG_SYNC_QUEUE
static QemuThread thread;
static void *cache_filter_main(void *args);
#endif

void cache_filter_init(void)
{
#ifdef CONFIG_SYNC_QUEUE
    qemu_thread_create(&thread, cache_filter_main, 
        NULL, QEMU_THREAD_DETACHED);
#endif
}

#ifdef CONFIG_SYNC_QUEUE
static void *cache_filter_main(void *args)
{
    const batch_t *batch = NULL;
    const request_t *request;
    uint64_t icount = 0;
    
#ifdef CONFIG_CACHE_FILTER
    cache_t cache;
    cache_create(&cache);
#endif
    
#ifdef CONFIG_IFETCH_TABLE
    const request_t *ifetch_table = NULL;
    const request_t *ifetch_ptr;
    int ifetch_count = 0;
#endif
    
    for (;;) {
        batch = sync_queue_get(1);
        
        // iblock with a NULL pointer is used to start a new trace
        request = (request_t *)batch->head;
        if (unlikely(!request->type.flags && !request->pointer)) {
#ifdef CONFIG_FILE_LOGGER
            trace_file_begin();
#endif
#ifdef CONFIG_CACHE_FILTER
            lru_reset(&cache);
#endif
            icount = 0;
            ++request;
        }
        
        for(; request < (request_t *)batch->tail; ++request) {
#ifdef CONFIG_IFETCH_TABLE
            // iblock
            if (!request->type.flags) {
                ifetch_table = request->pointer;
                ifetch_count = 0;
            }
            // FIXME iblock of the first block may be missing. skip all dfetch & istep!
            if (unlikely(!ifetch_table)) {
                continue;
            }
#endif            
            // dfetch & ifetch
            if (request->type.flags) {
#ifdef CONFIG_CACHE_FILTER
                cache_access(&cache, request, icount);
#else
#ifdef CONFIG_FILE_LOGGER
                trace_file_log(request->vaddr, request->paddr, request->type.flags, icount);
#endif
#endif
#ifndef CONFIG_IFETCH_TABLE
                icount += request->type.count;
#endif
            }
            
#ifdef CONFIG_IFETCH_TABLE
            // istep
            for (; ifetch_count < request->type.count; ++ifetch_count) {
                ifetch_ptr = &ifetch_table[ifetch_count];
#ifdef CONFIG_CACHE_FILTER
                cache_access(&cache, ifetch_ptr, icount);
#else
#ifdef CONFIG_FILE_LOGGER
                trace_file_log(ifetch_ptr->vaddr, ifetch_ptr->paddr, 
                               ifetch_ptr->type.flags, icount);
#endif
#endif
                icount += ifetch_ptr->type.count;
            }
#endif
        }
        
        sync_queue_put(1);
    }
    
    return NULL;
}
#endif
