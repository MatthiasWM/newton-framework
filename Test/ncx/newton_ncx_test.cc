/*
	File:		newton_ncx_test.cc

	Contains:	A test of the newton_ncx library (what NCX uses of the framework):
					it links only newton_ncx, initializes the object system with
					InitObjectSystemCore(), and exercises the framework functions
					the old NCX called, outside drawing: objects, strings, dates,
					iterators, lists, exceptions, printing, the internal store,
					NSOF both ways, a package. (Not yet: compressed large binaries,
					B55.)
					Built as C++17, as NCX is. Prints "ok <what>" per check.

	Usage:	newton_ncx_test <package file>
*/

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

#include <cstdio>
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

int
main(int argc, char * argv[])
{
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

/*-- a package --*/
	if (argc > 1)
	{
		NewtonPackage pkg(argv[1]);
		RefVar part(pkg.partRef(0));
		Check(pkg.directory() != NULL && NOTNIL(part), "NewtonPackage");
	}

	printf("%s\n", gFailed ? "FAILED" : "all ok");
	return gFailed ? 1 : 0;
}
