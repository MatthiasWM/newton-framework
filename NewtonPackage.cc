/*
	File:		NewtonPackage.cc

	Abstract:	An object that loads a Newton package file containing 32-bit big-endian Refs
					and presents part entries as 64-bit platform-endian Refs.

	Written by:	Newton Research Group, 2015.
*/

#include "Newton.h"

#include "Objects.h"
#include "NewtonPackage.h"
#include "PackageTypes.h"
#include "ObjHeader.h"
#include "Ref32.h"
#include "ROMResources.h"
#include "Symbols.h"


#if __LP64__
/* -----------------------------------------------------------------------------
	T Y P E S
----------------------------------------------------------------------------- */
#include <unordered_set>
typedef std::unordered_set<Ref32> ScanRefMap;

#include <map>
typedef std::map<Ref32, Ref> RefMap;

/* -----------------------------------------------------------------------------
	F O R W A R D   D E C L A R A T I O N S
----------------------------------------------------------------------------- */
bool		IsObjClass(Ref obj, const char * inClassName);

size_t	ScanRef(Ref32 inRef, const char * inPartAddr, long inPartOffset, ScanRefMap & ioMap);
Ref		CopyRef(Ref32 inRef, const char * inPartAddr, long inPartOffset, ArrayObject * &ioDstPtr, RefMap & ioMap);
void		UpdateRef(Ref * inRefPtr, RefMap & inMap);
#else
void		FixUpRef(Ref * inRefPtr, char * inBaseAddr);
#endif


const UShort kFromMacRomanLUT[] =
{
	0x0000,0x0001,0x0002,0x0003,0x0004,0x0005,0x0006,0x0007,0x0008,0x0009,0x000A,0x000B,0x000C,0x000D,0x000E,0x000F,
	0x0010,0x0011,0x0012,0x0013,0x0014,0x0015,0x0016,0x0017,0x0018,0x0019,0x001A,0x001B,0x001C,0x001D,0x001E,0x001F,
	0x0020,0x0021,0x0022,0x0023,0x0024,0x0025,0x0026,0x0027,0x0028,0x0029,0x002A,0x002B,0x002C,0x002D,0x002E,0x002F,
	0x0030,0x0031,0x0032,0x0033,0x0034,0x0035,0x0036,0x0037,0x0038,0x0039,0x003A,0x003B,0x003C,0x003D,0x003E,0x003F,
	0x0040,0x0041,0x0042,0x0043,0x0044,0x0045,0x0046,0x0047,0x0048,0x0049,0x004A,0x004B,0x004C,0x004D,0x004E,0x004F,
	0x0050,0x0051,0x0052,0x0053,0x0054,0x0055,0x0056,0x0057,0x0058,0x0059,0x005A,0x005B,0x005C,0x005D,0x005E,0x005F,
	0x0060,0x0061,0x0062,0x0063,0x0064,0x0065,0x0066,0x0067,0x0068,0x0069,0x006A,0x006B,0x006C,0x006D,0x006E,0x006F,
	0x0070,0x0071,0x0072,0x0073,0x0074,0x0075,0x0076,0x0077,0x0078,0x0079,0x007A,0x007B,0x007C,0x007D,0x007E,0x007F,

	0x00C4,0x00C5,0x00C7,0x00C9,0x00D1,0x00D6,0x00DC,0x00E1,0x00E0,0x00E2,0x00E4,0x00E3,0x00E5,0x00E7,0x00E9,0x00E8,
	0x00EA,0x00EB,0x00ED,0x00EC,0x00EE,0x00EF,0x00F1,0x00F3,0x00F2,0x00F4,0x00F6,0x00F5,0x00FA,0x00F9,0x00FB,0x00FC,
	0x2020,0x00B0,0x00A2,0x00A3,0x00A7,0x2022,0x00B6,0x00DF,0x00AE,0x00A9,0x2122,0x00B4,0x00A8,0x2260,0x00C6,0x00D8,
	0x221E,0x00B1,0x2264,0x2265,0x00A5,0x00B5,0x2202,0x2211,0x220F,0x03C0,0x222B,0x00AA,0x00BA,0x2126,0x00E6,0x00F8,
	0x00BF,0x00A1,0x00AC,0x221A,0x0192,0x2248,0x2206,0x00AB,0x00BB,0x2026,0x00A0,0x00C0,0x00C3,0x00D5,0x0152,0x0153,
	0x2013,0x2014,0x201C,0x201D,0x2018,0x2019,0x00F7,0x25CA,0x00FF,0x0178,0x2044,0x00A4,0x2039,0x203A,0xFB01,0xFB02,
	0x2021,0x00B7,0x201A,0x201E,0x2030,0x00C2,0x00CA,0x00C1,0x00CB,0x00C8,0x00CD,0x00CE,0x00CF,0x00CC,0x00D3,0x00D4,
	0xF7FF,0x00D2,0x00DA,0x00DB,0x00D9,0x0131,0x02C6,0x02DC,0x00AF,0x02D8,0x02D9,0x02DA,0x00B8,0x02DD,0x02DB,0x02C7
};


