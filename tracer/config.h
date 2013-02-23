#ifndef __CONFIG_H__
#define __CONFIG_H__

#include "qemu-common.h"
#include "tracer/trace_file.h"

#define CONFIG_IFETCH_TABLE
#define CONFIG_REQUEST_BATCH
#define CONFIG_SYNC_QUEUE
#define CONFIG_CACHE_FILTER
#define CONFIG_FILE_LOGGER

#define __size(b)   ((target_ulong)1<<(b))
#define __mask(x,b,o) ((x)&(__size((b)+(o))-__size(o)))
#define __trim(x,b,o) (((x)>>(o))&(__size(b)-1))
#define __cross(x,y,b) ((x)^(y)>>(b))

#define IFETCH_TABLE_SIZE 256

#define CURSOR_COUNT 2
#define BATCH_COUNT (1<<8)
#define BATCH_SIZE (1<<16)

#endif // __CONFIG_H__
