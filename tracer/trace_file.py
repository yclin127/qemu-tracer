# cache configuration, will be used by cache fitler
CACHE_LINE_BITS = 6
CACHE_SET_BITS  = 12
CACHE_WAY_COUNT = 8

# trace flag definition, same as in tracer.h
TRACER_TYPE_INSN = 0x1
TRACER_TYPE_DATA = 0x2
TRACER_TYPE_READ = 0x4
TRACER_TYPE_WRITE = 0x8
TRACER_TYPE_MEM_READ = 0x10
TRACER_TYPE_MEM_WRITE = 0x20
TRACER_TYPE_MEM_MMU = lambda flags: (flags>>16)&0xf
TRACER_TYPE_MEM_VCORE = lambda flags: (flags>>24)&0xf

def trace_file_init():
    pass

def trace_file_begin():
    pass

def trace_file_end():
    pass

def trace_file_log(vaddr, paddr, flags, icount):
    pass
