#ifndef __TRACE_FILE_H__
#define __TRACE_FILE_H__

#include "qemu-common.h"

extern int cache_line_bits;
extern int cache_set_bits;
extern int cache_way_count;

void trace_file_init(void);
void trace_file_begin(void);
void trace_file_end(void);
void trace_file_log(target_ulong vaddr, target_ulong paddr, uint32_t flags, uint64_t icount);

#endif // __STATISTIC_FILE_H__