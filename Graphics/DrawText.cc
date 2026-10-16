/*
	File:		DrawText.cc

	Contains:	Text drawing implementation.

	Written by:	Newton Research Group, 2007.
*/
#if 0
// text drawing function hierarchy

DrawRichString(CRichString & inStr, ULong inStart, size_t inLength, StyleRecord * inStyle, FPoint inPt, TextOptions * inOptions, TextBoundsInfo * outBoundsInfo)
->	DoRichString(inStr, inStart, inLength, inStyle, inPt, inOptions, outBoundsInfo, true);

MeasureRichString(CRichString & inStr, ULong inStart, size_t inLength, StyleRecord * inStyle, FPoint inPt, TextOptions * inOptions, TextBoundsInfo * outBoundsInfo)
->	DoRichString(inStr, inStart, inLength, inStyle, inPt, inOptions, outBoundsInfo, false);

	DoRichString(CRichString & inStr, ULong inStart, size_t inLength, StyleRecord * inStyle, FPoint inPt, TextOptions * inOptions, TextBoundsInfo * outBoundsInfo, bool inDoDraw)
	->	DoTextOnce(theText, inLength, stylePtrs, runLengths, inPt, inOptions, outBoundsInfo, inDoDraw);


DrawTextOnce(void * inText, size_t inLength, StyleRecord ** inStyles, short * inRuns, FPoint inPt, TextOptions * inOptions, TextBoundsInfo * outBoundsInfo)
->	DoTextOnce(inText, inLength, inStyles, inRuns, inPt, inOptions, outBoundsInfo, true);

MeasureTextOnce(void * inText, size_t inLength, StyleRecord ** inStyles, short * inRuns, FPoint inPt, TextOptions * inOptions, TextBoundsInfo * outBoundsInfo)
->	DoTextOnce(inText, inLength, inStyles, inRuns, inPt, inOptions, outBoundsInfo, false);

	DoTextOnce(void * inText, size_t inLength, StyleRecord ** inStyles, short * inRuns, FPoint inPt, TextOptions * inOptions, TextBoundsInfo * outBoundsInfo, bool inDoDraw)
	->	DrawTextObj
	->	DispatchCalcBounds


TextBox(CRichString & inStr, RefArg inFont, const Rect * inRect, ULong inJustifyH, ULong inJustifyV /*, long inTransferMode*/)
->	DrawSimpleParagraph(inStr, inFont, &box, inJustifyH, true/*, inTransferMode*/);

TextBounds(CRichString & inStr, RefArg inFont, Rect * ioRect, long inJustifyH)
->	DrawSimpleParagraph(inStr, inFont, ioRect, inJustifyH, false/*, 1*/);

	DrawSimpleParagraph(CRichString & inStr, RefArg inFont, Rect * ioRect, ULong inJustifyH, bool inDoDraw /*, long inTransferMode*/)
	uses line break table
	->	DrawSimpleLine(inStr, offset, &txLoc, &styleRecPtr, &txOpts, theBreaks, &lineWd, inDoDraw);

		DrawSimpleLine(CRichString & inStr, ULong inStart, FPoint * inLoc, StyleRecord ** inStyles, TextOptions * inOptions, RefArg inBreakTable, long * outLineWd, bool inDoDraw)
		creates QD TextObj
		->	GetTextObjBounds
		->	DrawTextObj

#endif

#include "QDHost.h"
#include "Geometry.h"
#include "Objects.h"
#include "DrawText.h"
#include "DrawShape.h"
#include "Ink.h"

#include "Globals.h"
#include "ROMResources.h"
#include "Lookup.h"
#include "Iterators.h"
#include "OSErrors.h"
#include "MagicPointers.h"
#include "Unicode.h"
#include "Strings.h"
#include "ViewFlags.h"
#include "Preference.h"
#include "UStringUtils.h"

#include <string>
#include <vector>


void			DrawSimpleParagraph(CRichString & inStr, RefArg inFont, Rect * ioRect, ULong inJustifyH, bool inDoDraw /*, long inTransferMode*/);
ArrayIndex	DoRichString(CRichString & inStr, ULong inStart, size_t inCount, StyleRecord * inStyle, FPoint inPt, TextOptions * inOptions, TextBoundsInfo * inBoundsInfo, bool inDoDraw);
ArrayIndex	DoTextOnce(void * inText, size_t inLength, StyleRecord ** inStyles, short * inRuns, FPoint inPt, TextOptions * inOptions, TextBoundsInfo * outBoundsInfo, bool inDoDraw);
ArrayIndex	MeasureTextOnce(void * inText, size_t inLength, StyleRecord ** inStyles, short * inRuns, FPoint inPt, TextOptions * inOptions, TextBoundsInfo * outBoundsInfo);

