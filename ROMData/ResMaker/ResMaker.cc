/*
	File:		ResMaker.cc

	Contains:	Newton object system ROM Ref* builder.
					We use the RSxxxx definitions from the ROM symbol file to read objects from the ROM image
					and recreate their source in C. These NewtonScript objects are then available
					in the C world.
					Presumably originally there would have been a tool to do the same thing from a .pkg file
					created by NTK.

					Useful grep expressions for creating a C function pointer file:
					^([^:\n]*)::.*$\n
					^([^(\n]*)(.*$\n

	Written by:	Newton Research Group, 2015.

	Run in ROMData (see the CMake target ResMaker):
		ResMaker rom.image -s RefStarSource.txt -sym 717006.cfunctions -o forNewton
		ResMaker rom.image -s RefStarSource.txt -via ../Toolkit.exp -via ../Editor.exp -sym 717006.cfunctions -o forNTK
	It writes <output>/ROMObjects.h, ROMSymbols.h, ROMSymbols.c, ROMData.c, and ../Frames/RSData.h,
	../Frames/RSSymbols.h. (Until 2026 it wrote GNU assembler: RefData.s, RefSymbols.s, MagicPointers.s,
	RSData.s, RSSymbols.s; the C holds the same objects, see ROMObjects.h.)


The ROM has:
003af000	gROMMagicPointerTable	(C array of Refs to MPxxxx, preceded by count)
003afda8	gROMSoupData
-- symbol objects (lots of 'em)
003afdc4	SYMcfunction
003afde0	SYM_2B
003afdf4	SYM_2D
003afe08	SYMaref
003afe20	SYMsetaref
..
0063ab38	SYMnoevilliveon
0063ace8	SYMstringeq
-- magic pointer objects (values are pointer Refs not addresses, but these are actual objects)
0063ad05	MP0000
0063ad31	MP0001
..
0064a971	MP0871
0064a9a9	MP0872
0067fa40	gROMSoupDataSize
-- RefStars
0067fa44	Rupbitmap
0067fa48	RSupbitmap
..
00681c94	Rroutemailicon
00681c98	RSroutemailicon
-- symbol RefStars (in alphabetical order)
00681ca0	RSSYM_2A
00681ca8	RSSYM_2B

*/

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <ctype.h>	// tolower

#include <string>
#include <vector>
#include <set>

#include "SymbolTable.h"
#include "Reporting.h"


#include "Objects.h"
#include "ObjHeader.h"
#include "Ref32.h"
#include "UStringUtils.h"
#include "ROMResources.h"

#include "ROMExtension.h"
#include "PackageParts.h"
#include "NewtonPackage.h"
#include "Iterators.h"


/* -----------------------------------------------------------------------------
	B u i l d   O p t i o n s
----------------------------------------------------------------------------- */
#define forROMExtensions 0
#define forMagicPointers 1
#define forRefStarData 1

/* -----------------------------------------------------------------------------
	F u n c t i o n   f r a m e   s l o t   i n d i c e s
----------------------------------------------------------------------------- */

enum
{
	kPlainCFunctionClassIndex,
	kPlainCFunctionPtrIndex,
	kPlainCFunctionNumArgsIndex
};


/* -----------------------------------------------------------------------------
	V a l u e s
	What a Ref slot of a generated object holds.
----------------------------------------------------------------------------- */

struct Value
{
	enum Kind { kNone, kRef, kPointer, kMagicPointer, kFunction };

	Kind			kind = kNone;
	Ref			ref = 0;		// kRef: the Ref (an integer or an immediate); kMagicPointer: its index
	std::string	label;		// kPointer: the object's label; kFunction: the C function's name

	bool			isValid() const { return kind != kNone; }

	static Value	MakeRef(Ref inRef)						{ Value v; v.kind = kRef; v.ref = inRef; return v; }
	static Value	MakePointer(const std::string & inLabel)	{ Value v; v.kind = kPointer; v.label = inLabel; return v; }
	static Value	MakeMagicPointer(Ref inIndex)			{ Value v; v.kind = kMagicPointer; v.ref = inIndex; return v; }
	static Value	MakeFunction(const std::string & inName)	{ Value v; v.kind = kFunction; v.label = inName; return v; }
};


/* -----------------------------------------------------------------------------
	G e n e r a t e d   O b j e c t s
	An object is its Ref-sized words (header, gc, class, slots) and, for a
	binary, its data. Objects go into sections:
		symbols			struct ROMSymbols gROMSymbols (ROMSymbols.c)
		data, magic		struct ROMData gROMData (ROMData.c): the RS objects and those
							they hold, then the magic pointer objects; gROMDataStart
							and gROMDataEnd are its bounds (ISINROM).
	A pointer to an object is resolved by its label: SYM... is in gROMSymbols,
	anything else in gROMData.
----------------------------------------------------------------------------- */

struct CObject
{
	std::string						label;
	size_t							headerCount = 0;	// the first words, on a line of their own
	std::vector<std::string>	words;				// initializers of the Ref-sized words
	std::string						tailType;			// e.g. "uint8_t d[118];", or empty
	std::string						tailInit;			// e.g. "{0x27,0x08}"
};

struct CSection
{
	std::vector<CObject>		objects;
	std::set<std::string>	labels;

	void	add(CObject & inObject);
};

struct RefStar
{
	std::string		name;
	std::string		init;
};

CSection						gSymbolSection;
CSection						gDataSection;
CSection						gMagicSection;
std::vector<RefStar>		gSymbolRefStars;
std::vector<RefStar>		gDataRefStars;
std::vector<std::string>	gMagicTable;
std::set<std::string>	gFunctions;				// C functions the data points to
std::set<std::string>	gPointerLabels;			// every label pointed to, to check they're all defined


/* -----------------------------------------------------------------------------
	F u n c t i o n   P r o t o t y p e s
----------------------------------------------------------------------------- */
extern unsigned B32ObjectFlags(Ref r);
extern Ptr B32BinaryData(Ref r);
extern ArrayIndex B32Length(Ref r);
extern bool B32EQRef(Ref a, Ref b);
extern Ref B32ClassOf(Ref r);
extern bool B32IsSubclass(Ref obj, const char * superName);
extern bool B32IsSymbol(Ref r);
extern const char * B32SymbolName(Ref r);
extern Ref B32GetTag(Ref inMap, ArrayIndex index, ArrayIndex * mapIndex);

extern void	UnRelocatePkg(void * inPkg, void * inBase);
extern "C" void InitObjectSystem(void);

Value PrintROMObject(CSection & inSection, const char * inName, Ref inTag, Ref inObj, bool inDefineObjects, bool inDefineSymbols);
Value PrintPkgObject(CSection & inSection, const char * inName, Ref inObj, bool inDefineObjects, bool inDefineSymbols);


/*------------------------------------------------------------------------------
	D a t a
------------------------------------------------------------------------------*/
#define kMaxExportFileCount 3

const char *	gCmdName;
bool				gDoProgress = false;
bool				gDoDump = false;
bool				gDoHelp = false;
bool				gHasViaOption = false;
bool				gForNTK = false;		// -via given: the data for NTK (its functions, protoProtoEditor, NTKFunctions.newtonpkg)
const char *	gExportFilename[kMaxExportFileCount];
int				gExportFileCount = 0;
const char *	gSourceFilename = NULL;
const char *	gSymbolFilename = NULL;
const char *	gOutputFilename = NULL;
const char *	gImageFilename = NULL;

char * ROMdata;
CSymbolTable * gExportSymbolTable;
CSymbol * gFunctionNames = NULL;
CSymbol * gEditorFunctionNames = NULL;
CPointerTable * gCSymbolTable;


/* -----------------------------------------------------------------------------
	C   s o u r c e   t e m p l a t e s
----------------------------------------------------------------------------- */

const char * kGeneratedBy =
"/*\n"
"	File:		%s\n\n"
"	Contains:	Newton object system ROM Ref* %s.\n"
"					This file is built automatically by ResMaker from the ROM image and RS symbols.\n"
"	Written by:	Newton Research Group, 2016.\n"
"*/\n\n";

