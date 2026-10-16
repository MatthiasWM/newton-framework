/*
	File:		QDHost.cc

	Contains:	The QuickDraw functions' primitives (QDHost.h): with FLTK
					(NEWTON_QD_FLTK) on FLTK's current surface, else nothing.

	Written by:	Newton Research Group, 2026.
*/

#if NEWTON_QD_FLTK
// FLTK only: the framework's headers #define names FLTK uses
#include <FL/Fl.H>
#include <FL/fl_draw.H>
#include <FL/Fl_Image.H>
#include <FL/Fl_PNG_Image.H>
#endif

#include "QDHost.h"

#include <math.h>
#include <string.h>
#include <string>
#include <vector>


/*------------------------------------------------------------------------------
	S t a t e
------------------------------------------------------------------------------*/

struct QDHostState
{
	QDColor	stroke = kQDBlack;
	QDColor	fill = kQDBlack;
	float		lineWidth = 1.0;
};

static QDHostState					gState;
static std::vector<QDHostState>	gSavedStates;

void		QDHostSetStrokeColor(QDColor inColor)	{ gState.stroke = inColor; }
QDColor	QDHostStrokeColor(void)						{ return gState.stroke; }
void		QDHostSetFillColor(QDColor inColor)		{ gState.fill = inColor; }
QDColor	QDHostFillColor(void)						{ return gState.fill; }
void		QDHostSetLineWidth(float inWidth)		{ gState.lineWidth = inWidth > 0 ? inWidth : 0; }
float		QDHostLineWidth(void)						{ return gState.lineWidth; }

void
QDHostSaveState(void)
{
	gSavedStates.push_back(gState);
}

void
QDHostRestoreState(void)
{
	if (!gSavedStates.empty()) {
		gState = gSavedStates.back();
		gSavedStates.pop_back();
	}
}


#if NEWTON_QD_FLTK

static const double kPi = 3.14159265358979323846;

bool
QDHostCanDraw(void)
{
	return true;
}


static void
SetColor(QDColor inColor)
{
	fl_color((uchar)(inColor >> 16), (uchar)(inColor >> 8), (uchar)inColor);
}


static int
PenWidth(void)
{
	return gState.lineWidth > 0 ? (int)(gState.lineWidth + 0.5) : 0;
}


/*------------------------------------------------------------------------------
	A pen of the pen's width (centred on the lines), until EndPen().
------------------------------------------------------------------------------*/

static bool
BeginPen(int inCapJoin = 0)
{
	int width = PenWidth();
	if (width == 0)
		return false;
	SetColor(gState.stroke);
	fl_line_style(FL_SOLID | inCapJoin, width);
	return true;
}

static void
EndPen(void)
{
	fl_line_style(0);
}


/*------------------------------------------------------------------------------
	Points on an oval: centre, radii, from inStart to inStart + inArc
	(radians, counterclockwise from 3 o'clock, y down as on the screen).
------------------------------------------------------------------------------*/

static void
OvalVertices(double inCX, double inCY, double inRX, double inRY, double inStart, double inArc)
{
	int segments = (int)(fabs(inArc) * (inRX > inRY ? inRX : inRY) / 2.0);
	if (segments < 16)
		segments = 16;
	for (int i = 0; i <= segments; ++i) {
		double a = inStart + inArc * i / segments;
		fl_vertex(inCX + inRX * cos(a), inCY - inRY * sin(a));
	}
}


void
QDHostStrokeLine(float inX1, float inY1, float inX2, float inY2)
{
	if (BeginPen()) {
		fl_begin_line();
		fl_vertex(inX1, inY1);
		fl_vertex(inX2, inY2);
		fl_end_line();
		EndPen();
	}
}


void
QDHostStrokeRect(int inLeft, int inTop, int inRight, int inBottom)
{
	int pen = PenWidth();
	int w = inRight - inLeft, h = inBottom - inTop;
	if (pen == 0 || w <= 0 || h <= 0)
		return;
	SetColor(gState.stroke);
	if (2 * pen >= w || 2 * pen >= h) {
		fl_rectf(inLeft, inTop, w, h);
	} else {
		fl_rectf(inLeft, inTop, w, pen);
		fl_rectf(inLeft, inBottom - pen, w, pen);
		fl_rectf(inLeft, inTop + pen, pen, h - 2 * pen);
		fl_rectf(inRight - pen, inTop + pen, pen, h - 2 * pen);
	}
}


