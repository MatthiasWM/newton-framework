/*
	File:		RDM.h

	Contains:	The ROM domain manager (reconstructed: not in the published
				headers; as the port's OS/RDM.h).
*/

#ifndef __RDM_H
#define __RDM_H

#ifndef __NEWTONTYPES_H
#include "NewtonTypes.h"
#endif

ULong	ROMDomainManagerFreePageCount(void);

#endif	/* __RDM_H */
