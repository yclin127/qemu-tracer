#!/usr/bin/env python

import gtk, gobject

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
TRACER_TYPE_TLB_WALK = 0x40
TRACER_TYPE_MEM_MMU = lambda flags: (flags>>16)&0xf
TRACER_TYPE_MEM_VCORE = lambda flags: (flags>>24)&0xf

from sys import *
from ctypes import *

class LOG(Structure):
	_fields_ = [
		('vaddr', c_ulong),
		('paddr', c_ulong),
		('flags', c_ulong),
		('icount', c_ulong),
	]

def on_pipe(fd, condition):
	length = (c_int*1)()
	stdin.readinto(length)
	length = length[0]
	# trace begin
	if length == 0:
		print >> stderr, '=== trace begin ==='
	# trace end
	elif length == -1:
		print >> stderr, '=== trace end ==='
	# trace batch
	else:
		length /= sizeof(LOG)
		batch = (LOG*length)()
		stdin.readinto(batch)
	return True

gobject.io_add_watch(stdin.fileno(), gobject.IO_IN, on_pipe, priority=gobject.PRIORITY_LOW)
gtk.main()
