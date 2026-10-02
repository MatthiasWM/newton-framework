/*
	File:		SystemNatives.cc

	Contains:	NewtonScript natives for the system: the serial number,
				batteries and power, the backlight, Gestalt, the screen's
				contrast and orientation, the tablet, heap statistics.
				(The original file's name is not known; newton-re calls it
				SystemNatives.)

	ROM:		The file is 0x20171C (FGetSerialNumber) .. 0x203DE8 (after
				FBatteryStatus), MP2x00 US 2.1 (717006). Here so far:
				0x202AE4 (FGetOrientation) .. 0x202FF4 (after
				GetActualHeapInfo); FGetHeapStats (0x202FF4..0x203510) is
				not yet identical (see there); the rest is still generated assembler.
*/

#include "Frames/objects.h"
#include "OS600/NewtonGestalt.h"
#include "Graphics/Screen.h"
#include "Frames/NewtGlobals.h"
#include "Recognition/Tablet.h"
#include "OS600/VirtualMemory.h"
#include "Frames/RSSymbols.h"
#include "NewtonMemory.h"
#include "NewtonWidgets.h"
#include "MemoryManager/MemMgr.h"
#include "OS/RDM.h"

extern "C" {
Ref		FGetOrientation(RefArg inRcvr);
Ref		FSetOrientation(RefArg inRcvr, RefArg inOrientation);
Ref		FStartBypassTablet(RefArg inRcvr);
Ref		FStopBypassTablet(RefArg inRcvr);
Ref		FInsertTabletSample(RefArg inRcvr, RefArg inX, RefArg inY, RefArg inZ, RefArg inTime);
Ref		FTabletBufferEmpty(RefArg inRcvr);
Ref		FEnablePowerStats(RefArg inRcvr, RefArg inEnable);
Ref		FGetPowerStats(RefArg inRcvr);
Ref		FResetPowerStats(RefArg inRcvr);
Ref		FGetHeapStats(RefArg inRcvr, RefArg inOptions);
}
Ref		FGetLCDContrast(RefArg inRcvr);


/*------------------------------------------------------------------------------
	Return the screen's orientation.
	Args:		inRcvr			the receiver
	Return:		an integer: kPortrait .. kLandscapeFlip
------------------------------------------------------------------------------*/

Ref
FGetOrientation(RefArg inRcvr)
{
	long	orientation;
	GetGrafInfo(4, &orientation);		// kGrafOrientation
	return MAKEINT(orientation);
}


/*------------------------------------------------------------------------------
	Set the screen's orientation.
	Args:		inRcvr			the receiver
				inOrientation	an integer
	Return:		nil
------------------------------------------------------------------------------*/

Ref
FSetOrientation(RefArg inRcvr, RefArg inOrientation)
{
	SetOrientation(RINT(inOrientation));
	return NILREF;
}


/*------------------------------------------------------------------------------
	Set the screen's orientation: the display, the tablet, the screen's
	GrafPort and NewtonScript's, and the screen size.
	Args:		inOrientation	kPortrait .. kLandscapeFlip
	Return:		--
------------------------------------------------------------------------------*/

void
SetOrientation(long inOrientation)
{
	SetGrafInfo(kGrafOrientation, inOrientation);
	TabSetOrientation(inOrientation);

	GrafPtr	port = &gGrafPort;
	GetGrafInfo(kGrafPixelMap, port);
	port->portRect = port->portBits.bounds;
	InitPortRgns(port);

	if (gNewtGlobals != NULL)
	{
		GetGrafInfo(kGrafPixelMap, gNewtGlobals->graf);
		gNewtGlobals->graf->portRect = gNewtGlobals->graf->portBits.bounds;
		InitPortRgns(gNewtGlobals->graf);
	}

	TUGestalt			gestalt;
	TGestaltSystemInfo	info;
	gestalt.Gestalt(kGestalt_SystemInfo, &info, sizeof(info));
	if (inOrientation == kLandscape || inOrientation == kLandscapeFlip)
	{
		screenWidth = (info.fScreenWidth > info.fScreenHeight) ? info.fScreenWidth : info.fScreenHeight;
		screenHeight = (info.fScreenWidth < info.fScreenHeight) ? info.fScreenWidth : info.fScreenHeight;
	}
	else
	{
		screenWidth = (info.fScreenWidth < info.fScreenHeight) ? info.fScreenWidth : info.fScreenHeight;
		screenHeight = (info.fScreenWidth > info.fScreenHeight) ? info.fScreenWidth : info.fScreenHeight;
	}
}


/*------------------------------------------------------------------------------
	Set the screen's size: nothing to do.
	Args:		inWidth
				inHeight
	Return:		--
------------------------------------------------------------------------------*/

