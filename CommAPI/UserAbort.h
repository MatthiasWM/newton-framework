/*
	File:		UserAbort.h

	Contains:	Interface to the CUserAbortEvent and CUserAbortHandler objects.

	Copyright:	� 1993, 1995 by Apple Computer, Inc., all rights reserved.
*/

#if !defined(__USERABORT_H)
#define __USERABORT_H 1

#ifndef __EVENTHANDLER_H
#include "EventHandler.h"
#endif

// in Newt/Main.c
extern NewtonErr InstallAbortHandler(ULong refCon, ULong eventID, NewtonErr result);
extern NewtonErr RemoveAbortHandler(ULong refCon);

#define kAbortEventId					'abrt'

//--------------------------------------------------------------------------------
//		CUserAbortEvent
//--------------------------------------------------------------------------------

class CUserAbortEvent : public CEvent
{
public:
					CUserAbortEvent();

	NewtonErr	fResult;		// result code for the abort (usually kError_Call_Aborted)
	ULong			fRefCon;		// client refCon (CUserAbortHandler*)
};


#endif	/* __USERABORT_H */