extern Ref	GetFontFamilySym(RefArg inFontSpec);
extern ULong	GetFontFace(RefArg inFontSpec);
extern ULong	GetFontSize(RefArg inFontSpec);


#pragma mark -
#pragma mark Host
/*------------------------------------------------------------------------------
	H o s t
	Text for QDHost: UTF-8, and the font as a family number, size, face.
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
	UTF-16 to UTF-8.
	Args:		inStr			the text
				inLength		number of UniChars
				outOffsets	if not NULL: for each UniChar (and the end), its
								offset in the UTF-8
	Return:	the UTF-8
------------------------------------------------------------------------------*/

static std::string
UTF8FromUniChars(const UniChar * inStr, size_t inLength, std::vector<size_t> * outOffsets = NULL)
{
	std::string utf8;
	utf8.reserve(inLength);
	if (outOffsets)
		outOffsets->assign(inLength + 1, 0);
	for (size_t i = 0; i < inLength; ++i)
	{
		if (outOffsets)
			(*outOffsets)[i] = utf8.size();
		ULong c = inStr[i];
		if (c >= 0xD800 && c < 0xDC00 && i + 1 < inLength && inStr[i+1] >= 0xDC00 && inStr[i+1] < 0xE000)
		{
			c = 0x10000 + ((c - 0xD800) << 10) + (inStr[i+1] - 0xDC00);
			++i;
			if (outOffsets)
				(*outOffsets)[i] = utf8.size();
		}
		if (c < 0x80) {
			utf8 += (char)c;
		} else if (c < 0x800) {
			utf8 += (char)(0xC0 | (c >> 6));
			utf8 += (char)(0x80 | (c & 0x3F));
		} else if (c < 0x10000) {
			utf8 += (char)(0xE0 | (c >> 12));
			utf8 += (char)(0x80 | ((c >> 6) & 0x3F));
			utf8 += (char)(0x80 | (c & 0x3F));
		} else {
			utf8 += (char)(0xF0 | (c >> 18));
			utf8 += (char)(0x80 | ((c >> 12) & 0x3F));
			utf8 += (char)(0x80 | ((c >> 6) & 0x3F));
			utf8 += (char)(0x80 | (c & 0x3F));
		}
	}
	if (outOffsets)
		(*outOffsets)[inLength] = utf8.size();
	return utf8;
}


/*------------------------------------------------------------------------------
	The family of a font: a symbol ('newYork, ...), or the font itself (from
	the fonts global, or SYS_fonts) -> tsSystem, tsFancy, tsSimple or
	tsHandwriting.
------------------------------------------------------------------------------*/

static bool
IsFontOf(RefArg inFamily, int inFamilyNum, RefArg inSymbol)
{
	if (ISNIL(inFamily))
		return false;
	if (EQ(inFamily, inSymbol))
		return true;
	RefVar sysFonts(SYS_fonts);
	if (IsArray(sysFonts) && Length(sysFonts) > (ArrayIndex)inFamilyNum && EQ(inFamily, GetArraySlot(sysFonts, inFamilyNum)))
		return true;
	RefVar fonts(GetGlobalVar(SYMA(fonts)));
	return IsFrame(fonts) && EQ(inFamily, GetProtoVariable(fonts, inSymbol));
}

static int
HostFontFamily(RefArg inFamily)
{
	if (IsFontOf(inFamily, tsFancy, SYMA(newYork)))
		return tsFancy;
	if (IsFontOf(inFamily, tsSimple, SYMA(geneva)))
		return tsSimple;
	if (IsFontOf(inFamily, 3, SYMA(handwriting)))
		return 3;
	return tsSystem;
}


static QDHostFont
HostFont(const StyleRecord * inStyle)
{
	QDHostFont font;
	font.family = HostFontFamily(inStyle->fontFamily);
	font.size = (int)(inStyle->fontSize + 0.5);
	font.face = inStyle->fontFace;
	return font;
}


static QDHostFont
HostFont(RefArg inFontSpec)
{
	QDHostFont font;
	RefVar family(GetFontFamilySym(inFontSpec));
	font.family = HostFontFamily(family);
	font.size = GetFontSize(inFontSpec);
	font.face = GetFontFace(inFontSpec);
	return font;
}


