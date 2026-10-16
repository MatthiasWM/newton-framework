/*
	File:		DrawImage.cc

	Contains:	Picture and bitmap creation and drawing functions.

	Written by:	Newton Research Group, 2007.
*/

#include "QDHost.h"
#include "Host/Pict.h"
#include "Unimplemented.h"
#include "Objects.h"
#include "QDPatterns.h"
#include "Geometry.h"
#include "DrawShape.h"
#include "ViewFlags.h"

#include "Iterators.h"
#include "RSSymbols.h"
#include "OSErrors.h"

#include <vector>
/*------------------------------------------------------------------------------
	F u n c t i o n   P r o t o t y p e s
------------------------------------------------------------------------------*/

extern "C" {
Ref		FPictureBounds(RefArg inRcvr, RefArg inPicture);
Ref		FPictToShape(RefArg inRcvr, RefArg inPicture);
Ref		FCopyBits(RefArg inRcvr, RefArg inImage, RefArg inX, RefArg inY, RefArg inTransferMode);
Ref		FGrayShrink(RefArg inRcvr, RefArg inArg1, RefArg inArg2);
Ref		FDrawIntoBitmap(RefArg inRcvr, RefArg inArg1, RefArg inArg2, RefArg inArg3);
Ref		FViewIntoBitmap(RefArg inRcvr, RefArg inArg1, RefArg inArg2, RefArg inArg3);
Ref		FGetBitmapPixel(RefArg inRcvr, RefArg inArg1, RefArg inArg2, RefArg inArg3);
Ref		FHitShape(RefArg inRcvr, RefArg inShape, RefArg inX, RefArg inY);
Ref		FPtInPicture(RefArg inRcvr, RefArg inX, RefArg inY, RefArg inPicture);
}

Rect *	PictureBounds(const unsigned char * inData, Rect * outRect);
void		DrawPicture(char * inPicture, size_t inPictureSize, const Rect * inRect);


/*------------------------------------------------------------------------------
	Draw a picture.
	Determine what kind of data is inIcon -
		PNG data
		Newton bitmap
		Newton shape
	Args:		inIcon
				inFrame
				inJustify
				inTransferMode
	Return:	--
------------------------------------------------------------------------------*/

void
DrawPicture(RefArg inIcon, const Rect * inFrame, ULong inJustify, int inTransferMode)
{
	Rect  iconRect;

	if (IsBinary(inIcon) && EQ(ClassOf(inIcon), SYMA(picture)))
	{
		CDataPtr	iconData(inIcon);
//		DrawPicture(iconData, Length(inIcon), Justify(PictureBounds(iconData, &iconRect), inFrame, inJustify));
	}

	else if (IsFrame(inIcon) && !IsInstance(inIcon, SYMA(bitmap))
			&& (FrameHasSlot(inIcon, SYMA(bits)) || FrameHasSlot(inIcon, SYMA(colorData))))
	{
		RefVar	iconBounds(GetFrameSlot(inIcon, SYMA(bounds)));
		if (!FromObject(iconBounds, &iconRect))
			ThrowMsg("bad pictBounds frame");
		Justify(&iconRect, inFrame, inJustify);
		if (inTransferMode == modeMask) {
			RefVar mask(GetFrameSlot(inIcon, SYMA(mask)));
			if (NOTNIL(mask)) {
				DrawBitmap(mask, &iconRect, modeBic);
			}
			DrawBitmap(inIcon, &iconRect, modeOr);
		} else if (inTransferMode < 0) {
			RefVar mask(GetFrameSlot(inIcon, SYMA(mask)));
			DrawBitmap(mask, &iconRect, -inTransferMode);
		} else {
			DrawBitmap(inIcon, &iconRect, inTransferMode);
		}
	}

	else
	{
		if (inJustify)
			Justify(ShapeBounds(inIcon, &iconRect), inFrame, inJustify);
		else
			iconRect = *inFrame;
		RefVar	style(AllocateFrame());
		RefVar	mode(MAKEINT(inTransferMode));
		SetFrameSlot(style, SYMA(transferMode), mode);
		DrawShape(inIcon, style, *(Point *)&iconRect);
	}
}