// ROMObjects.h: how the generated objects are laid out (the object system's
// ObjHeader: a size|flags word, a gc word, the class; Refs are pointer-sized).
const char * kObjectsHeader =
"#if !defined(__ROMOBJECTS_H)\n"
"#define __ROMOBJECTS_H 1\n\n"
"#include <stdint.h>\n\n"
"/* A Ref-sized word of an object: a Ref (integer, immediate), a pointer Ref\n"
"   (an object's address + 1), or a C function's address. Initialized with\n"
"   address constants, so the data needs no code to set it up. */\n"
"typedef union ROMRef\n"
"{\n"
"	intptr_t		i;\n"
"	char *		p;\n"
"	void			(*f)(void);\n"
"} ROMRef;\n\n"
"#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L\n"
"_Static_assert(sizeof(ROMRef) == sizeof(void *), \"a Ref is as wide as a pointer\");\n"
"#endif\n\n"
"#define kROMRefSize		((intptr_t)sizeof(ROMRef))\n"
"#define kROMHeaderSize	(3 * kROMRefSize)\n"
"#define kROMFlagsBinary	((intptr_t)0x40 << 24)\n"
"#define kROMFlagsArray	((intptr_t)0x41 << 24)\n"
"#define kROMFlagsFrame	((intptr_t)0x43 << 24)\n\n"
"/* the size|flags word of an object */\n"
"#define NR_BINARY(_len)		{ .i = kROMHeaderSize + (_len) + kROMFlagsBinary }\n"
"#define NR_ARRAY(_len)		{ .i = kROMHeaderSize + (_len) * kROMRefSize + kROMFlagsArray }\n"
"#define NR_FRAMEMAP(_len)	{ .i = kROMHeaderSize + ((_len) + 1) * kROMRefSize + kROMFlagsArray }\n"
"#define NR_FRAME(_len)		{ .i = kROMHeaderSize + (_len) * kROMRefSize + kROMFlagsFrame }\n\n"
"/* Refs */\n"
"#define NR_0					{ .i = 0 }\n"
"#define NR_NIL				{ .i = 2 }\n"
"#define NR_REF(_r)			{ .i = (_r) }\n"
"#define NR_MAGIC(_n)		{ .i = ((intptr_t)(_n) << 2) + 3 }\n"
"#define NR_SYM(_label)		{ .p = (char *)&gROMSymbols._label + 1 }\n"
"#define NR_OBJ(_label)		{ .p = (char *)&gROMData._label + 1 }\n"
"#define NR_FUNC(_name)		{ .f = _name }\n\n"
"#endif	/* __ROMOBJECTS_H */\n";


/* -----------------------------------------------------------------------------
	String/symbol utilities.
----------------------------------------------------------------------------- */

// Ustrlen (Utilities/UStringUtils) and symcmp (Frames/Symbols.cc) are the framework's.

void
MakeFourCharStr(ULong inVal, char * outStr)
{
	sprintf(outStr, "%c%c%c%c", inVal>>24, inVal>>16, inVal>>8, inVal);
}


std::string
Format(const char * inFormat, ...)
{
	char str[512];
	va_list args;
	va_start(args, inFormat);
	vsnprintf(str, sizeof(str), inFormat, args);
	va_end(args);
	return str;
}


#pragma mark -

const char *
MungeSymbol(const char * inSymbol)
{
	static char symName[128];
	char * symp = symName;
	for (const char * s = inSymbol; *s != 0; ++s) {
		UChar ch = *s;
		if (isalpha(ch) || isdigit(ch) || ch == '_') {
			*symp++ = ch;
		} else {
			sprintf(symp, "_%02X", ch);
			symp += 3;
		}
	}
	*symp = 0;
	return symName;
}


/* -----------------------------------------------------------------------------
	C initializers.
----------------------------------------------------------------------------- */

std::string
CInit(const Value & inValue)
{
	switch (inValue.kind) {
	case Value::kRef:
		if (inValue.ref == NILREF)
			return "NR_NIL";
		if (inValue.ref == 0)
			return "NR_0";
		if (inValue.ref < 0)	// (cast first: -0x80000004 alone is an unsigned int)
			return Format("NR_REF(-(intptr_t)0x%llX)", (unsigned long long)-(long long)inValue.ref);
		return Format("NR_REF(0x%llX)", (unsigned long long)inValue.ref);
	case Value::kPointer:
		gPointerLabels.insert(inValue.label);
		if (inValue.label.compare(0, 3, "SYM") == 0)
			return "NR_SYM(" + inValue.label + ")";
		return "NR_OBJ(" + inValue.label + ")";
	case Value::kMagicPointer:
		return Format("NR_MAGIC(%ld)", (long)inValue.ref);
	case Value::kFunction:
		gFunctions.insert(inValue.label);
		return "NR_FUNC(" + inValue.label + ")";
	case Value::kNone:
		break;
	}
	ExitWithMessage("an object slot without a value");
	return "";
}


void
CSection::add(CObject & inObject)
{
	if (!labels.insert(inObject.label).second)
		ExitWithMessage("object %s defined twice", inObject.label.c_str());
	objects.push_back(inObject);
}


/* -----------------------------------------------------------------------------
	Start an object: its header words.
----------------------------------------------------------------------------- */

CObject
MakeObject(const std::string & inLabel, std::initializer_list<std::string> inHeader)
{
	CObject obj;
	obj.label = inLabel;
	obj.words = inHeader;
	obj.headerCount = obj.words.size();
	return obj;
}


/* -----------------------------------------------------------------------------
	A C string literal.
----------------------------------------------------------------------------- */

std::string
CString(const char * inStr)
{
	std::string s = "\"";
	for (const UChar * p = (const UChar *)inStr; *p != 0; ++p) {
		UChar ch = *p;
		if (ch == '\\' || ch == '"') {
			s += '\\';
			s += (char)ch;
		} else if (ch >= 0x20 && ch <= 0x7E && ch != '?') {	// no '?': trigraphs
			s += (char)ch;
		} else {
			s += Format("\\%03o", ch);
		}
	}
	return s + "\"";
}


/* -----------------------------------------------------------------------------
	A plain C function: look up its name from its funcPtr.
----------------------------------------------------------------------------- */

Value
PlainCFuncName(Ref32 inPtr, Ref inTag)
{
	CPointer * symbol = gCSymbolTable->find(inPtr);
	if (symbol != NULL) {
		return Value::MakeFunction(symbol->fName);
	}
	printf("#%08x not found\n", inPtr);
	return Value::MakeFunction("FNullCFunc");
}


/* -----------------------------------------------------------------------------
	Print source of a symbol object.
----------------------------------------------------------------------------- */

ULong
SymbolHashFunction(const char * inName)
{
	char ch;
	ULong sum = 0;

	while ((ch = *inName++) != 0)
		sum += toupper(ch);

	return sum * 0x9E3779B9;	// 2654435769
}


void
PrintSymbol(CSection & inSection, const char * inName, const char * inSymbol)
{
	size_t len = strlen(inSymbol) + 1;
	CObject obj = MakeObject(std::string("SYM") + inName, { Format("NR_BINARY(4 + %zu)", len), "NR_0", CInit(Value::MakeRef(kSymbolClass)) });
	obj.tailType = Format("uint32_t hash; char name[%zu];", len);
	obj.tailInit = Format("0x%08X, ", SymbolHashFunction(inSymbol)) + CString(inSymbol);
	inSection.add(obj);
}


/* -----------------------------------------------------------------------------
	Print source of a string object.
	Assume UniChars are in host byte order.
----------------------------------------------------------------------------- */

void
PrintString(CSection & inSection, const char * inName, const Value & inClass, const UniChar * inString)
{
	ArrayIndex len = Ustrlen(inString);
	CObject obj = MakeObject(inName, { Format("NR_BINARY(%d)", (len+1)<<1), "NR_0", CInit(inClass) });
	obj.tailType = Format("uint16_t d[%d];", len+1);
	obj.tailInit = "{";
	for (ArrayIndex i = 0; i < len; ++i) {
		UniChar ch = inString[i];
		if (isalnum(ch) || (ch >= 0x20 && ch <= 0x7E && strchr(" !#$%&()*+,-./:;<=>@[]^_`{|}~", ch) != NULL)) {
			obj.tailInit += Format("'%c',", ch);
		} else {
			obj.tailInit += Format("0x%04X,", ch);
		}
	}
	obj.tailInit += "0}";
	inSection.add(obj);
}


