/*
	File:		EdgeList.cc

	Contains:	The gesture domain: a stroke on its own, its corners found
				and its shape classified (TEdgeListDomain), and the unit
				made for it (TEdgeListUnit).

	ROM:		0x20E3F8 (a static function, the corner search) .. 0x20F044
				(after TEdgeListUnit::GetInterpretation), MP2x00 US 2.1
				(717006). Not in place yet (#if 0, see each): the corner
				search, FindCorners (its caller), Collapse2.
*/

#include "Recognition/EdgeList.h"
#include "Recognition/Controller.h"
#include "Recognition/RecGlue.h"
#include "Recognition/Msg.h"
#include "Toolbox/FixedMath.h"
#include "NewtonWidgets.h"
#include "CLibrary/stdio.h"


/*------------------------------------------------------------------------------
	The corners of a stroke between two points: the points farthest from
	the line between them, on either side, are corners if they lie farther
	than the tolerance (recursively); a stretch without one has corners at
	its ends. Corners are flagged in the stroke; ioCount counts them.
------------------------------------------------------------------------------*/

#if 0
/* Not yet: identical but for three words (0x20E484: for a.y - b.y the ROM
   loads a.y first, here b.y). Found on the way: the two end points are not
   variables but common subexpressions (&inPts[inFirst], &inPts[inLast]; the
   compiler keeps them on the stack), the flips are words, the ranges are
   max - min written out each time, and a pointer to a (used for c1 and c2)
   puts the loads of the other subtractions in the ROM's order. It has no
   name in Apple's table, so FindCorners, which calls it, waits with it. */
static void
FindCornersBetween(SamplePt * inPts, long inFirst, long inLast, Fixed inTolerance, long * ioCount)
{
	long		split = -1;
	FPoint		pt;

	if (inLast - inFirst > 1)
	{
		FPoint		a, b;
		FPoint *	pa = &a;
		Fixed		c1, c2, length;
		long		min1Ref, min2Ref;
		long		flip1, flip2;
		long		idx3, idx2, idx1, idx0;
		Fixed		nx, ny;
		Fixed		max1, min1, max2, min2;
		long		i;

		GetPoint(&inPts[inFirst], &a);
		GetPoint(&inPts[inLast], &b);
		length = FixedLength(b.x - a.x, b.y - a.y);
		if (length < kFix1)
			length = kFix1;
		nx = FixedDivide(a.y - b.y, length / 12);
		ny = FixedDivide(b.x - a.x, length / 12);
		if (Abs(nx) < kFix1 && Abs(ny) < kFix1)
			nx = kFix1;
		length = FixedLength(nx, ny);
		c1 = FixedMultiply(nx, pa->x) + FixedMultiply(ny, pa->y);
		c2 = FixedMultiply(ny, pa->x) - FixedMultiply(nx, pa->y);
		min1Ref = min1 = max1 = 0;
		min2Ref = min2 = max2 = 0;
		flip1 = flip2 = 0;
		idx3 = idx2 = idx1 = idx0 = inFirst;
		for (i = inFirst + 1; i <= inLast; i++)
		{
			Fixed d;
			GetPoint(inPts + i, &pt);
			d = FixedMultiply(nx, pt.x) + FixedMultiply(ny, pt.y) - c1;
			if (flip1)
				d = -d;
			if (d > max1)
			{
				idx3 = i;
				min1 += d - max1;
				max1 = d;
			}
			else if (d < min1)
			{
				idx1 = idx3;
				if (d < min1Ref)
				{
					flip1 = !flip1;
					min1 = min1Ref - max1 - d;
					min1Ref = -max1;
					max1 = -d;
					idx3 = i;
				}
				else
					min1 = d;
			}
			d = FixedMultiply(ny, pt.x) - FixedMultiply(nx, pt.y) - c2;
			if (flip2)
				d = -d;
			if (d > max2)
			{
				idx2 = i;
				min2 += d - max2;
				max2 = d;
			}
			else if (d < min2)
			{
				idx0 = idx2;
				if (d < min2Ref)
				{
					flip2 = !flip2;
					min2 = min2Ref - max2 - d;
					min2Ref = -max2;
					max2 = -d;
					idx2 = i;
				}
				else
					min2 = d;
			}
		}
		if (FixedMultiply(inTolerance, length) >= max1 - min1)
			idx1 = inFirst;
		if (FixedMultiply(inTolerance, length) >= max2 - min2)
			idx0 = inFirst;
		if (idx1 > inFirst && (idx0 == inFirst || max1 - min1 > max2 - min2))
			split = idx1;
		else if (idx0 > inFirst)
			split = idx0;
	}

	if (split != -1)
	{
		FindCornersBetween(inPts, inFirst, split, inTolerance, ioCount);
		FindCornersBetween(inPts, split, inLast, inTolerance, ioCount);
	}
	else
	{
		if (!TestFlag(&inPts[inFirst], kCornerFlag))
		{
			SetFlag(&inPts[inFirst], kCornerFlag);
			(*ioCount)++;
		}
		if (!TestFlag(&inPts[inLast], kCornerFlag))
		{
			SetFlag(&inPts[inLast], kCornerFlag);
			(*ioCount)++;
		}
	}
}
#endif


