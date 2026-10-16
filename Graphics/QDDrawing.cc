/*
	File:		QDDrawing.cc

	Contains:	QD implementation for Newton, on the host's graphics
					(QDHost.h: FLTK, or nothing).

	Written by:	Newton Research Group.
*/

#include "QDHost.h"
#include "Objects.h"
#include "QDPatterns.h"
#include "QDDrawing.h"
#include "Geometry.h"

#include "ViewFlags.h"
#include "RSSymbols.h"

#include "Screen.h"


/*------------------------------------------------------------------------------
	D a t a
	Definition of the display's pixels (was in Screen.cc)
------------------------------------------------------------------------------*/

const ScreenParams gScreenConstants =		// was qdConstants 00380BCC
{	{ 0, 5, 4, 0, 3, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },	// xShift[bitsPerPixel]
	{ 0, 3, 2, 0, 1 },	// depthShift
	{ 0x00000000, 0x00000001, 0x00000003, 0x0000000F, 0x000000FF, 0x0000FFFF, 0xFFFFFFFF },	// pixelMask
	{ 0, 0x1F, 0x0F, 0, 0x07, 0, 0, 0, 0x03, 0, 0, 0, 0, 0, 0, 0, 0x01, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },	// xMask
	{ 0, 1, 0, 2, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5, 0 } };	// maskIndex[bitsPerPixel]


#pragma mark -
/*------------------------------------------------------------------------------
	Justify a rect within a frame.
	Args:		ioRect
				inFrame
				inJustify
	Return:	justified Rect
------------------------------------------------------------------------------*/

Rect *
Justify(Rect * ioRect, const Rect * inFrame, ULong inJustify)
{
	Rect  frame = *inFrame;
	Point rectOrigin = *(Point*)inFrame;
	Point rectSize;
	long  offset;

	rectSize.h = RectGetWidth(*ioRect);
	rectSize.v = RectGetHeight(*ioRect);

	if (RectGetWidth(frame) == 0
	&&  RectGetHeight(frame) == 0)
	{
		frame.right = frame.left + RectGetWidth(*ioRect);
		frame.bottom = frame.top + RectGetHeight(*ioRect);
	}

	switch (inJustify & vjVMask)
	{
	case vjTopV:
	// no need to do anything
		break;
	case vjCenterV:
		offset = (RectGetHeight(frame) - rectSize.v) / 2;
		if (offset > 0)
			rectOrigin.v += offset;
		break;
	case vjBottomV:
		offset = RectGetHeight(frame) - rectSize.v;
		rectOrigin.v += offset;
		break;
	case vjFullV:
		rectSize.v = RectGetHeight(frame);
		break;
	}

	switch (inJustify & vjHMask)
	{
	case vjLeftH:
	// no need to do anything
		break;
	case vjRightH:
		offset = RectGetWidth(frame) - rectSize.h;
		rectOrigin.h += offset;
		break;
	case vjCenterH:
		offset = (RectGetWidth(frame) - rectSize.h) / 2;
		if (offset > 0)
			rectOrigin.h += offset;
		break;
	case vjFullH:
		rectSize.h = RectGetWidth(frame);
		break;
	}

	SetRect(ioRect, rectOrigin.h, rectOrigin.v, rectOrigin.h + rectSize.h, rectOrigin.v + rectSize.v);
	return ioRect;
}


#pragma mark -

/*------------------------------------------------------------------------------
	D r a w i n g
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
	L i n e
------------------------------------------------------------------------------*/

void
LineNormal(void)
{
	SetLineWidth(1);
	QDHostSetStrokeColor(kQDBlack);
}


void
SetLineWidth(short inWidth)
{
	QDHostSetLineWidth(inWidth);
}


short
LineWidth(void)
{
	return (short) QDHostLineWidth();
}


void
StrokeLine(Point inFrom, Point inTo)
{
	QDHostStrokeLine(inFrom.h, inFrom.v, inTo.h, inTo.v);
}


