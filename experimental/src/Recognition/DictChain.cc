/*
	File:		DictChain.cc

	Contains:	TDictChain, a chain of dictionaries.

	ROM:		0x20CAB0 (TDictChain::TDictChain) .. 0x20CD24 (after
				UnlockChain), MP2x00 US 2.1 (717006).
*/

#include "Recognition/DictChain.h"
#include "NewtonMemory.h"
#include "NewtErrors.h"


TDictChain::TDictChain()
{ }


TDictChain *
TDictChain::Make(ULong inSize, ULong inCurrent)
{
	TDictChain * chain = new TDictChain;
	if (chain)
	{
		if (chain->IDictChain(inSize, inCurrent))
		{
			chain->Dispose();
			chain = NULL;
		}
	}
	return chain;
}


long
TDictChain::IDictChain(ULong inSize, ULong inCurrent)
{
	fData = NULL;
	long err = IDArray(sizeof(AirusAParmBlock **), inSize);
	if (err == noErr)
		fCurrent = inCurrent;
	return err;
}


long
TDictChain::RemoveDictFromChain(AirusAParmBlock ** inDict)
{
	long index = HandleToPosition(inDict);
	long err = this->Delete(index);
	if (err)
		return err;
	if (fCurrent == index)
		fCurrent = -1;
	return 0;
}


long
TDictChain::AddDictToChain(AirusAParmBlock ** inDict)
{
	return this->InsertEntry(fSize, (char *) &inDict);
}


AirusAParmBlock **
TDictChain::PositionToHandle(ULong index)
{
	AirusAParmBlock *** entry = (AirusAParmBlock ***) GetEntry(index);
	return entry == NULL ? NULL : *entry;
}


long
TDictChain::HandleToPosition(AirusAParmBlock ** inDict)
{
	Lock();
	AirusAParmBlock *** dicts = (AirusAParmBlock ***) *fData;
	ULong index = 0;
	for ( ; index < fSize; index++)
		if (dicts[index] == inDict)
			break;
	Unlock();
	if (index == fSize)
		index = -1;
	return index;
}


void
TDictChain::LockChain(void)
{
	Lock();
	Handle * dicts = (Handle *) *fData;
	ULong index = 0;
	for ( ; index < fSize; index++)
	{
		MoveHHi(dicts[index]);
		HLock(dicts[index]);
	}
	Unlock();
}


void
TDictChain::UnlockChain(void)
{
	Lock();
	Handle * dicts = (Handle *) *fData;
	ULong index = 0;
	for ( ; index < fSize; index++)
		HUnlock(dicts[index]);
	Unlock();
}