/*------------------------------------------------------------------------------
	T E d g e L i s t D o m a i n
------------------------------------------------------------------------------*/

TEdgeListDomain *
TEdgeListDomain::Make(TController * inController)
{
	TEdgeListDomain * domain = new TEdgeListDomain;
	domain->IEdgeListDomain(inController);
	return domain;
}


void
TEdgeListDomain::IEdgeListDomain(TController * inController)
{
	ULong type = kEdgeListDomainType;
	IDomain(inController, type, "EdgeList");
	AddPieceType(kStrokePieceType);
	inController->RegisterDomain(this);
}


void
TEdgeListDomain::Dispose(void)
{
	TDomain::Dispose();
}


void
TEdgeListDomain::Classify(TUnit * inUnit)
{
	TEdgeListUnit * unit = (TEdgeListUnit *) inUnit;
	if (OnlyStrokeWritten((TStrokeUnit *) unit->GetSub(0)))
	{
		FindCorners(unit);
		UnitInterpretation	interp;
		FRect				box;
		TDArray * corners = unit->GetCorners();
		if (corners && corners->fSize < 50)
		{
			InitInterpretation(&interp, 0, 0);
			unit->GetBBox(&box);
			Collapse2(corners);
			if (TestLine(corners, &interp) || TestCarets(corners, &interp) || TestScrub(corners, &box, &interp))
				goto good;
		}
		unit->SetFlags(0x00400000);
		unit->DoneUsingUnit();
		goto classified;
good:
		unit->AddInterpretation((char *) &interp);
	}
	else
	{
		unit->SetFlags(0x00400000);
		unit->DoneUsingUnit();
	}
classified:
	unit->EndUnit();
	fController->NewClassification(inUnit);
}


long
TEdgeListDomain::Group(TUnit * inPiece, dInfoRec * ioInfo)
{
	TAreaList * areas = inPiece->GetAreas();
	TEdgeListUnit * unit = TEdgeListUnit::Make(this, inPiece->fKind + 1, (TArray *) areas);
	if (areas)
		((TRecObject *) areas)->Dispose();
	if (unit == NULL)
		return 0;
	unit->AddSub(inPiece);
	unit->EndSubs();
	fController->NewGroup(unit);
	return 1;
}


#if 0
/* Identical, but it calls the static corner search above, which is not in
   place yet: until it is, both stay generated assembler. */
void
TEdgeListDomain::FindCorners(TUnit * inUnit)
{
	TEdgeListUnit * unit = (TEdgeListUnit *) inUnit;
	TStroke *	stroke = ((TStrokeUnit *) inUnit->GetSub(0))->fStroke;
	ULong		count = stroke->fSize;
	SamplePt *	p = stroke->GetPoint(0);
	ULong		i;
	long		corners;

	for (i = 0; i < count; i++, p++)
		UnsetFlag(p, kCornerFlag);
	SamplePt * pts = stroke->GetPoint(0);
	corners = 0;
	FindCornersBetween(pts, 0, count - 1, 4 * kFix1, &corners);
	TDArray * cornerList = TDArray::Make(sizeof(FPoint), corners);
	if (cornerList)
	{
		SamplePt * q = stroke->GetPoint(0);
		i = 0;
		corners = 0;
		for ( ; i < count; i++, q++)
		{
			if (TestFlag(q, kCornerFlag))
			{
				FPoint pt;
				GetPoint(q, &pt);
				cornerList->SetEntry(corners, (char *) &pt);
				corners++;
			}
		}
		unit->SetInterpretation(cornerList);
		cornerList->Dispose();
	}
}
#endif


/*------------------------------------------------------------------------------
	Corners too close together, or on a straight run, dropped.
------------------------------------------------------------------------------*/

#if 0
/* Not yet: identical but for 19 words, registers (the ROM keeps pts in r8
   and del in r7, here the other way round) and, in the second loop, i - 1
   computed again after Delete where here it is kept. Found on the way:
   CheapDistPoint returns an unsigned value (BCS) and gets (prev, p); the
   clamp is Apple's Max (NewtonWidgets.h), an inline call: its 0 in a
   register. */
