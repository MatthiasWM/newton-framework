/*
	File:		RecUnit.h

	Contains:	The recognizers' units: TUnit, TSIUnit (a unit with sub-units
				and interpretations), TStrokeUnit; the strokes they are made
				of (reconstructed: not in the published headers; virtual
				functions in the order of the ROM's vtables, layouts with
				newton-re's findings; names ours where Apple's table has
				none).
*/

#ifndef __RECUNIT_H
#define __RECUNIT_H

#ifndef __RECOBJECT_H
#include "Recognition/RecObject.h"
#endif

class TDomain;
class TUnitList;
class TStroke;
class TAreaList;


/*------------------------------------------------------------------------------
	An interpretation of a unit.
------------------------------------------------------------------------------*/

struct UnitInterpretation
{
	long			label;			// +00
	long			score;			// +04
	long			angle;			// +08
	TRecObject *	param;			// +0C
};

long	InitInterpretation(UnitInterpretation * outInterp, ULong inElementSize, ULong inCount);


/*------------------------------------------------------------------------------
	T U n i t
------------------------------------------------------------------------------*/

class TUnit : public TRecObject
{
public:
	virtual void		Dispose(void);
	virtual void		Dump(TMsg * outMsg);
	virtual ULong		SizeInBytes(void);
	virtual void		IDispose(void);
	virtual void		Clone(void);
	virtual Boolean		Release(void);
	virtual long		SubCount(void);
	virtual long		InterpretationCount(void);
	virtual long		GetBestInterpretation(void);
	virtual void		DumpName(TMsg * outMsg);
	virtual void		ClaimUnit(TUnitList * inList);
	virtual long		MarkUnit(TUnitList * inList, ULong inFlags);
	virtual void		Invalidate(void);
	virtual void		DoneUsingUnit(void);
	virtual long		CountStrokes(void);
	virtual TStroke *	GetStroke(ULong index);
	virtual TUnitList *	GetAllStrokes(void);
	virtual Boolean		OwnsStroke(void);
	virtual ULong		ContextID(void);
	virtual void		SetContextID(ULong inID);
	virtual long		AddSub(TUnit * inSub);
	virtual TUnit *		GetSub(ULong index);
	virtual void		DeleteSub(ULong index);
	virtual long		EndSubs(void);
	virtual long		AddInterpretation(char * inInterp);
	virtual UnitInterpretation *	GetInterpretation(ULong index);
	virtual Boolean		CheckInterpretationIndex(ULong index);
	virtual long		DeleteInterpretation(ULong index);
	virtual long		InsertInterpretation(ULong index);
	virtual char *		LockInterpretations(void);
	virtual void		UnlockInterpretations(void);
	virtual void		CompactInterpretations(void);
	virtual long		InterpretationReuse(ULong inCount, ULong inParamSize, ULong inParamCount);
	virtual TDArray *	GetSubsCopy(void);
	virtual long		GetLabel(ULong index);
	virtual long		GetScore(ULong index);
	virtual long		GetAngle(ULong index);
	virtual TRecObject *	GetParam(ULong index);
	virtual void		SetLabel(ULong index, ULong inLabel);
	virtual void		SetScore(ULong index, ULong inScore);
	virtual void		SetAngle(ULong index, long inAngle);
	virtual void		EndUnit(void);

	TAreaList *			GetAreas(void);
	FRect *				GetBBox(FRect * outBox);

	ULong				fType;			// +08
	Rect				fBBox;			// +0C
	TDomain *			fDomain;		// +14
	TRecObject *		fAreas;			// +18
	ULong				fStartTime;		// +1C
	UShort				fDuration;		// +20
	UShort				fElapsed;		// +22
	UChar				fKind;			// +24
	SChar				fUsers;			// +25
	SChar				fPriority;		// +26
	UChar				fDelay;			// +27
	UShort				fSubRange;		// +28
	UShort				fMinStroke;		// +2A
	UShort				fMaxStroke;		// +2C
};


class TUnitList : public TDArray
{ };


/*------------------------------------------------------------------------------
	T S I U n i t
------------------------------------------------------------------------------*/

class TSIUnit : public TUnit
{
public:
						TSIUnit();

	long				ISIUnit(TDomain * inDomain, ULong inType, ULong inKind, TArray * inAreas, ULong inInterpSize);

	virtual void		Dump(TMsg * outMsg);
	virtual ULong		SizeInBytes(void);
	virtual void		IDispose(void);
	virtual void		DoneUsingUnit(void);
	virtual void		EndUnit(void);

	UChar				fSubKind;		// +30
	UChar				fHasInterps;	// +31
	TRecObject *		fSubs;			// +34
	TDArray *			fInterps;		// +38
};


/*------------------------------------------------------------------------------
	S t r o k e s
------------------------------------------------------------------------------*/

/* A point: x and y packed with the pressure and flags */
struct SamplePt
{
	UShort		fX;
	UShort		fY;
};

#define kCornerFlag		2

void		GetPoint(SamplePt * inPt, FPoint * outPt);
ULong		TestFlag(SamplePt * inPt, ULong inFlag);
void		SetFlag(SamplePt * inPt, ULong inFlag);
void		UnsetFlag(SamplePt * inPt, ULong inFlag);

class TStroke : public TDArray
{
public:
	SamplePt *			GetPoint(long index);
};


class TStrokeUnit : public TSIUnit
{
public:
	ULong				fContextID;		// +3C
	TStroke *			fStroke;		// +40
};

#endif	/* __RECUNIT_H */