#pragma mark -
#pragma mark Styles
/*------------------------------------------------------------------------------
	S t y l e s
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
	Fill out a StyleRecord struct from a font spec Ref.
	Args:		inFontSpec
				outRec
	Return:	--
------------------------------------------------------------------------------*/

void
CreateTextStyleRecord(RefArg inFontSpec, StyleRecord * outRec)
{
	outRec->fontPattern = 0;

	if (ISINT(inFontSpec))
	{
		long  styleInt = RINT(inFontSpec);
		long  familyNum = styleInt & tsFamilyMask;
		outRec->fontFamily = (familyNum < Length(SYS_fonts)) ? GetArraySlot(SYS_fonts, familyNum) : NILREF;
		outRec->fontSize = (styleInt & tsSizeMask) >> tsSizeShift;
		outRec->fontFace = (styleInt & tsFaceMask) >> tsFaceShift;
	}
	else if (IsInkWord(inFontSpec))
	{
		InkWordInfo info;
		GetInkWordInfo(inFontSpec, &info);
		outRec->fontFamily = inFontSpec;
		outRec->fontSize = info.x20;
		outRec->fontFace = info.x10;
	}
	else if (IsFrame(inFontSpec))
	{
		RefVar	family(GetFrameSlot(inFontSpec, SYMA(family)));
		RefVar	fonts(GetGlobalVar(SYMA(fonts)));
		outRec->fontFamily = GetProtoVariable(fonts, family);
		outRec->fontSize = RINT(GetFrameSlot(inFontSpec, SYMA(size)));
		outRec->fontFace = RINT(GetFrameSlot(inFontSpec, SYMA(face)));
		RefVar	color(GetFrameSlot(inFontSpec, SYMA(color)));
		if (NOTNIL(color))
		{
/*			bool				sp00;
			PatternHandle  fontPattern;
			if (GetPattern(color, &sp00, &fontPattern, true))
			{
				outRec->x1C = fontPattern;
				outRec->fontPattern = AddressToRef(fontPattern);
			}
*/		}
	}

	if (ISNIL(outRec->fontFamily))
	{
		RefVar	userFont(GetPreference(SYMA(userFont)));
		if (ISINT(userFont))
		{
			long  familyNum = RINT(userFont) & tsFamilyMask;
			if (familyNum >= Length(SYS_fonts))
				familyNum = tsFancy;
			outRec->fontFamily = GetArraySlot(SYS_fonts, familyNum);
		}
		else  // MUST be a frame
		{
			RefVar	family(GetFrameSlot(userFont, SYMA(family)));
			RefVar	fonts(GetGlobalVar(SYMA(fonts)));
			outRec->fontFamily = GetProtoVariable(fonts, family);
		}
	}

	outRec->x10 = 0;
	outRec->x14 = 0;
	outRec->x18 = 0;
}


/*------------------------------------------------------------------------------
	Extract font info from a StyleRecord struct.
	Args:		inStyle
				outFontInfo
	Return:	--
------------------------------------------------------------------------------*/
struct FontEngineInfo
{
	long		x00;
	long		x04;
	long		x08;

	float		x3C;
	float		x40;

//				x80;		// has a desctructor

	RefStruct	xC0;
// size+C4
};

void
GetStyleFontInfo(StyleRecord * inStyle, FontInfo * outFontInfo)
{
#if defined(correct)
	FontEngineInfo info;
	OpenFont(GetCurrentPort()->portBits, inStyle, 1.0, 1.0, &info);
	if (info.x40 != 1.0)
	{
		float r5 = info.x00 * info.x40;
		float r6 = info.x04 * info.x40;
		float r0 = info.x08 * info.x40;
		outFontInfo->ascent = r5 + 0.5;
		outFontInfo->descent = r6 + 0.5;
		outFontInfo->leading = r0 + 0.5;
	}
	else
	{
		outFontInfo->ascent = info.x00;
		outFontInfo->descent = info.x04;
		outFontInfo->leading = info.x08;
	}
	if (info.x3C != 1.0)
		outFontInfo->widMax = info.x00 * info.x3C + 0.5;
	else
		outFontInfo->widMax = info.x0C;

#else
	QDHostFontInfo info;
	QDHostGetFontInfo(HostFont(inStyle), &info);
	outFontInfo->ascent = info.ascent;
	outFontInfo->descent = info.descent;
	outFontInfo->leading = info.leading;
	outFontInfo->widMax = info.widMax;
#endif
}


