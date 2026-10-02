/*
	File:		T28F016_SA_SVDriver.cc

	Contains:	The flash driver for Intel's 28F016SA and 28F016SV and
				Sharp's chips: identification, reading, writing through the
				page buffer, erasing, block locking.

	ROM:		0x203DE8 (Sizeof) .. 0x204698 (after Identify), MP2x00 US
				2.1 (717006).

	A command goes to every lane the range has: the command byte in each
	byte of a word, masked with the lanes (fLanes). The compiler makes
	x & 0x71717171 as BIC of the complement.
*/

#include "Stores/T28F016_SA_SVDriver.h"
#include "OS600/OSErrors.h"
#include "NewtErrors.h"

void	ShortTimerDelay(ULong inTicks);


/*------------------------------------------------------------------------------
	A command byte for every byte lane.
------------------------------------------------------------------------------*/

static inline ULong
Replicate(ULong inByte)
{
	inByte |= inByte << 8;
	return inByte | (inByte << 16);
}


size_t
T28F016_SA_SVDriver::Sizeof(void)
{
	return sizeof(T28F016_SA_SVDriver);
}


/*------------------------------------------------------------------------------
	Send the read status command until the status has the bits.
	Args:		inAddress		where commands go
				inLanes			the lanes
				inMask			the status bits to look at
				inValue			what they must be
				inStatusOffset	where the status is, from inAddress
	Return:		--
------------------------------------------------------------------------------*/

void
T28F016_SA_SVDriver::WaitForStatus(volatile ULong * inAddress, ULong inLanes, ULong inMask, ULong inValue, ULong inStatusOffset)
{
	ULong	command = inLanes & 0x71717171;		// read extended status
	*inAddress = command;
	ULong	mask = inMask & inLanes;
	ULong	value = inValue & inLanes;
	volatile ULong *	status = (volatile ULong *) ((char *) inAddress + inStatusOffset);
	while ((*status & mask) != value)
		*inAddress = command;
}


void
T28F016_SA_SVDriver::CleanUpDriverData(TFlashRange & inRange, TMemoryAllocator & inAllocator)
{
	inAllocator.Deallocate(inRange.fDriverData);
	inRange.fDriverData = NULL;
}


#if 0
/* Not yet: the ROM loads the lane count into r0 and the chip count into r2,
   one after the other (the chip count before the first test, used in two
   branches); here they come in the other registers and become one LDM.
   Until the form is found, it stays generated assembler. */
NewtonErr
T28F016_SA_SVDriver::InitializeDriverData(TFlashRange & inRange, TMemoryAllocator & inAllocator)
{
	S28F016DriverData *	data = (S28F016DriverData *) inAllocator.Allocate(sizeof(S28F016DriverData));
	if (data == NULL)
		return kError_No_Memory;
	inRange.fDriverData = data;
	ULong	laneCount = inRange.fLaneCount;
	if (laneCount == 4)
		data->fLanes = (inRange.fChipCount == 4) ? 0xFFFFFFFF : 0x00FF00FF;
	else if (laneCount == 2)
	{
		data->fLanes = (inRange.fChipCount == 2) ? 0x0000FFFF : 0x000000FF;
		if (inRange.fLanes == 0xFFFF0000)
			data->fLanes = data->fLanes << 16;
	}
	else
	{
		data->fLanes = 0x0000FF00;
		if (inRange.fLanes == 0xFF000000)
			data->fLanes = 0xFF000000;
	}
	if (inRange.fChipInfo.fWidth == 1)
	{
		data->fStatusOffset = 16;
		data->fBlockStatusOffset = 8;
	}
	else
	{
		data->fStatusOffset = 8;
		data->fBlockStatusOffset = 4;
	}
	data->fWordsLeft = 0;
	data->fEraseUnderway = false;
	data->fEraseSuspended = false;
	data->fResumeTwice = false;
	return noErr;
}
#endif