#pragma mark -
/* -----------------------------------------------------------------------------
	Constructor.
----------------------------------------------------------------------------- */

NewtonPackage::NewtonPackage(const char * inPkgPath)
{
	pkgFile = fopen(inPkgPath, "r");
	pkgMem = NULL;
	pkgDir = NULL;
	relocationData = NULL;
	part0Data.data = NULL;
	pkgPartData = NULL;
  align4flag = NULL;
}


NewtonPackage::NewtonPackage(void * inPkgData)
{
	pkgFile = NULL;
	pkgMem = inPkgData;
	pkgDir = NULL;
	relocationData = NULL;
	part0Data.data = NULL;
	pkgPartData = NULL;
  align4flag = NULL;
}


/* -----------------------------------------------------------------------------
	Destructor.
----------------------------------------------------------------------------- */

NewtonPackage::~NewtonPackage()
{
	// free data
	if (pkgDir)
		free(pkgDir);
	if (relocationData)
		free(relocationData);
	if (pkgPartData != NULL && pkgPartData != &part0Data)
		free(pkgPartData);
	// don’t free individual part data -- it is persistent
	// if client wants to free it they must keep a reference to it before the NewtonPackage is destroyed
	// close the file
	if (pkgFile)
		fclose(pkgFile);
  if (align4flag)
    free(align4flag);
}


/* -----------------------------------------------------------------------------
	Return the package directory.
	Args:		--
	Return:	package directory, platform byte-ordered; treat as read-only
----------------------------------------------------------------------------- */

PackageDirectory *
NewtonPackage::directory(void)
{
	XTRY
	{
		if (pkgMem != NULL) {
			// package source is in memory
			PackageDirectory * dir = (PackageDirectory *)pkgMem;
			// verify signature
			XFAIL(strncmp(dir->signature, kPackageMagicNumber, kPackageMagicLen) != 0)
			// allocate for the directory + part entries
			int pkgDirSize = CANONICAL_LONG(dir->directorySize);
			pkgDir = (PackageDirectory *)malloc(pkgDirSize);
			XFAIL(pkgDir == NULL)
			memcpy(pkgDir, pkgMem, pkgDirSize);
		} else {
			// package source is a file
			XFAIL(pkgFile == NULL)

			PackageDirectory dir;
			// read the directory
			fread(&dir, sizeof(PackageDirectory), 1, pkgFile);
			// verify signature
			XFAIL(strncmp(dir.signature, kPackageMagicNumber, kPackageMagicLen) != 0)
			// allocate for the directory + part entries
			int pkgDirSize = CANONICAL_LONG(dir.directorySize);
			pkgDir = (PackageDirectory *)malloc(pkgDirSize);
			XFAIL(pkgDir == NULL)
			// read all that
			fseek(pkgFile, 0, SEEK_SET);
			fread(pkgDir, pkgDirSize, 1, pkgFile);
		}

#if defined(hasByteSwapping)
		pkgDir->id = BYTE_SWAP_LONG(pkgDir->id);
		pkgDir->flags = BYTE_SWAP_LONG(pkgDir->flags);
		pkgDir->version = BYTE_SWAP_LONG(pkgDir->version);
		pkgDir->copyright.offset = BYTE_SWAP_SHORT(pkgDir->copyright.offset);
		pkgDir->copyright.length = BYTE_SWAP_SHORT(pkgDir->copyright.length);
		pkgDir->name.offset = BYTE_SWAP_SHORT(pkgDir->name.offset);
		pkgDir->name.length = BYTE_SWAP_SHORT(pkgDir->name.length);
		pkgDir->size = BYTE_SWAP_LONG(pkgDir->size);
		pkgDir->creationDate = BYTE_SWAP_LONG(pkgDir->creationDate);
		pkgDir->modifyDate = BYTE_SWAP_LONG(pkgDir->modifyDate);
		pkgDir->directorySize = BYTE_SWAP_LONG(pkgDir->directorySize);
		pkgDir->numParts = BYTE_SWAP_LONG(pkgDir->numParts);

		PartEntry * pe = pkgDir->parts;
		for (int i = 0; i < pkgDir->numParts; ++i, ++pe) {
			pe->offset = BYTE_SWAP_LONG(pe->offset);
			pe->size = BYTE_SWAP_LONG(pe->size);
			pe->size2 = pe->size;
			pe->type = BYTE_SWAP_LONG(pe->type);
			pe->flags = BYTE_SWAP_LONG(pe->flags);
			pe->info.offset = BYTE_SWAP_SHORT(pe->info.offset);
			pe->info.length = BYTE_SWAP_SHORT(pe->info.length);
			pe->compressor.offset = BYTE_SWAP_SHORT(pe->compressor.offset);
			pe->compressor.length = BYTE_SWAP_SHORT(pe->compressor.length);
		}

		// fix up name & copyright in directory data area
		char * pkgData = (char *)pkgDir + sizeof(PackageDirectory) + pkgDir->numParts * sizeof(PartEntry);

		if (pkgDir->name.length > 0) {
			UniChar * s = (UniChar *)(pkgData + pkgDir->name.offset);
			for (ArrayIndex i = 0, count = pkgDir->name.length / sizeof(UniChar); i < count; ++i, ++s)
				*s = BYTE_SWAP_SHORT(*s);
		}
		if (pkgDir->copyright.length > 0) {
			UniChar * s = (UniChar *)(pkgData + pkgDir->copyright.offset);
			for (ArrayIndex i = 0, count = pkgDir->copyright.length / sizeof(UniChar); i < count; ++i, ++s)
				*s = BYTE_SWAP_SHORT(*s);
		}
#endif

		//	if it’s a "package1" with relocation info then read that relocation info
		if ((pkgDir->signature[kPackageMagicLen] == '1') && FLAGTEST(pkgDir->flags, kRelocationFlag)) {
			if (pkgMem != NULL) {
				memcpy(&pkgRelo, (char *)pkgMem + pkgDir->directorySize, sizeof(RelocationHeader));
			} else {
				// read relocation header
				fseek(pkgFile, pkgDir->directorySize, SEEK_SET);
				fread(&pkgRelo, sizeof(RelocationHeader), 1, pkgFile);
			}

#if defined(hasByteSwapping)
			pkgRelo.relocationSize = BYTE_SWAP_LONG(pkgRelo.relocationSize);
			pkgRelo.pageSize = BYTE_SWAP_LONG(pkgRelo.pageSize);
			pkgRelo.numEntries = BYTE_SWAP_LONG(pkgRelo.numEntries);
			pkgRelo.baseAddress = BYTE_SWAP_LONG(pkgRelo.baseAddress);
#endif

			// read relocation data into memory
			size_t relocationDataSize = pkgRelo.relocationSize - sizeof(RelocationHeader);
			relocationData = (char *)malloc(relocationDataSize);
			XFAILIF(relocationData == NULL, free(pkgDir); pkgDir = NULL;)
			if (pkgMem != NULL) {
				memcpy(relocationData, (char *)pkgMem + pkgDir->directorySize + sizeof(RelocationHeader), relocationDataSize);
			} else {
				fread(relocationData, relocationDataSize, 1, pkgFile);
			}
		}
		else {
			pkgRelo.relocationSize = 0;
		}
    if (pkgDir->numParts)
      align4flag = (char*)calloc(pkgDir->numParts, 1);
	}
	XENDTRY;
	return pkgDir;
}