/*------------------------------------------------------------------------------
	Draw a picture: a PNG, or a QuickDraw picture (PICT) of bitmaps, as the
	ROM's and packages' are (Host/Pict.h decodes those); others: nothing.
	Args:		inPicture
				inPictureSize
				inRect
	Return:	--
------------------------------------------------------------------------------*/

void
DrawPicture(char * inPicture, size_t inPictureSize, const Rect * inRect)
{
	// a PNG
	if (QDHostDrawPNG((const uint8_t *)inPicture, inPictureSize, inRect->left, inRect->top, inRect->right, inRect->bottom))
		return;
	// a QuickDraw picture of bitmaps (Host/Pict.h)
	int width, height, rowBytes;
	std::vector<unsigned char> bits;
	if (pict::Decode((const unsigned char *)inPicture, inPictureSize, &width, &height, &rowBytes, &bits))
		DrawPixels((const char *)bits.data(), width, height, rowBytes, 1, NULL, inRect, modeOr);
}


// Read image size from PNG data.

typedef struct
{
	ULong		length;
	char		id[4];
	ULong		width;
	ULong		height;
	UChar		bitDepth;
	UChar		colorType;
	UChar		compressionMethod;
	UChar		filterMethod;
	UChar		interlaceMethod;
} PNGchunkIHDR;

ULong
PNGgetULong(const unsigned char * inData)
{
   return	((ULong)(*inData) << 24)
			 +	((ULong)(*(inData + 1)) << 16)
			 +	((ULong)(*(inData + 2)) << 8)
			 +	(ULong)(*(inData + 3));
}

Rect *
PictureBounds(const unsigned char * inData, Rect * outRect)
{
	if (memcmp(inData, "\x89PNG", 4) != 0)
	{
		// a QuickDraw picture: its frame (12 bytes are enough to tell)
		int top, left, bottom, right;
		if (!pict::Frame(inData, 12, &top, &left, &bottom, &right))
			ThrowMsg("picture is neither PNG nor PICT");
		SetRect(outRect, 0, 0, right - left, bottom - top);
		return outRect;
	}

	ULong	chunkLength;
	for (inData += 8; (chunkLength = PNGgetULong(inData)) != 0; inData += chunkLength)
	{
		if (memcmp(inData+4, "IHDR", 4) == 0)
		{
			if (chunkLength != 13)
				ThrowMsg("invalid PNG IHDR chunk");

			outRect->left = 0;
			outRect->top = 0;
			outRect->right = PNGgetULong(inData+8);
			outRect->bottom = PNGgetULong(inData+12);
			break;
		}
	}

	return outRect;
}


Ref
FPictureBounds(RefArg inRcvr, RefArg inPicture)
{
	Rect		bounds;
	CDataPtr	picData(inPicture);
	return ToObject(PictureBounds(picData, &bounds));
}


Ref
FPictToShape(RefArg inRcvr, RefArg inPicture, RefArg inRect)
{
	RefVar shape;
	CDataPtr pictData(inPicture);
	PictureShape * pd = (PictureShape *)(char *)pictData;
	newton_try
	{
		Rect rect;
		if (NOTNIL(inRect))
			FromObject(inRect, &rect);
		else
			memmove(&rect, &pd->bBox, sizeof(Rect));
//		shape = DrawPicture(&pd, &rect, 1);		// original draws a PictureHandle -- need to rework to use PNG?
	}
	cleanup
	{
		pictData.~CDataPtr();
	}
	end_try;
	return shape;
}


#pragma mark -
/*------------------------------------------------------------------------------
	C P i x e l O b j

	A class to convert 1-bit bitmaps to pixel maps.
------------------------------------------------------------------------------*/

class CPixelObj
{
public:
							CPixelObj();
							~CPixelObj();

	void					init(RefArg inBitmap);
	void					init(RefArg inBitmap, bool inUseMask);

	NativePixelMap *	pixMap(void)	const;
	NativePixelMap *	mask(void)		const;
	int					bitDepth(void)	const;

private:
	Ref					getFramBitmap(void);
	NativePixelMap *	framBitmapToPixMap(const FramBitmap * inFramBits);
	NativePixelMap *	framMaskToPixMap(const FramBitmap * inFramBits);

//	In a 64-bit world, we need to translate a 32-bit PixelMap.
	NativePixelMap *	pixMapToPixMap(const PixelMap * inPixMap);