/* -----------------------------------------------------------------------------
	Print the data of a binary object.
----------------------------------------------------------------------------- */

void
SetBytes(CObject & ioObj, const UChar * inData, ArrayIndex inLength)
{
	if (inLength == 0)
		return;
	ioObj.tailType = Format("uint8_t d[%d];", inLength);
	ioObj.tailInit = "{";
	for (ArrayIndex i = 0; i < inLength; ++i) {
		if (i > 0 && (i % 16) == 0)
			ioObj.tailInit += "\n\t\t";
		ioObj.tailInit += Format("0x%02X%s", inData[i], (i < inLength-1) ? "," : "");
	}
	ioObj.tailInit += "}";
}


/* -----------------------------------------------------------------------------
	Print protoProtoEditor frame.
----------------------------------------------------------------------------- */

void
PrintProtoProtoEditor(CSection & inSection)
{
	const char * name = "protoProtoEditor";
	ArrayIndex numOfFns = 0;
	for (CSymbol * fn = gEditorFunctionNames; fn != NULL; fn = fn->fNext) {
		++numOfFns;
	}
	CObject map = MakeObject(std::string(name) + "_map", { Format("NR_FRAMEMAP(%d)", numOfFns), "NR_0" });
	map.words.push_back(CInit(Value::MakeRef(0)));
	map.words.push_back(CInit(Value::MakeRef(NILREF)));
	for (CSymbol * fn = gEditorFunctionNames; fn != NULL; fn = fn->fNext) {
		map.words.push_back(CInit(Value::MakePointer(std::string("SYM") + fn->fName)));
	}
	inSection.add(map);

	CObject frame = MakeObject(name, { Format("NR_FRAME(%d)", numOfFns), "NR_0", CInit(Value::MakePointer(map.label)) });
	for (CSymbol * fn = gEditorFunctionNames; fn != NULL; fn = fn->fNext) {
		frame.words.push_back(CInit(Value::MakePointer(std::string("obj_") + fn->fName)));
	}
	inSection.add(frame);
}


/* -----------------------------------------------------------------------------
	Map a pointer Ref from offset to address within ROMdata block.
----------------------------------------------------------------------------- */

Ref
FixPointerRef(Ref r)
{
	if (ISREALPTR(r)) {
		return (Ref)ROMdata + r;
	} else if (ISINT(r)) {
		// handle negative Refs
		long n = RVALUE(r);
		if (n >= INT32_MAX>>2) {
			r = MAKEINT(n - (UINT32_MAX>>2) - 1);
		}
	}
	return r;
}


/* -----------------------------------------------------------------------------
	Print a frame map.
----------------------------------------------------------------------------- */

ArrayIndex
PrintFrameMap(CSection & inSection, const char * inName, Ref inMap)
{
	char nameStr[64];
	FrameMapObject32 * frMap = (FrameMapObject32 *)(inMap-1);
	ArrayIndex numOfSlots = ARRAY32LENGTH(frMap);	// includes supermap
	Ref supermapRef = CANONICAL_LONG(frMap->supermap);

	if NOTNIL(supermapRef) {
		Ref map = FixPointerRef(supermapRef);
		if (ISREALPTR(map)) {
			ObjHeader32 * elem = (ObjHeader32 *)(map-1);
			if (!FLAGTEST(elem->flags, kObjMarked)) {
				FLAGSET(elem->flags, kObjMarked);
				sprintf(nameStr, "obj_%08X", (ULong)supermapRef);
				PrintFrameMap(inSection, nameStr, map);
			}
		}
	}
	ArrayIndex fnCount = 0;
	if (!gForNTK) {
		// no external functions, no protoProtoEditor
	} else if (strcmp(inName, "gFunky_map") == 0) {
		// count external funtions
		for (CSymbol * fn = gFunctionNames; fn != NULL; fn = fn->fNext) {		// gExternalFunctionNames
			++fnCount;
		}
	} else if (strcmp(inName, "MP0546_map") == 0) {
		// count protoProtoEditor
		++fnCount;
		// and generate its frame
		PrintProtoProtoEditor(inSection);
	}
	CObject obj = MakeObject(inName, { Format("NR_FRAMEMAP(%d)", numOfSlots+fnCount-1), "NR_0" });	// exclude supermap
	Ref32 * p = &frMap->objClass;
	for (ArrayIndex slotsRemaining = numOfSlots+1; slotsRemaining > 0; --slotsRemaining, ++p) {
		Ref tag = FixPointerRef(CANONICAL_LONG(*p));
		sprintf(nameStr, "obj_%08X", (ULong)CANONICAL_LONG(*p));
		if (fnCount > 0 && ISINT(tag)) {
			FLAGCLEAR(tag, kMapSorted);
		}
		obj.words.push_back(CInit(PrintROMObject(inSection, nameStr, NILREF, tag, false, false)));
	}
	// append external functions
	if (fnCount > 0) {
		if (strcmp(inName, "gFunky_map") == 0) {
			for (CSymbol * fn = gFunctionNames; fn != NULL; fn = fn->fNext) {	// gExternalFunctionNames
				obj.words.push_back(CInit(Value::MakePointer(std::string("SYM") + fn->fName)));
			}
		} else /*if (strcmp(inName, "MP0546_map") == 0)*/ {
			// append protoProtoEditor
			obj.words.push_back(CInit(Value::MakePointer("SYMprotoProtoEditor")));
		}
	}
	inSection.add(obj);
	return fnCount;
}

void
PrintPkgFrameMap(CSection & inSection, const char * inName, Ref inMap)
{
	char nameStr[64];
	FrameMapObject * frMap = (FrameMapObject *)(inMap-1);
	ArrayIndex numOfSlots = ARRAYLENGTH(frMap);	// includes supermap
	Ref supermapRef = frMap->supermap;

	if NOTNIL(supermapRef) {
		Ref map = supermapRef;
		if (ISREALPTR(map)) {
			ObjHeader * elem = (ObjHeader *)(map-1);
			if (!FLAGTEST(elem->flags, kObjMarked)) {
				FLAGSET(elem->flags, kObjMarked);
				sprintf(nameStr, "obj_%08X", (ULong)supermapRef);
				PrintPkgFrameMap(inSection, nameStr, map);
			}
		}
	}
	CObject obj = MakeObject(inName, { Format("NR_FRAMEMAP(%d)", numOfSlots-1), "NR_0" });	// exclude supermap
	Ref * p = &frMap->objClass;
	for (ArrayIndex slotsRemaining = numOfSlots+1; slotsRemaining > 0; --slotsRemaining, ++p) {
		Ref tag = *p;
		sprintf(nameStr, "obj_%08X", (ULong)*p);
		obj.words.push_back(CInit(PrintPkgObject(inSection, nameStr, tag, false, false)));
	}
	inSection.add(obj);
}


/* -----------------------------------------------------------------------------
	Print a C function object.
----------------------------------------------------------------------------- */

void
PrintCFunction(CSection & inSection, const char * inName, const Value & inPointer, int inNumArgs, bool inDebug = false)
{
	std::string mapName = std::string(inName) + "_map";
	CObject map = MakeObject(mapName, { inDebug ? "NR_FRAMEMAP(4)" : "NR_FRAMEMAP(3)", "NR_0" });
	map.words.insert(map.words.end(), { "NR_0", "NR_NIL", CInit(Value::MakePointer("SYMclass")), CInit(Value::MakePointer("SYMfuncPtr")), CInit(Value::MakePointer("SYMnumArgs")) });
	if (inDebug)
		map.words.push_back(CInit(Value::MakePointer("SYMdocString")));
	inSection.add(map);

	CObject frame = MakeObject(inName, { inDebug ? "NR_FRAME(4)" : "NR_FRAME(3)", "NR_0", CInit(Value::MakePointer(mapName)) });
	frame.words.insert(frame.words.end(), { CInit(Value::MakeRef(kPlainCFunctionClass)), CInit(inPointer), CInit(Value::MakeRef(MAKEINT(inNumArgs))) });
	if (inDebug)
		frame.words.push_back("NR_NIL");
	inSection.add(frame);
}


