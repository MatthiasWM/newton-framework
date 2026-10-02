/*
	File:		Pipes.h

	Contains:	CPipe, a stream of bytes to read and write (reconstructed: not
				in the published headers). The virtual functions in the order
				of the ROM's vtables (tools/vtable.py).
*/

#ifndef __PIPES_H
#define __PIPES_H

#ifndef __NEWTON_H
#include "Newton.h"
#endif

class CPipe
{
public:
	virtual				~CPipe();

	virtual void		ReadSeek(long inOffset, int inSelector);
	virtual long		ReadPosition(void) const;
	virtual void		WriteSeek(long inOffset, int inSelector);
	virtual long		WritePosition(void) const;
	virtual void		ReadChunk(void * outBuf, long & ioSize, Boolean & outEOF);
	virtual void		WriteChunk(void * inBuf, long inSize, Boolean inFlush);
	virtual void		FlushRead(void);
	virtual void		FlushWrite(void);
	virtual void		Reset(void);
	virtual void		ResetRead(void);
	virtual void		ResetWrite(void);
	virtual void		Overflow(void);
	virtual void		Underflow(long inSize, Boolean & outEOF);
	virtual void		Abort(void);

	CPipe &				operator<<(long inValue);
};

#endif	/* __PIPES_H */
