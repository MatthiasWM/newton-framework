/*
	File:		newton_ncx_test.cc

	Contains:	A test of the newton_ncx library (what NCX uses of the framework):
					it links only newton_ncx, initializes the object system with
					InitObjectSystemCore(), and exercises the framework functions
					the old NCX called: objects, strings, dates, iterators, lists,
					exceptions, printing, the internal store, NSOF both ways, a
					package; and drawing (shapes, bitmaps, a screenshot's bits,
					text): with FLTK (NEWTON_QD_FLTK) into an image whose pixels
					are checked, without it the same calls, which draw nothing.
					Large binaries from NSOF (B69, B70). (Not yet: making
					compressed large binaries, B55.)
					Built as C++17, as NCX is. Prints "ok <what>" per check.

	Usage:	newton_ncx_test <package file>
*/

#if NEWTON_QD_FLTK
// FLTK first: the framework's headers #define names FLTK uses
#include <FL/Fl.H>
#include <FL/Fl_Image_Surface.H>
#include <FL/fl_draw.H>
#endif

#include "Objects.h"
#include "Iterators.h"
#include "RSSymbols.h"
#include "Unicode.h"
#include "NewtonScript.h"
#include "OSErrors.h"
#include "Pipes.h"
#include "NewtonPackage.h"
#include "UtilityClasses/List.h"
#include "UtilityClasses/ListIterator.h"
#include "DrawShape.h"
#include "QDHost.h"
#include "ViewFlags.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

extern "C" {
void	InitObjectSystemCore(void);
void	PrintObject(Ref obj, int indent);
int	REPprintf(const char * inFormat, ...);
void	REPflush(void);
Ref	FDate(RefArg inRcvr, RefArg inMinsSince1904);
Ref	FLBAllocCompressed(RefArg inStoreWrapper, RefArg inClass, RefArg inLength, RefArg inCompanderName, RefArg inCompanderParms);
Ref	ArrayInsert(RefArg ioArray, RefArg inObj, long index);
Ref	FFindStringInArray(RefArg inRcvr, RefArg inArray, RefArg inStr);
Ref	FStrEqual(RefArg inRcvr, RefArg inStr1, RefArg inStr2);
bool	IsWhiteSpace(UniChar c);
}
void	FlattenRef(RefArg inRef, CPipe & inPipe);
long	FlattenRefSize(RefArg inRef);
Ref	UnflattenRef(CPipe & inPipe);
Ref	GetStores(void);
Ref	SPrintObject(RefArg inObj);
Ref	MakeStringOfLength(const UniChar * str, ArrayIndex numChars);
bool	StrEmpty(RefArg inStr);
DeclareException(exBadArgs, exRootException);	// (Frames/Exceptions.cc)

static int gFailed = 0;

static void
Check(bool inOK, const char * inWhat)
{
	printf("%s %s\n", inOK ? "ok     " : "FAIL   ", inWhat);
	if (!inOK)
		++gFailed;
}

static Ref
RoundTrip(RefArg inObj, long * outSize)
{
	long size = FlattenRefSize(inObj);
	std::vector<char> buf(size);
	CPtrPipe out;
	out.init(buf.data(), size, false, NULL);
	FlattenRef(inObj, out);
	CPtrPipe in;
	in.init(buf.data(), size, false, NULL);
	*outSize = size;
	return UnflattenRef(in);
}

/*------------------------------------------------------------------------------
	D r a w i n g
------------------------------------------------------------------------------*/

static Ref
RectShape(RefArg inClass, short inLeft, short inTop, short inRight, short inBottom)
{
	RefVar shape(AllocateBinary(inClass, sizeof(Rect)));
	Rect * r = (Rect *)BinaryData(shape);	// shapes are native once read (MungeBounds)
	r->left = inLeft; r->top = inTop; r->right = inRight; r->bottom = inBottom;
	return shape;
}