/* -----------------------------------------------------------------------------
	Print an RS (RefStar) reference.
----------------------------------------------------------------------------- */

void
PrintRefStar(std::vector<RefStar> & inRefStars, const char * inName, const Value & inValue)
{
	inRefStars.push_back(RefStar{ inName, CInit(inValue) });
}


/* -----------------------------------------------------------------------------
	Extract const Ref* (RS) objects from ROM image and print their definitions.

	for top-level object
		switch object tag
			integer			N/A
			immediate		N/A
			magic pointer	print RS with "MAKEMAGICPTR(%ld)", RVALUE(obj)
			pointer
				frame			print map, frame
				array			print array
					maybe recurse
				binary
								print RS with obj name


	for frame/array element
		switch object tag
			integer			print Ref, but no RS
			immediate		print Ref, but no RS
			magic pointer	print Ref

				maybe recurse

	Args:		inSection	where the objects go
				inName		object name
				inObj			object
				inDefineObjects
				inDefineSymbols
	Return:	its value in a slot
----------------------------------------------------------------------------- */

Value
PrintROMObject(CSection & inSection, const char * inName, Ref inTag, Ref inObj, bool inDefineObjects, bool inDefineSymbols)
{
	Ref objClass;
	ArrayIndex i, numOfSlots;
	Value rsValue;

	switch (RTAG(inObj))
	{
	case kTagInteger:
/*-- integer --*/
		// cannot have integer Ref at top level

	case kTagImmed:
/*-- immediate --*/
		// cannot have immediate Ref at top level
		rsValue = Value::MakeRef(inObj);
		break;

	case kTagPointer: {
/*-- pointer --*/
			Ref val;
			char nameStr[64];
			// switch on type of object: slotted (frame/array) or binary
			unsigned flags = B32ObjectFlags(inObj);
			if (FLAGTEST(flags, kObjSlotted)) {
/*-- SLOTTED --*/
				if (FLAGTEST(flags, kObjFrame)) {
/*-- frame --*/
/*
foreach slot in frame
if pointer object not previously printed, print it using name derived from address
	might recurse
print frameMap
print frameHeader
foreach slot in frame
print object -- if pointer object use name above
*/
					if (inDefineObjects) {
						numOfSlots = B32Length(inObj);
						Ref32 map32 = ((FrameObject32 *)(inObj-1))->map;
						Ref map = FixPointerRef(CANONICAL_LONG(map32));
						Ref32 * tagPtr = ((FrameMapObject32 *)(map-1))->slot;
						Ref32 * p, * slotPtr = ((FrameObject32 *)(inObj-1))->slot;
		// PASS 1: define pointer objects contained in the frame
						for (i = 0, p = slotPtr; i < numOfSlots; ++i, ++p, ++tagPtr) {
							val = FixPointerRef(CANONICAL_LONG(*p));
							if (ISREALPTR(val)) {
								ObjHeader32 * elem = (ObjHeader32 *)(val-1);
								if (!FLAGTEST(elem->flags, kObjMarked)) {
									FLAGSET(elem->flags, kObjMarked);
									sprintf(nameStr, "obj_%08X", (ULong)CANONICAL_LONG(*p));
//						Ref tag = B32GetTag(inObj, i, NULL);
									PrintROMObject(inSection, nameStr, B32GetTag(map, i, NULL), val, true, false);
								}
							}
						}

		// PASS 2: define frame map
						sprintf(nameStr, "%s_map", inName);
						ArrayIndex fnCount = PrintFrameMap(inSection, nameStr, FixPointerRef(CANONICAL_LONG(((FrameObject32 *)(inObj-1))->map)));

		// PASS 3: define frame
						CObject obj = MakeObject(inName, { Format("NR_FRAME(%d)", numOfSlots+fnCount), "NR_0", CInit(Value::MakePointer(nameStr)) });
						bool isPlainCFunc = false;
						p = slotPtr;
						for (ArrayIndex slotIndex = 0; slotIndex < numOfSlots; ++slotIndex, ++p) {
							i = slotIndex % 16;	// (as the assembler version did: the index in its line of 16)
							val = FixPointerRef(CANONICAL_LONG(*p));
							if (i == kPlainCFunctionClassIndex && val == kPlainCFunctionClass) {
								isPlainCFunc = true;
							}
							if (i == kPlainCFunctionPtrIndex && isPlainCFunc) {
								obj.words.push_back(CInit(PlainCFuncName(CANONICAL_LONG(*p), inTag)));
							} else {
								sprintf(nameStr, "obj_%08X", (ULong)CANONICAL_LONG(*p));
								obj.words.push_back(CInit(PrintROMObject(inSection, nameStr, NILREF, val, false, false)));
							}
						}
						if (!gForNTK) {
						} else if (strcmp(inName, "gFunky") == 0) {
						// append external functions
							for (CSymbol * fn = gFunctionNames; fn != NULL; fn = fn->fNext) {
								obj.words.push_back(CInit(Value::MakePointer(std::string("obj_") + fn->fName)));
							}
						} else if (strcmp(inName, "MP0546") == 0) {
						// append protoProtoEditor
							obj.words.push_back(CInit(Value::MakePointer("protoProtoEditor")));
						}
						inSection.add(obj);
					}
					rsValue = Value::MakePointer(inName);
				}

				// insanity check
				else if (FLAGTEST(flags, kObjForward)) {
					printf("<forwarding pointer #%08lX...WTF!!!>", (unsigned long)inObj);
				}

				else {
/*-- array --*/
					if (inDefineObjects) {
						objClass = B32ClassOf(inObj);
					// print elements in array
						numOfSlots = B32Length(inObj);
/*
foreach slot in array
if pointer object not previously printed, print it using name inName & "-" & index
	might recurse
print arrayHeader
foreach element in array
print object -- if pointer object use name above
*/
						Ref32 * p, * slotPtr = ((FrameObject32 *)(inObj-1))->slot;
		// PASS 1: define pointer objects contained in the array
						for (i = 0, p = slotPtr; i < numOfSlots; ++i, ++p) {
							val = FixPointerRef(CANONICAL_LONG(*p));
							if (ISREALPTR(val)) {
								ObjHeader32 * elem = (ObjHeader32 *)(val-1);
								if (!FLAGTEST(elem->flags, kObjMarked)) {
									FLAGSET(elem->flags, kObjMarked);
									sprintf(nameStr, "obj_%08X", (ULong)CANONICAL_LONG(*p));
									PrintROMObject(inSection, nameStr, NILREF, val, true, false);
								}
							}
						}

		// PASS 2: define the array
						CObject obj = MakeObject(inName, { Format("NR_ARRAY(%d)", numOfSlots), "NR_0", CInit(PrintROMObject(inSection, inName, NILREF, objClass, false, false)) });
						p = slotPtr;
						for (i = 0; i < numOfSlots; ++i, ++p) {
							val = FixPointerRef(CANONICAL_LONG(*p));
							sprintf(nameStr, "obj_%08X", (ULong)CANONICAL_LONG(*p));
							obj.words.push_back(CInit(PrintROMObject(inSection, nameStr, NILREF, val, false, false)));
						}
						inSection.add(obj);
					}
					rsValue = Value::MakePointer(inName);
				}
			}

			else {
/*-- NOT SLOTTED --*/
				objClass = B32ClassOf(inObj);

				if (B32IsSymbol(inObj)) {
/*-- symbol --*/
					const char * sym = B32SymbolName(inObj);
					const char * symName = MungeSymbol(sym);
					if (inDefineSymbols) {
						PrintSymbol(inSection, symName, sym);
					}
					rsValue = Value::MakePointer(std::string("SYM") + symName);
				}

				else if (B32EQRef(objClass, SYMA(real))) {
/*-- real number --*/
					std::string realName = std::string("REAL") + inName;
					if (inDefineObjects) {
						// byte-swap double == 64 bits
						uint32_t * p = (uint32_t *)B32BinaryData(inObj);
						uint32_t p1 = p[1];
						p[1] = CANONICAL_LONG(*p);
						p[0] = CANONICAL_LONG(p1);

						CObject obj = MakeObject(realName, { "NR_BINARY(8)", "NR_0", CInit(Value::MakePointer("SYMreal")) });
						obj.tailType = "uint32_t d[2];";
						obj.tailInit = Format("{0x%08X,0x%08X}", p[0],p[1]);
						inSection.add(obj);
					}
					rsValue = Value::MakePointer(realName);
				}

//				else if (B32EQRef(objClass, SYMA(instructions)) && NOTNIL(GetFrameSlot(gVarFrame, SYMA(printInstructions)))) {
/*-- codeblock --*/
//					PrintInstructions(inObj);
//				}

				else if (B32IsSymbol(objClass)) {
					if (B32IsSubclass(objClass, "string")) {
/*-- string --*/
						if (inDefineObjects) {
							// byte-swap UniChar
							UniChar * str = (UniChar *)B32BinaryData(inObj);
							UniChar * s = str;
							for (ArrayIndex i = 0, len = B32Length(inObj)/sizeof(UniChar); i < len; ++i, ++s) {
								*s = CANONICAL_SHORT(*s);
							}
							PrintString(inSection, inName, PrintROMObject(inSection, inName, NILREF, objClass, false, false), str);
						}
						rsValue = Value::MakePointer(inName);
					} else {
/*-- specific binary object --*/
						if (inDefineObjects) {
							UChar * data = (UChar *)B32BinaryData(inObj);
							ArrayIndex len = B32Length(inObj);
							CObject obj = MakeObject(inName, { Format("NR_BINARY(%d)", len), "NR_0", CInit(PrintROMObject(inSection, inName, NILREF, objClass, false, false)) });
							SetBytes(obj, data, len);
							inSection.add(obj);
						}
						rsValue = Value::MakePointer(inName);
					}
				}

				else {
/*-- generic binary object --*/
					printf("<binary, class ");
					PrintROMObject(inSection, inName, NILREF, objClass, false, false);
					printf(", length %d>", B32Length(inObj));
				}
			}
		}
		break;

	case kTagMagicPtr:
/*-- magic pointer --*/
		rsValue = Value::MakeMagicPointer(RVALUE(inObj));
		break;
	}

	fflush(stdout);
	return rsValue;
}


