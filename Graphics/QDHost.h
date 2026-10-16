/*
	File:		QDHost.h

	Contains:	What the QuickDraw functions (QDDrawing.cc, DrawShape.cc,
					DrawImage.cc, DrawText.cc, DrawInk.cc) draw with: a few
					primitives in the host's graphics.

					With FLTK (NEWTON_QD_FLTK; QDHost.cc) they draw on FLTK's
					current surface: a window in its draw(), or an
					Fl_Image_Surface made current (an image to save, print, or
					copy). Without FLTK they draw nothing.

					Coordinates are the Newton's: pixels, the origin at the top
					left, y down. A rectangle is left, top, right, bottom, its
					edges between pixels (QuickDraw's): (0,0,10,10) is 10 by 10
					pixels. A colour is 0xRRGGBB.

					No framework headers here: QDHost.cc includes FLTK's, which
					clash with them (OVERRIDE, ...).

	Written by:	Newton Research Group, 2026.
*/

#if !defined(__QDHOST_H)
#define __QDHOST_H 1

#include <stddef.h>
#include <stdint.h>

typedef uint32_t QDColor;

// the Newton's patterns, as grays (vfWhite ... vfBlack)
const QDColor	kQDWhite = 0xFFFFFF;
const QDColor	kQDLightGray = 0xBFBFBF;
const QDColor	kQDGray = 0x808080;
const QDColor	kQDDarkGray = 0x404040;
const QDColor	kQDBlack = 0x000000;

/* True if drawing goes somewhere (FLTK is linked). */
bool		QDHostCanDraw(void);

/* The pen's and the fill's colours, and the pen's width (0: shapes have no
	outline). */
void		QDHostSetStrokeColor(QDColor inColor);
QDColor	QDHostStrokeColor(void);
void		QDHostSetFillColor(QDColor inColor);
QDColor	QDHostFillColor(void);
void		QDHostSetLineWidth(float inWidth);
float		QDHostLineWidth(void);

/* Save and restore the above, nested. */
void		QDHostSaveState(void);
void		QDHostRestoreState(void);

/* Shapes. The pen is inside a shape's rectangle (QuickDraw's), a line's
	pen is centred on it. Angles are QuickDraw's: degrees, clockwise from 12
	o'clock. An oval's corners (round rectangles) are the oval's width and
	height (diameters). */
void		QDHostStrokeLine(float inX1, float inY1, float inX2, float inY2);
void		QDHostStrokeRect(int inLeft, int inTop, int inRight, int inBottom);
void		QDHostFillRect(int inLeft, int inTop, int inRight, int inBottom);
void		QDHostStrokeRoundRect(int inLeft, int inTop, int inRight, int inBottom, int inOvalWd, int inOvalHt);
void		QDHostFillRoundRect(int inLeft, int inTop, int inRight, int inBottom, int inOvalWd, int inOvalHt);
void		QDHostStrokeOval(int inLeft, int inTop, int inRight, int inBottom);
void		QDHostFillOval(int inLeft, int inTop, int inRight, int inBottom);
void		QDHostStrokeArc(int inLeft, int inTop, int inRight, int inBottom, int inStartAngle, int inArcAngle);
void		QDHostFillArc(int inLeft, int inTop, int inRight, int inBottom, int inStartAngle, int inArcAngle);

/* Lines through points (x, y pairs: inCount points, 2 * inCount floats),
	closed back to the first if inClosed; a polygon filled. */
void		QDHostStrokePolyline(const float * inXY, size_t inCount, bool inClosed);
void		QDHostFillPolygon(const float * inXY, size_t inCount);

/* An image: inWidth by inHeight pixels of 4 bytes each (red, green, blue,
	alpha: 0 transparent, 255 opaque), row after row, scaled into the
	rectangle (each pixel a block of the surface's, not smoothed). */
void		QDHostDrawImage(const uint8_t * inRGBA, int inWidth, int inHeight, int inLeft, int inTop, int inRight, int inBottom);

/* A PNG file's data, scaled into the rectangle. False if it isn't a PNG
	(or there's no FLTK). */
bool		QDHostDrawPNG(const uint8_t * inData, size_t inSize, int inLeft, int inTop, int inRight, int inBottom);

/* Text: UTF-8. A font is a Newton family (tsSystem, tsFancy, tsSimple,
	tsHandwriting: Helvetica, Times, Helvetica, Helvetica for now), a size in
	pixels, and the face bits (kBoldFace, kItalicFace; the others: not yet). */
struct QDHostFont
{
	int		family;
	int		size;
	unsigned	face;
};

struct QDHostFontInfo
{
	int		ascent;
	int		descent;
	int		leading;
	int		widMax;
};

void		QDHostGetFontInfo(const QDHostFont & inFont, QDHostFontInfo * outInfo);
float		QDHostTextWidth(const char * inUTF8, size_t inLength, const QDHostFont & inFont);

/* A line of text, its baseline's left end at inX, inY. */
void		QDHostDrawText(const char * inUTF8, size_t inLength, float inX, float inY, const QDHostFont & inFont, QDColor inColor);

/* Text wrapped to the rectangle's width, from its top; inJustifyH: 0 left,
	1 centred, 2 right. Draws it if inDraw. The height the text takes, in
	pixels (without FLTK: an estimate). */
int		QDHostTextBox(const char * inUTF8, size_t inLength, int inLeft, int inTop, int inRight, int inBottom,
							  const QDHostFont & inFont, QDColor inColor, int inJustifyH, bool inDraw);

#endif	/* __QDHOST_H */