void
SetScreenSize(long inWidth, long inHeight)
{ }


/*------------------------------------------------------------------------------
	Tablet bypass: samples come from NewtonScript instead of the tablet.
------------------------------------------------------------------------------*/

Ref
FStartBypassTablet(RefArg inRcvr)
{
	return MAKEINT(StartBypassTablet());
}


Ref
FStopBypassTablet(RefArg inRcvr)
{
	return MAKEINT(StopBypassTablet());
}


/*------------------------------------------------------------------------------
	Put a sample into the tablet buffer.
	Args:		inRcvr
				inX, inY		the point
				inZ				the pressure
				inTime			its time stamp
	Return:		an error code, as an integer
------------------------------------------------------------------------------*/

Ref
FInsertTabletSample(RefArg inRcvr, RefArg inX, RefArg inY, RefArg inZ, RefArg inTime)
{
	ULong	sample = (RINT(inX) << 21) | ((RINT(inY) & 0x3FFF) << 7) | (RINT(inZ) & 0x0F);
	return MAKEINT(InsertTabletSample(sample, RINT(inTime)));
}


Ref
FTabletBufferEmpty(RefArg inRcvr)
{
	return TabletBufferEmpty() ? TRUEREF : NILREF;
}


/*------------------------------------------------------------------------------
	Return the screen's contrast.
	Args:		inRcvr
	Return:		an integer
------------------------------------------------------------------------------*/

Ref
FGetLCDContrast(RefArg inRcvr)
{
	long	contrast;
	GetGrafInfo(kGrafContrast, &contrast);
	return MAKEINT(contrast);
}


/*------------------------------------------------------------------------------
	Power statistics: how long the processor, the screen, serial and sound
	have been on, collected while gCollectCPUStats is set.
------------------------------------------------------------------------------*/

Ref
FEnablePowerStats(RefArg inRcvr, RefArg inEnable)
{
	gCollectCPUStats = NOTNIL(inEnable);
	return MAKEBOOLEAN(gCollectCPUStats);
}


Ref
FGetPowerStats(RefArg inRcvr)
{
	RefVar	stats(Clone(RA(canonicalpowerstats)));
	SetFrameSlot(stats, SYMA(timeatcoldboot), MAKEINT(gGlobalsThatLiveAcrossReboot.fTimeAtColdBoot - 0xA76C6BBC));
	SetFrameSlot(stats, SYMA(processorofftime), MAKEINT(gGlobalsThatLiveAcrossReboot.fProcessorOnTime));
	SetFrameSlot(stats, SYMA(screenontime), MAKEINT(gGlobalsThatLiveAcrossReboot.fScreenOnTime));
	SetFrameSlot(stats, SYMA(serialontime), MAKEINT(gGlobalsThatLiveAcrossReboot.fSerialOnTime));
	SetFrameSlot(stats, SYMA(soundontime), MAKEINT(gGlobalsThatLiveAcrossReboot.fSoundOnTime));
	return stats;
}


Ref
FResetPowerStats(RefArg inRcvr)
{
	gGlobalsThatLiveAcrossReboot.fProcessorOnTime = 0;
	gGlobalsThatLiveAcrossReboot.fScreenOnTime = 0;
	gGlobalsThatLiveAcrossReboot.fSerialOnTime = 0;
	gGlobalsThatLiveAcrossReboot.fSoundOnTime = 0;
	return NILREF;
}


/*------------------------------------------------------------------------------
	Walk a heap: where it starts and ends, how many blocks it has, how much
	of it is free. Starts over when the heap changes underway.
	Args:		inHeap
				outStart, outEnd	the first block, and the end of the last
				outUsedBlocks		the number of blocks
				outFreeSize			the free blocks' size
	Return:		--
------------------------------------------------------------------------------*/

void
GetActualHeapInfo(Heap inHeap, void ** outStart, void ** outEnd, long * outUsedBlocks, long * outFreeSize)
{
	void *	block;

	for ( ; ; )
	{
		*outUsedBlocks = 0;
		*outFreeSize = 0;
		long	seed = HeapSeed(inHeap);
		block = NULL;
		Boolean	isFirst = true;
		for ( ; ; )
		{
			TObjectId	blockOwner;
			Size		blockSize;
			int	blockType = NextHeapBlock(inHeap, seed, block, &block, NULL, NULL, NULL, &blockSize, &blockOwner);
			if (blockType == kMM_HeapSeedFailure)
				break;
			if (blockType == kMM_HeapEndBlock)
				goto done;
			if (isFirst)
			{
				*outEnd = *outStart = block;
				isFirst = false;
			}
			(*outUsedBlocks)++;
			*outEnd = (char *) *outEnd + blockSize;
			if (blockType == kMM_HeapFreeBlock)
			{
				long	freeSize = *outFreeSize;		// (as a variable: the ROM's registers)
				*outFreeSize = freeSize + blockSize;
			}
		}
	}
done:
	;
}


