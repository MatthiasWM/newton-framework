/*
	File:		T28F016_SA_SVDriver.h

	Contains:	The driver for Intel's 28F016SA and 28F016SV and Sharp's
				flash chips, the internal flash's (reconstructed: not in the
				published headers). The class implements the TFlashDriver
				protocol (its glue, ClassInfo__19T28F016_SA_SVDriverSFv at
				0x384820, is generated). Names ours where Apple's table has
				none.
*/

#ifndef __T28F016_SA_SVDRIVER_H
#define __T28F016_SA_SVDRIVER_H

#ifndef __FLASHRANGE_H
#include "Stores/FlashRange.h"
#endif

/* The driver's state, per range (TFlashRange::fDriverData) */
struct S28F016DriverData
{
	ULong		fLanes;				// +00 the lanes' bits: a command is (command x 4) & fLanes
	ULong		fStatusOffset;		// +04 the status register, from where a command goes
	ULong		fBlockStatusOffset;	// +08 a block's status, from its address
	ULong		fWordsLeft;			// +0C to write (BeginWrite)
	ULong		fBufferWordsLeft;	// +10 to load into the page buffer
	ULong		fWriteAddress;		// +14 where the next word goes
	ULong		fBufferWords;		// +18 the page buffer's load
	Boolean		fEraseUnderway;		// +1C
	Boolean		fEraseSuspended;	// +1D by StartReadingArray
	Boolean		fResumeTwice;		// +1E
};

class T28F016_SA_SVDriver
{
public:
	enum eWaitOption { kWait, kDontWait };

	static size_t	Sizeof(void);

	NewtonErr	Init(TMemoryAllocator & inAllocator);
	void		CleanUp(TMemoryAllocator & inAllocator);
	Boolean		Identify(ULong inAddress, ULong inLanes, SFlashChipInformation & outInfo);
	NewtonErr	InitializeDriverData(TFlashRange & inRange, TMemoryAllocator & inAllocator);
	void		CleanUpDriverData(TFlashRange & inRange, TMemoryAllocator & inAllocator);
	void		StartReadingArray(TFlashRange & inRange);
	void		DoneReadingArray(TFlashRange & inRange);
	void		BeginWrite(TFlashRange & inRange, ULong inAddress, ULong inWords);
	void		Write(ULong inValue, ULong inMask, ULong inAddress, TFlashRange & inRange);
	NewtonErr	ReportWriteResult(TFlashRange & inRange, ULong inBlockAddress);
	NewtonErr	StartErase(TFlashRange & inRange, ULong inBlockAddress);
	Boolean		IsEraseComplete(TFlashRange & inRange, ULong inBlockAddress, long & outResult);
	NewtonErr	LockBlock(TFlashRange & inRange, ULong inBlockAddress);
	void		ResetBlockStatus(TFlashRange & inRange, ULong inBlockAddress);

private:
	void		StartLoadingPageBuffer(TFlashRange & inRange);
	Boolean		ReportWriteEraseStatus(TFlashRange & inRange, ULong inBlockAddress, long & outResult, eWaitOption inOption);
	void		IssueCommonBlockCommand(TFlashRange & inRange, ULong inBlockAddress, ULong inCommand);

	static void	WaitForStatus(volatile ULong * inAddress, ULong inLanes, ULong inMask, ULong inValue, ULong inStatusOffset);
	static void	WaitForDeviceWSMReady(volatile ULong * inAddress, ULong inLanes, ULong inStatusOffset);
	static void	WaitForQueueAndPageBuffer(volatile ULong * inAddress, ULong inLanes, ULong inStatusOffset);
	static void	WaitForBlockQueue(volatile ULong * inAddress, ULong inLanes, ULong inStatusOffset);
	static void	CleanErrorStatus(volatile ULong * inAddress, ULong inLanes, ULong inStatusOffset);

	long		fReserved[4];		// (the protocol's)
};

#endif	/* __T28F016_SA_SVDRIVER_H */
