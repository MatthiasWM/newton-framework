/*
	File:		MemoryLanes.h

	Contains:	RAM configuration declarations.

	Writen by:  Newton Research Group.
*/

#if !defined(__MEMORYLANES_H)
#define __MEMORYLANES_H 1

#include "KernelTypes.h"


#define kMaxMemoryLanes 4


struct SFlashLaneInfo
{
	long	x00;	// really don't know
};


/*------------------------------------------------------------------------------
	S B a n k I n f o
------------------------------------------------------------------------------*/

struct SBankInfo
{
	size_t	normalRAMSize(void);

// see RAMConfigEntry in ROMExtension.h
	PAddr		ramPhysStart;
	size_t	ramSize;
	ULong		tag;
	ULong		laneSize;
	ULong		laneCount;
};


enum EBankDesignation
{
	kBank0,			// really don't know
	kBank1,
	kBank2
};


/*------------------------------------------------------------------------------
	C R A M T a b l e

	The RAM table describes the physical RAM available on the Newton device -
	where it is located in the memory map and what word width it has.
------------------------------------------------------------------------------*/

class CRAMTable
{
public:
	static void			init(SBankInfo * ioBank);
	static NewtonErr	add(SBankInfo * ioBank, SBankInfo * info);
	static NewtonErr	remove(PAddr, ULong, EBankDesignation, ULong);
	static PAddr		getPPage(ULong inTag, SBankInfo * inBank);
	static PAddr		getPPageWithTag(ULong inTag);
	static size_t		getRAMSize(void);
};


#endif	/* __MEMORYLANES_H */

