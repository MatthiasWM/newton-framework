/*
	File:		Tablet.h

	Contains:	The tablet (reconstructed: not in the published headers; as
				the port's Recognition/Tablet.h).
*/

#ifndef __TABLET_H
#define __TABLET_H

#ifndef __NEWTONTYPES_H
#include "NewtonTypes.h"
#endif

void		TabSetOrientation(long inOrientation);
NewtonErr	StartBypassTablet(void);
NewtonErr	StopBypassTablet(void);
NewtonErr	InsertTabletSample(ULong inSample, ULong inTime);
Boolean		TabletBufferEmpty(void);

#endif	/* __TABLET_H */