/* -----------------------------------------------------------------------------
	Return part entry info.
	Args:		inPartNo		the part number, typically 0
	Return:	part info entry, platform byte-ordered; treat as read-only
----------------------------------------------------------------------------- */

const PartEntry *
NewtonPackage::partEntry(ArrayIndex inPartNo)
{
	// load the directory if necessary
	if (pkgDir == NULL) {
		directory();
	}
	// sanity check
	if (inPartNo >= pkgDir->numParts) {
		return NULL;
	}

	return &pkgDir->parts[inPartNo];
}


/* -----------------------------------------------------------------------------
	Return part data loaded from the .pkg file containing the ref.
	We assume the NewtonPackage is persistent (it certainly is for the Newton
	framework) but in the case of eg the Package Inspector we need to dispose
	the package data when the package inspector window is closed. To accomplish
	this we expose the underlying allocated part Ref data so it can be free()d.
	Args:		inPartNo		the part number, typically 0
	Return:	pointer to malloc()d Ref part data
----------------------------------------------------------------------------- */

MemAllocation *
NewtonPackage::partPkgData(ArrayIndex inPartNo)
{
	// load the directory if necessary
	if (pkgDir == NULL) {
		directory();
	}
	// sanity check
	if (inPartNo >= pkgDir->numParts) {
		return NULL;
	}

	return &pkgPartData[inPartNo];
}


/* -----------------------------------------------------------------------------
	Return the part Ref for a part in the package.
	Args:		inPartNo		the part number, typically 0
	Return:	part Ref
----------------------------------------------------------------------------- */