/*------------------------------------------------------------------------------
	R e c t
------------------------------------------------------------------------------*/

void
StrokeRect(const Rect * inBox)
{
	StrokeRect(*inBox);
}

void
StrokeRect(Rect inBox)
{
	QDHostStrokeRect(inBox.left, inBox.top, inBox.right, inBox.bottom);
}


void
FillRect(const Rect * inBox)
{
	FillRect(*inBox);
}

void
FillRect(Rect inBox)
{
	QDHostFillRect(inBox.left, inBox.top, inBox.right, inBox.bottom);
}


/*------------------------------------------------------------------------------
	R o u n d R e c t
------------------------------------------------------------------------------*/
void
StrokeRoundRect(Rect inBox, short inOvalWd, short inOvalHt)
{
	QDHostStrokeRoundRect(inBox.left, inBox.top, inBox.right, inBox.bottom, inOvalWd, inOvalHt);
}


void
FillRoundRect(Rect inBox, short inOvalWd, short inOvalHt)
{
	QDHostFillRoundRect(inBox.left, inBox.top, inBox.right, inBox.bottom, inOvalWd, inOvalHt);
}


/*------------------------------------------------------------------------------
	O v a l
------------------------------------------------------------------------------*/

void
StrokeOval(const Rect * inBox)
{
	QDHostStrokeOval(inBox->left, inBox->top, inBox->right, inBox->bottom);
}


void
FillOval(const Rect * inBox)
{
	QDHostFillOval(inBox->left, inBox->top, inBox->right, inBox->bottom);
}


/*------------------------------------------------------------------------------
	P o l y g o n
------------------------------------------------------------------------------*/

// hack for NCX
// inData: a 16-bit 0, the number of points, then the points (v, h), all
// big-endian; drawn at inX, inY with a pen 2 wide
void
DrawPolygon(char * inData, short inX, short inY)
{
	short sh, * p = (short *)inData;
	sh = *p++;	// unused
	sh = *p++;
	ArrayIndex count = CANONICAL_SHORT(sh);

	if (count < 2)
		return;

	float * xy = new float[2 * count];
	for (ArrayIndex i = 0; i < count; ++i)
	{
		sh = *p++;
		xy[2*i+1] = inY + CANONICAL_SHORT(sh);
		sh = *p++;
		xy[2*i] = inX + CANONICAL_SHORT(sh);
	}
	QDHostSaveState();
	QDHostSetLineWidth(2);
	QDHostStrokePolyline(xy, count, false);
	QDHostRestoreState();
	delete[] xy;
}


/*------------------------------------------------------------------------------
	A polygon's points as x, y floats (for QDHost); the caller deletes[]
	them.
------------------------------------------------------------------------------*/

static float *
PolyPoints(PolygonShape * inShape, int * outCount)
{
	Point * p = inShape->points;
	int count = (inShape->size - 12) / sizeof(Point);
	if (count < 0)
		count = 0;
	float * xy = new float[2 * count + 2];
	for (int i = 0; i < count; ++i, ++p)
	{
		xy[2*i] = p->h;
		xy[2*i+1] = p->v;
	}
	*outCount = count;
	return xy;
}


void
KillPoly(PolygonShape * inShape)
{
	free(inShape);
}


void
OffsetPoly(PolygonShape * inShape, short dx, short dy)
{
	if (dx != 0 && dy != 0)
	{
		Point * p = inShape->points;
		for (ArrayIndex i = 0, count = (inShape->size - 12) / sizeof(Point); i < count; ++i, ++p)
		{
			p->h += dx;
			p->v += dy;
		}
	}
}


void
StrokePoly(PolygonShape * inShape)
{
	int count;
	float * xy = PolyPoints(inShape, &count);
	QDHostStrokePolyline(xy, count, false);
	delete[] xy;
}