/*------------------------------------------------------------------------------
	Convert view justification flags to flush|justification values.
	Args:		inViewJustify
				outJustification	0.0 => flush
										1.0 => full
	Return:	flushness			0.0 => left
										0.5 => centre
										1.0 => right
------------------------------------------------------------------------------*/

float
ConvertToFlush(ULong inViewJustify, float * outJustification)
{
	*outJustification = 0.0;
	switch(inViewJustify & vjHMask)
	{
	case vjFullH:
		*outJustification = 1.0;
	case vjLeftH:
		return 0.0;
	case vjRightH:
		return 1.0;
	case vjCenterH:
		return 0.5;
	}
	return 0.0;		// keeps the compiiler quiet
}


#pragma mark -
#pragma mark Drawing
/*------------------------------------------------------------------------------
	D r a w i n g
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
	Draw a string of text.
	Args:		inText				array of UniChar
				inLength				number of UniChars to render
				inStyles				array of pointers to style records
				inRuns				array of run lengths matching styles
				inPt					location of text
				inOptions
				outBoundsInfo
	Return:	--
------------------------------------------------------------------------------*/

void
DrawTextOnce(void * inText, size_t inLength, StyleRecord ** inStyles, short * inRuns, FPoint inPt, TextOptions * inOptions, TextBoundsInfo * outBoundsInfo)
{
	DoTextOnce(inText, inLength, inStyles, inRuns, inPt, inOptions, outBoundsInfo, true);
}


ArrayIndex
MeasureOnce(UniChar * inText, size_t inLength, StyleRecord * inStyle)
{
	TextBoundsInfo txBounds;
	MeasureTextOnce(inText, inLength, &inStyle, NULL, gZeroFPoint, NULL, &txBounds);
	return txBounds.width;	// float -> ArrayIndex
}

ArrayIndex
MeasureTextOnce(void * inText, size_t inLength, StyleRecord ** inStyles, short * inRuns, FPoint inPt, TextOptions * inOptions, TextBoundsInfo * outBoundsInfo)
{
	return DoTextOnce(inText, inLength, inStyles, inRuns, inPt, inOptions, outBoundsInfo, false);
}


/*------------------------------------------------------------------------------
	Draw a rich string of text.
	Args:		inStr					rich string
				inStart				offset to start of text
				inLength				length of text
				inStyle
				inPt					*baseline* of text
				inOptions
				outBoundsInfo
	Return:	--
------------------------------------------------------------------------------*/

void
DrawRichString(CRichString & inStr, ULong inStart, size_t inLength, StyleRecord * inStyle, FPoint inPt, TextOptions * inOptions, TextBoundsInfo * outBoundsInfo)
{
	DoRichString(inStr, inStart, inLength, inStyle, inPt, inOptions, outBoundsInfo, true);
}

ArrayIndex
MeasureRichString(CRichString & inStr, ULong inStart, size_t inLength, StyleRecord * inStyle, FPoint inPt, TextOptions * inOptions, TextBoundsInfo * outBoundsInfo)
{
	return DoRichString(inStr, inStart, inLength, inStyle, inPt, inOptions, outBoundsInfo, false);
}


/*------------------------------------------------------------------------------
	Draw text.
	Args:		inStr					a rich string
				inStart				offset to start of text
				inLength				length of text
				inStyle
				inPt					location of text
				inOptions
				outBoundsInfo
				inDoDraw				false => do measure
	Return:	--
------------------------------------------------------------------------------*/

