#include "tracer/trace_file.h"
#include "tracer/cache_filter.h"
#include "tracer/common.h"

static batch_t batch;
static int backend[2];

#define safe_read(fd,buf,sz) \
    if (read(fd,buf,sz)!=sz) {\
        fprintf(stderr,"an error occurred in read().\n");\
        exit(-1);\
    }
#define safe_write(fd,buf,sz) \
    if (write(fd,buf,sz)!=sz) {\
        fprintf(stderr,"an error occurred in write().\n");\
        exit(-1);\
    }

static void trace_file_flush(void)
{
    int size = batch.tail-batch.head;
    
    safe_write(backend[1], &size, sizeof(size));
    safe_write(backend[1], batch.head, size);
    
    batch.tail = batch.head;
}

void trace_file_init(void)
{
    batch.head = g_malloc(BATCH_SIZE);
    
    if (pipe(backend)) {
        fprintf(stderr, "an error occurred in pipe().\n");
        exit(-1);
    }
    
    const char *path = "tracer/trace_file.py";
    switch (fork()) {
        case -1:
            fprintf(stderr, "an error occurred in fork().\n");
            exit(-1);
        case 0:
            dup2(backend[0], 0);
            close(backend[0]);
            dup2(backend[1], 1);
            close(backend[1]);
            execlp(path, "python", NULL);
            fprintf(stderr, "an error occurred in exec(\"%s\").\n", path);
            exit(-1);
    }
    
    safe_read(backend[0], &cache_line_bits, sizeof(int));
    safe_read(backend[0], &cache_set_bits, sizeof(int));
    safe_read(backend[0], &cache_way_count, sizeof(int));
    
    safe_read(backend[0], &tlb_page_bits, sizeof(int));
    safe_read(backend[0], &tlb_set_bits, sizeof(int));
    safe_read(backend[0], &tlb_way_count, sizeof(int));
}

void trace_file_begin(void)
{
    batch.tail = batch.head;
    
    int command = 0;
    safe_write(backend[1], &command, sizeof(command));
}

void trace_file_end(void)
{
    if (batch.tail != batch.head) {
        trace_file_flush();
    }
    
    int command = -1;
    safe_write(backend[1], &command, sizeof(command));
}

void trace_file_log(target_ulong vaddr, target_ulong paddr, uint64_t flags, uint64_t icount)
{
    log_t *log = (log_t*)batch.tail;
    batch.tail += sizeof(log_t);
    
    log->vaddr = vaddr;
    log->paddr = paddr;
    log->flags = flags;
    log->icount = icount;
    
    if (batch.tail-batch.head > PACKAGE_SIZE-sizeof(log_t)) {
        trace_file_flush();
    }
}
