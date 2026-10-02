/*
	File:		Controller.h

	Contains:	TController, the recognition controller: what the domains
				hand their pieces and units to (reconstructed: not in the
				published headers; only what the sources so far use).
*/

#ifndef __CONTROLLER_H
#define __CONTROLLER_H

#ifndef __RECOBJECT_H
#include "Recognition/RecObject.h"
#endif

class TDomain;
class TUnit;

class TController : public TRecObject
{
public:
	void				RegisterDomain(TDomain * inDomain);
	ULong				NewClassification(TUnit * inPiece);
	void				NewGroup(TUnit * inUnit);
};

#endif	/* __CONTROLLER_H */
