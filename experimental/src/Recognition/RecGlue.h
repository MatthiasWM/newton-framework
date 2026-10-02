/*
	File:		RecGlue.h

	Contains:	The recognizers' memory and file glue: Handles as the
				Macintosh had them, over the Newton's memory (reconstructed:
				not in the published headers; the original file's name is not
				known). In the ROM at 0x11B858..0x11B93C.
*/

#ifndef __RECGLUE_H
#define __RECGLUE_H

#ifndef __NEWTONMEMORY_H
#include "NewtonMemory.h"
#endif

long		MemoryError(void);
void		MoveBlock(char * inFrom, char * inTo, long inSize);
long		SaveResource(char ** inHandle, long inType, long inID, char * inName);
char **		MakeHandle(long inSize);
long		CopyHandle(char *** ioHandle);
void		DeleteHandle(char ** inHandle);
long		SizeOfHandle(char ** inHandle);
long		ResizeHandle(char ** inHandle, long inSize);
void		LockHandle(char ** inHandle);
void		UnlockHandle(char ** inHandle);
void		NameHandle(char ** inHandle, ULong inName);

#endif	/* __RECGLUE_H */