void
QDHostFillRect(int inLeft, int inTop, int inRight, int inBottom)
{
	if (inRight > inLeft && inBottom > inTop) {
		SetColor(gState.fill);
		fl_rectf(inLeft, inTop, inRight - inLeft, inBottom - inTop);
	}
}


static int
CornerRadius(int inWidth, int inHeight, int inOvalWd, int inOvalHt)
{
	int r = (inOvalWd < inOvalHt ? inOvalWd : inOvalHt) / 2;
	int maxR = (inWidth < inHeight ? inWidth : inHeight) / 2;
	return r < 0 ? 0 : (r > maxR ? maxR : r);
}


void
QDHostStrokeRoundRect(int inLeft, int inTop, int inRight, int inBottom, int inOvalWd, int inOvalHt)
{
	int pen = PenWidth();
	int w = inRight - inLeft, h = inBottom - inTop;
	if (pen == 0 || w <= 0 || h <= 0)
		return;
	SetColor(gState.stroke);
	int r = CornerRadius(w, h, inOvalWd, inOvalHt);
	// the pen inside the shape: rings of one pixel
	for (int i = 0; i < pen && 2 * i < w && 2 * i < h; ++i)
		fl_rounded_rect(inLeft + i, inTop + i, w - 2 * i, h - 2 * i, r > i ? r - i : 0);
}


void
QDHostFillRoundRect(int inLeft, int inTop, int inRight, int inBottom, int inOvalWd, int inOvalHt)
{
	int w = inRight - inLeft, h = inBottom - inTop;
	if (w <= 0 || h <= 0)
		return;
	SetColor(gState.fill);
	fl_rounded_rectf(inLeft, inTop, w, h, CornerRadius(w, h, inOvalWd, inOvalHt));
}


/*------------------------------------------------------------------------------
	Ovals and arcs: the pen centred half its width inside the rectangle.
	QuickDraw's angles to radians counterclockwise from 3 o'clock.
------------------------------------------------------------------------------*/

static void
StrokeOvalArc(int inLeft, int inTop, int inRight, int inBottom, double inStart, double inArc, bool inClosed)
{
	int w = inRight - inLeft, h = inBottom - inTop;
	if (w <= 0 || h <= 0 || !BeginPen())
		return;
	double inset = PenWidth() / 2.0;
	double rx = w / 2.0 - inset, ry = h / 2.0 - inset;
	if (rx < 0) rx = 0;
	if (ry < 0) ry = 0;
	if (inClosed) fl_begin_loop(); else fl_begin_line();
	OvalVertices(inLeft + w / 2.0, inTop + h / 2.0, rx, ry, inStart, inArc);
	if (inClosed) fl_end_loop(); else fl_end_line();
	EndPen();
}

void
QDHostStrokeOval(int inLeft, int inTop, int inRight, int inBottom)
{
	StrokeOvalArc(inLeft, inTop, inRight, inBottom, 0.0, 2.0 * kPi, true);
}


void
QDHostFillOval(int inLeft, int inTop, int inRight, int inBottom)
{
	if (inRight > inLeft && inBottom > inTop) {
		SetColor(gState.fill);
		fl_pie(inLeft, inTop, inRight - inLeft, inBottom - inTop, 0.0, 360.0);
	}
}


void
QDHostStrokeArc(int inLeft, int inTop, int inRight, int inBottom, int inStartAngle, int inArcAngle)
{
	// QuickDraw: clockwise from 12 o'clock
	double start = (90.0 - inStartAngle) * kPi / 180.0;
	StrokeOvalArc(inLeft, inTop, inRight, inBottom, start, -inArcAngle * kPi / 180.0, false);
}


void
QDHostFillArc(int inLeft, int inTop, int inRight, int inBottom, int inStartAngle, int inArcAngle)
{
	if (inRight <= inLeft || inBottom <= inTop || inArcAngle == 0)
		return;
	double a1 = 90.0 - inStartAngle - inArcAngle, a2 = 90.0 - inStartAngle;
	if (a1 > a2) {
		double a = a1; a1 = a2; a2 = a;
	}
	SetColor(gState.fill);
	fl_pie(inLeft, inTop, inRight - inLeft, inBottom - inTop, a1, a2);
}


void
QDHostStrokePolyline(const float * inXY, size_t inCount, bool inClosed)
{
	if (inCount < 2 || !BeginPen(FL_CAP_ROUND | FL_JOIN_ROUND))
		return;
	if (inClosed) fl_begin_loop(); else fl_begin_line();
	for (size_t i = 0; i < inCount; ++i)
		fl_vertex(inXY[2*i], inXY[2*i+1]);
	if (inClosed) fl_end_loop(); else fl_end_line();
	EndPen();
}