	RefVar				fBitmapRef; // +00
	NativePixelMap		fPixmap;		// +04
	NativePixelMap *	fPixPtr;		// +20
	NativePixelMap		fMask;
	NativePixelMap *	fMaskPtr;	// +24
	UChar *				fColorTable;// +28
	int					fBitDepth;  // +2C
	bool					fIsLocked;  // +30
};

inline	NativePixelMap *	CPixelObj::pixMap(void)		const		{ return fPixPtr; }
inline	NativePixelMap *	CPixelObj::mask(void)		const		{ return fMaskPtr; }
inline	int					CPixelObj::bitDepth(void)	const		{ return fBitDepth; }


CPixelObj::CPixelObj()
{
	fBitDepth = kOneBitDepth;
	fColorTable = NULL;
	fIsLocked = false;
	fPixPtr = &fPixmap;
}


CPixelObj::~CPixelObj()
{
	if (fColorTable)
		FreePtr((Ptr)fColorTable);
	if (fIsLocked)
		UnlockRef(fBitmapRef);
}


void
CPixelObj::init(RefArg inBitmap)
{
	fBitmapRef = inBitmap;
	// we cannot handle class='picture
	if (IsInstance(inBitmap, SYMA(picture)))
		ThrowErr(exGraf, -8803);

	// get a ref to the bitmap, whatever slot it's in
	if (IsFrame(inBitmap)) {
		if (IsInstance(inBitmap, SYMA(bitmap))) {
			RefVar colorData(GetFrameSlot(inBitmap, SYMA(colorData)));
			if (ISNIL(colorData)) {
				fBitmapRef = GetFrameSlot(inBitmap, SYMA(data));
			} else {
				fBitmapRef = getFramBitmap();
			}
		} else {
			fBitmapRef = getFramBitmap();
		}
	}

	LockRef(fBitmapRef);
	fIsLocked = true;

	// create a PixelMap from the bitmap
	if (IsInstance(fBitmapRef, SYMA(pixels))) {
		fPixPtr = pixMapToPixMap((const PixelMap *)BinaryData(fBitmapRef));	// originally just points to (PixelMap *)BinaryData() but we need to convert to NativePixelMap
		fBitDepth = PixelDepth(fPixPtr);		// not original
	} else {
		fPixPtr = framBitmapToPixMap((const FramBitmap *)BinaryData(fBitmapRef));
	}

#if 0
	// we don't mask by drawing with srcBic transferMode any more
	RefVar	theMask = GetFrameSlot(inBitmap, SYMA(mask));
	if (NOTNIL(theMask)) {
		CDataPtr  maskData(theMask);
		fMaskPtr = framMaskToPixMap((const FramBitmap *)(char *)maskData);
	} else
#endif
	fMaskPtr = NULL;
}


void
CPixelObj::init(RefArg inBitmap, bool inUseMask)
{
	fMaskPtr = NULL;
	fBitmapRef = inBitmap;
	if (IsInstance(inBitmap, SYMA(picture)))
		ThrowErr(exGraf, -8803);

	bool isPixelData = false;
	RefVar bits(GetFrameSlot(inBitmap, SYMA(data)));
	if (NOTNIL(bits)) {
		isPixelData = IsInstance(bits, SYMA(pixels));
	}

	LockRef(fBitmapRef);
	fIsLocked = true;
	if (isPixelData) {
		fPixPtr = pixMapToPixMap((const PixelMap *)BinaryData(fBitmapRef));
	} else {
		bits = GetFrameSlot(inBitmap, SYMA(mask));
		if (NOTNIL(bits)) {
			CDataPtr  obj(bits);
			fMaskPtr = framBitmapToPixMap((const FramBitmap *)(char *)obj);
		}
		if (inUseMask || fMaskPtr == NULL) {
			fPixPtr = framBitmapToPixMap((const FramBitmap *)BinaryData(getFramBitmap()));
		}
	}
}