Ref
NewtonPackage::partRef(ArrayIndex inPartNo)
{
	// load the directory if necessary
	if (pkgDir == NULL) {
		directory();
	}
	// sanity check
	if (inPartNo >= pkgDir->numParts) {
		return NILREF;
	}

	// create the array of per-part partData pointers if necessary
	if (pkgPartData == NULL) {
		if (pkgDir->numParts == 1) {
			pkgPartData = &part0Data;
		} else {
			pkgPartData = (MemAllocation *)malloc(pkgDir->numParts*sizeof(MemAllocation));
		}
	}

	MemAllocation * pkgAllocation = pkgPartData + inPartNo;
	pkgAllocation->data = NULL;
	pkgAllocation->size = 0;

	const PartEntry * thePart = partEntry(inPartNo);

	// we can only handle NOS parts
	if ((thePart->flags & kPartTypeMask) != kNOSPart) {
		return NILREF;
	}

	// read part into memory
	char * partData;
	size_t partSize = thePart->size;
	partData = (char *)malloc(partSize);	// will leak if partRef called more than once for the same partNo
	if (partData == NULL)
		return NILREF;

	long partOffset = LONGALIGN(pkgDir->directorySize + pkgRelo.relocationSize + thePart->offset);
	if (pkgMem != NULL) {
		memcpy(partData, (char *)pkgMem + partOffset, partSize);
	} else {
		fseek(pkgFile, partOffset, SEEK_SET);
		fread(partData, partSize, 1, pkgFile);
	}

  // The Package file format uses byte 7 in the "nos" part data to let us
  // know if the alignment between objects is 8 bytes (0), or 4 bytes (1).
  // 4 byte alignment works only with NOS2.
  if (partSize >= 8) {
    align4flag[inPartNo] = partData[7];
  }

	// adjust any relocation info
	if (pkgRelo.relocationSize != 0)
	{
		long  delta = (long)partData - pkgRelo.baseAddress;
		RelocationEntry *	set = (RelocationEntry *)relocationData;
#if 0
		for (ArrayIndex setNum = 0; setNum < pkgRelo.numEntries; ++setNum) {
			int32_t * p, * pageBase = (int32_t *)(partData + CANONICAL_SHORT(set->pageNumber) * pkgRelo.pageSize);
			ArrayIndex offsetCount = CANONICAL_SHORT(set->offsetCount);
			for (ArrayIndex offsetNum = 0; offsetNum < offsetCount; ++offsetNum) {
				p = pageBase + set->offsets[offsetNum];
				*p = CANONICAL_LONG(*p) + delta;	// was -; but should be + surely?
			}
			set = (RelocationEntry *)((char *)set
					+ sizeof(UShort) * 2					// RelocationEntry less offsets
					+ LONGALIGN(offsetCount));
		}
#endif
	}

	// locate the part frame
	ArrayObject32 * pkgRoot = (ArrayObject32 *)partData;
#if __LP64__
/*
	So, now:
		partData = address of part data read from pkg file
		partOffset = offset into part of Ref data
	Refs in the .pkg file are offsets into the file and need to be fixed up to run-time addresses
*/
	ScanRefMap scanRefMap;
	size_t part64Size = ScanRef(CANONICAL_LONG(REF(partOffset)), partData, partOffset, scanRefMap);
	char * part64Data = (char *)malloc(part64Size);
	if (part64Data == NULL) {
		free(partData), partData = NULL;
		return NILREF;
	}

	ArrayObject * newRoot = (ArrayObject *)part64Data;
	ArrayObject * dstPtr = newRoot;
	RefMap map;
	CopyRef(CANONICAL_LONG(REF(partOffset)), partData, partOffset, dstPtr, map);
	UpdateRef(newRoot->slot, map);	// Ref offsets -> addresses

	// don’t need the 32-bit part data any more
	free(partData);
	// but we will need to free the 64-bit part data at some point
	pkgAllocation->data = part64Data;
	pkgAllocation->size = part64Size;
	// point to the 64-bit refs we want
	return newRoot->slot[0];
#else
	pkgAllocation->data = partData;
	pkgAllocation->size = partSize;

	// fix up those refs from offsets to addresses, and do byte-swapping as appropriate
	FixUpRef(pkgRoot->slot, partData - partOffset);

	// point to the refs we want
	return pkgRoot->slot[0];
#endif
}

static void SetFrameSlotFourCC(RefArg frame, RefArg symbol, ULong fourcc) {
	if ( (isprint(fourcc&0xff)) && (isprint((fourcc>>8)&0xff)) && (isprint((fourcc>>16)&0xff)) && (isprint((fourcc>>24)&0xff)) ) {
		union { ULong id; char idChars[5]; } idBuffer;
		idBuffer.id = htonl(fourcc);
		idBuffer.idChars[4] = '\0';	// null-terminate
		SetFrameSlot(frame, symbol, MakeStringFromCString(idBuffer.idChars));
	} else {
		ULong idValue = htonl(fourcc);
		Ref bin = AllocateBinary(SYMA(binary), 4);
		memcpy(BinaryData(bin), &idValue, 4);
		SetFrameSlot(frame, symbol, bin);
	}
}

/* -----------------------------------------------------------------------------
	Return a Ref for the entire package file.
	Return:	package Ref
----------------------------------------------------------------------------- */