void
FillPoly(PolygonShape * inShape)
{
	int count;
	float * xy = PolyPoints(inShape, &count);
	QDHostFillPolygon(xy, count);
	delete[] xy;
}


/*------------------------------------------------------------------------------
	R e g i o n
------------------------------------------------------------------------------*/

void
OffsetRgn(RegionShape * inShape, short dx, short dy)
{}

void
StrokeRgn(RegionShape * inShape)
{}

void
FillRgn(RegionShape * inShape)
{}


/*------------------------------------------------------------------------------
	A r c
------------------------------------------------------------------------------*/

void
StrokeArc(WedgeShape * inShape, short inArc1, short inArc2)
{
	const Rect & r = inShape->rect;
	QDHostStrokeArc(r.left, r.top, r.right, r.bottom, inArc1, inArc2);
}

void
FillArc(WedgeShape * inShape, short inArc1, short inArc2)
{
	const Rect & r = inShape->rect;
	QDHostFillArc(r.left, r.top, r.right, r.bottom, inArc1, inArc2);
}

#pragma mark -
/*------------------------------------------------------------------------------
	Return a pointer to the bitmap in a PixelMap.
	Args:		pixmap		the PixelMap
	Return:  pointer
------------------------------------------------------------------------------*/

Ptr
PixelMapBits(const PixelMap * inPixmap)
{
	ULong storage = inPixmap->pixMapFlags & kPixMapStorage;
	if (storage == kPixMapOffset) {
		return (Ptr)inPixmap + inPixmap->baseAddr;
	} else if (storage == kPixMapHandle) {
printf("PixelMapBits() using baseAddr as Handle!\n");
		return *(Handle)(uintptr_t)inPixmap->baseAddr;
	} else if (storage == kPixMapPtr) {
printf("PixelMapBits() using baseAddr as Ptr!\n");
		return (Ptr)(uintptr_t)inPixmap->baseAddr;
	}
	return NULL;
}


Ptr
PixelMapBits(const NativePixelMap * inPixmap)
{
	ULong storage = inPixmap->pixMapFlags & kPixMapStorage;
	if (storage == kPixMapPtr) {
		return (Ptr)inPixmap->baseAddr;
	// shouldn't really be anything else
	} else if (storage == kPixMapOffset) {
		return (Ptr)inPixmap + (uintptr_t)inPixmap->baseAddr;
	}
	return NULL;
}


#pragma mark -
/* -----------------------------------------------------------------------------
	PixelMap hit testing.
----------------------------------------------------------------------------- */

bool
PtInPixelMap(const NativePixelMap * inPixmap, int inX, int inY)
{
	if (inX >= 0 && inX < RectGetWidth(inPixmap->bounds)
	&&  inY >= 0 && inY < RectGetHeight(inPixmap->bounds))
	{
		int bitsPerPixel = PixelDepth(inPixmap);
		// point to row containing target pixel
		ULong * pixPtr = (ULong *)(PixelMapBits(inPixmap) + inY * inPixmap->rowBytes);
		// read word containing target pixel
		ULong pixWord = pixPtr[inX >> gScreenConstants.xShift[bitsPerPixel]];
		// get index of pixel in that word
		int pixIndex = inX & gScreenConstants.xMask[bitsPerPixel];
		int bitIndex = bitsPerPixel * pixIndex;
		// shift pixel down
		ULong pixBits = pixWord >> (32 - bitsPerPixel - bitIndex);
		int maskIndex = gScreenConstants.maskIndex[bitsPerPixel];
		return (pixBits & gScreenConstants.pixelMask[maskIndex]) != 0;
	}
	return false;
}


