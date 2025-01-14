/*
	File:		DrawImage.cc

	Contains:	Picture and bitmap creation and drawing functions.

	Written by:	Newton Research Group, 2007.
*/

#include "Quartz.h"
#include "Objects.h"
#include "QDPatterns.h"
#include "Geometry.h"
#include "DrawShape.h"
#include "ViewFlags.h"

#include "Iterators.h"
#include "RSSymbols.h"
#include "OSErrors.h"

extern "C" OSErr	PtrToHand(const void * srcPtr, Handle * dstHndl, long size);

extern int gScreenHeight;
/*------------------------------------------------------------------------------
	F u n c t i o n   P r o t o t y p e s
------------------------------------------------------------------------------*/

extern "C" {
Ref		FPictureBounds(RefArg inRcvr, RefArg inPicture);
Ref		FPictToShape(RefArg inRcvr, RefArg inPicture);
Ref		FCopyBits(RefArg inRcvr, RefArg inImage, RefArg inX, RefArg inY, RefArg inTransferMode);
Ref		FDrawXBitmap(RefArg inRcvr, RefArg inArg1, RefArg inArg2, RefArg inArg3);
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
	printf("---- DrawPicture(0x%p, (%d, %d, %d, %d), %08x, %d)\n",
			 &inIcon, inFrame->left, inFrame->top, inFrame->right, inFrame->bottom,
			 inJustify, inTransferMode);

	if (IsBinary(inIcon) && EQ(ClassOf(inIcon), SYMA(picture)))
	{
		CDataPtr	iconData(inIcon);
// TODO: this output PICT encoded data, or here it is supposed to draw a png form the resources.
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
	Draw a PNG picture.
	Pictures are always PNGs in Newton 3.0.
	NOTE	Might be able to use the QuickTime function GraphicsImportCreateCGImage
			declared in ImageCompression.h to draw PICTs.
			http://developer.apple.com/technotes/tn/tn1195.html
	Args:		inPicture
				inPictureSize
				inRect
	Return:	--
------------------------------------------------------------------------------*/
//#include <QuickTime/QuickTime.h>
// QuickTime is not available to 64-bit clients

void
DrawPicture(char * inPicture, size_t inPictureSize, const Rect * inRect)
{
#if defined(forFramework) && 0 // !defined(forNTK)
	// draw legacy PICTs
	Handle						dataRef;
	ComponentInstance			dataRefHandler = NULL;
	GraphicsImportComponent	gi;
	OSErr							err;
	ComponentResult			result;
	CGImageRef					image;

	// prefix PICT data w/ 512 byte header
	size_t pictSize = 512 + inPictureSize;
	Ptr pictPtr = NewPtr(pictSize);		// where is the FreePtr()?
	memmove(pictPtr + 512, inPicture, inPictureSize);

	// create dataRef from pointer
	PointerDataRefRecord		dataRec;
	dataRec.data = pictPtr;
	dataRec.dataLength = pictSize;
	err = PtrToHand(&dataRec, &dataRef, sizeof(PointerDataRefRecord));

	// create data handler
	err = OpenADataHandler(
			dataRef,							/* data reference */
			PointerDataHandlerSubType,	/* data ref. type */
			NULL,								/* anchor data ref. */
			(OSType)0,						/* anchor data ref. type */
			NULL,								/* time base for data handler */
			kDataHCanRead,					/* flag for data handler usage */
			&dataRefHandler);				/* returns the data handler */

	// set data ref extensions to describe the data
	Handle						extension;
	// set the data ref extension for no filename
	unsigned char				filename = 0;
	err = PtrToHand(&filename, &extension, sizeof(filename));
	err = DataHSetDataRefExtension(dataRefHandler, extension, kDataRefExtensionFileName);
	FreeHandle(extension);

	// set the data ref extension for the data handler
	OSType						picType = EndianU32_NtoB('PICT');
	err = PtrToHand(&picType, &extension, sizeof(picType));
	err = DataHSetDataRefExtension(dataRefHandler, extension, kDataRefExtensionMacOSFileType);
	FreeHandle(extension);

	// dispose old data ref handle because it does not contain our new changes
	FreeHandle(dataRef);
	dataRef = NULL;
	// re-acquire data reference from the data handler to get the new changes
	err = DataHGetDataRef(dataRefHandler, &dataRef);
	CloseComponent(dataRefHandler);

	err = GetGraphicsImporterForDataRef(dataRef, PointerDataHandlerSubType, &gi);
	result = GraphicsImportCreateCGImage(gi, &image, kGraphicsImportCreateCGImageUsingCurrentSettings);
	FreeHandle(dataRef);
	CloseComponent(gi);
#else
	// draw PNG
	CGDataProviderRef source = CGDataProviderCreateWithData(NULL, inPicture, inPictureSize, NULL);
	CGImageRef  image = CGImageCreateWithPNGDataProvider(source, NULL, true, kCGRenderingIntentDefault);
#endif
	CGContextDrawImage(quartz, MakeCGRect(*inRect), image);
	CGImageRelease(image);
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
		ThrowMsg("picture is not PNG");

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

	// get a ref to the bitmap, whatever slot it’s in
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
	// we don’t mask by drawing with srcBic transferMode any more
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
		int	portDepth = 8; // for CG grayscale
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
		// it’s already been swapped
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
printf("CPixelObj::pixMapToPixMap() PixelMap bounds=%d,%d, %d,%d", fPixPtr->bounds.top,fPixPtr->bounds.left, fPixPtr->bounds.bottom,fPixPtr->bounds.right);
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
	// ALWAYS point to the data -- ignore the bitmap’s baseAddr
	fPixPtr->baseAddr = (Ptr)inBitmap->data;

	fPixPtr->rowBytes = CANONICAL_SHORT(inBitmap->rowBytes);
	if (inBitmap->reserved1 == 0x11EB) {
		// it’s already been swapped
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
	// ALWAYS point to the data -- ignore the bitmap’s baseAddr
	fMask.baseAddr = (Ptr)inBitmap->data;

	fMask.rowBytes = CANONICAL_SHORT(inBitmap->rowBytes);
	if (inBitmap->reserved1 == 0x11EB) {
		// it’s already been swapped
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

static const unsigned char colorTable1bit[] = { 0xFF, 0x00 };
static const unsigned char colorTable2bit[] = { 0xFF, 0xAA, 0x55, 0x00 };
static const unsigned char colorTable4bit[] = { 0xFF, 0xEE, 0xDD, 0xCC, 0xBB, 0xAA, 0x99, 0x88,
																0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11, 0x00 };
static const unsigned char * colorTable[5] = { NULL, colorTable1bit, colorTable2bit, NULL, colorTable4bit };

void
DrawBitmap(RefArg inBitmap, const Rect * inRect, int inTransferMode)
{
//	[myView lockFocus];
//	imageContext = (CGContextRef)[[NSGraphicsContext currentContext]
//											graphicsPort];
//	...
//	[myView unlockFocus];
// Original code disassembled:
#if 0
DrawBitmap__FRC6RefVarP5TRectl:         @ 0x0003E9B8: DrawBitmap(RefVar const &, TRect *, long)
    @ label = 'DrawBitmap__FRC6RefVarP5TRectl'
    @ ARM R0 = type: 'RefVar' (unknown indirection)
    @ ARM R1 = type: 'TRect'*
    @ ARM R2 = type: 'long'             
    @ name = 'DrawBitmap'
    mov     r12, sp                     @ 0x0003E9B8 0xE1A0C00D - .... 
	// r12 = function frame
    stmdb   sp!, {r4-r6, r11, r12, lr-pc}   @ 0x0003E9BC 0xE92DD870 - .-.p
	// save registers
    sub     r11, r12, #4                @ [ 0x00000004 ] 0x0003E9C0 0xE24CB004 - .L..
	// r11 = function frame plus space
	 mov     r6, r0                      @ 0x0003E9C4 0xE1A06000 - ..`.
	// r6 = inBitmap_Ref
	 mov     r4, r1                      @ 0x0003E9C8 0xE1A04001 - ..@.
	// r4 = inRect_Ptr
    mov     r5, r2                      @ 0x0003E9CC 0xE1A05002 - ..P.
	// r5 = inTransferMode
    sub     sp, sp, #52                 @ [ 0x00000034 ] 0x0003E9D0 0xE24DD034 - .M.4
	// make room for 52 bytes on the stack
	 mov     r0, sp                      @ 0x0003E9D4 0xE1A0000D - ....
	// r0 = top of stack
	 bl      VEC___ct__9TPixelObjFv      @ 0x0003E9D8 0xEB6A857D - .j.}
	// create  a PixelObj on the stack
    mov     r0, #0                      @ [ 0x00000000 ] 0x0003E9DC 0xE3A00000 - ....
	// r0 = 0
    str     r0, [sp, #-108]!            @ 0x0003E9E0 0xE52D006C - .-.l
	// sp-108 = 0
    add     r0, sp, #8                  @ [ 0x00000008 ] 0x0003E9E4 0xE28D0008 - ....
	// r0 = sp+8 (&jmp_buf)
    bl      VEC_setjmp                  @ 0x0003E9E8 0xEB6DCFE1 - .m..
	// fill the jump buffer with the current state so we can return here on error
    teq     r0, #0                      @ [ 0x00000000 ] 0x0003E9EC 0xE3300000 - .0..
	// setjump original call, or return from longjmp?
    bne     L0003EABC                   @ 0x0003E9F0 0x1A000031 - ...1
	// jump if we come from longjmp (an error occured)
    mov     r0, sp                      @ 0x0003E9F4 0xE1A0000D - ....
	// r0 points to top of stack where we wrote 0 (arg 0: CatchHeader*)
    bl      VEC_AddExceptionHandler     @ 0x0003E9F8 0xEB6E859F - .n..
	// Add the exception handle (so longjmp is the start of an exception)
    mov     r1, r6                      @ 0x0003E9FC 0xE1A01006 - ....
	// r1 = inBitmap_Ref (arg 1: Bitmap)
    add     r0, sp, #108                @ [ 0x0000006C ] 0x0003EA00 0xE28D006C - ...l
	// r0 = PixelObj on stack (arg 0)
    bl      VEC_Init__9TPixelObjFRC6RefVar  @ 0x0003EA04 0xEB6A8574 - .j.t
	// void CPixelObj::init(RefArg inBitmap)
    ldr     r6, [sp, #140]              @ 0x0003EA08 0xE59D608C - ..`.
	// space on stack
    ldr     r0, [r4, #2]                @ 0x0003EA0C 0xE5940002 - ....
    mov     r0, r0, asr #16             @ 0x0003EA10 0xE1A00840 - ...@
	// r0 = inRect->left?
    ldr     r1, [r4, #6]                @ 0x0003EA14 0xE5941006 - ....
	// r1 = inRect->right?
    teq     r0, r1, asr #16             @ 0x0003EA18 0xE1300841 - .0.A
	// are they the same?
    bne     L0003EA70                   @ 0x0003EA1C 0x1A000013 - ....
	// if not, jump. If they are the same, take the width from the Bitmap?
    ldr     r1, [r6, #14]               @ 0x0003EA20 0xE596100E - ....
    mov     r1, r1, lsr #16             @ 0x0003EA24 0xE1A01821 - ...! 
    add     r1, r0, r1                  @ 0x0003EA28 0xE0801001 - ....
    ldr     r0, [r6, #10]               @ 0x0003EA2C 0xE596000A - .... 
    mov     r0, r0, lsr #16             @ 0x0003EA30 0xE1A00820 - .... 
    sub     r0, r1, r0                  @ 0x0003EA34 0xE0410000 - .A..
    strb    r0, [r4, #7]                @ 0x0003EA38 0xE5C40007 - .... 
    mov     r0, r0, asr #8              @ 0x0003EA3C 0xE1A00440 - ...@ 
    strb    r0, [r4, #6]                @ 0x0003EA40 0xE5C40006 - .... 
    ldr     r0, [r6, #12]               @ 0x0003EA44 0xE596000C - .... 
    mov     r0, r0, lsr #16             @ 0x0003EA48 0xE1A00820 - .... 
    ldr     r1, [r4]                    @ 0x0003EA4C 0xE5941000 - ....
    mov     r1, r1, lsr #16             @ 0x0003EA50 0xE1A01821 - ...! 
    add     r1, r0, r1                  @ 0x0003EA54 0xE0801001 - ....
    ldr     r0, [r6, #8]                @ 0x0003EA58 0xE5960008 - .... 
    mov     r0, r0, lsr #16             @ 0x0003EA5C 0xE1A00820 - .... 
    sub     r0, r1, r0                  @ 0x0003EA60 0xE0410000 - .A..
    strb    r0, [r4, #5]                @ 0x0003EA64 0xE5C40005 - .... 
    mov     r0, r0, asr #8              @ 0x0003EA68 0xE1A00440 - ...@ 
    strb    r0, [r4, #4]                @ 0x0003EA6C 0xE5C40004 - .... 
L0003EA70:
    sub     sp, sp, #4                  @ [ 0x00000004 ] 0x0003EA70 0xE24DD004 - .M.. 
	// make room for 4 more bytes on the stack (GrafPort Ptr)
    mov     r0, sp                      @ 0x0003EA74 0xE1A0000D - ....
	// r0 = top of stack (arg 0)
    bl      VEC_GetPort__FPP8GrafPort   @ 0x0003EA78 0xEB6EC349 - .n.I
	// get the grafport pointer: GetPort(GrafPort **)
    mov     r3, #0                      @ [ 0x00000000 ] 0x0003EA7C 0xE3A03000 - ..0.
	// r3 = 0 (arg5, Region**)
    mov     r2, r5                      @ 0x0003EA80 0xE1A02005 - ....
	// r2 = inTransferMode (arg4)
    stmdb   sp!, {r2, r3}               @ 0x0003EA84 0xE92D000C - .-..
	// push r2 and r3
    add     r2, r6, #8                  @ [ 0x00000008 ] 0x0003EA88 0xE2862008 - ....
	// r2 = pixelmap rect (arg2)
    mov     r3, r4                      @ 0x0003EA8C 0xE1A03004 - ..0.
	// r3 = inRect? (arg3)
    mov     r0, r6                      @ 0x0003EA90 0xE1A00006 - ....
	// r0 = pixelmap (arg0)
    ldr     r1, [sp, #8]                @ 0x0003EA94 0xE59D1008 - ....
	// r1 = address on stack (GarfPort*)
    bl      VEC_CopyBits__FP8PixelMapT1P4RectT3lPP6Region  @ 0x0003EA98 0xEB6EC340 - .n.@
	// CopyBits(PixelMap *, PixelMap *, Rect *, Rect *, long, Region **)
    add     sp, sp, #12                 @ [ 0x0000000C ] 0x0003EA9C 0xE28DD00C - ....
	// restore sp
    mov     r0, sp                      @ 0x0003EAA0 0xE1A0000D - ....
	// r0 = exception handle
    bl      VEC_ExitHandler             @ 0x0003EAA4 0xEB6E8983 - .n..
	// ExitHandler(handler)
    add     sp, sp, #108                @ [ 0x0000006C ] 0x0003EAA8 0xE28DD06C - ...l
	// pop exception data
    mov     r0, sp                      @ 0x0003EAAC 0xE1A0000D - ....
    mov     r1, #0                      @ [ 0x00000000 ] 0x0003EAB0 0xE3A01000 - .... 
    bl      VEC___dt__9TPixelObjFv      @ 0x0003EAB4 0xEB6A8547 - .j.G
	// call the TPixelObj destructor
    ldmdb   r11, {r4-r6, r11, sp, pc}   @ 0x0003EAB8 0xE91BA870 - ...p
	// and return from the function
L0003EABC:
	// Exception handler exits here
    add     r0, sp, #108                @ [ 0x0000006C ] 0x0003EABC 0xE28D006C - ...l
	// fix the stack
    mov     r1, #0                      @ [ 0x00000000 ] 0x0003EAC0 0xE3A01000 - ....
    bl      VEC___dt__9TPixelObjFv      @ 0x0003EAC4 0xEB6A8543 - .j.C
	// destroy the PixelObj
    mov     r0, sp                      @ 0x0003EAC8 0xE1A0000D - ....
    bl      VEC_NextHandler             @ 0x0003EACC 0xEB6E8DA0 - .n..
	// call the next exception handler
    b       L0003EABC                   @ 0x0003EAD0 0xEAFFFFF9 - ....
	// huh??? Does the call above not return?

#endif

	CPixelObj	pix;
	newton_try
	{
		pix.init(inBitmap);

		size_t	pixmapWidth = RectGetWidth(pix.pixMap()->bounds);
		size_t	pixmapHeight = RectGetHeight(pix.pixMap()->bounds);

		CGRect	imageRect = MakeCGRect(*inRect);
		if (RectGetWidth(*inRect) == 0)
		{
			imageRect.size.width = pixmapWidth;
			imageRect.size.height = pixmapHeight;
		}

		CGImageRef			image = NULL;
		CGColorSpaceRef	baseColorSpace = CGColorSpaceCreateWithName(kCGColorSpaceGenericGray);
		CGColorSpaceRef	colorSpace = CGColorSpaceCreateIndexed(baseColorSpace, (1 << pix.bitDepth())-1, colorTable[pix.bitDepth()]);
		CGDataProviderRef source = CGDataProviderCreateWithData(NULL, PixelMapBits(pix.pixMap()), pixmapHeight * pix.pixMap()->rowBytes, NULL);
		if (source)
		{
			// create entire image
			image = CGImageCreate(pixmapWidth, pixmapHeight, pix.bitDepth(), pix.bitDepth(), pix.pixMap()->rowBytes,
										colorSpace, kCGImageAlphaNone, source,
										NULL, false, kCGRenderingIntentSaturation);
			CGDataProviderRelease(source);

			if (pix.mask() != NULL) {
				// image is masked
				CGDataProviderRef maskSource;
				if ((maskSource = CGDataProviderCreateWithData(NULL, PixelMapBits(pix.mask()), pixmapHeight * pix.mask()->rowBytes, NULL)) != NULL) {
					CGImageRef	fullImage = image;
					CGColorSpaceRef maskColorSpace = CGColorSpaceCreateDeviceGray();
					CGImageRef	mask = CGImageCreate(pixmapWidth, pixmapHeight, 1, 1, pix.mask()->rowBytes,
																maskColorSpace, kCGImageAlphaNone, maskSource,
																NULL, false, kCGRenderingIntentDefault);
					CGColorSpaceRelease(maskColorSpace);
					CGDataProviderRelease(maskSource);
					if (mask) {
						// recreate image using original full image with mask
						image = CGImageCreateWithMask(fullImage, mask);
						CGImageRelease(fullImage);
						CGImageRelease(mask);
					}
				}
			}
		}
		CGColorSpaceRelease(colorSpace);
		CGColorSpaceRelease(baseColorSpace);

		if (image)
		{
			CGContextDrawImage(quartz, imageRect, image);
			CGImageRelease(image);
		}
	}
	newton_catch_all
	{
		pix.~CPixelObj();
	}
	end_try;
}



/*------------------------------------------------------------------------------
	Draw a bitmap.
	Not part of the original Newton suite, but required by NCX to draw
	screenshots.
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
	CGImageRef			image = NULL;
	CGColorSpaceRef	baseColorSpace = CGColorSpaceCreateWithName(kCGColorSpaceGenericGray);
	CGColorSpaceRef	colorSpace = CGColorSpaceCreateIndexed(baseColorSpace, (1 << inDepth)-1, colorTable[inDepth]);
	CGDataProviderRef source = CGDataProviderCreateWithData(NULL, inBits, inHeight * inRowBytes, NULL);
	if (source)
	{
		image = CGImageCreate(inWidth, inHeight, inDepth, inDepth, inRowBytes,
									colorSpace, kCGImageAlphaNone, source,
									NULL, false, kCGRenderingIntentDefault);
		CGDataProviderRelease(source);
	}
	CGColorSpaceRelease(colorSpace);
	CGColorSpaceRelease(baseColorSpace);

	if (image)
	{
		CGRect	imageRect = CGRectMake(0.0, 0.0, inWidth, inHeight);
		CGContextDrawImage(quartz, imageRect, image);
		CGImageRelease(image);
	}
}


/*------------------------------------------------------------------------------
	The dreaded CopyBits.
	Args:		inImage
				inX
				inY
				inTransferMode
	Return:	NILREF
------------------------------------------------------------------------------*/

#if 0
CopyBits__FP8PixelMapT1P4RectT3lPP6Region:  @ 0x002AE440: CopyBits(PixelMap *, PixelMap *, Rect *, Rect *, long, Region **)
    @ label = 'CopyBits__FP8PixelMapT1P4RectT3lPP6Region'
    @ ARM R0 = type: 'PixelMap'*
    @ ARM R1 = type: 'PixelMap'*
    @ ARM R2 = type: 'Rect'*
    @ ARM R3 = type: 'Rect'*
    @ frame[-4] = type: 'long'
    @ frame[-8] = type: 'Region' (unknown indirection)
    @ name = 'CopyBits'
    mov     r12, sp                     @ 0x002AE440 0xE1A0C00D - .... 
    stmdb   sp!, {r4-r12, lr-pc}        @ 0x002AE444 0xE92DDFF0 - .-.. 
    sub     r11, r12, #4                @ [ 0x00000004 ] 0x002AE448 0xE24CB004 - .L.. 
    mov     r5, r0                      @ 0x002AE44C 0xE1A05000 - ..P. 
    mov     r4, r1                      @ 0x002AE450 0xE1A04001 - ..@.
    mov     r6, r2                      @ 0x002AE454 0xE1A06002 - ..`.
    mov     r7, r3                      @ 0x002AE458 0xE1A07003 - ..p. 
    ldr     r9, [r11, #8]               @ 0x002AE45C 0xE59B9008 - .... 
    ldr     r10, [r11, #4]              @ 0x002AE460 0xE59BA004 - .... 
    bl      VEC_GetCurrentPort__Fv      @ 0x002AE464 0xEB6284D4 - .b..
	// GetCurrentPort(void)
    mov     r8, r0                      @ 0x002AE468 0xE1A08000 - ....
    mov     r1, r6                      @ 0x002AE46C 0xE1A01006 - .... 
    mov     r0, r5                      @ 0x002AE470 0xE1A00005 - .... 
    bl      VEC_StartProtectSrcBits__FP8PixelMapP4Rect  @ 0x002AE474 0xEB62996E - .b.n
	// StartProtectSrcBits(PixelMap *, Rect *) : does not do anything
    teq     r8, #0                      @ [ 0x00000000 ] 0x002AE478 0xE3380000 - .8..
    beq     L002AE4EC                   @ 0x002AE47C 0x0A00001A - ....
    tst     r8, #1                      @ [ 0x00000001 ] 0x002AE480 0xE3180001 - .... 
    bne     L002AE4EC                   @ 0x002AE484 0x1A000018 - ....
    mov     r0, r8                      @ 0x002AE488 0xE1A00008 - .... 
    bl      VEC_GetPixelMapBits__FP8PixelMap  @ 0x002AE48C 0xEB65785D - .ex] 
	// GetPixelMapBits(PixelMap *)
    str     r0, [sp, #-4]!              @ 0x002AE490 0xE52D0004 - .-..
    mov     r0, r4                      @ 0x002AE494 0xE1A00004 - ....
    bl      VEC_GetPixelMapBits__FP8PixelMap  @ 0x002AE498 0xEB65785A - .exZ
	// GetPixelMapBits(PixelMap *)
    ldr     r1, [sp], #4                @ 0x002AE49C 0xE49D1004 - ....
    teq     r1, r0                      @ 0x002AE4A0 0xE1310000 - .1..
    ldreq   r0, [r8, #8]                @ 0x002AE4A4 0x05980008 - .... 
    moveq   r0, r0, asr #16             @ 0x002AE4A8 0x01A00840 - ...@ 
    ldreq   r1, [r4, #8]                @ 0x002AE4AC 0x05941008 - .... 
    teqeq   r0, r1, asr #16             @ 0x002AE4B0 0x01300841 - .0.A 
    ldreq   r0, [r8, #10]               @ 0x002AE4B4 0x0598000A - .... 
    moveq   r0, r0, asr #16             @ 0x002AE4B8 0x01A00840 - ...@ 
    ldreq   r1, [r4, #10]               @ 0x002AE4BC 0x0594100A - .... 
    teqeq   r0, r1, asr #16             @ 0x002AE4C0 0x01300841 - .0.A 
    bne     L002AE4EC                   @ 0x002AE4C4 0x1A000008 - ....
    mov     r3, r9                      @ 0x002AE4C8 0xE1A03009 - ..0.
    stmdb   sp!, {r3}                   @ 0x002AE4CC 0xE92D0008 - .-.. 
    mov     r3, r10                     @ 0x002AE4D0 0xE1A0300A - ..0.
    mov     r2, r7                      @ 0x002AE4D4 0xE1A02007 - ....
    mov     r1, r6                      @ 0x002AE4D8 0xE1A01006 - ....
    mov     r0, r5                      @ 0x002AE4DC 0xE1A00005 - ....
    bl      VEC_CallBits__FP8PixelMapP4RectT2lPP6Region  @ 0x002AE4E0 0xEB627860 - .bx`
	// CallBits(PixelMap *, Rect *, Rect *, long, Region **)
    add     sp, sp, #4                  @ [ 0x00000004 ] 0x002AE4E4 0xE28DD004 - ....
    b       L002AE530                   @ 0x002AE4E8 0xEA000010 - ....
L002AE4EC:
    ldr     r0, L002AE53C               @ [ wideHandle (0x0C1056F0) ] 0x002AE4EC 0xE59F0048 - ...H
    teq     r9, #0                      @ [ 0x00000000 ] 0x002AE4F0 0xE3390000 - .9.. 
    movne   r3, r9                      @ 0x002AE4F4 0x11A03009 - ..0.
    ldreq   r3, [r0]                    @ 0x002AE4F8 0x05903000 - ..0.
    stmdb   sp!, {r3}                   @ 0x002AE4FC 0xE92D0008 - .-.. 
    ldr     r0, [r0]                    @ 0x002AE500 0xE5900000 - ....
    mov     r3, r0                      @ 0x002AE504 0xE1A03000 - ..0.
    stmdb   sp!, {r3}                   @ 0x002AE508 0xE92D0008 - .-.. 
    mov     r3, r0                      @ 0x002AE50C 0xE1A03000 - ..0.
    mov     r2, r10                     @ 0x002AE510 0xE1A0200A - ....
    stmdb   sp!, {r2, r3}               @ 0x002AE514 0xE92D000C - .-.. 
    mov     r3, r7                      @ 0x002AE518 0xE1A03007 - ..0.
    mov     r2, r6                      @ 0x002AE51C 0xE1A02006 - ....
    mov     r1, r4                      @ 0x002AE520 0xE1A01004 - ....
    mov     r0, r5                      @ 0x002AE524 0xE1A00005 - ....
    bl      VEC_StretchBits__FP8PixelMapT1P4RectT3lPP6RegionN26  @ 0x002AE528 0xEB629951 - .b.Q
	// StretchBits(PixelMap *, PixelMap *, Rect *, Rect *, long, Region **, Region **, Region **)
    add     sp, sp, #16                 @ [ 0x00000010 ] 0x002AE52C 0xE28DD010 - ....
L002AE530:
    mov     r0, r5                      @ 0x002AE530 0xE1A00005 - ....
    ldmdb   r11, {r4-r11, sp, lr}       @ 0x002AE534 0xE91B6FF0 - ..o.
    b       VEC_StopProtectSrcBits__FP8PixelMap  @ 0x002AE538 0xEA62994C - .b.L
	// StopProtectSrcBits__FP8PixelMap : does not do anything
L002AE53C:
    .word   0x0C1056F0          @ 0x002AE53C "..V." 202397424 (flags_type_arm_word) wideHandle?

// This jumps to one of the drawing calls in qdConstants
CallBits__FP8PixelMapP4RectT2lPP6Region:    @ 0x002AD604: CallBits(PixelMap *, Rect *, Rect *, long, Region **)
    @ label = 'CallBits__FP8PixelMapP4RectT2lPP6Region'
    @ ARM R0 = type: 'PixelMap'*
    @ ARM R1 = type: 'Rect'*
    @ ARM R2 = type: 'Rect'*
    @ ARM R3 = type: 'long'
    @ frame[-4] = type: 'Region' (unknown indirection)
    @ name = 'CallBits'
    mov     r12, sp                     @ 0x002AD604 0xE1A0C00D - ....
    stmdb   sp!, {r4-r8, r11, r12, lr-pc}   @ 0x002AD608 0xE92DD9F0 - .-.. 
    sub     r11, r12, #4                @ [ 0x00000004 ] 0x002AD60C 0xE24CB004 - .L.. 
    mov     r7, r0                      @ 0x002AD610 0xE1A07000 - ..p. 
    mov     r6, r1                      @ 0x002AD614 0xE1A06001 - ..`. 
    mov     r5, r2                      @ 0x002AD618 0xE1A05002 - ..P. 
    mov     r4, r3                      @ 0x002AD61C 0xE1A04003 - ..@. 
    ldr     r8, [r11, #4]               @ 0x002AD620 0xE59B8004 - .... 
    bl      VEC_GetCurrentPort__Fv      @ 0x002AD624 0xEB628864 - .b.d 
    ldr     r0, [r0, #64]               @ 0x002AD628 0xE5900040 - ...@ 
    teq     r0, #0                      @ [ 0x00000000 ] 0x002AD62C 0xE3300000 - .0.. 
    ldrne   r12, [r0, #4]!              @ 0x002AD630 0x15B0C004 - .... 
    ldreq   r0, L002AD660               @ [ qdConstants (0x00380BCC) ] 0x002AD634 0x059F0024 - ...$
    ldreq   r12, [r0, #4]!              @ 0x002AD638 0x05B0C004 - .... 
    mov     r3, r8                      @ 0x002AD63C 0xE1A03008 - ..0.
    stmdb   sp!, {r3}                   @ 0x002AD640 0xE92D0008 - .-.. 
    mov     r3, r4                      @ 0x002AD644 0xE1A03004 - ..0. 
    mov     r2, r5                      @ 0x002AD648 0xE1A02005 - .... 
    mov     r1, r6                      @ 0x002AD64C 0xE1A01006 - .... 
    mov     r0, r7                      @ 0x002AD650 0xE1A00007 - .... 
    mov     lr, pc                      @ 0x002AD654 0xE1A0E00F - .... 
    mov     pc, r12                     // probably calling StdBits here
    ldmdb   r11, {r4-r8, r11, sp, pc}   @ 0x002AD65C 0xE91BA9F0 - ....
L002AD660:
    .word   0x00380BCC          @ 0x002AD660 ".8.." 3673036 (flags_type_arm_word) qdConstants?

#endif

#include "View.h"
#if !defined(forFramework)
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

	// make the frame empty - Justify will use the image’s bounds
	bounds.right = bounds.left;
	bounds.bottom = bounds.top;

	int mode = ISNIL(inTransferMode) ? modeCopy : RVALUE(inTransferMode);
	DrawPicture(inImage, &bounds, vjLeftH + vjTopV, mode);
	return NILREF;
}


Ref		FDrawXBitmap(RefArg inRcvr, RefArg inArg1, RefArg inArg2, RefArg inArg3) { return NILREF; }
Ref		FGrayShrink(RefArg inRcvr, RefArg inArg1, RefArg inArg2) { return NILREF; }
Ref		FDrawIntoBitmap(RefArg inRcvr, RefArg inArg1, RefArg inArg2, RefArg inArg3) { return NILREF; }
Ref		FViewIntoBitmap(RefArg inRcvr, RefArg inArg1, RefArg inArg2, RefArg inArg3) { return NILREF; }

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