ArrayIndex
DoRichString(CRichString & inStr, ULong inStart, size_t inLength, StyleRecord * inStyle, FPoint inPt, TextOptions * inOptions, TextBoundsInfo * outBoundsInfo, bool inDoDraw)
{
	ArrayIndex	strLen;
	//sp-14
//	bool sp0C = inDoDraw;
	UniChar *	theText = inStr.grabPtr() + inStart;	// sp08
//	short *	sp04 = NULL;

	if (inStr.format() == 0)
	{
		strLen = DoTextOnce(theText, inLength, &inStyle, NULL, inPt, inOptions, outBoundsInfo, inDoDraw);
	}

	else
	{
		Ptr *				runData = NULL, * runDataPtr;			// r6, r8
		short *			runLengths = NULL;							// r7
		StyleRecord **	stylePtrs, ** stylePtrPtr;		// r5, r10
		StyleRecord *	styleRecs = NULL,  * styleRecPtr;		// sp24r, r4
		//sp-04
		ArrayIndex		numOfInkWords = inStr.numInkWordsInRange(inStart, inLength);		// sp00r
		if (numOfInkWords != 0)
		{
			size_t	numOfRuns = inStr.numInkAndTextRunsInRange(inStart, inLength);	// r9
			runData = new Ptr[numOfRuns];
			runLengths = new short[numOfRuns];
			inStr.getLengthsAndDataInRange(inStart, inLength, runLengths, runData);
			//sp-20
			StyleRecord	theStyle = *inStyle;
			styleRecs/*sp24r*/ = new StyleRecord[numOfRuns];
			stylePtrs = new StyleRecord*[numOfRuns];
			//sp-04
		//	RefVar	sp00r;
			Ptr	dataPtr;
			for (styleRecPtr = styleRecs, stylePtrPtr = stylePtrs, runDataPtr = runData;
				  numOfRuns != 0;
				  numOfRuns--, styleRecPtr++, stylePtrPtr++, runDataPtr++)
			{
				if ((dataPtr = *runDataPtr) != 0)
					styleRecPtr->fontFamily = AddressToRef(dataPtr);
				else
					styleRecPtr->fontFamily = theStyle.fontFamily;
				styleRecPtr->fontSize = theStyle.fontSize;
				styleRecPtr->fontFace = theStyle.fontFace;
				styleRecPtr->fontPattern = theStyle.fontPattern;
				styleRecPtr->x14 = theStyle.x14;
				styleRecPtr->x18 = theStyle.x18;
				*stylePtrPtr = styleRecPtr;
			}
		}
		else
			stylePtrs = &inStyle;
		strLen = DoTextOnce(theText, inLength, stylePtrs, runLengths, inPt, inOptions, outBoundsInfo, inDoDraw);
		if (numOfInkWords != 0)
		{
			delete[] styleRecs;
			delete[] stylePtrs;
			delete[] runLengths;
			delete runData;
		}
	}

	inStr.releasePtr();
	return strLen;
}

/*------------------------------------------------------------------------------
	Draw or measure a line of text (the host's). All of it in the first
	style (ink words' runs: not yet).
	Args:		inText				array of UniChar
				inLength				number of UniChars
				inStyles				array of pointers to style records
				inRuns				array of run lengths matching styles
				inPt					*baseline* of text, its left end
				inOptions			width: the line's (for justification, and what
										fits); justification: 0.0 left, 0.5 centre,
										1.0 right
				outBoundsInfo		width: the text's
				inDoDraw				false => do measure
	Return:	number of UniChars that fit the options' width (all of them if
				there's no width)
------------------------------------------------------------------------------*/

ArrayIndex
DoTextOnce(void * inText, size_t inLength, StyleRecord ** inStyles, short * inRuns, FPoint inPt, TextOptions * inOptions, TextBoundsInfo * outBoundsInfo, bool inDoDraw)
{
	const UniChar * text = (const UniChar *)inText;
	QDHostFont font = HostFont(inStyles[0]);
	std::vector<size_t> offsets;
	std::string utf8 = UTF8FromUniChars(text, inLength, &offsets);
	float width = QDHostTextWidth(utf8.data(), utf8.size(), font);

	if (inDoDraw)
	{
		float x = inPt.x;
		if (inOptions && inOptions->width > width)
			x += (inOptions->width - width) * inOptions->justification;
		QDHostDrawText(utf8.data(), utf8.size(), x, inPt.y, font, kQDBlack);
	}

	if (outBoundsInfo)
		outBoundsInfo->width = width;
//		DispatchCalcBounds(textRef, outBoundsInfo);

	// how much fits: the longest start that isn't wider
	ArrayIndex fit = inLength;
	if (inOptions && inOptions->width > 0 && width > inOptions->width)
	{
		ArrayIndex lo = 0, hi = inLength;
		while (lo < hi)
		{
			ArrayIndex mid = (lo + hi + 1) / 2;
			if (QDHostTextWidth(utf8.data(), offsets[mid], font) <= inOptions->width)
				lo = mid;
			else
				hi = mid - 1;
		}
		fit = lo;
	}
	return fit;
}


