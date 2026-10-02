/*
	File:		NewtonScript.h

	Contains:	Calling NewtonScript from C++ (reconstructed: not in the
				published headers; as the port's Frames/NewtonScript.h).
*/

#ifndef __NEWTONSCRIPT_H
#define __NEWTONSCRIPT_H

#ifndef __OBJECTS_H
#include "Frames/objects.h"
#endif

Ref		NSSendRootMessage(RefArg inSym);
Ref		NSSendRootMessage(RefArg inSym, RefArg inArg1);
Ref		NSSendRootMessage(RefArg inSym, RefArg inArg1, RefArg inArg2);

#endif	/* __NEWTONSCRIPT_H */
