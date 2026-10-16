/*
	File:		HostMemory.cc

	Contains:	Memory from the host OS (see HostMemory.h).

	Written by:	Newton Research Group.
*/

#include "HostMemory.h"

#if defined(_WIN32)

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

void *
HostMapAnonymous(size_t inSize)
{
	return VirtualAlloc(NULL, inSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
}

void *
HostMapFile(const char * inPath, size_t inSize, bool * outIsNew)
{
	HANDLE file = CreateFileA(inPath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, NULL,
									  OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (file == INVALID_HANDLE_VALUE)
		return NULL;
	*outIsNew = (GetLastError() != ERROR_ALREADY_EXISTS);
	LARGE_INTEGER size;
	size.QuadPart = (LONGLONG)inSize;
	HANDLE mapping = CreateFileMappingA(file, NULL, PAGE_READWRITE, size.HighPart, size.LowPart, NULL);	// grows the file to inSize
	void * addr = NULL;
	if (mapping != NULL) {
		addr = MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, inSize);
		CloseHandle(mapping);	// the view keeps the mapping
	}
	CloseHandle(file);
	return addr;
}

void
HostUnmap(void * inAddr, size_t inSize)
{
	MEMORY_BASIC_INFORMATION info;
	if (VirtualQuery(inAddr, &info, sizeof(info)) != 0 && info.Type == MEM_MAPPED)
		UnmapViewOfFile(inAddr);
	else
		VirtualFree(inAddr, 0, MEM_RELEASE);
}

#else

#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#ifdef __APPLE__
#include <mach/vm_statistics.h>
#endif

void *
HostMapAnonymous(size_t inSize)
{
#ifdef __APPLE__
	void * addr = mmap(NULL, inSize, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, VM_MAKE_TAG(VM_MEMORY_APPLICATION_SPECIFIC_1), 0);
#else
	void * addr = mmap(NULL, inSize, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
#endif
	return (addr == MAP_FAILED) ? NULL : addr;
}

void *
HostMapFile(const char * inPath, size_t inSize, bool * outIsNew)
{
	*outIsNew = false;
	int fd = open(inPath, O_RDWR);
	if (fd < 0) {
		// create new backing file
		fd = open(inPath, O_RDWR | O_CREAT | O_TRUNC, S_IRWXU | S_IRWXG);
		if (fd < 0)
			return NULL;
		if (ftruncate(fd, inSize) != 0) {
			close(fd);
			return NULL;
		}
		*outIsNew = true;
	}
	void * addr = mmap(NULL, inSize, PROT_READ | PROT_WRITE, MAP_FILE | MAP_SHARED, fd, 0);
	close(fd);	// the mapping stays
	return (addr == MAP_FAILED) ? NULL : addr;
}

void
HostUnmap(void * inAddr, size_t inSize)
{
	munmap(inAddr, inSize);
}

#endif