void
QDHostFillPolygon(const float * inXY, size_t inCount)
{
	if (inCount < 3)
		return;
	SetColor(gState.fill);
	fl_begin_complex_polygon();
	for (size_t i = 0; i < inCount; ++i)
		fl_vertex(inXY[2*i], inXY[2*i+1]);
	fl_end_complex_polygon();
}


/*------------------------------------------------------------------------------
	Images: scaled into the rectangle, each pixel a block (the Newton's
	bitmaps are pixel art). Scaled here, to the surface's pixels (a window on
	a high resolution screen has more than FLTK's units), nearest neighbour:
	drawn 1:1 then, which nothing smooths (Quartz would, scaling).
	inImage: d() bytes a pixel, 1 to 4 (gray, gray and alpha, RGB, RGBA).
------------------------------------------------------------------------------*/

static void
DrawScaled(const Fl_Image * inImage, int inLeft, int inTop, int inRight, int inBottom)
{
	int w = inRight - inLeft, h = inBottom - inTop;
	int srcW = inImage->data_w(), srcH = inImage->data_h(), d = inImage->d();
	if (w <= 0 || h <= 0 || srcW <= 0 || srcH <= 0 || d < 1 || d > 4 || inImage->count() < 1 || inImage->data()[0] == NULL)
		return;
	const uchar * src = (const uchar *)inImage->data()[0];
	int ld = inImage->ld() ? inImage->ld() : srcW * d;
	float scale = fl_graphics_driver->scale();
	int dstW = (int)(w * scale + 0.5), dstH = (int)(h * scale + 0.5);
	if (dstW < 1) dstW = 1;
	if (dstH < 1) dstH = 1;
	std::vector<uchar> rgba((size_t)dstW * dstH * 4);
	uchar * p = rgba.data();
	for (int y = 0; y < dstH; ++y)
	{
		const uchar * row = src + (size_t)(y * srcH / dstH) * ld;
		for (int x = 0; x < dstW; ++x, p += 4)
		{
			const uchar * s = row + (size_t)(x * srcW / dstW) * d;
			switch (d)
			{
			case 1: p[0] = p[1] = p[2] = s[0]; p[3] = 255; break;
			case 2: p[0] = p[1] = p[2] = s[0]; p[3] = s[1]; break;
			case 3: p[0] = s[0]; p[1] = s[1]; p[2] = s[2]; p[3] = 255; break;
			case 4: p[0] = s[0]; p[1] = s[1]; p[2] = s[2]; p[3] = s[3]; break;
			}
		}
	}
	Fl_RGB_Image image(rgba.data(), dstW, dstH, 4);
	image.scale(w, h, 0, 1);
	image.draw(inLeft, inTop);
}


void
QDHostDrawImage(const uint8_t * inRGBA, int inWidth, int inHeight, int inLeft, int inTop, int inRight, int inBottom)
{
	if (inWidth <= 0 || inHeight <= 0)
		return;
	Fl_RGB_Image image(inRGBA, inWidth, inHeight, 4);
	DrawScaled(&image, inLeft, inTop, inRight, inBottom);
}


bool
QDHostDrawPNG(const uint8_t * inData, size_t inSize, int inLeft, int inTop, int inRight, int inBottom)
{
	if (inSize < 8 || memcmp(inData, "\x89PNG", 4) != 0)
		return false;
	Fl_PNG_Image image(NULL, inData, (int)inSize);
	if (image.fail())
		return false;
	DrawScaled(&image, inLeft, inTop, inRight, inBottom);
	return true;
}


/*------------------------------------------------------------------------------
	Text.
------------------------------------------------------------------------------*/

static void
SetFont(const QDHostFont & inFont)
{
	Fl_Font font = (inFont.family == 1) ? FL_TIMES : FL_HELVETICA;	// tsFancy: New York
	if (inFont.face & 1)		// kBoldFace
		font += FL_BOLD;
	if (inFont.face & 2)		// kItalicFace
		font += FL_ITALIC;
	fl_font(font, inFont.size > 0 ? inFont.size : 10);
}


