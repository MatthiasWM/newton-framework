/*
	File:		RecObject.h

	Contains:	The recognizers' base classes: TRecObject, and the dynamic
				arrays TArray and TDArray kept in Handles (reconstructed: not
				in the published headers; the port's CRecObject, CArray,
				CDArray, Apple's names and layouts).

				The virtual functions are declared in the order of the ROM's
				vtables (tools/vtable.py): a virtual call goes by index.
*/

#ifndef __RECOBJECT_H
#define __RECOBJECT_H

#ifndef __NEWTON_H
#include "Newton.h"
#endif

#ifndef __OBJECTS_H
#include "Frames/objects.h"
#endif

class TMsg;


/*------------------------------------------------------------------------------
	T R e c O b j e c t
------------------------------------------------------------------------------*/

class TRecObject
{
public:
						TRecObject();
						~TRecObject();

	virtual void		Dispose(void);
	virtual void		Dump(TMsg * outMsg);
	virtual ULong		SizeInBytes(void);
	virtual long		CopyInto(TRecObject * outObject);

	void				SetFlags(ULong inBits);
	void				UnsetFlags(ULong inBits);
	Boolean				TestFlags(ULong inBits);
	void				DumpObject(char * inStr);

	ULong				fFlags;			// +04
};


/*------------------------------------------------------------------------------
	A n   i t e r a t o r   o v e r   a   T A r r a y
------------------------------------------------------------------------------*/

struct TArrayIterator;
typedef char * (*IterProcPtr)(TArrayIterator *);

struct TArrayIterator
{
	char **			fMem;			// +00 the array's Handle
	char *			fMemPtr;		// +04 where it was
	char *			fCurrent;		// +08
	ULong			fElementSize;	// +0C
	ULong			fIndex;			// +10
	ULong			fSize;			// +14
	IterProcPtr		fGetNext;		// +18
	IterProcPtr		fGetCurrent;	// +1C
};

char *	GetNext(TArrayIterator * ioIter);
void	RemoveCurrent(TArrayIterator * ioIter);
char *	GetCur(TArrayIterator * ioIter);


/*------------------------------------------------------------------------------
	T A r r a y
------------------------------------------------------------------------------*/

class TArray : public TRecObject
{
public:
						TArray();
						~TArray();

	static TArray *		Make(ULong inElementSize, ULong inSize);

	virtual void		Dispose(void);
	virtual void		Dump(TMsg * outMsg);
	virtual ULong		SizeInBytes(void);
	virtual long		CopyInto(TRecObject * outObject);
	virtual void		IDispose(void);
	virtual long		Add(void);
	virtual char *		AddEntry(void);
	virtual char *		GetEntry(ULong index);
	virtual void		SetEntry(ULong index, char * inData);
	virtual void		Compact(void);
	virtual void		CutToIndex(ULong index);
	virtual void		Clear(void);
	virtual long		Reuse(ULong inSize);
	virtual long		Load(ULong, ULong, ULong, ULong);
	virtual long		LoadFromSoup(RefArg inHeaders, RefArg inData, ULong index);
	virtual long		Save(ULong, ULong, ULong, ULong);

	long				IArray(ULong inElementSize, ULong inSize);
	char *				GetIterator(TArrayIterator * outIter);
	void				Clone(void);
	Boolean				Release(void);
	void				Lock(void);
	void				Unlock(void);

	ULong				fElementSize;	// +08
	ULong				fSize;			// +0C entries in use
	ULong				fFree;			// +10 entries allocated beyond them
	ULong				fChunkSize;		// +14 entries to grow by
	long				fRefCount;		// +18
	char **				fData;			// +1C
};


/*------------------------------------------------------------------------------
	T D A r r a y
------------------------------------------------------------------------------*/

class TDArray : public TArray
{
public:
						TDArray();

	static TDArray *	Make(ULong inElementSize, ULong inSize);
	long				IDArray(ULong inElementSize, ULong inSize);

	virtual long		Delete(ULong index);
	virtual long		DeleteEntries(ULong index, ULong inCount);
	virtual long		Insert(ULong index);
	virtual long		InsertEntry(ULong index, char * inData);
	virtual long		InsertEntries(ULong index, char * inData, ULong inCount);
};

#endif	/* __RECOBJECT_H */