int
PtInCPixelMap(const NativePixelMap * inPixmap, int inX, int inY)
{
	if (inX >= 0 && inX < RectGetWidth(inPixmap->bounds)
	&&  inY >= 0 && inY < RectGetHeight(inPixmap->bounds))
	{
		int bitsPerPixel = PixelDepth(inPixmap);
		// point to row containing target pixel
		ULong * pixPtr = (ULong *)(PixelMapBits(inPixmap) + inY * inPixmap->rowBytes);
		// read word containing target pixel
		ULong pixWord = pixPtr[inX >> gScreenConstants.xShift[bitsPerPixel]];
		// get index of pixel in that word
		int pixIndex = inX & gScreenConstants.xMask[bitsPerPixel];
		int bitIndex = bitsPerPixel * pixIndex;
		// shift pixel down
		ULong pixBits = pixWord >> (32 - bitsPerPixel - bitIndex);
		int maskIndex = gScreenConstants.maskIndex[bitsPerPixel];
		return (pixBits & gScreenConstants.pixelMask[maskIndex]);
	}
	return -1;
}


int
PtInMask(const NativePixelMap * inPixmap, int inX, int inY)
{
	if (inX >= 0 && inX < RectGetWidth(inPixmap->bounds)
	&&  inY >= 0 && inY < RectGetHeight(inPixmap->bounds))
	{
		int bitsPerPixel = PixelDepth(inPixmap);
		// point to row containing target pixel
		ULong * pixPtr = (ULong *)(PixelMapBits(inPixmap) + inY * inPixmap->rowBytes);
		// read word containing target pixel
		ULong pixWord = pixPtr[inX >> gScreenConstants.xShift[bitsPerPixel]];
		// get index of pixel in that word
		int pixIndex = inX & gScreenConstants.xMask[bitsPerPixel];
		int bitIndex = bitsPerPixel * pixIndex;
		// shift pixel down
		ULong pixBits = pixWord >> (32 - bitsPerPixel - bitIndex);
		int maskIndex = gScreenConstants.maskIndex[bitsPerPixel];
		if ((pixBits & gScreenConstants.pixelMask[maskIndex]) != 0)
			return 0;
	}
	return -1;
}


#pragma mark -
/*------------------------------------------------------------------------------
	C o l o u r s
------------------------------------------------------------------------------*/

ULong
PackRGBvalues(ULong inR, ULong inG, ULong inB)
{
	return 0x10000000 + ((inR & 0xFF00) << 8) + (inG & 0xFF00) + ((inB & 0xFF00) >> 8);
}


void
UnpackRGBvalues(ULong inRGB, ULong * outR, ULong * outG, ULong * outB)
{
	*outR = (inRGB >> 16) & 0xFF;
	*outG = (inRGB >> 8) & 0xFF;
	*outB = inRGB & 0xFF;
	*outR += (*outR << 8);
	*outG += (*outG << 8);
	*outB += (*outB << 8);
}


/*------------------------------------------------------------------------------
	PackRGB(red, green, blue), GetRed(rgb), GetGreen(rgb), GetBlue(rgb):
	the NewtonScript functions (ROM 0x000E387C ff.), 16 bits per component.
	The ROM throws exGrafException -8809 for a component over 65535; newtc
	has no exGrafException: a frames error with that code and the value.
------------------------------------------------------------------------------*/

extern "C" Ref
FPackRGB(RefArg inRcvr, RefArg inRed, RefArg inGreen, RefArg inBlue)
{
	long r = RINT(inRed), g = RINT(inGreen), b = RINT(inBlue);
	if ((ULong)r > 0xFFFF || (ULong)g > 0xFFFF || (ULong)b > 0xFFFF)
		ThrowExFramesWithBadValue(-8809, (ULong)r > 0xFFFF ? inRed : ((ULong)g > 0xFFFF ? inGreen : inBlue));
	return MAKEINT(PackRGBvalues(r, g, b));
}

extern "C" Ref
FGetRed(RefArg inRcvr, RefArg inRGB)
{
	ULong r, g, b;
	UnpackRGBvalues(RINT(inRGB), &r, &g, &b);
	return MAKEINT(r);
}

