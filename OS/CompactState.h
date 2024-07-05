/*
	File:		CompactState.h

	Contains:	CompactState declarations.

	Written by:	Newton Research Group, 2010.
*/

#if !defined(__COMPACTSTATE_H)
#define __COMPACTSTATE_H 1

#include "StoreDriver.h"


struct SCompactState
{
	void	init(void);
	bool	isValid(void);
	bool	inProgress(void);

	ULong				signature1;
	ULong				refCount;
	int				x08;
	int				x0C;
	int				x10;
	ULong				signature2;
	ULong				x18;
	ULong				x1C;
	ULong				x20;
	ULong				x24;
	ULong				x28;
	ULong				x2C;
	ULong				x30;
	CStoreDriver	x34;
};


#endif	/* __COMPACTSTATE_H */

