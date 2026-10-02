/*
	File:		MemMgr.h

	Contains:	The memory manager's heaps (reconstructed: not in the
				published headers; as the port's "Memory Manager/MemMgr.h",
				where these are inline: in the ROM they are functions).
*/

#ifndef __MEMMGR_H
#define __MEMMGR_H

#ifndef __NEWTONMEMORY_H
#include "NewtonMemory.h"
#endif

extern "C" {
Heap	GetFixedHeap(Heap inHeap);
Heap	GetRelocHeap(Heap inHeap);
}

#endif	/* __MEMMGR_H */