extern "C" Ref
FGetGreen(RefArg inRcvr, RefArg inRGB)
{
	ULong r, g, b;
	UnpackRGBvalues(RINT(inRGB), &r, &g, &b);
	return MAKEINT(g);
}

extern "C" Ref
FGetBlue(RefArg inRcvr, RefArg inRGB)
{
	ULong r, g, b;
	UnpackRGBvalues(RINT(inRGB), &r, &g, &b);
	return MAKEINT(b);
}

/* This is the Grayscale Conversion according to the Luminance Model
	used by NTSC and JPEG for nonlinear data in a gamma working space:
	Y = 0.299R + 0.587G + 0.114B
*/
ULong
RGBtoGray(ULong inR, ULong inG, ULong inB, int inNumOfBits, int inDepth)
{
	ULong  gray = 19589 * inR
					+ 38443 * inG
					+  7497 * inB;

	return ~gray >> (32 - inNumOfBits);
}

#pragma mark -

/*------------------------------------------------------------------------------
	P a t t e r n s
------------------------------------------------------------------------------*/

#if 0
void
MakeSimpleStyle(RefArg inFamily, long inSize, long inFace)
{
	RefStruct		sp;
	PatternHandle  pat = NULL; // sp1C
	;
	sp = inFontSpec;
	style->f00 = new RefStruct(sp00);
	sp.f04 = inArg1;
	sp.f08 = inArg2;
	sp.f0C = 0;
	sp.f10 = 0;
	sp.f14 = 0;

	if (pat)
		DisposePattern(pat);
}


typedef struct
{
	PixelMap pix;
	char		pat[8];
} SimplePattern;

PatternHandle
MakeSimplePattern(long inBits0, long inBits1, long inBits2, long inBits3, long inBits4, long inBits5, long inBits6, long inBits7)
{
	PatternHandle  pat = (PatternHandle) NewHandle(sizeof(SimplePattern));	// NOTE NewHandle deprecated
	if (pat)
	{
		PixelMap *  pixmap = *pat;
		pixmap->baseAddr = offsetof(SimplePattern, pat);	// actually an offset, obviously
		pixmap->rowBytes = 1;
		SetRect(&pixmap->bounds, 0, 0, 8, 8);
		pixmap->pixMapFlags = kPixMapOffset + kOneBitDepth;
		pixmap->deviceRes.h =
		pixmap->deviceRes.v = kDefaultDPI;
		pixmap->grayTable = NULL;
		((SimplePattern*)pixmap)->pat[0] = inBits0;
		((SimplePattern*)pixmap)->pat[1] = inBits1;
		((SimplePattern*)pixmap)->pat[2] = inBits2;
		((SimplePattern*)pixmap)->pat[3] = inBits3;
		((SimplePattern*)pixmap)->pat[4] = inBits4;
		((SimplePattern*)pixmap)->pat[5] = inBits5;
		((SimplePattern*)pixmap)->pat[6] = inBits6;
		((SimplePattern*)pixmap)->pat[7] = inBits7;
	}
	return pat;
}
#endif

/*------------------------------------------------------------------------------
	An 8 by 8 pattern (8 bytes, a row each, 1 black) as its gray: the share of
	its pixels that are black.
------------------------------------------------------------------------------*/

QDColor
MakeSimplePattern(const char * inBits)
{
	int black = 0;
	for (ArrayIndex i = 0; i < kPatternDataSize; ++i)
		for (UChar bits = inBits[i]; bits != 0; bits >>= 1)
			black += bits & 1;
	ULong gray = 255 - (black * 255 + 32) / 64;
	return (gray << 16) | (gray << 8) | gray;
}


/*------------------------------------------------------------------------------
	Set the pattern for drawing a view's frame.
	Used only by View.cc.
	Args:		inPatNo
	Return:	true	if a standard pattern was set
				false		if a custom pattern was specified
------------------------------------------------------------------------------*/