Ref NewtonPackage::packageRef()
{
	PackageDirectory *dir = directory();
	if (dir == NULL)
		return NILREF;
	char * pkgData = (char *)dir + sizeof(PackageDirectory) + dir->numParts * sizeof(PartEntry);

	Ref package = AllocateFrame();
	if (strncmp(dir->signature, "package0", 8) == 0) {
		SetFrameSlot(package, MakeSymbol("signature"), MakeSymbol("package0"));
	} else if (strncmp(dir->signature, "package1", 8) == 0) {
		SetFrameSlot(package, MakeSymbol("signature"), MakeSymbol("package1"));
	} else {
		SetFrameSlot(package, MakeSymbol("signature"), MakeSymbol("unknown"));
	}
	SetFrameSlotFourCC(package, MakeSymbol("id"), dir->id);
	Ref flags = AllocateFrame();
	if (FLAGTEST(dir->flags, kAutoRemoveFlag)) {
		SetFrameSlot(flags, MakeSymbol("autoRemove"), TRUEREF);
	}
	if (FLAGTEST(dir->flags, kCopyProtectFlag)) {
		SetFrameSlot(flags, MakeSymbol("copyProtect"), TRUEREF);
	}
	if (FLAGTEST(dir->flags, kInvisibleFlag)) {
		SetFrameSlot(flags, MakeSymbol("invisible"), TRUEREF);
	}
	if (FLAGTEST(dir->flags, kNoCompressionFlag)) {
		SetFrameSlot(flags, MakeSymbol("noCompression"), TRUEREF);
	}
	if (FLAGTEST(dir->flags, kRelocationFlag)) {
		SetFrameSlot(flags, MakeSymbol("relocation"), TRUEREF);
	}
	if (FLAGTEST(dir->flags, kUseFasterCompressionFlag)) {
		SetFrameSlot(flags, MakeSymbol("useFasterCompression"), TRUEREF);
	}
	SetFrameSlot(package, MakeSymbol("flags"), flags);
	SetFrameSlot(package, MakeSymbol("version"), MAKEINT(dir->version));
	if (dir->copyright.length > 0) {
		UniChar *copyright = (UniChar *)(pkgData + dir->copyright.offset);
		SetFrameSlot(package, MakeSymbol("copyright"), MakeString(copyright));
	}
	if (dir->name.length) {
		UniChar *name = (UniChar *)(pkgData + dir->name.offset);
		SetFrameSlot(package, MakeSymbol("name"), MakeString(name));
	}
	SetFrameSlot(package, MakeSymbol("size"), MAKEINT(dir->size));
	SetFrameSlot(package, MakeSymbol("creationDate"), MAKEINT(dir->creationDate));
	SetFrameSlot(package, MakeSymbol("modifyDate"), MAKEINT(dir->modifyDate));
	SetFrameSlot(package, MakeSymbol("reserved3"), MAKEINT(dir->reserved3));
	SetFrameSlot(package, MakeSymbol("directorySize"), MAKEINT(dir->directorySize));
	// Creator Info goes from part part0info_end to directorySize
	PartEntry &lastPart = dir->parts[dir->numParts-1];
	const char *infoStart = pkgData + lastPart.info.offset + lastPart.info.length;
	const char *infoEnd = (char*)dir + dir->directorySize;
	size_t infoSize = infoEnd - infoStart;
	if (infoSize > 0) {
		SetFrameSlot(package, MakeSymbol("info"), MakeStringFromCString(infoStart, (int)infoSize));
	}

	Ref partArray = AllocateArray(SYMA(array), dir->numParts);
	for (ULong p = 0; p < dir->numParts; p++) {
		Ref partFrame = AllocateFrame();
		SetArraySlot(partArray, p, partFrame);
		const PartEntry *part = partEntry(p);
		Ref partRef = this->partRef(p);
		SetFrameSlot(partFrame, SYMA(offset), MAKEINT(part->offset));
		SetFrameSlot(partFrame, SYMA(size), MAKEINT(part->size));
		SetFrameSlotFourCC(partFrame, SYMA(type), part->type);
		Ref flags = AllocateFrame();
		switch (part->flags & kPartTypeMask) {
			case kProtocolPart:
				SetFrameSlot(flags, MakeSymbol("type"), MakeSymbol("protocol"));
				break;
      case kNOSPart:
        SetFrameSlot(flags, MakeSymbol("type"), MakeSymbol("nos"));
        if (align4flag[p])
          SetFrameSlot(flags, MakeSymbol("nos2"), TRUEREF);
        break;
			case kRawPart:
				SetFrameSlot(flags, MakeSymbol("type"), MakeSymbol("raw"));
				break;
			case kPackagePart:
				SetFrameSlot(flags, MakeSymbol("type"), MakeSymbol("package"));
				break;
		}
		if (FLAGTEST(part->flags, kAutoLoadPartFlag)) {
			SetFrameSlot(flags, MakeSymbol("autoLoad"), TRUEREF);
		}
		if (FLAGTEST(part->flags, kAutoRemovePartFlag)) {
			SetFrameSlot(flags, MakeSymbol("autoRemove"), TRUEREF);
		}
		if (FLAGTEST(part->flags, kCompressedFlag)) {
			SetFrameSlot(flags, MakeSymbol("compressed"), TRUEREF);
		}
		if (FLAGTEST(part->flags, kNotifyFlag)) {
			SetFrameSlot(flags, MakeSymbol("notify"), TRUEREF);
		}
		if (FLAGTEST(part->flags, kAutoCopyFlag)) {
			SetFrameSlot(flags, MakeSymbol("autoCopy"), TRUEREF);
		}
		SetFrameSlot(partFrame, MakeSymbol("flags"), flags);

    if (part->info.length > 0) {
      const char *start = pkgData + part->info.offset;
      int length = part->info.length;
      SetFrameSlot(partFrame, MakeSymbol("info"), MakeStringFromCString(start, length));
    }
    SetFrameSlot(partFrame, SYMA(data), partRef);
	}
	SetFrameSlot(package, MakeSymbol("part"), partArray);

	return package;
}

