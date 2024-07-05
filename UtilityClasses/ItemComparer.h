/*
	File:		ItemComparer.h

	Contains:	Interface to the CItemComparer class.

	Written by:	Newton Research Group.
*/

#if !defined(__ITEMCOMPARER_H)
#define __ITEMCOMPARER_H 1

#if !defined(__ITEMTESTER_H)
#include "ItemTester.h"
#endif


/*--------------------------------------------------------------------------------
	C I t e m C o m p a r e r

	NOTE
	CItemComparer::testItem should return
		kItemLessThanCriteria for (fItem < criteria)
		kItemGreaterThanCriteria for (fItem > criteria)
		kItemEqualCriteria for (fItem == criteria)
	This will keep the CSortedList sorted in ascending order.

	Change the value of inForward in CArrayIterator::CArrayIterator
	to false to view a list in descending order.
--------------------------------------------------------------------------------*/

class CItemComparer : public CItemTester
{
public:
					CItemComparer();
					CItemComparer(const void * inItem, const void * inKeyValue = NULL);

	void			setTestItem(const void * inItem);
	void			setKeyValue(const void * inKeyValue);

// functions that wrap inline methods so they can be used by external callers
	void 			xuSetTestItem(const void * inItem);
	void 			xuSetKeyValue(const void * inKeyValue);

	virtual CompareResult	testItem(const void * inItem) const;

protected:
	const void *	fItem;
	const void *	fKey;
};


/*--------------------------------------------------------------------------------
	C I t e m C o m p a r e r   I n l i n e s
--------------------------------------------------------------------------------*/

inline void CItemComparer::setTestItem(const void * inItem)
{ fItem = inItem; }

inline void CItemComparer::setKeyValue(const void * inKeyValue)
{ fKey = inKeyValue; }

#endif	/*	__ITEMCOMPARER_H	*/