void
T28F016_SA_SVDriver::StartReadingArray(TFlashRange & inRange)
{
	S28F016DriverData *	data = (S28F016DriverData *) inRange.fDriverData;
	ULong	lanes = data->fLanes;
	volatile ULong *	address = (volatile ULong *) inRange.fWriteAddress;
	if (data->fEraseUnderway)
	{
		*address = lanes & 0xB0B0B0B0;			// suspend
		WaitForDeviceWSMReady(address, lanes, data->fStatusOffset);
		ULong	none = !lanes;				// always 0: a bug (BUGS.md B1)
		if ((none & *address & lanes & 0x40404040) != 0)
		{
			data->fResumeTwice = true;
			data->fEraseUnderway = false;
			data->fEraseSuspended = false;
		}
		else
		{
			data->fResumeTwice = false;
			data->fEraseSuspended = true;
		}
		ShortTimerDelay(74);
	}
	else
		WaitForDeviceWSMReady(address, lanes, data->fStatusOffset);
	*address = lanes;							// read array
}


void
T28F016_SA_SVDriver::DoneReadingArray(TFlashRange & inRange)
{
	S28F016DriverData *	data = (S28F016DriverData *) inRange.fDriverData;
	if (data->fEraseSuspended)
	{
		volatile ULong *	address = (volatile ULong *) inRange.fWriteAddress;
		*address = data->fLanes & 0xD0D0D0D0;	// resume
		data->fEraseSuspended = false;
	}
}


void
T28F016_SA_SVDriver::BeginWrite(TFlashRange & inRange, ULong inAddress, ULong inWords)
{
	S28F016DriverData *	data = (S28F016DriverData *) inRange.fDriverData;
	data->fWordsLeft = inWords;
	data->fWriteAddress = inAddress;
	StartLoadingPageBuffer(inRange);
}


#if 0
/* Not yet: identical but for one register (0x204060: the ROM computes
   inValue | ~inMask into r1, this into r0). Until the form is found, it
   stays generated assembler. */
void
T28F016_SA_SVDriver::Write(ULong inValue, ULong inMask, ULong inAddress, TFlashRange & inRange)
{
	inMask = ~inMask;
	*(volatile ULong *) inAddress = inValue | inMask;
	S28F016DriverData *	data = (S28F016DriverData *) inRange.fDriverData;
	data->fBufferWordsLeft--;
	if (data->fBufferWordsLeft != 0)
		return;
	volatile ULong *	address;
	ULong	lanes = data->fLanes;
	address = (volatile ULong *) inRange.StartOfBlockWriteVirtualAddress(inAddress);
	WaitForQueueAndPageBuffer(address, lanes, data->fStatusOffset);
	*address = lanes & 0x0C0C0C0C;				// write the page buffer
	*address = Replicate(data->fBufferWords - 1) & lanes;
	*(volatile ULong *) data->fWriteAddress = 0;
	data->fWordsLeft -= data->fBufferWords;
	if (data->fWordsLeft == 0)
		return;
	data->fWriteAddress = inAddress + 4;
	StartLoadingPageBuffer(inRange);
}
#endif


void
T28F016_SA_SVDriver::StartLoadingPageBuffer(TFlashRange & inRange)
{
	S28F016DriverData *	data = (S28F016DriverData *) inRange.fDriverData;
	volatile ULong *	address = (volatile ULong *) inRange.fWriteAddress;
	ULong	lanes = data->fLanes;
	WaitForStatus(address, lanes, 0x02020202, 0x02020202, data->fStatusOffset);
	ULong	words = (inRange.fChipInfo.fWidth == 1) ? 256 : 128;
	if (data->fWordsLeft <= words)
		words = data->fWordsLeft;
	data->fBufferWordsLeft = data->fBufferWords = words;
	ULong	count = Replicate(words - 1) & lanes;
	*address = lanes & 0xE0E0E0E0;				// load the page buffer
	*address = count;
	*address = 0;
}