bool
SetPattern(int inPatNo)
{
	bool		isSet = true;
	QDColor	ptn = kQDBlack;

	switch (inPatNo)
	{
	case vfWhite:
		ptn = kQDWhite;
		break;
	case vfLtGray:
		ptn = kQDLightGray;
		break;
	case vfGray:
		ptn = kQDGray;
		break;
	case vfDkGray:
		ptn = kQDDarkGray;
		break;
	case vfBlack:
		ptn = kQDBlack;
		break;
	case 12:
		ptn = kQDBlack;
		break;
	case vfDragger:
	case vfMatte:
		ptn = kQDGray;
		break;
	case vfCustom:
		break;
	default:
		isSet = false;
	}
	if (isSet)
	{
		QDHostSetStrokeColor(ptn);
		QDHostSetFillColor(ptn);
	}

	return isSet;
}


/*------------------------------------------------------------------------------
	A standard pattern: whitePat ... blackPat (not vfWhite ... vfBlack, which
	are one more).
------------------------------------------------------------------------------*/

QDColor
GetStdPattern(int inPatNo)
{
	switch (inPatNo)
	{
	case whitePat:
		return kQDWhite;
	case ltGrayPat:
		return kQDLightGray;
	case grayPat:
		return kQDGray;
	case dkGrayPat:
		return kQDDarkGray;
	}
	return kQDBlack;
}


/*------------------------------------------------------------------------------
	A colour, 16 bits per component.
------------------------------------------------------------------------------*/

QDColor
GetStdGrayPattern(ULong inR, ULong inG, ULong inB)
{
	return ((inR & 0xFF00) << 8) | (inG & 0xFF00) | ((inB & 0xFF00) >> 8);
}


/*------------------------------------------------------------------------------
	A pattern (a fillPattern, penPattern, ... in a style) as a colour: a
	standard pattern's number (vfWhite ... vfBlack), an RGB value (PackRGB),
	an 8 by 8 pattern (a binary), a ditherPattern frame (its foreground).
	Args:		inPatNo
				ioHasPattern	set if a pattern was found
				ioPat				the colour
				inDefault		the result if no pattern was found
	Return:	true if there's something to draw with
------------------------------------------------------------------------------*/

bool
GetPattern(RefArg inPatNo, bool * ioHasPattern, QDColor * ioPat, bool inDefault)
{
	bool	gotPattern = inDefault;

	if (ISINT(inPatNo))
	{
		int patNo = RINT(inPatNo);
		bool isOpaque = (patNo > 0);
		if (isOpaque)
		{
			if ((patNo & 0x10000000) != 0)	// it's not actually an index but an RGB value
			{
				ULong	r;
				ULong	g;
				ULong	b;
				UnpackRGBvalues(patNo, &r, &g, &b);
				*ioPat = GetStdGrayPattern(r,g,b);
			}
			else
			{
				*ioPat = GetStdPattern((patNo & 0xFF) - 1);
			}
			*ioHasPattern = true;
			gotPattern = true;
		}
	}

	else if (IsFrame(inPatNo))
	{
		if (EQ(ClassOf(inPatNo), SYMA(ditherPattern)))
		{
			// its foreground (white if none), not dithered
			ULong	r = 0xFFFF, g = 0xFFFF, b = 0xFFFF;
			RefVar item(GetFrameSlot(inPatNo, SYMA(foreground)));
			if (ISINT(item))
				UnpackRGBvalues(RINT(item), &r, &g, &b);
			*ioPat = GetStdGrayPattern(r,g,b);
			*ioHasPattern = true;
			gotPattern = true;
		}
	}

	else if (IsBinary(inPatNo) && Length(inPatNo) >= kPatternDataSize)
	{
		CDataPtr	patData(inPatNo);
		*ioPat = MakeSimplePattern(patData);
		*ioHasPattern = true;
		gotPattern = true;
	}

	return gotPattern;
}
