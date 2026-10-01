// Probe: FGetOrientation and FSetOrientation, ROM 0x202AE4..0x202B3C,
// with Apple's objects.h (its inline RINT).

#include "Frames/objects.h"

long	GetGrafInfo(long inSelector, void * outInfo);
void	SetOrientation(long inOrientation);

extern "C" Ref FGetOrientation(RefArg inRcvr);
extern "C" Ref FSetOrientation(RefArg inRcvr, RefArg inOrientation);

Ref
FGetOrientation(RefArg inRcvr)
{
	long	orientation;
	GetGrafInfo(4, &orientation);		// kGrafOrientation
	return MAKEINT(orientation);
}

Ref
FSetOrientation(RefArg inRcvr, RefArg inOrientation)
{
	SetOrientation(RINT(inOrientation));
	return NILREF;
}