const char *
PkgSymbolName(Ref r)
{
	const char * target[] =	{
		"debuggerInfo","InstallScript",
		0 };

	SymbolObject * obj = (SymbolObject *)(r-1);
	const char * sym = obj->name;
	// apply case correction
	for (const char ** s = target; *s != 0; ++s) {
		if (symcmp(sym, *s) == 0) {
			sym = *s;
			break;
		}
	}
	return sym;
}


/* -----------------------------------------------------------------------------
	Extract const Ref* (RS) objects from package and print their definitions.
	Args:		inSection		where the objects go
				inName			object name
				inObj			object
	Return:	its value in a slot
----------------------------------------------------------------------------- */

Value
PrintPkgObject(CSection & inSection, const char * inName, Ref inObj, bool inDefineObjects, bool inDefineSymbols)
{
	Ref objClass;
	ArrayIndex i, numOfSlots;
	Value rsValue;

	switch (RTAG(inObj))
	{
	case kTagInteger:
/*-- integer --*/
		// cannot have integer Ref at top level

	case kTagImmed:
/*-- immediate --*/
		// cannot have immediate Ref at top level
		rsValue = Value::MakeRef(inObj);
		break;

	case kTagPointer: {
/*-- pointer --*/
			Ref val;
			char nameStr[64];
			// switch on type of object: slotted (frame/array) or binary
			unsigned flags = ObjectFlags(inObj);
			if (FLAGTEST(flags, kObjSlotted)) {
/*-- SLOTTED --*/
				if (FLAGTEST(flags, kObjFrame)) {
/*-- frame --*/
					if (inDefineObjects) {
						numOfSlots = Length(inObj);
						Ref map = ((FrameObject *)(inObj-1))->map;
						Ref * tagPtr = ((FrameMapObject *)(map-1))->slot;
						Ref * p, * slotPtr = ((FrameObject *)(inObj-1))->slot;
		// PASS 1: define pointer objects contained in the frame
						for (i = 0, p = slotPtr; i < numOfSlots; ++i, ++p, ++tagPtr) {
							val = *p;
							if (ISREALPTR(val)) {
								ObjHeader * elem = (ObjHeader *)(val-1);
								if (!FLAGTEST(elem->flags, kObjMarked)) {
									FLAGSET(elem->flags, kObjMarked);
									sprintf(nameStr, "obj_%08X", (ULong)*p);
									PrintPkgObject(inSection, nameStr, val, true, false);
								}
							}
						}

		// PASS 2: define frame map
						sprintf(nameStr, "%s_map", inName);
						PrintPkgFrameMap(inSection, nameStr, ((FrameObject *)(inObj-1))->map);

		// PASS 3: define frame
						CObject obj = MakeObject(inName, { Format("NR_FRAME(%d)", numOfSlots), "NR_0", CInit(Value::MakePointer(nameStr)) });
						p = slotPtr;
						for (i = 0; i < numOfSlots; ++i, ++p) {
							val = *p;
							sprintf(nameStr, "obj_%08X", (ULong)*p);
							obj.words.push_back(CInit(PrintPkgObject(inSection, nameStr, val, false, false)));
						}
						inSection.add(obj);
					}
					rsValue = Value::MakePointer(inName);
				}

				// insanity check
				else if (FLAGTEST(flags, kObjForward)) {
					printf("<forwarding pointer #%08lX...WTF!!!>", (unsigned long)inObj);
				}

				else {
/*-- array --*/
					if (inDefineObjects) {
						objClass = ClassOf(inObj);
					// print elements in array
						numOfSlots = Length(inObj);
						Ref * p, * slotPtr = ((FrameObject *)(inObj-1))->slot;
		// PASS 1: define pointer objects contained in the array
						for (i = 0, p = slotPtr; i < numOfSlots; ++i, ++p) {
							val = *p;
							if (ISREALPTR(val)) {
								ObjHeader * elem = (ObjHeader *)(val-1);
								if (!FLAGTEST(elem->flags, kObjMarked)) {
									FLAGSET(elem->flags, kObjMarked);
									sprintf(nameStr, "obj_%08X", (ULong)*p);
									PrintPkgObject(inSection, nameStr, val, true, false);
								}
							}
						}

		// PASS 2: define the array
						CObject obj = MakeObject(inName, { Format("NR_ARRAY(%d)", numOfSlots), "NR_0", CInit(PrintPkgObject(inSection, inName, objClass, false, false)) });
						p = slotPtr;
						for (i = 0; i < numOfSlots; ++i, ++p) {
							val = *p;
							sprintf(nameStr, "obj_%08X", (ULong)*p);
							obj.words.push_back(CInit(PrintPkgObject(inSection, nameStr, val, false, false)));
						}
						inSection.add(obj);
					}
					rsValue = Value::MakePointer(inName);
				}
			}

			else {
/*-- NOT SLOTTED --*/
				objClass = ClassOf(inObj);

				if (IsSymbol(inObj)) {
/*-- symbol --*/
					const char * sym = PkgSymbolName(inObj);
					const char * symName = MungeSymbol(sym);
					if (inDefineSymbols) {
						PrintSymbol(inSection, symName, sym);
					}
					rsValue = Value::MakePointer(std::string("SYM") + symName);
				}

				else if (EQRef(objClass, SYMA(real))) {
/*-- real number --*/
					std::string realName = std::string("REAL") + inName;
					if (inDefineObjects) {
						uint32_t * p = (uint32_t *)BinaryData(inObj);
						CObject obj = MakeObject(realName, { "NR_BINARY(8)", "NR_0", CInit(Value::MakePointer("SYMreal")) });
						obj.tailType = "uint32_t d[2];";
						obj.tailInit = Format("{0x%08X,0x%08X}", p[0],p[1]);
						inSection.add(obj);
					}
					rsValue = Value::MakePointer(realName);
				}

//				else if (EQRef(objClass, SYMA(instructions)) && NOTNIL(GetFrameSlot(gVarFrame, SYMA(printInstructions)))) {
/*-- codeblock --*/
//					PrintInstructions(inObj);
//				}

				else if (IsSymbol(objClass)) {
					if (IsSubclass(objClass, SYMA(string))) {
/*-- string --*/
						if (inDefineObjects) {
							// the package loader has already helpfully byte-swapped UniChar
							PrintString(inSection, inName, PrintPkgObject(inSection, inName, objClass, false, false), (UniChar *)BinaryData(inObj));
						}
						rsValue = Value::MakePointer(inName);
					} else {
/*-- specific binary object --*/
						if (inDefineObjects) {
							UChar * data = (UChar *)BinaryData(inObj);
							ArrayIndex len = Length(inObj);
							CObject obj = MakeObject(inName, { Format("NR_BINARY(%d)", len), "NR_0", CInit(PrintPkgObject(inSection, inName, objClass, false, false)) });
							SetBytes(obj, data, len);
							inSection.add(obj);
						}
						rsValue = Value::MakePointer(inName);
					}
				}

				else {
/*-- generic binary object --*/
					printf("<binary, class ");
					PrintPkgObject(inSection, inName, objClass, false, false);
					printf(", length %d>", Length(inObj));
				}
			}
		}
		break;

	case kTagMagicPtr:
/*-- magic pointer --*/
		rsValue = Value::MakeMagicPointer(RVALUE(inObj));
		break;
	}

	fflush(stdout);
	return rsValue;
}