static Ref
MakeStyle(RefArg inSlot1, RefArg inValue1, RefArg inSlot2 = RA(NILREF), RefArg inValue2 = RA(NILREF))
{
	RefVar style(AllocateFrame());
	SetFrameSlot(style, inSlot1, inValue1);
	if (NOTNIL(inSlot2))
		SetFrameSlot(style, inSlot2, inValue2);
	return style;
}

static void
Draw(void)
{
	Point zero = { 0, 0 };
	// a screenshot's bits (at 0, 0): 4 bits deep, 0 white, 15 black
	const char bits[] = { 0x0F, (char)0xF0, 0x0F, (char)0xF0 };
	DrawBits(bits, 2, 4, 2, 4);
	// a black rectangle, a gray oval, a frame 2 wide, a line
	DrawShape(RectShape(SYMA(rectangle), 10, 10, 30, 30), MakeStyle(SYMA(fillPattern), MAKEINT(vfBlack)), zero);
	DrawShape(RectShape(SYMA(oval), 40, 10, 60, 30), MakeStyle(SYMA(fillPattern), MAKEINT(vfGray), SYMA(penSize), MAKEINT(0)), zero);
	DrawShape(RectShape(SYMA(rectangle), 70, 10, 90, 30), MakeStyle(SYMA(penSize), MAKEINT(2)), zero);
	DrawShape(RectShape(SYMA(line), 100, 10, 100, 30), RA(NILREF), zero);	// its Rect: y1, x1, y2, x2
	// an icon: 8 by 8, 1 bit, its left half black; 1:1 and scaled 2:1
	const unsigned char header[] = { 0,0,0,0, 0,2, 0,0, 0,0, 0,0, 0,8, 0,8 };	// big-endian: rowBytes 2, bounds 0,0,8,8
	RefVar iconBits(AllocateBinary(SYMA(bits), sizeof(header) + 16));
	memcpy(BinaryData(iconBits), header, sizeof(header));
	for (int i = 0; i < 8; ++i) {
		((unsigned char *)BinaryData(iconBits))[sizeof(header) + 2*i] = 0xF0;
		((unsigned char *)BinaryData(iconBits))[sizeof(header) + 2*i + 1] = 0x00;
	}
	RefVar icon(AllocateFrame());
	SetFrameSlot(icon, SYMA(bits), iconBits);
	Rect r;
	r.left = 10; r.top = 40; r.right = 18; r.bottom = 48;
	DrawBitmap(icon, &r, modeCopy);
	r.left = 30; r.top = 40; r.right = 46; r.bottom = 56;
	DrawBitmap(icon, &r, modeCopy);
	// NCX's ink polygon: 0, the number of points, the points (v, h); big-endian
	const unsigned char poly[] = { 0,0, 0,3, 0,60, 0,10, 0,60, 0,30, 0,80, 0,30 };
	DrawPolygon((char *)poly, 100, 0);
	// text: a line on its baseline (the bounds' bottom), a box wrapped from its top
	RefVar text(AllocateFrame());
	SetClass(text, SYMA(text));
	SetFrameSlot(text, SYMA(bounds), RectShape(SYMA(boundsRect), 10, 70, 100, 90));
	SetFrameSlot(text, SYMA(data), MakeStringFromCString("Hello NCX"));
	DrawShape(text, MakeStyle(SYMA(font), MAKEINT(tsSimple + tsSize(12))), zero);
}

