/*
	File:		DictChain.h

	Contains:	TDictChain, a chain of dictionaries: a TDArray of their
				Handles (reconstructed: not in the published headers).
*/

#ifndef __DICTCHAIN_H
#define __DICTCHAIN_H

#ifndef __RECOBJECT_H
#include "Recognition/RecObject.h"
#endif

struct AirusAParmBlock;

class TDictChain : public TDArray
{
public:
						TDictChain();

	static TDictChain *	Make(ULong inSize, ULong inCurrent);
	long				IDictChain(ULong inSize, ULong inCurrent);

	long				RemoveDictFromChain(AirusAParmBlock ** inDict);
	long				AddDictToChain(AirusAParmBlock ** inDict);
	AirusAParmBlock **	PositionToHandle(ULong index);
	long				HandleToPosition(AirusAParmBlock ** inDict);
	void				LockChain(void);
	void				UnlockChain(void);

	long				fCurrent;		// +20 a position, -1 for none
};

#endif	/* __DICTCHAIN_H */