Ref
CPixelObj::getFramBitmap(void)
{
	RefVar	framBitmap;
	RefVar	colorTable;
	RefVar	colorData(GetFrameSlot(fBitmapRef, SYMA(colorData)));
	if (ISNIL(colorData))
		framBitmap = GetFrameSlot(fBitmapRef, SYMA(bits));
	else
	{
#if 0
		GrafPtr  thePort;
		GetPort(&thePort);
		int	portDepth = PixelDepth(&thePort->portBits);
#endif
		int	portDepth = 8; // the host draws grays
		if (IsArray(colorData)) {
		// find colorData that matches display most closely
			int	depth;
			int	maxDepth = 32767; // improbably large number
			int	minDepth = 0;
			CObjectIterator	iter(colorData);
			for ( ; !iter.done(); iter.next()) {
				depth = RINT(GetFrameSlot(iter.value(), SYMA(bitDepth)));
				if (depth == portDepth) {
					fBitDepth = depth;
					framBitmap = GetFrameSlot(iter.value(), SYMA(cBits));
					colorTable = GetFrameSlot(iter.value(), SYMA(colorTable));
				} else if ((portDepth < depth && depth < maxDepth && (maxDepth = depth, 1))
						|| (maxDepth == 32767 && portDepth > depth && depth > minDepth && (minDepth = depth, 1))) {
					fBitDepth = depth;
					framBitmap = GetFrameSlot(iter.value(), SYMA(cBits));
					colorTable = GetFrameSlot(iter.value(), SYMA(colorTable));
				}
			}
		} else {
			fBitDepth = RINT(GetFrameSlot(colorData, SYMA(bitDepth)));
			framBitmap = GetFrameSlot(colorData, SYMA(cBits));
			colorTable = GetFrameSlot(colorData, SYMA(colorTable));
		}

		if (fBitDepth > 1 && NOTNIL(colorTable)) {
			CDataPtr	colorTableData(colorTable);
			ArrayIndex numOfColors = Length(colorTable);
			ArrayIndex colorTableSize = 1 << fBitDepth;
			fColorTable = (UChar *)NewPtr(colorTableSize);
			if (fColorTable) {
				numOfColors /= 8;
				if (numOfColors > colorTableSize)
					numOfColors = colorTableSize;
#if 0
				Ptr defaultColorPtr = PixelMapBits(*GetFgPattern());
				for (ArrayIndex i = 0; i < colorTableSize; ++i) {
					fColorTable[i] = *defaultColorPtr;
				}
#endif
				ULong		r, g, b;
				short *	colorTablePtr = (short *)(char *)colorTableData;
				for (ArrayIndex i = 0; i < numOfColors; ++i) {
					r = *colorTablePtr++;
					g = *colorTablePtr++;
					b = *colorTablePtr++;
					colorTablePtr++;
					fColorTable[i] = RGBtoGray(r,g,b, fBitDepth, fBitDepth);
				}
			}
		}
	}

	return framBitmap;
}


NativePixelMap *
CPixelObj::pixMapToPixMap(const PixelMap * inPixmap)
{
	fPixPtr->rowBytes = CANONICAL_SHORT(inPixmap->rowBytes);
	fPixPtr->reserved1 = 0;
	if (inPixmap->reserved1 == 0x11EB) {
		// it's already been swapped
		fPixPtr->bounds.top = inPixmap->bounds.top;
		fPixPtr->bounds.left = inPixmap->bounds.left;
		fPixPtr->bounds.bottom = inPixmap->bounds.bottom;
		fPixPtr->bounds.right = inPixmap->bounds.right;
	} else {
		fPixPtr->bounds.top = CANONICAL_SHORT(inPixmap->bounds.top);
		fPixPtr->bounds.left = CANONICAL_SHORT(inPixmap->bounds.left);
		fPixPtr->bounds.bottom = CANONICAL_SHORT(inPixmap->bounds.bottom);
		fPixPtr->bounds.right = CANONICAL_SHORT(inPixmap->bounds.right);
	}
	fPixPtr->pixMapFlags = CANONICAL_LONG(inPixmap->pixMapFlags);
	fPixPtr->deviceRes.h = CANONICAL_SHORT(inPixmap->deviceRes.h);
	fPixPtr->deviceRes.v = CANONICAL_SHORT(inPixmap->deviceRes.v);

	fPixPtr->grayTable = 0;	//(UChar *)CANONICAL_LONG(inPixmap->grayTable);
	// our native pixmap baseAddr is ALWAYS a Ptr
	if ((fPixPtr->pixMapFlags & kPixMapStorage) == kPixMapOffset) {
		uint32_t offset = CANONICAL_LONG(inPixmap->baseAddr);
		fPixPtr->baseAddr = (Ptr)inPixmap + offset;
		fPixPtr->pixMapFlags = (fPixPtr->pixMapFlags & ~kPixMapStorage) | kPixMapPtr;
	} else {
printf("PixelMapBits() using baseAddr as Ptr!\n");
	}

	return fPixPtr;
}