static void
TestDrawing(void)
{
	RefVar rect(RectShape(SYMA(rectangle), 10, 20, 30, 40));
	Rect bounds;
	ShapeBounds(rect, &bounds);
	Check(bounds.left == 10 && bounds.top == 20 && bounds.right == 30 && bounds.bottom == 40, "ShapeBounds");
	Rect a = { 0, 0, 10, 10 }, b = { 5, 5, 20, 30 }, u;	// top, left, bottom, right
	UnionRect(&a, &b, &u);
	Check(u.top == 0 && u.left == 0 && u.bottom == 20 && u.right == 30, "UnionRect");

#if NEWTON_QD_FLTK
	const int W = 160, H = 120;
	Fl_Image_Surface surface(W, H);
	Fl_Surface_Device::push_current(&surface);
	fl_color(FL_WHITE);
	fl_rectf(0, 0, W, H);
	Draw();
	Fl_RGB_Image * image = surface.image();
	Fl_Surface_Device::pop_current();
	Check(QDHostCanDraw() && image != NULL && image->data_w() == W && image->data_h() == H, "QDHost: FLTK, an image surface");
	if (image == NULL)
		return;
	const unsigned char * px = (const unsigned char *)image->data()[0];
	int d = image->d(), ld = image->ld() ? image->ld() : W * d;
	auto gray = [&](int x, int y) { return (int)px[y * ld + x * d]; };
	auto dark = [&](int x, int y) { return gray(x, y) < 64; };
	auto white = [&](int x, int y) { return gray(x, y) > 224; };
	auto anyDark = [&](int l, int t, int r, int b) {
		for (int y = t; y < b; ++y) for (int x = l; x < r; ++x) if (dark(x, y)) return true;
		return false; };
	Check(white(0, 0) && dark(1, 0) && dark(2, 1) && white(3, 1), "DrawBits (a screenshot's 4-bit pixels)");
	Check(dark(10, 10) && dark(20, 20) && dark(29, 29) && white(30, 20) && white(20, 30) && white(9, 20), "DrawShape: a filled rectangle, QuickDraw's edges");
	Check(abs(gray(50, 20) - 0x80) < 16 && white(41, 11) && white(59, 29), "DrawShape: an oval, gray (vfGray)");
	Check(dark(70, 20) && dark(71, 20) && white(72, 20) && white(80, 20) && dark(88, 20) && dark(89, 20) && white(90, 20), "DrawShape: a frame, the pen 2 wide inside");
	Check(dark(100, 20), "DrawShape: a line");
	Check(dark(10, 40) && dark(13, 47) && white(14, 40) && white(17, 47) && white(18, 44), "DrawBitmap: 1:1");
	Check(dark(30, 40) && dark(37, 55) && white(38, 40) && white(45, 55), "DrawBitmap: scaled 2:1");
	Check(dark(120, 60) && dark(130, 70) && white(120, 75), "DrawPolygon (NCX's ink)");
	Check(anyDark(10, 74, 100, 90) && !anyDark(10, 91, 100, 100), "DrawShape: text");
	delete image;
#else
	Draw();
	Check(!QDHostCanDraw(), "QDHost: no FLTK, drawing does nothing");
#endif
}


