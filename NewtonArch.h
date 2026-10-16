/*
	File:		NewtonArch.h

	Contains:	The host's pointer size, for C, C++ and the assembler (.s)
					files: preprocessor definitions only.

	NEWT_64BIT is 1 when pointers are 64 bits wide, 0 otherwise. Refs are as
	wide as pointers. Tested instead of __LP64__, which only LP64 systems
	define: 64-bit Windows is LLP64 (long is 32 bits, pointers 64).
*/

#if !defined(__NEWTONARCH_H)
#define __NEWTONARCH_H 1

#if defined(__LP64__) || defined(_WIN64) || (defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 8)
#define NEWT_64BIT 1
#else
#define NEWT_64BIT 0
#endif

#endif	/* __NEWTONARCH_H */