#pragma mark -

#if __LP64__
/* -----------------------------------------------------------------------------
	Pass 1: calculate space needed for 64-bit refs
			  no modifications to Ref data
	Refs are offsets from the start of the .pkg file and in big-endian byte-order

	Args:		inRef				32-bit Ref -- offset from start of .pkg file
				inPartAddr		address of part data
				inPartOffset	offset of part from start of .pkg file
				ioMap				map of Refs we have encountered
	Return:	memory requirement of 64-bit Ref
----------------------------------------------------------------------------- */

size_t
ScanRef(Ref32 inRef, const char * inPartAddr, long inPartOffset, ScanRefMap & ioMap)
{
	Ref32 ref = CANONICAL_LONG(inRef);
	if (ISREALPTR(ref)) {
		if (ioMap.count(ref) > 0) {
			// ignore this object if it has already been scanned
			return 0;
		}
		ioMap.insert(ref);

		ArrayObject32 * obj = (ArrayObject32 *)(inPartAddr + ((long)PTR(ref) - inPartOffset));
		size_t refSize = sizeof(ArrayObject);
		// for frames, class is actually the map which needs fixing too; non-pointer refs may need byte-swapping anyway so we always need to do this
		refSize += ScanRef(obj->objClass, inPartAddr, inPartOffset, ioMap);

		ArrayIndex objSize = CANONICAL_SIZE(obj->size);
		//	if it’s a frame / array, step through each slot / element adding those
		if ((obj->flags & kObjSlotted) != 0) {
			Ref32 * refPtr = obj->slot;
			for (ArrayIndex count = (objSize - sizeof(ArrayObject32)) / sizeof(Ref32); count > 0; --count, ++refPtr) {
				refSize += sizeof(Ref) + ScanRef(*refPtr, inPartAddr, inPartOffset, ioMap);
			}
		} else {
			refSize += (objSize - sizeof(BinaryObject32));
		}
		return LONGALIGN(refSize);
	}
	// else it’s an immediate
	return 0;
}


/* -----------------------------------------------------------------------------
	Pass 2: copy 32-bit package source -> 64-bit platform Refs
			  adjust byte-order while we’re at it
	Args:		inRef			Refs are offsets from inPartAddr and in big-endian byte-order
				inPartAddr
				inPartOffset
				ioDstPtr			platform-oriented Refs
				ioMap
	Return:	64-bit platform-endian Ref
----------------------------------------------------------------------------- */

#if defined(hasByteSwapping)
bool
IsSymClass(SymbolObject32 * inSym, const char * inClassName)
{
	char * subName = inSym->name;
	for ( ; *subName && *inClassName; subName++, inClassName++) {
		if (tolower(*subName) != tolower(*inClassName)) {
			return false;
		}
	}
	return (*inClassName == 0 && (*subName == 0 || *subName == '.'));
}
#endif



