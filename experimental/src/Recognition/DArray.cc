/*
	File:		DArray.cc

	Contains:	TDArray, a TArray entries can be inserted into and deleted
				from.

	ROM:		0x20C764 (TDArray::TDArray) .. 0x20CAB0 (after
				InsertEntries), MP2x00 US 2.1 (717006).
*/

#include "Recognition/RecObject.h"
#include "Recognition/RecGlue.h"
#include "NewtErrors.h"


TDArray::TDArray()
{ }


TDArray *
TDArray::Make(ULong inElementSize, ULong inSize)
{
	TDArray * array = new TDArray;
	if (array)
	{
		array->fData = NULL;
		if (array->IArray(inElementSize, inSize))
		{
			array->Dispose();
			array = NULL;
		}
	}
	return array;
}


long
TDArray::IDArray(ULong inElementSize, ULong inSize)
{
	long err = IArray(inElementSize, inSize);
	NameHandle(fData, 'dDta');
	return err;
}


long
TDArray::Delete(ULong index)
{
	return DeleteEntries(index, 1);
}


long
TDArray::DeleteEntries(ULong index, ULong inCount)
{
	if (inCount == 0)
		return 0;
	if (index < 0 || index >= fSize)
		return -1;
	if (index + inCount >= fSize)
		inCount = fSize - index;
	char * dst = GetEntry(index);
	if (dst)
	{
		ULong next = index + inCount;
		char * src = GetEntry(next);
		if (src)
			MoveBlock(src, dst, (fSize - next) * fElementSize);
		fSize -= inCount;
		fFree += inCount;
	}
	return index;
}


long
TDArray::Insert(ULong index)
{
	ULong next = index + 1;
	char * entry = GetEntry(index);
	if (Add() == -1)
		return -1;
	if (entry)
	{
		char * src = GetEntry(index);
		char * dst = GetEntry(next);
		MoveBlock(src, dst, fElementSize * (fSize - next));
	}
	else
		index = fSize - 1;
	return index;
}


long
TDArray::InsertEntry(ULong index, char * inData)
{
	long i = Insert(index);
	if (i != -1)
		MoveBlock(inData, GetEntry(i), fElementSize);
	return i;
}


long
TDArray::InsertEntries(ULong index, char * inData, ULong inCount)
{
	char *	src = GetEntry(index);
	char *	dst = GetEntry(index + inCount);	// NULL past the end, and stale if the Handle moves (BUGS.md B9)
	ULong	insertSize = inCount * fElementSize;
	ULong	moveSize = (fSize - index) * fElementSize;
	if (ResizeHandle(fData, (fSize + fChunkSize) * fElementSize + insertSize) == noErr)
	{
		MoveBlock(src, dst, moveSize);
		MoveBlock(inData, src, insertSize);
		fSize += inCount;
		fFlags |= 1;
	}
	else
		index = -1;
	return index;
}
