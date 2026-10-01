/*
	File:		PMuxService.h

	Contains:	Protocol interface for Openning a connection end.

	Copyright:	© 1992-1995 by Apple Computer, Inc., all rights reserved.

	Derived from v1 (2/13/92) internal.

*/



#ifndef	__PMUXSERVICE_H
#define __PMUXSERVICE_H

#ifndef __CONFIGCOMMUNICATIONS_H
#include "Communications/ConfigCommunications.h"
#endif

#ifndef __PROTOCOLS_H
#include "OS600/Protocols.h"
#endif

#ifndef __OPTIONARRAY_H
#include "CommAPI/OptionArray.h"
#endif

#ifndef __CMSERVICE_H
#include "CommAPI/CMService.h"
#endif

class TCMService;

//--------------------------------------------------------------------------------
//		PMuxService
//--------------------------------------------------------------------------------
PROTOCOL PMuxService : public TProtocol
{
	public:
		static	PMuxService*			New(char*);		// allocate variables, initialize state, return this
		void							Delete();		// clean up
				
		NewtonErr						OpenConnectionEnd(TOptionArray* options, ULong muxToolServiceId, char* pmuxImplementationNamePtr, TCMService* service, Size heapSize);
};


#endif /* __PMUXSERVICE_H */