Ref
CopyRef(Ref32 inRef, const char * inPartAddr, long inPartOffset, ArrayObject * &ioDstPtr, RefMap & ioMap)
{
	Ref32 srcRef = CANONICAL_LONG(inRef);
	if (ISREALPTR(srcRef)) {
		RefMap::iterator findMapping = ioMap.find(srcRef);
		if (findMapping != ioMap.end()) {
			// we have already fixed up this ref -- return its pointer ref
			return findMapping->second;
		}
		// map 32-bit big-endian package-relative offset ref -> 64-bit pointer ref
		ArrayObject * dstPtr = ioDstPtr;
		Ref ref = REF(dstPtr);
		std::pair<Ref32, Ref> mapping = std::make_pair(srcRef, ref);
		ioMap.insert(mapping);

		ArrayObject32 * srcPtr = (ArrayObject32 *)(inPartAddr + ((long)PTR(srcRef) - inPartOffset));	// might not actually be an ArrayObject, but header/class are common to all pointer objects
		size_t srcSize = CANONICAL_SIZE(srcPtr->size);
		ArrayIndex count;
		size_t dstSize;
		if ((srcPtr->flags & kObjSlotted)) {
			//	work out size for 64-bit Ref slots
			count = (srcSize - sizeof(ArrayObject32)) / sizeof(Ref32);
			dstSize = sizeof(ArrayObject) + count * sizeof(Ref);
		} else {
			// adjust for change in header size
			dstSize = srcSize - sizeof(BinaryObject32) + sizeof(BinaryObject);
		}
		dstPtr->size = dstSize;
		dstPtr->flags = kObjReadOnly | (srcPtr->flags & kObjMask);
		dstPtr->gc.stuff = 0;

		//	update/align ioDstPtr to next object
		ioDstPtr = (ArrayObject *)((char *)ioDstPtr + LONGALIGN(dstSize));
		// for frames, class is actually the map which needs fixing too; non-slotted refs may need byte-swapping anyway so we always need to do this
		dstPtr->objClass = CopyRef(srcPtr->objClass, inPartAddr, inPartOffset, ioDstPtr, ioMap);

		if ((srcPtr->flags & kObjSlotted)) {
			//	iterate over src slots; fix them up
			Ref32 * srcRefPtr = srcPtr->slot;
			Ref * dstRefPtr = dstPtr->slot;
			for ( ; count > 0; --count, ++srcRefPtr, ++dstRefPtr) {
				*dstRefPtr = CopyRef(*srcRefPtr, inPartAddr, inPartOffset, ioDstPtr, ioMap);
			}
		} else {
			memcpy(dstPtr->slot, srcPtr->slot, dstSize - sizeof(ArrayObject));
#if defined(hasByteSwapping)
			SymbolObject32 * sym;
			Ref srcClass = CANONICAL_LONG(srcPtr->objClass);
			if (srcClass == kSymbolClass) {
				// symbol -- byte-swap hash
				SymbolObject * sym = (SymbolObject *)dstPtr;
				sym->hash = BYTE_SWAP_LONG(sym->hash);
//NSLog(@"'%s", sym->name);
			} else if (ISPTR(srcClass) && (sym = ((SymbolObject32 *)(inPartAddr + ((long)PTR(srcClass) - inPartOffset)))), sym->objClass == CANONICAL_LONG(kSymbolClass)) {
				if (IsSymClass(sym, "string")) {
					// string -- byte-swap UniChar characters
					UniChar * s = (UniChar *)dstPtr->slot;
					for (count = (dstSize - sizeof(StringObject)) / sizeof(UniChar); count > 0; --count, ++s)
						*s = BYTE_SWAP_SHORT(*s);
//NSLog(@"\"%@\"", [NSString stringWithCharacters:(const UniChar *)srcPtr->slot length:(dstSize - sizeof(StringObject32)) / sizeof(UniChar)]);
				} else if (IsSymClass(sym, "real")) {
					// real number -- byte-swap 64-bit double
					uint32_t tmp;
					uint32_t * dbp = (uint32_t *)dstPtr->slot;
					tmp = BYTE_SWAP_LONG(dbp[1]);
					dbp[1] = BYTE_SWAP_LONG(dbp[0]);
					dbp[0] = tmp;
				} else if (IsSymClass(sym, "UniC")) {
					// EncodingMap -- byte-swap UniChar characters
					UShort * table = (UShort *)dstPtr->slot;
					UShort formatId, unicodeTableSize;
					*table = formatId = BYTE_SWAP_SHORT(*table), ++table;
					if (formatId == 0) {
						// it’s 8-bit to UniCode
						*table = unicodeTableSize = BYTE_SWAP_SHORT(*table), ++table;
						*table = BYTE_SWAP_SHORT(*table), ++table;		// revision
						*table = BYTE_SWAP_SHORT(*table), ++table;		// tableInfo
						for (ArrayIndex i = 0; i < unicodeTableSize; ++i, ++table) {
							*table = BYTE_SWAP_SHORT(*table);
						}
					} else if (formatId == 4) {
						// it’s UniCode to 8-bit
						*table = BYTE_SWAP_SHORT(*table), ++table;		// revision
						*table = BYTE_SWAP_SHORT(*table), ++table;		// tableInfo
						*table = unicodeTableSize = BYTE_SWAP_SHORT(*table), ++table;
						for (ArrayIndex i = 0; i < unicodeTableSize*3; ++i, ++table) {
							*table = BYTE_SWAP_SHORT(*table);
						}
					}
				}
			}
#endif
		}
		return ref;
	}
	return srcRef;
}


/* -----------------------------------------------------------------------------
	Pass 3: fix up 64-bit refs at new addresses
	For pointer refs, fix up the class.
	For slotted refs, fix up all slots recursively.

	Args:		inRefPtr		pointer to ref to be fixed up
				ioMap			mapping of pointer Ref, pkg relative offset -> address
	Return:	--
----------------------------------------------------------------------------- */

void
UpdateRef(Ref * inRefPtr, RefMap & inMap)
{
	Ref ref = *inRefPtr;
	if (ISREALPTR(ref)) {
		RefMap::iterator mapping = inMap.find(ref);
		if (mapping != inMap.end()) {
			*inRefPtr = mapping->second;
			ArrayObject *  obj = (ArrayObject *)PTR(*inRefPtr);
			// for frames, class is actually the map which needs fixing too; non-pointer refs may need byte-swapping anyway so we always need to do this
			UpdateRef(&obj->objClass, inMap);

			//	if it’s a frame / array, step through each slot / element fixing those
			if ((obj->flags & kObjSlotted)) {
				Ref * refPtr = obj->slot;
				for (ArrayIndex count = ARRAYLENGTH(obj); count > 0; --count, ++refPtr) {
					UpdateRef(refPtr, inMap);
				}
			}
		}
	}
}