void
QDHostGetFontInfo(const QDHostFont & inFont, QDHostFontInfo * outInfo)
{
	SetFont(inFont);
	outInfo->descent = fl_descent();
	outInfo->ascent = fl_height() - outInfo->descent;
	outInfo->leading = 0;	// in FLTK's height
	outInfo->widMax = (int)(fl_width("W", 1) + 0.5);
}


float
QDHostTextWidth(const char * inUTF8, size_t inLength, const QDHostFont & inFont)
{
	SetFont(inFont);
	return (float)fl_width(inUTF8, (int)inLength);
}


void
QDHostDrawText(const char * inUTF8, size_t inLength, float inX, float inY, const QDHostFont & inFont, QDColor inColor)
{
	SetFont(inFont);
	SetColor(inColor);
	fl_draw(inUTF8, (int)inLength, inX, inY);
}


int
QDHostTextBox(const char * inUTF8, size_t inLength, int inLeft, int inTop, int inRight, int inBottom,
				  const QDHostFont & inFont, QDColor inColor, int inJustifyH, bool inDraw)
{
	std::string text(inUTF8, inLength);	// fl_measure, fl_draw: a C string
	SetFont(inFont);
	int w = inRight - inLeft, h = 0;
	fl_measure(text.c_str(), w, h, 0);
	if (inDraw) {
		Fl_Align align = FL_ALIGN_TOP | FL_ALIGN_INSIDE | FL_ALIGN_WRAP;
		align |= (inJustifyH == 1) ? 0 : ((inJustifyH == 2) ? FL_ALIGN_RIGHT : FL_ALIGN_LEFT);
		SetColor(inColor);
		fl_draw(text.c_str(), inLeft, inTop, inRight - inLeft, (inBottom > inTop + h) ? inBottom - inTop : h, align, NULL, 0);
	}
	return h;
}


#else	/* no FLTK: nothing is drawn */

/*------------------------------------------------------------------------------
	Text without FLTK: an estimate (a character 0.6 of the size wide, a line
	1.2 of it high), so that measuring gives something plausible.
------------------------------------------------------------------------------*/

static size_t
CountCodePoints(const char * inUTF8, size_t inLength)
{
	size_t count = 0;
	for (size_t i = 0; i < inLength; ++i)
		if ((inUTF8[i] & 0xC0) != 0x80)
			++count;
	return count;
}


bool		QDHostCanDraw(void) { return false; }
void		QDHostStrokeLine(float, float, float, float) { }
void		QDHostStrokeRect(int, int, int, int) { }
void		QDHostFillRect(int, int, int, int) { }
void		QDHostStrokeRoundRect(int, int, int, int, int, int) { }
void		QDHostFillRoundRect(int, int, int, int, int, int) { }
void		QDHostStrokeOval(int, int, int, int) { }
void		QDHostFillOval(int, int, int, int) { }
void		QDHostStrokeArc(int, int, int, int, int, int) { }
void		QDHostFillArc(int, int, int, int, int, int) { }
void		QDHostStrokePolyline(const float *, size_t, bool) { }
void		QDHostFillPolygon(const float *, size_t) { }
void		QDHostDrawImage(const uint8_t *, int, int, int, int, int, int) { }
bool		QDHostDrawPNG(const uint8_t *, size_t, int, int, int, int) { return false; }
void		QDHostDrawText(const char *, size_t, float, float, const QDHostFont &, QDColor) { }

void
QDHostGetFontInfo(const QDHostFont & inFont, QDHostFontInfo * outInfo)
{
	int size = inFont.size > 0 ? inFont.size : 10;
	outInfo->ascent = (size * 4 + 2) / 5;
	outInfo->descent = size - outInfo->ascent;
	outInfo->leading = (size + 4) / 5;
	outInfo->widMax = size;
}

float
QDHostTextWidth(const char * inUTF8, size_t inLength, const QDHostFont & inFont)
{
	int size = inFont.size > 0 ? inFont.size : 10;
	return 0.6f * size * CountCodePoints(inUTF8, inLength);
}

int
QDHostTextBox(const char * inUTF8, size_t inLength, int inLeft, int inTop, int inRight, int inBottom,
				  const QDHostFont & inFont, QDColor inColor, int inJustifyH, bool inDraw)
{
	int size = inFont.size > 0 ? inFont.size : 10;
	int boxWidth = inRight - inLeft;
	float width = QDHostTextWidth(inUTF8, inLength, inFont);
	int lines = (boxWidth > 0) ? (int)ceil(width / boxWidth) : 1;
	return (lines > 0 ? lines : 1) * (size * 6 + 4) / 5;
}

#endif