#pragma mark -
/* -----------------------------------------------------------------------------
	Write the C source.
----------------------------------------------------------------------------- */

FILE *
OpenOutput(const char * inName)
{
	std::string path = inName;
	if (gOutputFilename)
		path = std::string(gOutputFilename) + "/" + inName;
	FILE * fp = fopen(path.c_str(), "w");
	if (fp == NULL)
		ExitWithMessage("can't write \"%s\"", path.c_str());
	return fp;
}


void
WriteWords(FILE * inFP, const std::vector<std::string> & inWords, size_t inFrom, const char * inIndent)
{
	for (size_t i = inFrom; i < inWords.size(); ++i) {
		if ((i - inFrom) % 16 == 0)
			fprintf(inFP, "\n%s", inIndent);
		fprintf(inFP, "%s%s", inWords[i].c_str(), (i < inWords.size()-1) ? ", " : "");
	}
}


void
WriteSectionType(FILE * inFP, const CSection & inSection)
{
	for (const CObject & obj : inSection.objects) {
		fprintf(inFP, "\tstruct { ROMRef r[%zu];%s%s } %s;\n", obj.words.size(), obj.tailType.empty() ? "" : " ", obj.tailType.c_str(), obj.label.c_str());
	}
}


void
WriteSectionInit(FILE * inFP, const CSection & inSection)
{
	for (const CObject & obj : inSection.objects) {
		fprintf(inFP, "\t.%s = {{", obj.label.c_str());
		for (size_t i = 0; i < obj.headerCount; ++i)
			fprintf(inFP, "%s%s", obj.words[i].c_str(), (i < obj.words.size()-1) ? ", " : "");
		WriteWords(inFP, obj.words, obj.headerCount, "\t\t");
		fprintf(inFP, "}");
		if (!obj.tailType.empty())
			fprintf(inFP, ",\n\t\t%s", obj.tailInit.c_str());
		fprintf(inFP, "},\n");
	}
}


void
WriteRefStars(FILE * inFP, const char * inStructName, const std::vector<RefStar> & inRefStars)
{
	// R<name>: the object's Ref (a RefHandle's ref), RS<name>: its address (Ref *)
	fprintf(inFP, "static struct {\n");
	for (const RefStar & rs : inRefStars)
		fprintf(inFP, "\tROMRef %s;\n", rs.name.c_str());
	fprintf(inFP, "} %s = {\n", inStructName);
	for (const RefStar & rs : inRefStars)
		fprintf(inFP, "\t.%s = %s,\n", rs.name.c_str(), rs.init.c_str());
	fprintf(inFP, "};\n\n");
	for (const RefStar & rs : inRefStars)
		fprintf(inFP, "intptr_t * RS%s = &%s.%s.i;\n", rs.name.c_str(), inStructName, rs.name.c_str());
}


void
CheckLabels(void)
{
	for (const std::string & label : gPointerLabels) {
		bool isSymbol = label.compare(0, 3, "SYM") == 0;
		bool defined = isSymbol ? gSymbolSection.labels.count(label) != 0
										: gDataSection.labels.count(label) != 0 || gMagicSection.labels.count(label) != 0;
		if (!defined)
			ExitWithMessage("%s is pointed to but not defined", label.c_str());
	}
}


void
WriteCSource(void)
{
	CheckLabels();

	FILE * fp = OpenOutput("ROMObjects.h");
	fprintf(fp, kGeneratedBy, "ROMObjects.h", "object layout");
	fputs(kObjectsHeader, fp);
	fclose(fp);

/*-- symbols --*/
	fp = OpenOutput("ROMSymbols.h");
	fprintf(fp, kGeneratedBy, "ROMSymbols.h", "symbol objects");
	fprintf(fp, "#include \"ROMObjects.h\"\n\n");
	fprintf(fp, "struct ROMSymbols {\n");
	WriteSectionType(fp, gSymbolSection);
	fprintf(fp, "};\n\nextern struct ROMSymbols gROMSymbols;\n");
	fclose(fp);

	fp = OpenOutput("ROMSymbols.c");
	fprintf(fp, kGeneratedBy, "ROMSymbols.c", "symbol data");
	fprintf(fp, "#include \"ROMSymbols.h\"\n\n");
	fprintf(fp, "struct ROMSymbols gROMSymbols = {\n");
	WriteSectionInit(fp, gSymbolSection);
	fprintf(fp, "};\n\n");
	WriteRefStars(fp, "gSymbolRefs", gSymbolRefStars);
	fclose(fp);

/*-- objects --*/
	fp = OpenOutput("ROMData.c");
	fprintf(fp, kGeneratedBy, "ROMData.c", "data");
	fprintf(fp, "#include \"ROMSymbols.h\"\n\n");
	fprintf(fp, "/* the C functions in the data (their real types don't matter here) */\n");
	for (const std::string & fn : gFunctions)
		fprintf(fp, "extern void %s(void);\n", fn.c_str());
	fprintf(fp, "\nstruct ROMData {\n");
	WriteSectionType(fp, gDataSection);
	fprintf(fp, "\t/* the magic pointers' objects */\n");
	WriteSectionType(fp, gMagicSection);
	fprintf(fp, "};\n\n");
	fprintf(fp, "struct ROMData gROMData = {\n");
	WriteSectionInit(fp, gDataSection);
	WriteSectionInit(fp, gMagicSection);
	fprintf(fp, "};\n\n");
	fprintf(fp, "/* ISINROM(r): r points into gROMData */\n");
	fprintf(fp, "void * gROMDataStart = &gROMData;\n");
	fprintf(fp, "void * gROMDataEnd = (char *)&gROMData + sizeof(gROMData);\n\n");
	fprintf(fp, "struct {\n\tuint32_t numOfPointers;\n\tuint32_t pad;\n\tROMRef magicPointer[%zu];\n} gROMMagicPointerTable = { %zu, 0, {", gMagicTable.size(), gMagicTable.size());
	WriteWords(fp, gMagicTable, 0, "\t");
	fprintf(fp, "}};\n\n");
	WriteRefStars(fp, "gDataRefs", gDataRefStars);
	fclose(fp);
}


/* -----------------------------------------------------------------------------
	The headers declaring the RS* objects for the object system's C++ code.
	C linkage: the definitions are C (ROMData.c, ROMSymbols.c).
----------------------------------------------------------------------------- */

void
BeginRSHeader(FILE * inFP, const char * inName)
{
	fprintf(inFP, kGeneratedBy, inName, "declarations");
	fprintf(inFP, "#ifdef __cplusplus\nextern \"C\" {\n#endif\n\n");
}

void
EndRSHeader(FILE * inFP)
{
	fprintf(inFP, "\n#ifdef __cplusplus\n}\n#endif\n");
}


#pragma mark -
/* -----------------------------------------------------------------------------
	Command line options.
----------------------------------------------------------------------------- */

