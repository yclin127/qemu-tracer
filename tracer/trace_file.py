#!/usr/bin/env python

from sys import stdin, stdout, stderr
# stdin: pipe (memory trace)
# stdout: pipe (configuration)
# stderr: stderr

'''
  LLC & TLB Configuration
'''

from struct import pack

# cache_line_bits, cache_set_bits, cache_way_count
stdout.write(pack('iii', 6, 12, 1<<3))
# tlb_page_bits, tlb_set_bits, tlb_way_count
stdout.write(pack('iii', 12, 7, 1<<3))
stdout.flush()

'''
  Trace Logger
'''

TRACER_TYPE_INSN  = 0x1
TRACER_TYPE_DATA  = 0x2
TRACER_TYPE_READ  = 0x4
TRACER_TYPE_WRITE = 0x8
TRACER_TYPE_MEM_READ  = 0x10
TRACER_TYPE_MEM_WRITE = 0x20
TRACER_TYPE_TLB_WALK  = 0x40
TRACER_TYPE_TLB_EVICT = 0x80
TRACER_TYPE_MEM_MMU   = lambda flags: (flags>>16)&0xf
TRACER_TYPE_MEM_VCORE = lambda flags: (flags>>24)&0xf

from ctypes import *

class RECORD(Structure):
	_fields_ = [
		('vaddr', c_ulong),
		('paddr', c_ulong),
		('flags', c_ulong),
		('icount', c_ulong),
	]

def log(batch):
	global G_icount, G_access, G_read, G_write
	for record in batch:
		'''
		record.vaddr: virtual address
		record.paddr: physical adddress
		record.flags: access information (see TRACER_TYPE_* definition)
		record.icount: instruction count
		'''
		for access in batch:
			if access.flags & TRACER_TYPE_MEM_READ:
				G_access += 1
				G_read += 1
			elif access.flags & TRACER_TYPE_MEM_WRITE:
				G_access += 1
				G_write += 1
			if access.icount - G_icount > 100000:
				print >> stderr, access.icount, G_access, G_read, G_write
				G_icount += 100000

def on_pipe(fd, condition):
	global G_access, G_read, G_write, G_icount
	length = (c_int*1)()
	stdin.readinto(length)
	length = length[0]
	# trace begin
	if length == 0:
		print >> stderr, '=== trace begin ==='
		G_access = 0
		G_read = 0
		G_write = 0
		G_icount = 0
	# trace end
	elif length == -1:
		print >> stderr, '=== trace end ==='
	# trace batch
	else:
		length /= sizeof(RECORD)
		batch = (RECORD*length)()
		stdin.readinto(batch)
		log(batch)
	return True

import gtk, gobject

gobject.io_add_watch(stdin.fileno(), gobject.IO_IN, on_pipe, priority=gobject.PRIORITY_LOW)
gtk.main()
