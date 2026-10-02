/*
	File:		Array.cc

	Contains:	TArray, the recognizers' dynamic array: entries of one size
				in a Handle, grown by a chunk at a time; its iterator.

	ROM:		0x208E98 (TArray::TArray) .. 0x209654 (after Unlock), MP2x00
				US 2.1 (717006).
*/

#include "Recognition/RecObject.h"
#include "Recognition/RecGlue.h"
#include "Recognition/Msg.h"
#include "NewtErrors.h"
#include "CLibrary/stdio.h"


/*------------------------------------------------------------------------------
	T A r r a y
------------------------------------------------------------------------------*/

TArray::TArray()
{ }


TArray::~TArray()
{ }


ULong
TArray::SizeInBytes(void)
{
	ULong dataSize = fData ? SizeOfHandle(fData) : 0;
	return TRecObject::SizeInBytes() + dataSize;
}


long
TArray::CopyInto(TRecObject * outObject)
{
	char **		data;
	long		err = 1;

	if (outObject != NULL && (err = TRecObject::CopyInto(outObject)) == noErr)
	{
		TArray * target = (TArray *) outObject;
		data = fData;
		err = data ? CopyHandle(&data) : 1;
		target->fData = data;
		target->fElementSize = fElementSize;
		target->fSize = err ? 0 : fSize;
		target->fFree = fFree;
		target->fChunkSize = fChunkSize;
		target->fRefCount = 0;
	}
	return err;
}


long
TArray::Reuse(ULong inSize)
{
	fFree = 0;
	long err = fData ? ResizeHandle(fData, fElementSize * (fChunkSize + inSize)) : 1;
	if (err)
		inSize = 0;
	fSize = inSize;
	return err;
}


void
TArray::Compact(void)
{
	if (fFree != 0)
	{
		fFree = 0;
		if (fData)
			ResizeHandle(fData, fSize * fElementSize);
	}
}


long
TArray::Load(ULong, ULong, ULong, ULong)
{
	return 0;
}


long
TArray::LoadFromSoup(RefArg inHeaders, RefArg inData, ULong index)
{
	ULong *		header = (ULong *) BinaryData(GetArraySlot(inHeaders, index));
	Ptr			data = BinaryData(GetArraySlot(inData, index));
	char **		handle = NewFakeHandle(data, Length(GetArraySlot(inData, index)));
	fElementSize = header[0];
	fSize = header[1];
	fFree = 0;
	fChunkSize = header[2];
	DeleteHandle(fData);
	fData = handle;
	return 1;
}


#if 0
/* Not yet: identical but for three words (0x209168: the ROM loads fSize
   into r0 and the Handle's pointer into r1 for the second store; here the
   other way round, whichever way the stores are written). Until the form
   is found, it stays generated assembler. */
long
TArray::Save(ULong inHeaderType, ULong inHeaderID, ULong inDataType, ULong inDataID)
{
	Compact();
	char ** header = MakeHandle(3 * sizeof(ULong));
	NameHandle(header, 'asav');
	if (header)
	{
		((ULong *) *header)[0] = fElementSize;
		((ULong *) *header)[1] = fSize;
		((ULong *) *header)[2] = fChunkSize;
		SaveResource(header, inHeaderType, inHeaderID, NULL);
		SaveResource(fData, inDataType, inDataID, NULL);
		DeleteHandle(header);
	}
	return 1;
}
#endif


char *
TArray::GetEntry(ULong index)
{
	if (fData == NULL || index < 0 || index >= fSize)
		return NULL;
	return *fData + index * fElementSize;
}


char *
TArray::GetIterator(TArrayIterator * outIter)
{
	outIter->fGetNext = GetNext;
	outIter->fGetCurrent = GetCur;
	if (fData != NULL && fSize != 0)
	{
		char ** data = fData;
		outIter->fMem = data;
		outIter->fCurrent = outIter->fMemPtr = *data;
	}
	else
	{
		outIter->fMem = NULL;
		outIter->fMemPtr = NULL;
		outIter->fCurrent = NULL;
	}
	outIter->fElementSize = fElementSize;
	outIter->fIndex = 0;
	outIter->fSize = fSize;
	return outIter->fCurrent;
}

