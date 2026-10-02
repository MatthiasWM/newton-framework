/*
	File:		FlashRange.h

	Contains:	A range of flash memory and the allocator its driver gets
				(reconstructed: not in the published headers; the port has a
				Stores/FlashRange.h). Layouts as the ROM's code uses them;
				names ours where Apple's table has none.
*/

#ifndef __FLASHRANGE_H
#define __FLASHRANGE_H

#ifndef __FLASHDRIVER_H
#include "Stores/FlashDriver.h"
#endif

class TFlashDriver;

class TMemoryAllocator
{
public:
	virtual void *	Allocate(ULong inSize) = 0;
	virtual void	Deallocate(void * inBlock) = 0;
};

class TFlashRange
{
public:
	virtual void	Delete(TMemoryAllocator & inAllocator);
	virtual			~TFlashRange();
	virtual ULong	StartOfBlockWriteVirtualAddress(ULong inAddress) const = 0;	// where to send a block's commands

	TFlashDriver *	fDriver;			// +04
	ULong			fStart;				// +08 the first flash address of the range
	ULong			fReadAddress;		// +0C where it is to read
	ULong			fWriteAddress;		// +10 where it is to write: commands go there
	ULong			fLanes;				// +14 the byte lanes it takes (eMemoryLane)
	SFlashChipInformation	fChipInfo;	// +18
	ULong			fSize;				// +30
	ULong			fChipCount;			// +34 chips side by side
	ULong			fLaneCount;			// +38
	ULong			fBlockSize;			// +3C
	void *			fDriverData;		// +40 the driver's (InitializeDriverData)
};

#endif	/* __FLASHRANGE_H */
