/*
	File:		ItemTester.h

	Contains:	Interface to the CItemTester class.

	Written by:	Newton Research Group.
*/

#if !defined(__ITEMTESTER_H)
#define __ITEMTESTER_H 1

#if !defined(__NEWTON_H)
#include "Newton.h"
#endif


enum CompareResult
{
	kItemLessThanCriteria = -1,
	kItemEqualCriteria = 0,
	kItemGreaterThanCriteria = 1
};


/*----------------------------------------------------------------------
	C I t e m T e s t e r
----------------------------------------------------------------------*/

class CItemTester : public SingleObject
{
public:
	virtual						~CItemTester() {}
	virtual CompareResult	testItem(const void * inItem) const;
};


#endif	/*	__ITEMTESTER_H	*/