void
AssureNextArg(const char ** inArg, const char ** inArgLimit, const char * inMessage)
{
	if (inArg + 1 >= inArgLimit)
		UsageExitWithMessage("no %s follows \"%s\"", inMessage, *inArg);
}


int
hcistrcmp(const char * inStr, const char * inOptionStr)
{
	char  ch;
	int	delta;
	do {
		ch = *inStr++;
		if ((delta = *inOptionStr++ - tolower(ch)) != 0)
			return delta;
	} while (ch != 0);
	return 0;
}


const char **
ParseOptions(const char ** inArg, const char ** inArgLimit)
{
	const char **	inputFilenameLimit = inArg;
	const char *	option;

	for ( ; inArg < inArgLimit; inArg++) {
		option = *inArg;
		if (hcistrcmp(option, "-help") == 0) {
			gDoHelp = true;
		} else if (hcistrcmp(option, "-p") == 0) {
			gDoProgress = true;
		} else if (hcistrcmp(option, "-dump") == 0) {
			gDoDump = true;
		} else if (hcistrcmp(option, "-s") == 0) {
			AssureNextArg(inArg, inArgLimit, "Ref source filename");
			gSourceFilename = *++inArg;
		} else if (hcistrcmp(option, "-sym") == 0) {
			AssureNextArg(inArg, inArgLimit, "plainC function name file");
			gSymbolFilename = *++inArg;
		} else if (hcistrcmp(option, "-via") == 0) {
			AssureNextArg(inArg, inArgLimit, "function name file");
			if (gExportFileCount >= kMaxExportFileCount) {
				ExitWithMessage("only %d function name files allowed", kMaxExportFileCount);
			}
			gHasViaOption = true;
			gExportFilename[gExportFileCount++] = *++inArg;
		} else if (hcistrcmp(option, "-o") == 0) {
			AssureNextArg(inArg, inArgLimit, "output folder");
			gOutputFilename = *++inArg;
		} else if (*option == '-') {
			UsageExitWithMessage("%s is not an option", option);
		} else {
			*inputFilenameLimit++ = option;
		}
	}

	return inputFilenameLimit;
}


#pragma mark -
/* -----------------------------------------------------------------------------
	And this is where our story REALLY starts...
	Extract const Ref* (RS) objects from ROM image and print their definitions.
	foreach line in RefStarSymbols decalaration file
		scan address(=offset into ROM image), symbol name
		declare symbol name in header file
		define symbol data in data file
			maybe recurse
		define RS reference in RS file
	Args:		argc			number of command line args
				argv			those args in full
	Return:	exit code
----------------------------------------------------------------------------- */

