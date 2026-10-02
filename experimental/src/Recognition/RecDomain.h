/*
	File:		RecDomain.h

	Contains:	TDomain, a recognition domain: what classifies and groups
				units of some types (reconstructed: not in the published
				headers; the port's CRecDomain, Apple's names). Virtual
				functions in the order of the ROM's vtable.
*/

#ifndef __RECDOMAIN_H
#define __RECDOMAIN_H

#ifndef __RECOBJECT_H
#include "Recognition/RecObject.h"
#endif

class TController;
class TUnit;
class TRecArea;
struct dInfoRec;

class TDomain : public TRecObject
{
public:
						TDomain();
						~TDomain();

	static TDomain *	Make(TController * inController, ULong inType, char * inName);
	static Boolean		VUnitInClass(ULong inUnitType, ULong inClass);
	void				IDomain(TController * inController, ULong inType, char * inName);
	void				AddPieceType(ULong inType);

	virtual void		Dispose(void);
	virtual void		Dump(TMsg * outMsg);
	virtual ULong		SizeInBytes(void);
	virtual void		Classify(TUnit * inUnit);
	virtual void		Reclassify(TUnit * inUnit);
	virtual long		Group(TUnit * inUnit, dInfoRec * ioInfo);
	virtual long		PreGroup(TUnit * inUnit);
	virtual void		DumpName(TMsg * outMsg);
	virtual long		PruneDictionary(TUnit * inUnit);
	virtual long		PruneConstraints(TUnit * inUnit);
	virtual long		DomainParameter(ULong inSelector, ULong ioParam, ULong inArg3);
	virtual Boolean		SetParameters(char ** inParams);
	virtual void		InvalParameters(void);
	virtual void		ConfigureSubDomain(TRecArea * inArea);
	virtual long		CompleteUnit(void);

	TController *		fController;	// +08
	TTypeList *			fPieceTypes;	// +0C
	ULong				fDomainType;	// +10
	char *				fName;			// +14
	ULong				f18;			// +18
	ULong				f1C;			// +1C
	char **				fParams;		// +20
};

#endif	/* __RECDOMAIN_H */