NativePixelMap *
CPixelObj::framBitmapToPixMap(const FramBitmap * inBitmap)
{
	// ALWAYS point to the data -- ignore the bitmap's baseAddr
	fPixPtr->baseAddr = (Ptr)inBitmap->data;

	fPixPtr->rowBytes = CANONICAL_SHORT(inBitmap->rowBytes);
	if (inBitmap->reserved1 == 0x11EB) {
		// it's already been swapped
		fPixPtr->bounds.top = inBitmap->bounds.top;
		fPixPtr->bounds.left = inBitmap->bounds.left;
		fPixPtr->bounds.bottom = inBitmap->bounds.bottom;
		fPixPtr->bounds.right = inBitmap->bounds.right;
	} else {
		fPixPtr->bounds.top = CANONICAL_SHORT(inBitmap->bounds.top);
		fPixPtr->bounds.left = CANONICAL_SHORT(inBitmap->bounds.left);
		fPixPtr->bounds.bottom = CANONICAL_SHORT(inBitmap->bounds.bottom);
		fPixPtr->bounds.right = CANONICAL_SHORT(inBitmap->bounds.right);
	}
	fPixPtr->pixMapFlags = kPixMapPtr + fBitDepth;
	fPixPtr->deviceRes.h =
	fPixPtr->deviceRes.v = kDefaultDPI;
	fPixPtr->grayTable = fColorTable;
	if (fColorTable)
		fPixPtr->pixMapFlags |= kPixMapGrayTable;

	return fPixPtr;
}


NativePixelMap *
CPixelObj::framMaskToPixMap(const FramBitmap * inBitmap)
{
	// ALWAYS point to the data -- ignore the bitmap's baseAddr
	fMask.baseAddr = (Ptr)inBitmap->data;

	fMask.rowBytes = CANONICAL_SHORT(inBitmap->rowBytes);
	if (inBitmap->reserved1 == 0x11EB) {
		// it's already been swapped
		fMask.bounds.top = inBitmap->bounds.top;
		fMask.bounds.left = inBitmap->bounds.left;
		fMask.bounds.bottom = inBitmap->bounds.bottom;
		fMask.bounds.right = inBitmap->bounds.right;
	} else {
		fMask.bounds.top = CANONICAL_SHORT(inBitmap->bounds.top);
		fMask.bounds.left = CANONICAL_SHORT(inBitmap->bounds.left);
		fMask.bounds.bottom = CANONICAL_SHORT(inBitmap->bounds.bottom);
		fMask.bounds.right = CANONICAL_SHORT(inBitmap->bounds.right);
	}
	fMask.pixMapFlags = kPixMapPtr + 1;		// always 1 bit deep ??
	fMask.deviceRes.h =
	fMask.deviceRes.v = kDefaultDPI;
	fMask.grayTable = NULL;

	return &fMask;
}

#pragma mark -

/*------------------------------------------------------------------------------
	Draw a bitmap.
	Args:		inBitmap
				inRect
				inTransferMode
	Return:	--
------------------------------------------------------------------------------*/

