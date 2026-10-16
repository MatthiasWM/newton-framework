/*
	File:		HostMemory.h

	Contains:	Memory from the host OS: anonymous pages and files mapped into
					memory (mmap on macOS and Linux, VirtualAlloc and file mappings
					on Windows).

	Written by:	Newton Research Group.
*/

#if !defined(__HOSTMEMORY_H)
#define __HOSTMEMORY_H 1

#include <stddef.h>

/* inSize bytes of zeroed, read-write memory; NULL if there is none */
void *	HostMapAnonymous(size_t inSize);

/* The file at inPath mapped read-write and shared (changes go to the file),
	inSize bytes; the file is created that big if it doesn't exist, and
	*outIsNew says so. NULL if it can't be. */
void *	HostMapFile(const char * inPath, size_t inSize, bool * outIsNew);

/* Undo HostMapAnonymous or HostMapFile. */
void		HostUnmap(void * inAddr, size_t inSize);

#endif	/* __HOSTMEMORY_H */
