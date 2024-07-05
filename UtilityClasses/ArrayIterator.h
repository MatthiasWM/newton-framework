/*
	File:		ArrayIterator.h

	Contains:	Interface to the CArrayIterator class.

	Written by:	Newton Research Group.
*/

#if !defined(__ARRAYITERATOR_H)
#define __ARRAYITERATOR_H 1

#if !defined(__LIST_H)
#include "List.h"
#endif


enum IterateDirection
{
	kIterateBackward,
	kIterateForward
};



/*------------------------------------------------------------------------------
	C A r r a y I t e r a t o r
------------------------------------------------------------------------------*/

class CArrayIterator : public SingleObject
{
public:
					CArrayIterator();
					CArrayIterator(CDynamicArray * inDynamicArray);
					CArrayIterator(CDynamicArray * inDynamicArray, bool inForward);
					CArrayIterator(CDynamicArray * inDynamicArray,
							ArrayIndex inLowBound,
							ArrayIndex inHighBound, bool inForward);
					~CArrayIterator();

	void			init(void);
	void			init(CDynamicArray * inDynamicArray,
							ArrayIndex inLowBound,
							ArrayIndex inHighBound, bool inForward);

	void			initBounds(ArrayIndex inLowBound, ArrayIndex inHighBound, bool inForward);
	void			reset(void);
	void			resetBounds(bool inForward = kIterateForward);

	void			switchArray(CDynamicArray * inNewArray, bool inForward = kIterateForward);

	ArrayIndex	firstIndex(void);
	ArrayIndex	nextIndex(void);
	ArrayIndex	currentIndex(void);

	void			removeElementsAt(ArrayIndex index, ArrayIndex inCount);
	void			insertElementsBefore(ArrayIndex index, ArrayIndex inCount);
	void			deleteArray(void);

	bool			more(void);

protected:
	void			advance(void);

	CDynamicArray *	fDynamicArray;			// the associated dynamic array

	ArrayIndex			fCurrentIndex;			// current index of this iteration
	ArrayIndex			fLowBound;				// lower bound of iteration in progress
	ArrayIndex			fHighBound;				// upper bound of iteration in progress
	bool					fIterateForward;		// if iteration is forward or backward

private:
	friend class CDynamicArray;
	friend class CList;
	friend class CSortedList;

	CArrayIterator *	appendToList(CArrayIterator * inList);
	CArrayIterator *	removeFromList(void);

	CArrayIterator *	fPreviousLink;			// link to previous iterator
	CArrayIterator *	fNextLink;				// link to next iterator
};

#endif	/*	__ARRAYITERATOR_H	*/

