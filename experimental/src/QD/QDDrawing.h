/*
	File:		QDDrawing.h

	Contains:	QuickDraw's drawing calls as the printing code uses them
				(reconstructed: the published NewtQD.h has the types only).
*/

#ifndef __QDDRAWING_H
#define __QDDRAWING_H

#ifndef __NEWTQD_H
#include "QD/NewtQD.h"
#endif

void			SetRect(Rect * outRect, long inLeft, long inTop, long inRight, long inBottom);
RgnHandle		NewRgn(void);
void			DisposeRgn(RgnHandle inRgn);
void			RectRgn(RgnHandle ioRgn, Rect * inRect);
GrafPtr			SetPort(GrafPort * inPort);
void			OpenPort(GrafPort * inPort);
void			ClosePort(GrafPort * inPort);
void			SetPortBits(PixelMap * inBits);
void			SetOrigin(long inH, long inV);
void			PenNormal(void);
PatternHandle	GetStdPattern(UChar inSelector);

#endif	/* __QDDRAWING_H */
