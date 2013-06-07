#include "tracer/trace_file.h"
#include "tracer/cache_filter.h"
#include "tracer/common.h"

static batch_t batch;
static int backend[2];

static void trace_file_flush(void)
{
    int size = batch.tail-batch.head;
    
    if (write(backend[1], &size, sizeof(size)) != sizeof(size)) {
        fprintf(stderr, "an error occurred in write().\n");
        exit(-1);
    }
    if (write(backend[1], batch.head, size) != size) {
        fprintf(stderr, "an error occurred in write().\n");
        exit(-1);
    }
    
    batch.tail = batch.head;
}

void trace_file_init(void)
{
    batch.head = g_malloc(BATCH_SIZE);
    
    if (pipe(backend)) {
        fprintf(stderr, "an error occurred in pipe().\n");
        exit(-1);
    }
    fprintf(stderr, "%d\n", fcntl(backend[0], F_SETPIPE_SZ, 0));
    fprintf(stderr, "%d\n", fcntl(backend[1], F_SETPIPE_SZ, 0));
    
    switch (fork()) {
        case -1:
            fprintf(stderr, "an error occurred in fork().\n");
            exit(-1);
        case 0:
            dup2(backend[0], 0);
            close(backend[0]);
            dup2(backend[1], 1);
            close(backend[1]);
            execlp("tracer/trace_file.py", "python", NULL);
            fprintf(stderr, "an error occurred in exec().\n");
            exit(-1);
    }
        
}

void trace_file_begin(void)
{
    batch.tail = batch.head;
    
    int command = 0;
    if (write(backend[1], &command, sizeof(command)) != sizeof(command)) {
        fprintf(stderr, "an error occurred in write().\n");
        exit(-1);
    }
}

void trace_file_end(void)
{
    if (batch.tail != batch.head) {
        trace_file_flush();
    }
    
    int command = -1;
    if (write(backend[1], &command, sizeof(command)) != sizeof(command)) {
        fprintf(stderr, "an error occurred in write().\n");
        exit(-1);
    }
}

void trace_file_log(target_ulong vaddr, target_ulong paddr, uint64_t flags, uint64_t icount)
{
    log_t *log = (log_t*)batch.tail;
    batch.tail += sizeof(log_t);
    
    log->vaddr = vaddr;
    log->paddr = paddr;
    log->flags = flags;
    log->icount = icount;
    
    if (batch.tail-batch.head > BATCH_SIZE-sizeof(log_t)) {
        trace_file_flush();
    }
}