TArray *
TArray::Make(ULong inElementSize, ULong inSize)
{
	TArray * array = new TArray;
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


/*------------------------------------------------------------------------------
	The iterator. The array's Handle can move between calls: the pointers
	follow it.
------------------------------------------------------------------------------*/

#if 0
/* Not yet: identical but for three words (0x2092EC: the ROM loads
   fElementSize, then fCurrent; here one LDM, fCurrent first). Until the
   form is found, it stays generated assembler. */
char *
GetNext(TArrayIterator * ioIter)
{
	if (ioIter->fMemPtr != *ioIter->fMem)
	{
		long delta = *ioIter->fMem - ioIter->fMemPtr;
		ioIter->fMemPtr = *ioIter->fMem;
		ioIter->fCurrent += delta;
	}
	ioIter->fIndex++;
	return ioIter->fCurrent += ioIter->fElementSize;
}
#endif


void
RemoveCurrent(TArrayIterator * ioIter)
{
	ioIter->fSize--;
	ioIter->fIndex--;
	ioIter->fCurrent -= ioIter->fElementSize;
}


char *
GetCur(TArrayIterator * ioIter)
{
	if (ioIter->fMemPtr != *ioIter->fMem)
	{
		long delta = *ioIter->fMem - ioIter->fMemPtr;
		ioIter->fMemPtr = *ioIter->fMem;
		ioIter->fCurrent += delta;
	}
	return ioIter->fCurrent;
}


/*------------------------------------------------------------------------------
	Changing the size.
------------------------------------------------------------------------------*/

void
TArray::Clear(void)
{
	CutToIndex(0);
}


void
TArray::CutToIndex(ULong index)
{
	ULong cut = fSize - index;
	fSize -= cut;
	fFree += cut;
}


#if 0
/* Not yet: identical but for one word (0x2093F0: the ROM tests the
   second ResizeHandle's result with CMP, not TEQ, then BNE). Until the
   form is found, it stays generated assembler. */
long
TArray::Add(void)
{
	ULong	free = fFree;
	ULong	size = fSize;
	ULong	chunk = fChunkSize;
	long	index = -1;
	ULong	newFree = free - 1;
	if (free == 0)
	{
		newFree = chunk;
		if (ResizeHandle(fData, (size + chunk + 1) * fElementSize) != noErr)
		{
			newFree = 0;
			if (ResizeHandle(fData, (size + 1) * fElementSize) != noErr)
				goto done;
		}
	}
	index = size++;
	fFlags |= 1;
done:
	fSize = size;
	fFree = newFree;
	return index;
}
#endif


char *
TArray::AddEntry(void)
{
	return GetEntry(Add());
}


void
TArray::SetEntry(ULong index, char * inData)
{
	char * entry = GetEntry(index);
	if (entry)
		MoveBlock(inData, entry, fElementSize);
}


long
TArray::IArray(ULong inElementSize, ULong inSize)
{
	long err = noErr;
	fChunkSize = 6;
	fFlags = 0;
	fFree = inSize ? 0 : 6;
	fElementSize = inElementSize;
	fSize = inSize;
	if (fData)
	{
		if ((err = ResizeHandle(fData, inElementSize * (fFree + inSize))) != noErr)
		{
			DeleteHandle(fData);
			fData = NULL;
		}
	}
	else
	{
		char ** data = MakeHandle(inElementSize * (fFree + inSize));
		NameHandle(data, 'adta');
		fData = data;
		if (data != NULL)
			goto done;
		err = MemoryError();
	}
	if (err)
		fSize = 0;
done:
	fRefCount = 0;
	return err;
}


void
TArray::Dump(TMsg * outMsg)
{
	char buf[100];
	sprintf(buf, "\n\tes: %ld  cnt: %ld  free: %ld", fElementSize, fSize, fFree);
	outMsg->MsgStr(buf);
	outMsg->MsgLF();
}


void
TArray::Dispose(void)
{
	if (Release())
		IDispose();
}


void
TArray::IDispose(void)
{
	if (fData)
		DeleteHandle(fData);
	delete this;
}


void
TArray::Clone(void)
{
	fRefCount++;
}


Boolean
TArray::Release(void)
{
	return --fRefCount < 0;
}


void
TArray::Lock(void)
{
	if (fData)
		LockHandle(fData);
}


void
TArray::Unlock(void)
{
	if (fData)
		UnlockHandle(fData);
}
