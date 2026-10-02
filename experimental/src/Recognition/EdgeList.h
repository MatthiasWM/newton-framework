/*
	File:		EdgeList.h

	Contains:	The gesture domain: TEdgeListDomain groups each stroke on its
				own, finds its corners and classifies its shape (a line, a
				caret, a scrub); TEdgeListUnit is the unit it makes
				(reconstructed: not in the published headers).
*/

#ifndef __EDGELIST_H
#define __EDGELIST_H

#ifndef __RECDOMAIN_H
#include "Recognition/RecDomain.h"
#endif

#ifndef __RECUNIT_H
#include "Recognition/RecUnit.h"
#endif

#define kEdgeListDomainType		'SCRB'
#define kStrokePieceType		'STRK'

class TEdgeListUnit : public TSIUnit
{
public:
	static TEdgeListUnit *	Make(TDomain * inDomain, ULong inKind, TArray * inAreas);
	long				IEdgeListUnit(TDomain * inDomain, ULong inKind, TArray * inAreas);

	virtual void		Dump(TMsg * outMsg);
	virtual ULong		SizeInBytes(void);
	virtual void		IDispose(void);
	virtual long		InterpretationCount(void);
	virtual void		DoneUsingUnit(void);
	virtual long		AddInterpretation(char * inInterp);
	virtual UnitInterpretation *	GetInterpretation(ULong index);
	virtual void		EndUnit(void);

	void				SetInterpretation(TDArray * inCorners);
	TDArray *			GetCorners(void);

	TDArray *			fCorners;		// +3C the stroke's corners, FPoints
	long				fInterpCount;	// +40
	UnitInterpretation	fInterp;		// +44
};


class TEdgeListDomain : public TDomain
{
public:
	static TEdgeListDomain *	Make(TController * inController);
	void				IEdgeListDomain(TController * inController);

	virtual void		Dispose(void);
	virtual void		Classify(TUnit * inUnit);
	virtual long		Group(TUnit * inPiece, dInfoRec * ioInfo);

	void				FindCorners(TUnit * inUnit);
};

void		Collapse2(TDArray * ioCorners);
Boolean		TestLine(TDArray * inCorners, UnitInterpretation * outInterp);
Boolean		TestCarets(TDArray * inCorners, UnitInterpretation * outInterp);
Boolean		TestScrub(TDArray * inCorners, FRect * inBBox, UnitInterpretation * outInterp);

/* geometry */
ULong		CheapDistPoint(FPoint * inA, FPoint * inB);	// unsigned: Collapse2 compares with BCS
long		PtsToAngleR(FPoint * inA, FPoint * inB);
void		NORM(long * ioAngle);
Boolean		OnlyStrokeWritten(TStrokeUnit * inUnit);

extern "C" Fixed	FixedLength(Fixed inX, Fixed inY);

#endif	/* __EDGELIST_H */