void
DrawBitmap(RefArg inBitmap, const Rect * inRect, int inTransferMode)
{
	CPixelObj	pix;
	newton_try
	{
		pix.init(inBitmap);

		const NativePixelMap * pixmap = pix.pixMap();
		int	pixmapWidth = RectGetWidth(pixmap->bounds);
		int	pixmapHeight = RectGetHeight(pixmap->bounds);

		Rect	imageRect = *inRect;
		if (RectGetWidth(*inRect) == 0)
		{
			imageRect.right = imageRect.left + pixmapWidth;
			imageRect.bottom = imageRect.top + pixmapHeight;
		}
		DrawPixels(PixelMapBits(pixmap), pixmapWidth, pixmapHeight, pixmap->rowBytes, pix.bitDepth(),
					  (pixmap->pixMapFlags & kPixMapGrayTable) ? pixmap->grayTable : NULL, &imageRect, inTransferMode);
	}
	newton_catch_all
	{
		pix.~CPixelObj();
	}
	end_try;
}


/*------------------------------------------------------------------------------
	Draw pixels: of 1, 2, 4 or 8 bits, row after row of inRowBytes, the
	leftmost pixel in the high bits. 0 is white, the highest value black,
	the others grays between (or, with a gray table, the table's value for
	the pixel's: a PixelMap's grayTable). Scaled into inRect.
	Not original: the Newton blits into its screen's PixelMap.
	Args:		inBits			the pixels
				inWidth			number of pixels in a row
				inHeight			number of rows
				inRowBytes		row offset
				inDepth			number of bits per pixel
				inGrayTable		gray for each pixel value, or NULL
				inRect			where to draw them
				inTransferMode	modeCopy: all of them; modeOr: what isn't
									white (the others transparent); modeBic: what
									is black, in white; modeNotCopy, modeNotOr:
									inverted; others: as modeOr (modeXor: the
									host can't)
	Return:	--
------------------------------------------------------------------------------*/

void
DrawPixels(const char * inBits, int inWidth, int inHeight, int inRowBytes, int inDepth, const UChar * inGrayTable, const Rect * inRect, int inTransferMode)
{
	if (inBits == NULL || inWidth <= 0 || inHeight <= 0
	|| (inDepth != 1 && inDepth != 2 && inDepth != 4 && inDepth != 8)
	|| !QDHostCanDraw())
		return;

	int maxValue = (1 << inDepth) - 1;
	std::vector<uint8_t> rgba((size_t)inWidth * inHeight * 4);
	uint8_t * p = rgba.data();
	for (int y = 0; y < inHeight; ++y)
	{
		const UChar * row = (const UChar *)inBits + (size_t)y * inRowBytes;
		for (int x = 0; x < inWidth; ++x)
		{
			int bitOffset = x * inDepth;
			int value = (row[bitOffset >> 3] >> (8 - inDepth - (bitOffset & 7))) & maxValue;
			if (inGrayTable)
				value = inGrayTable[value] & maxValue;
			uint8_t gray = 255 - value * 255 / maxValue;
			uint8_t alpha = 255;
			switch (inTransferMode)
			{
			case modeCopy:
				break;
			case modeNotCopy:
				gray = 255 - gray;
				break;
			case modeBic:
				alpha = (value == maxValue) ? 255 : 0;
				gray = 255;
				break;
			case modeNotOr:
				alpha = (value != maxValue) ? 255 : 0;
				gray = 255 - gray;
				break;
			default:
				alpha = (value != 0) ? 255 : 0;
				break;
			}
			*p++ = gray;
			*p++ = gray;
			*p++ = gray;
			*p++ = alpha;
		}
	}
	QDHostDrawImage(rgba.data(), inWidth, inHeight, inRect->left, inRect->top, inRect->right, inRect->bottom);
}


/*------------------------------------------------------------------------------
	Draw a bitmap.
	Not part of the original Newton suite, but required by NCX to draw
	screenshots: at 0,0, 1:1.
	Args:		inBits			bitmap data
				inHeight			height of bitmap
				inWidth			width of bitmap
				inRowBytes		row offset
				inDepth			number of bits per pixel
	Return:	--
------------------------------------------------------------------------------*/

void
DrawBits(const char * inBits, unsigned inHeight, unsigned inWidth, unsigned inRowBytes, unsigned inDepth)
{
	Rect rect;
	SetRect(&rect, 0, 0, inWidth, inHeight);
	DrawPixels(inBits, inWidth, inHeight, inRowBytes, inDepth, NULL, &rect, modeCopy);
}