int
main(int argc, const char * argv[])
{
	gCmdName = argv[0];
	/*const char **  inputFilenameLimit =*/ ParseOptions(&argv[1], &argv[argc]);
	gForNTK = gHasViaOption;
	InitObjectSystem();		// for NTKFunctions.newtonpkg

	gImageFilename = argv[1];
	FILE * fp = fopen(gImageFilename, "r");
	if (fp == NULL)
		ExitWithMessage("can't open ROM image file \"%s\"", gImageFilename);

/*== read ROM image into memory ==*/
	size_t ROMsize;
	fseek(fp, 0, SEEK_END);
	ROMsize = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	ROMdata = (char *)malloc(ROMsize);	// yup, all 8MB of it
	fread(ROMdata, ROMsize, 1, fp);
	fclose(fp);

/*== export ROM extensions ==*/
#if forROMExtensions
	RExHeader * rex = (RExHeader *)(ROMdata + 0x0071FC4C);	// it's at a well-known address in the ROM image

#if defined(hasByteSwapping)
	// byte-swap the directory
	ULong * ulp = (ULong *)rex;
	for (ArrayIndex i = 0; i < sizeof(RExHeader)/sizeof(ULong); ++i, ++ulp) {
		*ulp = BYTE_SWAP_LONG(*ulp);
	}
	// including config directory entries
	ConfigEntry * entry = rex->table;
	for (ArrayIndex i = 0; i < rex->count; ++i, ++entry) {
		// byte swap directory entry
		entry->tag = BYTE_SWAP_LONG(entry->tag);
		entry->offset = BYTE_SWAP_LONG(entry->offset);
		entry->length = BYTE_SWAP_LONG(entry->length);
	}
#endif

	char str[9];
	MakeFourCharStr(rex->signatureA, str);
	MakeFourCharStr(rex->signatureB, str+4);
	printf("ROM Extension\n"
			 "signature: %s\n"
			 "header version: %d\n"
			 "manufacturer / version: %08X / %08X\n"
			 "REx id: %d\n\n"
			 "%d table entries:\n", str, rex->headerVersion, rex->manufacturer, rex->version, rex->id, rex->count);
	for (ArrayIndex i = 0; i < rex->count; ++i) {
		ConfigEntry * entry = &rex->table[i];
		MakeFourCharStr(entry->tag, str);
		printf("  %s[%d]\n", str, entry->length);
		if (entry->tag == kPackageListTag) {
			PackageDirectory * pkg = (PackageDirectory *)((Ptr)rex + entry->offset);
			size_t pkgSize;
			ArrayIndex pkgi = 1;
			for (size_t sizeRemaining = entry->length; sizeRemaining > 0; sizeRemaining -= pkgSize, ++pkgi) {
				UnRelocatePkg(pkg, ROMdata);
				pkgSize = CANONICAL_LONG(pkg->size);
				char pkgName[16];
				sprintf(pkgName, "Package-%d.newtonpkg", pkgi);
				FILE * pkgFile = fopen(pkgName, "w");
				fwrite(pkg, pkgSize, 1, pkgFile);
				fclose(pkgFile);
				pkg = (PackageDirectory *)((Ptr)pkg + ALIGN(pkgSize,4));
			}
		}
	}
#endif /*forROMExtensions*/


/*== read implemented plain C function names into symbol table ==*/
	gExportSymbolTable = new CSymbolTable(4*KByte);
	if (gForNTK && FillSymbolTable(gExportSymbolTable, &gFunctionNames, &gEditorFunctionNames))		// Toolkit.exp and Editor.exp
		ExitWithMessage("unable to build a valid function name table");
	gCSymbolTable = new CPointerTable(16*KByte);
	if (FillSymbolTable(gCSymbolTable))							// 717006.cfunctions
		ExitWithMessage("unable to build a valid function pointer table");

	unsigned int rAddr;
	Ref rRef;
	Ref32 * rsRef;
	char rsName[256];
	Value rsValue;

/*== first handle magic pointers ==*/
#if forMagicPointers
	struct MagicPointerTable
	{
		ArrayIndex	numOfPointers;
		Ref32			magicPointer[];
	};
	MagicPointerTable * mp = (MagicPointerTable *)(ROMdata + 0x003AF000);	// it's at a well-known address in the ROM image
	ArrayIndex count = CANONICAL_LONG(mp->numOfPointers);

/*-- define the contents of the table --*/
	// A few entries aren't objects of their own but bare symbols ('obsolete,
	// 'espy, 'helvetica), which PrintROMObject doesn't define here: their table
	// entry is that symbol's Ref itself, not a pointer to an MPxxxx object
	// (that pointed at a word holding the Ref, so @42 wasn't an object).
	Ref32 * mpp = mp->magicPointer;
	for (ArrayIndex i = 0; i < count; ++i, ++mpp) {
		sprintf(rsName, "MP%04d", i);
		rRef = FixPointerRef(CANONICAL_LONG(*mpp));
		size_t prevCount = gMagicSection.objects.size();
		rsValue = PrintROMObject(gMagicSection, rsName, NILREF, rRef, true, false);
		if (gMagicSection.objects.size() == prevCount)
			gMagicTable.push_back(CInit(rsValue));
		else
			gMagicTable.push_back(CInit(Value::MakePointer(rsName)));
	}
#endif /*forMagicPointers*/

#if forRefStarData
/*== read object pointers and names ==*/
	fp = fopen(gSourceFilename, "r");
	if (fp == NULL)
		ExitWithMessage("can't open source file \"%s\"", gSourceFilename);

/*-- PASS 1: build header file, locate symbol table array object --*/
	FILE * fp_h = fopen("../Frames/RSData.h", "w");
	BeginRSHeader(fp_h, "RefStar.h");
	// scan address, name of object
	rRef = NILREF;
	int lineLen;
	char line[300];
	while (!feof(fp)) {
		lineLen = 0;
		line[0] = 0;
		if (fgets(line, 300, fp) != NULL) {
			if (line[0] != '\n') {
				int	numOfItems = sscanf(line, "%8x%255s%n", &rAddr, rsName, &lineLen);
				if (lineLen == 255) {
					ExitWithMessage("input line too long in file \"%s\"", gSourceFilename);
				}
				if (numOfItems > 1 && line[0] != ';') {
					if (strlen(rsName) > 254) {
						ExitWithMessage("encountered function name longer than 254 characters in file \"%s\"", gSourceFilename);
					}
					fprintf(fp_h, "extern Ref* RS%s;\n", rsName);
					if (strcmp(rsName, "symbolTable") == 0) {
						rsRef = (Ref32 *)(ROMdata + rAddr);
						rRef = FixPointerRef(CANONICAL_LONG(*rsRef));
					}
				}
			}
		}
	}

/*-- declare objects that the original created at runtime --*/
	fprintf(fp_h, "extern Ref* RS%s;\n", "space");
	fprintf(fp_h, "extern Ref* RS%s;\n", "cFunctionPrototype");
	fprintf(fp_h, "extern Ref* RS%s;\n", "debugCFunctionPrototype");

	fprintf(fp_h, "\n/*-- for NTK --*/\n");
	fprintf(fp_h, "extern Ref* RS%s;\n", "constantFunctions");
	fprintf(fp_h, "extern Ref* RS%s;\n", "formInstallScript");
	fprintf(fp_h, "extern Ref* RS%s;\n", "formRemoveScript");
	fprintf(fp_h, "extern Ref* RS%s;\n", "autoInstallScript");

	EndRSHeader(fp_h);
	fclose(fp_h);

/*-- build symbol data --*/
	fp_h = fopen("../Frames/RSSymbols.h", "w");
	BeginRSHeader(fp_h, "RefStarSymbols.h");

	ArrayIndex i, numOfSlots = B32Length(rRef);
	Ref sym;
	Ref32 * p, * slotPtr = ((FrameObject32 *)(rRef-1))->slot;
	for (i = 0, p = slotPtr; i < numOfSlots; ++i, ++p) {
		sym = FixPointerRef(CANONICAL_LONG(*p));
		if (B32IsSymbol(sym)) {
			const char * symName = MungeSymbol(B32SymbolName(sym));
			Value rsValue = PrintROMObject(gSymbolSection, symName, NILREF, sym, true, true);
			sprintf(rsName, "SYM%s", symName);
			PrintRefStar(gSymbolRefStars, rsName, rsValue);
			fprintf(fp_h, "extern Ref* RSSYM%s;\n", symName);
		}
	}

/*-- symbols that don't appear in the original symbol table but that we'd rather not MakeSymbol() --*/
	const char * extraSym[] = { "compilerCompatibility", "closed", "DebugHashToName", "dbg1", 0 };
	for (const char ** symp = extraSym; *symp != 0; ++symp ) {
		const char * sym = *symp;
		const char * symName = MungeSymbol(sym);
		PrintSymbol(gSymbolSection, symName, sym);
		sprintf(rsName, "SYM%s", symName);
		PrintRefStar(gSymbolRefStars, rsName, Value::MakePointer(rsName));
		fprintf(fp_h, "extern Ref* RSSYM%s;\n", symName);
	}

/*-- symbols used by NTK --*/
	const char * ntkSym[] = { "GetKeyHandler","RemoveScript","partData","control",
									  "__origFunctions","__origVars","__platform",
									  "platformFunctions","platformWeakFunctions","platformVariables","platformConstants",
									  "SetPartFrameSlot","GetPartFrameSlot","thisView","beforeScript","afterScript",
									  "protoEditor","protoProtoEditor",
									   0 };
	fprintf(fp_h, "\n/*-- for NTK --*/\n");
	for (const char ** symp = ntkSym; *symp != 0; ++symp ) {
		const char * sym = *symp;
		const char * symName = MungeSymbol(sym);
		fprintf(fp_h, "extern Ref* RSSYM%s;\n", symName);
	}
	if (gForNTK) {
	for (const char ** symp = ntkSym; *symp != 0; ++symp ) {
		const char * sym = *symp;
		const char * symName = MungeSymbol(sym);
		if (strcmp(symName, "GetKeyHandler") != 0) {	// GetKeyHandler has already been defined
			PrintSymbol(gSymbolSection, symName, sym);
		}
		sprintf(rsName, "SYM%s", symName);
		PrintRefStar(gSymbolRefStars, rsName, Value::MakePointer(rsName));
	}
	for (CSymbol * fn = gFunctionNames; fn != NULL; fn = fn->fNext) {	// should be NTKFunctions.newtonpkg > externalFunctions?
		const char * sym = fn->fName;
		const char * symName = MungeSymbol(sym);
		if (strcmp(symName, "ReplaceSelection") != 0) {	// ReplaceSelection has already been defined
			PrintSymbol(gSymbolSection, symName, sym);
		}
	}
	}

	EndRSHeader(fp_h);
	fclose(fp_h);

/*-- PASS 2: build object data --*/
	if (gForNTK) {
	// define external functions
	for (CSymbol * fn = gFunctionNames; fn != NULL; fn = fn->fNext) {
		char fnName[64];
		sprintf(fnName, "obj_%s", fn->fName);
		PrintCFunction(gDataSection, fnName, Value::MakeFunction(fn->fCName + 1), fn->fNumArgs);	// fCName: "_" + the C name
	}
	}
	// scan address, name of object
	fseek(fp, 0, SEEK_SET);
	while (!feof(fp)) {
		lineLen = 0;
		line[0] = 0;
		if (fgets(line, 300, fp) != NULL) {
			if (line[0] != '\n') {
				int numOfItems = sscanf(line, "%8x%255s%n", &rAddr, rsName, &lineLen);
				if (numOfItems > 1 && line[0] != ';') {
					rsRef = (Ref32 *)(ROMdata + rAddr);
					rRef = FixPointerRef(CANONICAL_LONG(*rsRef));

					rsValue = PrintROMObject(gDataSection, rsName, NILREF, rRef, true, false);
					if (ISPTR(rRef) && rsValue.isValid()) {
						PrintRefStar(gDataRefStars, rsName, rsValue);
					}
				}
			}
		}
	}

	if (gForNTK) {
	NewtonPackage scriptPkg("NTKFunctions.newtonpkg");
	// load constantFunctions
	RefVar fns(GetArraySlot(scriptPkg.partRef(0), 1));
	PrintPkgObject(gDataSection, "constantFunctions", fns, true, false);
	PrintRefStar(gDataRefStars, "constantFunctions", Value::MakePointer("constantFunctions"));
	// load install/remove scripts
	RefVar scripts(GetArraySlot(scriptPkg.partRef(0), 2));
	FOREACH_WITH_TAG(scripts, tag, fn)
		// print the script
		const char * fname = SymbolName(tag);
		PrintPkgObject(gDataSection, fname, fn, true, false);
		PrintRefStar(gDataRefStars, fname, Value::MakePointer(fname));
	END_FOREACH;
	}

/*-- objects that the original created at runtime --*/
	const UniChar STRspace[] = { CANONICAL_SHORT(' '), 0 };	// (sic: 0x2000 on a little-endian host, as before; see B52)
	PrintString(gDataSection, "space", Value::MakePointer("SYMstring"), STRspace);
	PrintRefStar(gDataRefStars, "space", Value::MakePointer("space"));

	PrintCFunction(gDataSection, "cFunctionPrototype", Value::MakeRef(0), 0);
	PrintRefStar(gDataRefStars, "cFunctionPrototype", Value::MakePointer("cFunctionPrototype"));
	PrintCFunction(gDataSection, "debugCFunctionPrototype", Value::MakeRef(0), 0, true);
	PrintRefStar(gDataRefStars, "debugCFunctionPrototype", Value::MakePointer("debugCFunctionPrototype"));

	fclose(fp);
#endif /*forRefStarData*/

	WriteCSource();
	return 0;
}