NewtonErr
T28F016_SA_SVDriver::ReportWriteResult(TFlashRange & inRange, ULong inBlockAddress)
{
	long	result;
	ReportWriteEraseStatus(inRange, inBlockAddress, result, kWait);
	return result;
}


#if 0
/* Not yet: the same instructions in other registers: the ROM keeps the
   driver's data in r12 and the address in r0, this in callee-saved
   registers. Until the form is found, it stays generated assembler. */
Boolean
T28F016_SA_SVDriver::ReportWriteEraseStatus(TFlashRange & inRange, ULong inBlockAddress, long & outResult, eWaitOption inOption)
{
	S28F016DriverData *	data = (S28F016DriverData *) inRange.fDriverData;
	volatile ULong *	address = (volatile ULong *) inBlockAddress;
	ULong	lanes = data->fLanes;
	ULong	mask = lanes & 0x80808080;		// ready
	ULong	value = lanes & 0x80808080;
	volatile ULong *	status = (volatile ULong *) (data->fBlockStatusOffset + inBlockAddress);
	ULong	command = lanes & 0x71717171;		// read extended status
	*address = command;
	ULong	state;
	if (inOption == kWait)
	{
		while (((state = *status) & mask) != value)
			*address = command;
	}
	else
	{
		state = *status;
		if ((state & mask) != value)
			return false;
	}
	long	err = noErr;
	state &= lanes;
	if (state & 0x20202020)
		err = (state & 0x04040404) ? kError_Flash_Vpp_Low : kError_Flash_Write_Failed;
	outResult = err;
	if (err != noErr)
		CleanErrorStatus(address, lanes, data->fStatusOffset);
	ShortTimerDelay(74);
	return true;
}
#endif


void
T28F016_SA_SVDriver::IssueCommonBlockCommand(TFlashRange & inRange, ULong inBlockAddress, ULong inCommand)
{
	volatile ULong *	address = (volatile ULong *) inBlockAddress;
	S28F016DriverData *	data = (S28F016DriverData *) inRange.fDriverData;
	ULong	lanes = data->fLanes;
	WaitForBlockQueue(address, lanes, data->fBlockStatusOffset);
	*address = inCommand & lanes;
	ULong	confirm = lanes & 0xD0D0D0D0;
	*address = confirm;
	if (data->fResumeTwice)
	{
		data->fResumeTwice = false;
		*address = confirm;
	}
}


void
T28F016_SA_SVDriver::WaitForDeviceWSMReady(volatile ULong * inAddress, ULong inLanes, ULong inStatusOffset)
{
	WaitForStatus(inAddress, inLanes, 0x80808080, 0x80808080, inStatusOffset);
}


NewtonErr
T28F016_SA_SVDriver::StartErase(TFlashRange & inRange, ULong inBlockAddress)
{
	volatile ULong *	address = (volatile ULong *) inBlockAddress;
	S28F016DriverData *	data = (S28F016DriverData *) inRange.fDriverData;
	ULong	lanes = data->fLanes;
	WaitForDeviceWSMReady(address, lanes, data->fStatusOffset);
	IssueCommonBlockCommand(inRange, inBlockAddress, 0x20202020);	// erase
	return noErr;
}


Boolean
T28F016_SA_SVDriver::IsEraseComplete(TFlashRange & inRange, ULong inBlockAddress, long & outResult)
{
	return ReportWriteEraseStatus(inRange, inBlockAddress, outResult, kDontWait);
}


NewtonErr
T28F016_SA_SVDriver::LockBlock(TFlashRange & inRange, ULong inBlockAddress)
{
	IssueCommonBlockCommand(inRange, inBlockAddress, 0x77777777);	// lock
	return ReportWriteResult(inRange, inBlockAddress);
}