/*------------------------------------------------------------------------------
	The dreaded CopyBits.
	Args:		inImage
				inX
				inY
				inTransferMode
	Return:	NILREF
------------------------------------------------------------------------------*/
#if !defined(forFramework)
#include "View.h"
extern CView *	FailGetView(RefArg inContext);
#endif

void
ToGlobalCoordinates(RefArg inView, short * outLeft, short * outTop, short * outRight, short * outBottom)
{
#if !defined(forFramework)
	Point globalOrigin = *(Point *)&(FailGetView(inView)->viewBounds);
	if (outLeft)
		*outLeft += globalOrigin.h;
	if (outTop)
		*outTop += globalOrigin.v;
	if (outRight)
		*outRight += globalOrigin.h;
	if (outBottom)
		*outBottom += globalOrigin.v;
#endif
}


Ref
FCopyBits(RefArg inRcvr, RefArg inImage, RefArg inX, RefArg inY, RefArg inTransferMode)
{
	if (NOTINT(inX) || NOTINT(inY))
		ThrowMsg("param not an integer");

	Rect  bounds;
	bounds.left = RINT(inX);
	bounds.top = RINT(inY);
	ToGlobalCoordinates(inRcvr, &bounds.left, &bounds.top, NULL, NULL);

	// make the frame empty - Justify will use the image's bounds
	bounds.right = bounds.left;
	bounds.bottom = bounds.top;

	int mode = ISNIL(inTransferMode) ? modeCopy : RVALUE(inTransferMode);
	DrawPicture(inImage, &bounds, vjLeftH + vjTopV, mode);
	return NILREF;
}


NS_STUB(FGrayShrink, RefArg inRcvr, RefArg inArg1, RefArg inArg2)
// DrawIntoBitmap, ViewIntoBitmap: Host/Graphics.cc, Host/ViewMethods.cc

bool
HitShape(RefArg inShape, Point inPt, RefArg ioPath)
{ return false; }

Ref
FHitShape(RefArg inRcvr, RefArg inShape, RefArg inX, RefArg inY)
{
	Point pt;
	pt.h = RINT(inX);
	pt.v = RINT(inY);
	RefVar path(AllocateArray(SYMA(pathExpr), 0));
	bool isHit = HitShape(inShape, pt, path);
	ArrayIndex pathLength = Length(path);
	if (pathLength == 0)
		return MAKEBOOLEAN(isHit);
	// reverse path slot order
	RefVar slot1, slot2;
	for (ArrayIndex i1 = 0; i1 < pathLength/2; ++i1)
	{
		ArrayIndex i2 = (pathLength - 1) - i1;
		slot1 = GetArraySlot(path, i2);
		slot2 = GetArraySlot(path, i1);
		SetArraySlot(path, i2, slot2);
		SetArraySlot(path, i1, slot1);
	}
	return path;
}


Ref
PtInPicture(RefArg inX, RefArg inY, RefArg inPicture, bool inUseMask)
{
	CPixelObj pix;
	RefVar result;
	newton_try
	{
		pix.init(inPicture, inUseMask);
		int x = RINT(inX);
		int y = RINT(inY);
		if (inUseMask)
		{
			if (pix.mask() && PtInMask(pix.mask(), x, y))
				result = MAKEINT(-1);
			else
				result = MAKEINT(PtInCPixelMap(pix.pixMap(), x, y));
		}
		else
		{
			result = MAKEBOOLEAN(PtInPixelMap(pix.mask()?pix.mask():pix.pixMap(), x, y));
		}
	}
	cleanup
	{
		pix.~CPixelObj();
	}
	end_try;
	return result;
}


Ref
FGetBitmapPixel(RefArg inRcvr, RefArg inX, RefArg inY, RefArg inBitmap)
{
	return PtInPicture(inX, inY, inBitmap, true);
}


Ref
FPtInPicture(RefArg inRcvr, RefArg inX, RefArg inY, RefArg inPicture)
{
	return PtInPicture(inX, inY, inPicture, false);
}