/*------------------------------------------------------------------------------
	Draw text justified into a box.
	Args:		inStr					a rich string
				inFont				font frame
				inRect				the box into which to draw
				inJustifyH			justification
				inJustifyV
				inTransferMode		no longer used
	Return:	--
------------------------------------------------------------------------------*/

void
TextBox(CRichString & inStr, RefArg inFont, const Rect * inRect, ULong inJustifyH, ULong inJustifyV /*, long inTransferMode*/)
{
	Rect  box = *inRect;

	if (inJustifyV != vjTopV)
	{
		// calculate actual box size for justification
		box.bottom = box.top;
		DrawSimpleParagraph(inStr, inFont, &box, inJustifyH, false /*, 1*/);
		int  delta = RectGetHeight(*inRect) - RectGetHeight(box);
		box = *inRect;
		if (inJustifyV == vjCenterV)
			OffsetRect(&box, 0, delta/2);
		else if (inJustifyV == vjBottomV)
			OffsetRect(&box, 0, delta);
	}

	DrawSimpleParagraph(inStr, inFont, &box, inJustifyH, true /*, inTransferMode*/);
}


/*------------------------------------------------------------------------------
	Measure text bounds.
	Args:		inStr					a rich string
				inFont				font frame
				inRect				the box into which to draw
				inJustifyH			justification
	Return:	--
------------------------------------------------------------------------------*/

void
TextBounds(CRichString & inStr, RefArg inFont, Rect * ioRect, long inJustifyH)
{
	DrawSimpleParagraph(inStr, inFont, ioRect, inJustifyH, false/*, 1*/);
}


#pragma mark -
#pragma mark Paragraphs
/*------------------------------------------------------------------------------
	Draw or measure text wrapped into a box (the host's).
	Args:		inStr					a rich string
				inFont				font spec
				ioRect				the box; measuring: its bottom is set to the
										text's
				inJustifyH			vjLeftH, vjCenterH, vjRightH
				inDoDraw				false => do measure
	Return:	--
------------------------------------------------------------------------------*/

void
DrawSimpleParagraph(CRichString & inStr, RefArg inFont, Rect * ioRect, ULong inJustifyH, bool inDoDraw /*, long inTransferMode*/)
{
	std::string utf8 = UTF8FromUniChars(inStr.grabPtr(), inStr.length());
	inStr.releasePtr();
	int justify = 0;
	switch (inJustifyH & vjHMask)
	{
	case vjCenterH:	justify = 1; break;
	case vjRightH:		justify = 2; break;
	}
	int height = QDHostTextBox(utf8.data(), utf8.size(), ioRect->left, ioRect->top, ioRect->right, ioRect->bottom,
										HostFont(inFont), kQDBlack, justify, inDoDraw);
	if (!inDoDraw)
		ioRect->bottom = ioRect->top + height;
}


/* -----------------------------------------------------------------------------
	Truncate a rich string to a width, with an ellipsis.
	(Was in Frames/Strings.cc: it measures text, so it is graphics, and the
	object system doesn't need graphics.)
----------------------------------------------------------------------------- */

const UniChar gEllipsis[] = { 0x2026, 0 };

extern "C" Ref
FStyledStrTruncate(RefArg inRcvr, RefArg inStr, RefArg inWidth, RefArg inFont)
{
	CRichString str(inStr);
	StyleRecord style;
	CreateTextStyleRecord(inFont, &style);
	float txWd = RINT(inWidth);
	TextOptions options;
	options.transferMode = modeOr;
	options.width = txWd;
	TextBoundsInfo boundsInfo;
	// find how much of the string fits in the given width
	ArrayIndex strLen = str.length();
	ArrayIndex fitLen = MeasureRichString(str, 0, strLen, &style, gZeroFPoint, &options, &boundsInfo);
	if (fitLen < strLen)
	{
		// doesn't fit -- truncate string and append ellipsis
		// measure width of ellipsis char
		StyleRecord * stylePtr = &style;
		MeasureTextOnce((void *)gEllipsis, 1, &stylePtr, NULL, gZeroFPoint, &options, &boundsInfo);
		// reduce box width by that amount
		options.width = txWd - boundsInfo.width;
		ArrayIndex truncStrEnd = MeasureRichString(str, 0, str.length(), &style, gZeroFPoint, &options, &boundsInfo) + 1;
		// truncate the str to fit the box
		str.deleteRange(truncStrEnd, str.length() - truncStrEnd);
		// append ellipsis
		str.setChar(truncStrEnd-1, gEllipsis[0]);
	}

	return inStr;
}
