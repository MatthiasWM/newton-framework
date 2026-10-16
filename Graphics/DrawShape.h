/*
	File:		DrawShape.h

	Contains:	Shape drawing declarations.

	Written by:	Newton Research Group, 2007.
*/

#if !defined(__DRAWSHAPE_H)
#define __DRAWSHAPE_H 1

#include "QDDrawing.h"

class RefVar;
typedef const RefVar& RefArg;		// as Objects.h


/*------------------------------------------------------------------------------
	F u n c t i o n   P r o t o t y p e s
------------------------------------------------------------------------------*/

extern bool		FromObject(RefArg inObj, Rect * outBounds);
extern Ref		ToObject(const Rect * inBounds);

extern Rect *  Justify(Rect * ioRect, const Rect * inFrame, ULong inJustify);

extern Rect *	ShapeBounds(RefArg inShape, Rect * outBounds);
extern void		DrawShape(RefArg inShape, RefArg inStyle, Point inOffset);

extern void		DrawPicture(RefArg inIcon, const Rect * inFrame, ULong inJustify, int inTransferMode);
extern void		DrawBitmap(RefArg inBitmap, const Rect * inFrame, int inTransferMode);
extern void		DrawPicture(char * inPicture, size_t inPictureSize, const Rect * inRect);
extern Rect *	PictureBounds(const unsigned char * inData, Rect * outRect);
extern void		DrawPixels(const char * inBits, int inWidth, int inHeight, int inRowBytes, int inDepth, const UChar * inGrayTable, const Rect * inRect, int inTransferMode);
extern void		DrawBits(const char * inBits, unsigned inHeight, unsigned inWidth, unsigned inRowBytes, unsigned inDepth);
extern void		DrawPolygon(char * inData, short inX, short inY);
#if defined(hasByteSwapping)
extern void		MungeBounds(RefArg inShape);
#endif

extern bool		PtInPixelMap(const NativePixelMap * inPixmap, int inX, int inY);
extern int		PtInCPixelMap(const NativePixelMap * inPixmap, int inX, int inY);
extern int		PtInMask(const NativePixelMap * inPixmap, int inX, int inY);

#endif	/* __DRAWSHAPE_H */
