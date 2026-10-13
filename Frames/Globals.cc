/*
	File:		Globals.c

	Contains:	Global object definitions.

	Written by:	Newton Research Group.
*/

#include "Objects.h"

/*----------------------------------------------------------------------
	G l o b a l s
------------------------------------------------------------------------
	This is a GC root so it will be updated automatically.
----------------------------------------------------------------------*/

Ref	gVarFrame;
Ref *	RSgVarFrame = &gVarFrame;


/*----------------------------------------------------------------------
	SetGlobal(value, symbol): what `global x := value` compiles to (the
	compiler pushes the value, then the symbol). As the ROM's FSetGlobal
	(0x002B72C8): sets the global variable (its symbol made internal, a
	package's may go away) and returns the symbol. (It was a stub.)
----------------------------------------------------------------------*/

extern "C" Ref FSetGlobal(RefArg inRcvr, RefArg inValue, RefArg inTag);

Ref
FSetGlobal(RefArg inRcvr, RefArg inValue, RefArg inTag)
{
	RefVar tag(EnsureInternal(inTag));
	SetFrameSlot(RA(gVarFrame), tag, inValue);
	return inTag;
}