void
T28F016_SA_SVDriver::WaitForQueueAndPageBuffer(volatile ULong * inAddress, ULong inLanes, ULong inStatusOffset)
{
	WaitForStatus(inAddress, inLanes, 0x0A0A0A0A, 0x02020202, inStatusOffset);
}


void
T28F016_SA_SVDriver::WaitForBlockQueue(volatile ULong * inAddress, ULong inLanes, ULong inStatusOffset)
{
	WaitForStatus(inAddress, inLanes, 0x08080808, 0, inStatusOffset);
}


void
T28F016_SA_SVDriver::CleanErrorStatus(volatile ULong * inAddress, ULong inLanes, ULong inStatusOffset)
{
	WaitForStatus(inAddress, inLanes, 0xC0C0C0C0, 0x80808080, inStatusOffset);
	*inAddress = inLanes & 0x50505050;			// clear the status register
}


void
T28F016_SA_SVDriver::ResetBlockStatus(TFlashRange & inRange, ULong inBlockAddress)
{
	volatile ULong *	address = (volatile ULong *) inBlockAddress;
	S28F016DriverData *	data = (S28F016DriverData *) inRange.fDriverData;
	ULong	lanes = data->fLanes;
	WaitForQueueAndPageBuffer(address, lanes, data->fStatusOffset);
	*address = lanes & 0x97979797;				// upload status bits
	*address = lanes & 0xD0D0D0D0;
	CleanErrorStatus(address, lanes, data->fStatusOffset);
}


void
T28F016_SA_SVDriver::CleanUp(TMemoryAllocator & inAllocator)
{ }


NewtonErr
T28F016_SA_SVDriver::Init(TMemoryAllocator & inAllocator)
{
	return noErr;
}


/*------------------------------------------------------------------------------
	Whether the chips at an address are ours, and what they are.
	Args:		inAddress		where they are
				inLanes			which byte lanes to look at
				outInfo			what they are
	Return:		true if they are ours
------------------------------------------------------------------------------*/

Boolean
T28F016_SA_SVDriver::Identify(ULong inAddress, ULong inLanes, SFlashChipInformation & outInfo)
{
	volatile ULong *	address = (volatile ULong *) inAddress;
	ULong	mask = 0xFF;
	ULong	width = 1;
	ULong	shift;
	switch (inLanes)
	{
	case 0x000000FF:	shift = 0; break;
	case 0x0000FF00:	shift = 8; break;
	case 0x0000FFFF:	shift = 0; mask = 0xFFFF; width = 2; break;
	case 0x00FF0000:	shift = 16; break;
	case 0xFF000000:	shift = 24; break;
	case 0xFFFF0000:	shift = 16; mask = 0xFFFF; width = 2; break;
	default:			return false;
	}
	ULong	lanes = mask << shift;
	ULong	readArray = mask << shift;
	ULong	reset = (mask << shift) & 0x80808080;
	for (ULong i = 0; i < 257; i++)
	{
		*address = reset;
		*address = readArray;
	}
	*address = readArray;
	*address = lanes & 0x90909090;				// read identifier
	ULong	manufacturer = mask & (address[0] >> shift);
	ULong	device = mask & (address[1] >> shift);
	*address = readArray;						// read array
	if (manufacturer == 0x89)
	{
		if (device != (mask & 0x66A0))
			return false;
	}
	else if (manufacturer == 0xB0 && device == (mask & 0x6688))
		;
	else if (manufacturer != 0xB0 || device != (mask & 0x66A8))
		return false;
	outInfo.fManufacturer = manufacturer;
	outInfo.fDevice = device;
	if (manufacturer == 0x89)
		outInfo.fVppKind = 2;
	else if (manufacturer == 0xB0)
		outInfo.fVppKind = 1;
	else
		return false;
	outInfo.fWidth = width;
	outInfo.fChipSize = (device == (mask & 0x66A8)) ? 0x100000 : 0x200000;
	outInfo.fBlockSize = 0x10000;
	return true;
}
