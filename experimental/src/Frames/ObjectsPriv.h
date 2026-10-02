/*
	File:		ObjectsPriv.h

	Contains:	NewtonScript object functions the published objects.h leaves
				out (reconstructed: as the port's Frames/Objects.h).
*/

#ifndef __OBJECTSPRIV_H
#define __OBJECTSPRIV_H

#ifndef __OBJECTS_H
#include "Frames/objects.h"
#endif

Ref		MakeArray(long inLength);

#endif	/* __OBJECTSPRIV_H */