void
Collapse2(TDArray * ioCorners)
{
	long		turn1, turn2;
	ULong		count = ioCorners->fSize;
	FPoint *	pts = (FPoint *) ioCorners->GetEntry(0);
	ULong		i;

	if (count <= 2)
		return;
	for (i = 1; i < count; i++)
	{
		if (CheapDistPoint(&pts[i - 1], &pts[i]) < 7 * kFix1)
		{
			ULong del = i;
			count--;
			if (count > i)
			{
				if (i > 1)
				{
					long a1 = PtsToAngleR(&pts[i - 1], &pts[i - 2]);
					long a2 = PtsToAngleR(&pts[i], &pts[i - 1]);
					long a3 = PtsToAngleR(&pts[i + 1], &pts[i]);
					turn1 = a2 - a1;
					turn2 = a3 - a2;
					NORM(&turn1);
					NORM(&turn2);
					if ((turn1 >= 0 ? turn1 : -turn1) >= (turn2 >= 0 ? turn2 : -turn2))
						goto remove;
				}
				del--;
			}
remove:
			ioCorners->Delete(del);
			i = Max(del - 1, 0);
		}
	}

	long prevAngle = PtsToAngleR(&pts[1], &pts[0]);
	for (i = 2; i < count; i++)
	{
		long angle = PtsToAngleR(&pts[i], &pts[i - 1]);
		turn1 = angle - prevAngle;
		NORM(&turn1);
		if ((turn1 >= 0 ? turn1 : -turn1) < 0xA0D9)
		{
			ioCorners->Delete(i - 1);
			count--;
			i--;
			angle = PtsToAngleR(&pts[i], &pts[i - 1]);
		}
		prevAngle = angle;
	}
}
#endif


/*------------------------------------------------------------------------------
	T E d g e L i s t U n i t
------------------------------------------------------------------------------*/

TEdgeListUnit *
TEdgeListUnit::Make(TDomain * inDomain, ULong inKind, TArray * inAreas)
{
	TEdgeListUnit * unit = new TEdgeListUnit;
	if (unit)
	{
		if (unit->IEdgeListUnit(inDomain, inKind, inAreas))
		{
			unit->Dispose();
			unit = NULL;
		}
	}
	return unit;
}


long
TEdgeListUnit::IEdgeListUnit(TDomain * inDomain, ULong inKind, TArray * inAreas)
{
	ULong type = kEdgeListDomainType;
	InitInterpretation(&fInterp, 0, 0);
	fInterpCount = 1;
	long err = ISIUnit(inDomain, type, inKind, inAreas, sizeof(UnitInterpretation));
	fCorners = NULL;
	return err;
}


void
TEdgeListUnit::SetInterpretation(TDArray * inCorners)
{
	inCorners->Clone();
	fCorners = inCorners;
	EndUnit();
}


void
TEdgeListUnit::EndUnit(void)
{
	TDArray * corners = GetCorners();
	if (corners)
		corners->Compact();
	TSIUnit::EndUnit();
}


void
TEdgeListUnit::Dump(TMsg * outMsg)
{
	char buf[256];
	TDArray * corners = GetCorners();
	outMsg->MsgStr("EdgeList: ");
	TSIUnit::Dump(outMsg);
	if (corners)
	{
		sprintf(buf, " %ld corners", corners->fSize);
		outMsg->MsgStr(buf);
	}
	outMsg->MsgLF();
}


void
TEdgeListUnit::IDispose(void)
{
	TDArray * corners = GetCorners();
	if (corners)
		corners->Dispose();
	TSIUnit::IDispose();
}


void
TEdgeListUnit::DoneUsingUnit(void)
{
	TDArray * corners = GetCorners();
	if (corners)
		corners->Dispose();
	fCorners = NULL;
	TSIUnit::DoneUsingUnit();
}


ULong
TEdgeListUnit::SizeInBytes(void)
{
	TDArray * corners = GetCorners();
	ULong size = TSIUnit::SizeInBytes();
	if (corners)
		size += corners->SizeInBytes();
	return size;
}


TDArray *
TEdgeListUnit::GetCorners(void)
{
	return fCorners;
}


long
TEdgeListUnit::InterpretationCount(void)
{
	return fInterpCount;
}


long
TEdgeListUnit::AddInterpretation(char * inInterp)
{
	MoveBlock(inInterp, (char *) &fInterp, sizeof(UnitInterpretation));
	fInterpCount = 1;
	return 0;
}


UnitInterpretation *
TEdgeListUnit::GetInterpretation(ULong index)
{
	return &fInterp;
}