/*------------------------------------------------------------------------------
	Heap statistics: the fixed (Ptr) heap, the relocatable (Handle) heap,
	the NewtonScript frames heap, and the system's free memory.
	Args:		inRcvr
				inOptions		nil, or a frame: garbageCollectFrames (collect
								before measuring the frames heap),
								includeSystemReleasable (count releasable
								memory and free ROM domain pages as free)
	Return:		a frame
------------------------------------------------------------------------------*/

#if 0
/* Not yet: identical but for three words (0x2032BC..0x2032C4). The ROM
   loads framesHeapStart, then framesHeapEnd, for the subtraction; this
   loads framesHeapEnd first and the two loads become one LDM. The order
   is the register allocator's, and depends on the rest of the function
   (removing one SetFrameSlot line flips it); the form of the source that
   gives Apple's order is still to be found. Until then FGetHeapStats
   stays generated assembler. */
Ref
FGetHeapStats(RefArg inRcvr, RefArg inOptions)
{
	RefVar	collectGarbage(NILREF);
	RefVar	includeSystemReleasable(NILREF);
	if (NOTNIL(inOptions))
	{
		collectGarbage = GetFrameSlotRefArg(inOptions, SYM(garbageCollectFrames));
		includeSystemReleasable = GetFrameSlotRefArg(inOptions, SYM(includeSystemReleasable));
	}

	long	ptrUsedBlocks = 0;
	long	handleUsedBlocks = 0;
	long	ptrFreeSize = 0;
	long	handleFreeSize = 0;
	ULong	framesFreeSize = 0;
	void *	handleHeapStart;
	void *	handleHeapEnd;
	void *	ptrHeapStart;
	void *	ptrHeapEnd;
	Ptr		framesHeapStart;
	Ptr		framesHeapEnd;
	ULong	framesLargestFree;
	long	ptrHeapSize;
	long	handleHeapSize;
	long	systemFreeSize;
	RefVar	stats(AllocateFrame());

	// the Ptr heap and the Handle heap
	Heap	heap = GetFixedHeap(GetHeap());
	GetActualHeapInfo(heap, &ptrHeapStart, &ptrHeapEnd, &ptrUsedBlocks, &ptrFreeSize);
	ptrHeapSize = (char *) ptrHeapEnd - (char *) ptrHeapStart;
	SetFrameSlot(stats, SYM(ptrHeapStart), MAKEINT((ULong) ptrHeapStart >> 2));
	SetFrameSlot(stats, SYM(ptrHeapSize), MAKEINT(ptrHeapSize));
	SetFrameSlot(stats, SYM(ptrFreeSize), MAKEINT(ptrFreeSize));

	heap = GetRelocHeap(GetHeap());
	GetActualHeapInfo(heap, &handleHeapStart, &handleHeapEnd, &handleUsedBlocks, &handleFreeSize);
	handleHeapSize = (char *) handleHeapEnd - (char *) handleHeapStart;
	SetFrameSlot(stats, SYM(handleHeapStart), MAKEINT((ULong) handleHeapStart >> 2));
	SetFrameSlot(stats, SYM(handleHeapSize), MAKEINT(handleHeapSize));
	SetFrameSlot(stats, SYM(handleFreeSize), MAKEINT(handleFreeSize));

	// the frames heap
	long	framesHeapSize;
	if (NOTNIL(collectGarbage))
		GC();
	HeapBounds(&framesHeapStart, &framesHeapEnd);
	framesHeapSize = framesHeapEnd - framesHeapStart;
	Statistics(&framesFreeSize, &framesLargestFree);
	SetFrameSlot(stats, SYM(framesHeapStart), MAKEINT((ULong) framesHeapStart >> 2));
	SetFrameSlot(stats, SYM(framesHeapSize), MAKEINT(framesHeapSize));
	SetFrameSlot(stats, SYM(framesFreeSize), MAKEINT(framesFreeSize));

	// the system
	systemFreeSize = TotalSystemFree();
	ULong	romPages;
	if (NOTNIL(includeSystemReleasable))
	{
		ULong	releasable, stackSpaceUsed, pagesUsed;
		GetSystemReleasable(&releasable, &stackSpaceUsed, &pagesUsed);
		systemFreeSize += releasable;
		romPages = ROMDomainManagerFreePageCount();
		systemFreeSize += romPages * kPageSize;
	}
	SetFrameSlot(stats, SYM(systemFreeSize), MAKEINT(systemFreeSize));

	return stats;
}
#endif