int
main(int argc, char * argv[])
{
	setvbuf(stdout, NULL, _IONBF, 0);	// a crash keeps what was printed
	InitObjectSystemCore();
	Check(true, "InitObjectSystemCore");

/*-- frames, arrays, symbols, classes --*/
	RefVar frame(AllocateFrame());
	SetFrameSlot(frame, SYMA(name), MakeStringFromCString("Matt"));
	SetFrameSlot(frame, MakeSymbol("count"), MAKEINT(42));
	SetFrameSlot(frame, MakeSymbol("ratio"), MakeReal(2.5));
	Check(FrameHasSlot(frame, SYMA(name)) && !FrameHasSlot(frame, MakeSymbol("nothing")), "FrameHasSlot");
	Check(IsFrame(frame) && IsInt(GetFrameSlot(frame, MakeSymbol("count"))) && IsReal(GetFrameSlot(frame, MakeSymbol("ratio"))),
			"IsFrame, IsInt, IsReal");
	Check(CDouble(GetFrameSlot(frame, MakeSymbol("ratio"))) == 2.5, "CDouble");

	RefVar array(MakeArray(0));
	AddArraySlot(array, MakeSymbol("b"));
	ArrayInsert(array, MakeSymbol("a"), 0);
	AddArraySlot(array, MAKECHAR('x'));
	AddArraySlot(array, RA(NILREF));
	ArrayRemoveCount(array, 3, 1);
	Check(IsArray(array) && Length(array) == 3 && EQRef(GetArraySlot(array, 0), MakeSymbol("a")), "AddArraySlot, ArrayInsert, ArrayRemoveCount");
	Check(strcmp(SymbolName(GetArraySlot(array, 1)), "b") == 0 && IsSymbol(GetArraySlot(array, 1)), "SymbolName, IsSymbol");
	SetClass(array, MakeSymbol("letters"));
	Check(EQRef(ClassOf(array), MakeSymbol("letters")), "SetClass, ClassOf");
	SetFrameSlot(frame, MakeSymbol("list"), array);

	RefVar copy(DeepClone(frame));
	SetArraySlot(GetFrameSlot(copy, MakeSymbol("list")), 0, MAKEINT(1));
	Check(EQRef(GetArraySlot(array, 0), MakeSymbol("a")), "DeepClone copies the array");
	RefVar shallow(Clone(frame));
	Check(EQRef(GetFrameSlot(shallow, MakeSymbol("list")), array), "Clone shares the array");

/*-- strings --*/
	const UniChar hello[] = { 'h', 'e', 'l', 'l', 'o', ' ', 'N', 'C', 'X' };
	RefVar str(MakeStringOfLength(hello, 5));
	Check(IsString(str) && Length(str) == 6 * sizeof(UniChar), "MakeStringOfLength");
	Check(NOTNIL(FStrEqual(RA(NILREF), str, MakeStringFromCString("HELLO"))), "FStrEqual (case-insensitive)");
	Check(StrEmpty(MakeStringFromCString("")) && !StrEmpty(str), "StrEmpty");
	Check(IsWhiteSpace(' ') && !IsWhiteSpace('x'), "IsWhiteSpace");
	RefVar names(MakeArray(0));
	AddArraySlot(names, MakeStringFromCString("Names"));
	AddArraySlot(names, MakeStringFromCString("Dates"));
	RefVar found(FFindStringInArray(RA(NILREF), names, MakeStringFromCString("Dates")));
	Check(ISINT(found) && RINT(found) == 1 && ISNIL(FFindStringInArray(RA(NILREF), names, MakeStringFromCString("Soups"))),
			"FFindStringInArray");

/*-- binaries --*/
	RefVar bin(AllocateBinary(SYMA(binary), 4));
	memcpy(BinaryData(bin), "abcd", 4);
	{
		char * p = (char *)LockedBinaryPtr(bin);
		Check(IsBinary(bin) && memcmp(p, "abcd", 4) == 0, "AllocateBinary, LockedBinaryPtr");
		UnlockRefArg(bin);
	}
	{
		CDataPtr data(bin);
		Check(memcmp((char *)data, "abcd", 4) == 0, "CDataPtr");
	}

/*-- dates --*/
	RefVar date(FDate(RA(NILREF), MAKEINT(60 * 24 * 366)));	// 1 Jan 1905 (1904 had 366 days)
	Check(IsFrame(date) && RINT(GetFrameSlot(date, SYMA(year))) == 1905, "FDate");

/*-- iterators, lists --*/
	ArrayIndex slots = 0;
	CObjectIterator iter(frame);
	for ( ; !iter.done(); iter.next())
		++slots;
	Check(slots == 4, "CObjectIterator");
	CList * list = CList::make();
	list->insertAt(0, (void *)1);
	list->insertAt(1, (void *)2);
	CListIterator listIter(list);
	long sum = 0;
	for (void * item = listIter.firstItem(); listIter.more(); item = listIter.nextItem())
		sum += (long)item;
	Check(sum == 3, "CList, CListIterator");
	delete list;

/*-- exceptions --*/
	bool caught = false;
	newton_try
	{
		ThrowErr(exBadArgs, kNSErrNotAFrame);
	}
	newton_catch(exBadArgs)
	{
		caught = true;
	}
	end_try;
	Check(caught, "newton_try, ThrowErr, newton_catch");

/*-- printing --*/
	RefVar text(SPrintObject(MAKEINT(1234)));
	Check(IsString(text), "SPrintObject");
	PrintObject(frame, 0);
	REPprintf("\n");
	REPflush();

/*-- the internal store --*/
	RefVar stores(GetStores());
	Check(IsArray(stores) && Length(stores) > 0, "GetStores: the internal store");
	// Not yet: FLBAllocCompressed (MP3, Works import) needs a store with large
	// objects, which newtc's internal store isn't (B55).
	RefVar samples(AllocateBinary(SYMA(samples), 100000));
	memset(BinaryData(samples), 0x55, 100000);

/*-- NSOF both ways --*/
	SetFrameSlot(frame, MakeSymbol("data"), bin);
	SetFrameSlot(frame, MakeSymbol("date"), date);
	long size;
	RefVar back(RoundTrip(frame, &size));
	Check(RINT(GetFrameSlot(back, MakeSymbol("count"))) == 42
		&& NOTNIL(FStrEqual(RA(NILREF), GetFrameSlot(back, SYMA(name)), MakeStringFromCString("Matt")))
		&& Length(GetFrameSlot(back, MakeSymbol("list"))) == 3
		&& EQRef(ClassOf(GetFrameSlot(back, MakeSymbol("list"))), MakeSymbol("letters"))
		&& CDouble(GetFrameSlot(back, MakeSymbol("ratio"))) == 2.5
		&& memcmp(BinaryData(GetFrameSlot(back, MakeSymbol("data"))), "abcd", 4) == 0, "FlattenRef, UnflattenRef");
	RefVar backSamples(RoundTrip(samples, &size));
	Check(Length(backSamples) == 100000 && ((unsigned char *)BinaryData(backSamples))[99999] == 0x55, "a 100 KB binary through NSOF");

/*-- large binaries (VBOs) as a Newton sends them, read into plain binaries
     on the host (B70): NSOF kNSLargeBinary (12), the class, compressed?,
     the stream's size, the compander name's and parameters' sizes, 0, the
     name (a T for its C), the stream. A large object (a compander), then a
     package in two compressed chunks (a decompressor, B69). --*/
	{
		std::vector<unsigned char> nsof = { 0x02, 0x0C, 0x07,0x04,'b','l','o','b', 0 };
		const char * compander = "TSimpleStoreCompander";
		const ULong header[] = { 64, (ULong)strlen(compander), 0, 0 };
		for (ULong w : header)
			for (int i = 24; i >= 0; i -= 8)
				nsof.push_back((unsigned char)(w >> i));
		nsof.insert(nsof.end(), compander, compander + strlen(compander));
		for (int i = 0; i < 64; ++i)
			nsof.push_back((unsigned char)(i * 3));
		CPtrPipe in;
		in.init(nsof.data(), nsof.size(), false, NULL);
		RefVar blob(UnflattenRef(in));
		Check(IsBinary(blob) && Length(blob) == 64 && EQRef(ClassOf(blob), MakeSymbol("blob"))
			&& ((unsigned char *)BinaryData(blob))[63] == 189, "a large binary from NSOF, as a binary");
	}
	{
		const size_t pkgSize = 1500;
		std::vector<unsigned char> pkg(pkgSize);
		for (size_t i = 0; i < pkgSize; ++i)
			pkg[i] = (unsigned char)(i % 251);
		memcpy(pkg.data(), "package0", 8);
		std::vector<unsigned char> stream;
		auto put = [](std::vector<unsigned char> & v, ULong w) { for (int i = 24; i >= 0; i -= 8) v.push_back((unsigned char)(w >> i)); };
		put(stream, 1);							// root flags: a package
		put(stream, pkgSize);
		for (size_t offset = 0; offset < pkgSize; offset += 1024)
		{
			size_t count = pkgSize - offset < 1024 ? pkgSize - offset : 1024;
			put(stream, 4 + count);				// a chunk: its relocation header (none), its bytes
			put(stream, 0);
			stream.insert(stream.end(), pkg.begin() + offset, pkg.begin() + offset + count);
		}
		std::vector<unsigned char> nsof = { 0x02, 0x0C, 0x07,0x07,'p','a','c','k','a','g','e', 1 };
		const char * decompressor = "TSimpleStoreDecompressor";
		put(nsof, stream.size());
		put(nsof, strlen(decompressor));
		put(nsof, 0);
		put(nsof, 0);
		nsof.insert(nsof.end(), decompressor, decompressor + strlen(decompressor));
		nsof.insert(nsof.end(), stream.begin(), stream.end());
		CPtrPipe in;
		in.init(nsof.data(), nsof.size(), false, NULL);
		RefVar package;
		newton_try
		{
			package = UnflattenRef(in);
		}
		newton_catch_all
		{
			package = NILREF;
		}
		end_try;
		Check(IsBinary(package) && Length(package) == pkgSize
			&& memcmp(BinaryData(package), pkg.data(), pkgSize) == 0, "a compressed package from NSOF, decompressed");
	}

/*-- the bytes: NSOF is big-endian on every host (a round trip alone
     wouldn't see a host that doesn't swap: it reads what it wrote) --*/
	{
		RefVar golden(MakeArray(0));
		AddArraySlot(golden, MAKEINT(0x1234567));
		AddArraySlot(golden, MakeStringFromCString("AB"));
		AddArraySlot(golden, MakeSymbol("sym"));
		AddArraySlot(golden, MakeReal(1.5));
		const unsigned char expected[] = {
			0x02, 0x05,0x04,										// version 2, plain array of 4
			0x00,0xFF,0x04,0x8D,0x15,0x9C,					// immediate 0x048D159C (Ref of 0x1234567)
			0x08,0x06,0x00,0x41,0x00,0x42,0x00,0x00,		// string "AB": UTF-16BE
			0x07,0x03,0x73,0x79,0x6D,							// symbol 'sym
			0x03,0x08,0x07,0x04,0x72,0x65,0x61,0x6C,		// binary of 8, class 'real
			0x3F,0xF8,0x00,0x00,0x00,0x00,0x00,0x00 };	// 1.5, big-endian
		long len = FlattenRefSize(golden);
		std::vector<unsigned char> bytes(len);
		CPtrPipe out;
		out.init(bytes.data(), len, false, NULL);
		FlattenRef(golden, out);
		Check(len == (long)sizeof(expected) && memcmp(bytes.data(), expected, sizeof(expected)) == 0, "NSOF bytes (big-endian)");
	}

/*-- a package: its parts are big-endian too --*/
	if (argc > 1)
	{
		NewtonPackage pkg(argv[1]);
		RefVar part(pkg.partRef(0));
		// NTKFunctions' part: {externalFunctions: {..., DefPureFn: 'FDefPureFn, ...}, ...}
		RefVar fns(IsFrame(part) ? GetFrameSlot(part, MakeSymbol("externalFunctions")) : (Ref)NILREF);
		RefVar fn(IsFrame(fns) ? GetFrameSlot(fns, MakeSymbol("DefPureFn")) : (Ref)NILREF);
		Check(pkg.directory() != NULL && IsSymbol(fn) && strcmp(SymbolName(fn), "FDefPureFn") == 0,
				"NewtonPackage (NTKFunctions: externalFunctions.DefPureFn = 'FDefPureFn)");
	}

/*-- drawing --*/
	TestDrawing();

	printf("%s\n", gFailed ? "FAILED" : "all ok");
	return gFailed ? 1 : 0;
}
