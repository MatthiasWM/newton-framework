/*
	File:		Gestalt.h

	Contains:	Gestalt from NewtonScript (reconstructed: not in the
				published headers; the functions are in the port's
				OS/Gestalt.cc).
*/

#ifndef __GESTALT_H
#define __GESTALT_H

#ifndef __NEWTONGESTALT_H
#include "OS600/NewtonGestalt.h"
#endif

void	VersionString(TGestaltSystemInfo * inInfo, UniChar * outString);

#endif	/* __GESTALT_H */
