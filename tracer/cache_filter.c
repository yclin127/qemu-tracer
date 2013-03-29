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
    
    // make sure there's nothing funny going on around here.
    // otherwise, there're probablity something wrong in code marker or memory tracer.
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
                // write back
                trace_file_log(data->vtag, line->tag, data->flags | TRACER_TYPE_MEM_WRITE, icount);
            }
            // miss
            trace_file_log(vaddr, paddr, request->type.flags | TRACER_TYPE_MEM_READ, icount);
            line->tag = paddr;
            data->vtag = vaddr;
            data->flags = 0;
            miss = 1;
        }
        data->flags |= request->type.flags;
        
        assert(counter++ < 3);
        
        // chop up requests if they cross cachelines
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

static QemuThread cache_filter_thread;
static QemuSemaphore cache_filter_ready;
static void *cache_filter_main(void *args);

void cache_filter_init(void)
{
    qemu_sem_init(&cache_filter_ready, 0);
    qemu_thread_create(&cache_filter_thread, cache_filter_main, 
        NULL, QEMU_THREAD_DETACHED);
    qemu_sem_wait(&cache_filter_ready);
}

static void *cache_filter_main(void *args)
{
    const batch_t *batch = NULL;
    const request_t *request;
    uint64_t icount = 0;
    
    const request_t *ifetch_table = NULL;
    const request_t *ifetch_ptr;
    int ifetch_count = 0;
    
    cache_t cache;
    cache_create(&cache);

    trace_file_init();
    
    qemu_sem_post(&cache_filter_ready);
    
    for (;;) {
        batch = sync_queue_get(1);
        request = (request_t *)batch->head;
        
        // iblock with a NULL pointer is used to begin a new trace
        if (unlikely(!request->type.flags && request->pointer == NULL)) {
            trace_file_begin();
            lru_reset(&cache);
            icount = 0;
            ++request;
        }
        // iblock with a -1 pointer is used to end current trace
        if (unlikely(!request->type.flags && request->pointer == (void *)-1)) {
            trace_file_end();
            ++request;
        }
        
        for(; request < (request_t *)batch->tail; ++request) {
            // iblock
            if (!request->type.flags) {
                ifetch_table = request->pointer;
                ifetch_count = 0;
            }
            // FIXME iblock of the first block may be missing. skip all dfetch & istep!
            if (unlikely(!ifetch_table)) {
                continue;
            }
            
            // dfetch & ifetch
            if (request->type.flags) {
                cache_access(&cache, request, icount);
                icount += request->type.count;
            }
            
            // istep
            for (; ifetch_count < request->type.count; ++ifetch_count) {
                ifetch_ptr = &ifetch_table[ifetch_count];
                cache_access(&cache, ifetch_ptr, icount);
                icount += ifetch_ptr->type.count;
            }
        }
        
        sync_queue_put(1);
    }
    
    return NULL;
}
