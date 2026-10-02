/*
	File:		NewtGlobals.h

	Contains:	NewtonScript's global data (reconstructed: not in the
				published headers; as the port's Frames/NewtGlobals.h, with
				Apple's class names).
*/

#ifndef __NEWTGLOBALS_H
#define __NEWTGLOBALS_H

#ifndef __NEWTONTYPES_H
#include "NewtonTypes.h"
#endif
#ifndef __NEWTQD_H
#include "QD/NewtQD.h"
#endif

class TInterpreter;

struct NewtGlobals
{
	long			stackPos;		// +00
	TInterpreter *	interpreter;	// +04
	VAddr			stackTop;		// +08
	GrafPtr			graf;			// +0C
	Ptr				buf;			// +10
	Ptr				bufPtr;			// +14
};

extern NewtGlobals *	gNewtGlobals;	// 0C1054B0

#endif	/* __NEWTGLOBALS_H */