#else
#pragma mark -
/* -----------------------------------------------------------------------------
	FOR 32-BIT PLATFORM
	Fix up a ref: change offset -> address, adjust for platform byte order.
	For pointer refs, fix up the class.
	For slotted refs, fix up all slots recursively.

	Args:		inRefPtr		pointer to ref to be fixed up = offset from inBaseAddr
				inBaseAddr	base address of package from which refs are offsets
	Return:	--
----------------------------------------------------------------------------- */

#if defined(hasByteSwapping)
bool
IsObjClass(Ref obj, const char * inClassName)
{
	if (ISPTR(obj) && ((SymbolObject *)PTR(obj))->objClass == kSymbolClass) {
		const char * subName = SymbolName(obj);
		for ( ; *subName && *inClassName; subName++, inClassName++) {
			if (tolower(*subName) != tolower(*inClassName)) {
				return false;
			}
		}
		return (*inClassName == 0 && (*subName == 0 || *subName == '.'));
	}
	return false;
}
#endif


void
FixUpRef(Ref * inRefPtr, char * inBaseAddr)
{
#if defined(hasByteSwapping)
	// byte-swap ref in memory
	*inRefPtr = BYTE_SWAP_LONG(*inRefPtr);
#endif

	if (ISREALPTR(*inRefPtr) && *inRefPtr < (Ref)inBaseAddr)	// assume offset MUST be less than address
	{
		// ref has not been fixed from offset to address, so do it
		*inRefPtr += (Ref) inBaseAddr;

		ArrayObject * obj = (ArrayObject *)PTR(*inRefPtr);
		// if gc set, this object has already been fixed up
		if (obj->gc.stuff < 4) {
			obj->gc.stuff = 4;
			obj->size = CANONICAL_SIZE(obj->size);
			// for frames, class is actually the map which needs fixing too; non-pointer refs may need byte-swapping anyway so we always need to do this
			FixUpRef(&obj->objClass, inBaseAddr);

			//	if it’s a frame / array, step through each slot / element fixing those
			if ((obj->flags & kObjSlotted)) {
				Ref * refPtr = obj->slot;
				for (ArrayIndex count = (objSize - sizeof(ArrayObject)) / sizeof(Ref); count > 0; --count, ++refPtr) {
					FixUpRef(refPtr, inBaseAddr);
				}
			}
#if defined(hasByteSwapping)
			// else object is some form of binary; symbol, string, real or encoding table (for example) which needs byte-swapping
			else {
				Ref objClass = obj->objClass;
				if (objClass == kSymbolClass) {
					// symbol -- byte-swap hash
					SymbolObject * sym = (SymbolObject *)obj;
					sym->hash = BYTE_SWAP_LONG(sym->hash);
				} else if (IsObjClass(objClass, "string")) {
					// string -- byte-swap UniChar characters
					StringObject * str = (StringObject *)obj;
					UniChar * s = str->str;
					for (ArrayIndex count = (str->size - sizeof(StringObject32)) / sizeof(UniChar); count > 0; --count, ++s)
						*s = BYTE_SWAP_SHORT(*s);
				} else if (IsObjClass(objClass, "real")) {
					// real number -- byte-swap 64-bit double
					uint32_t tmp;
					uint32_t * dbp = (uint32_t *)((BinaryObject *)obj)->data;
					tmp = BYTE_SWAP_LONG(dbp[1]);
					dbp[1] = BYTE_SWAP_LONG(dbp[0]);
					dbp[0] = tmp;
				} else if (IsObjClass(objClass, "UniC")) {
					// EncodingMap -- byte-swap UniChar characters
					UShort * table = (UShort *)&obj->slot[0];
					UShort formatId, unicodeTableSize;

					*table = formatId = BYTE_SWAP_SHORT(*table), ++table;
					if (formatId == 0) {
						// it’s 8-bit to UniCode
						*table = unicodeTableSize = BYTE_SWAP_SHORT(*table), ++table;
						*table = BYTE_SWAP_SHORT(*table), ++table;		// revision
						*table = BYTE_SWAP_SHORT(*table), ++table;		// tableInfo
						for (ArrayIndex i = 0; i < unicodeTableSize; ++i, ++table) {
							*table = BYTE_SWAP_SHORT(*table);
						}
					} else if (formatId == 4) {
						// it’s UniCode to 8-bit
						*table = BYTE_SWAP_SHORT(*table), ++table;		// revision
						*table = BYTE_SWAP_SHORT(*table), ++table;		// tableInfo
						*table = unicodeTableSize = BYTE_SWAP_SHORT(*table), ++table;
						for (ArrayIndex i = 0; i < unicodeTableSize*3; ++i, ++table) {
							*table = BYTE_SWAP_SHORT(*table);
						}
					}
				}
			}
#endif
		}
	}
}

#endif

