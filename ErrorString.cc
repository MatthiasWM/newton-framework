

#include <filesystem>
#include <string>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <map>

extern "C" const char * GetFramesErrorString(int inErr);
extern "C" const char * GetMagicPointerString(int inMP);
extern "C" const char * StoreBackingFile(const char * inStoreName);

/* -----------------------------------------------------------------------------
	Return a string describing an error code.
	Args:		inErr		a NewtonErr code
	Return:	C string
----------------------------------------------------------------------------- */

extern const std::map<int, std::string> error_LUT;

const char *
GetFramesErrorString(int inErr)
{
#if 1
	// There seems no such table, so we just return the decimal
  auto errMsg = error_LUT.find(inErr);
  if (errMsg != error_LUT.end()) {
    return errMsg->second.c_str();
  }

	static char buffer[32];
	snprintf(buffer, 32, "%d", inErr);
	return buffer;
#else
	NSString * key = [NSString stringWithFormat: @"%d", inErr];
	NSString * errStr = NSLocalizedStringFromTable(key, @"Error", NULL);
	return errStr.UTF8String;
#endif
}


/* -----------------------------------------------------------------------------
	Return a string describing a magic pointer.
	Args:		inMP		a magic pointer
	Return:	C string
----------------------------------------------------------------------------- */

const char *
GetMagicPointerString(int inMP)
{
#if 1
	// There seems no such table, so we just return the decimal, prepended with the '@'
	static char buffer[32];
	snprintf(buffer, 32, "@%d", inMP);
	return buffer;
#else
	NSString * key = [NSString stringWithFormat: @"%d", inMP];
	NSString * mpStr = NSLocalizedStringFromTable(key, @"MagicPointer", NULL);
	if (isdigit([mpStr characterAtIndex:0])) {
		mpStr = [@"@" stringByAppendingString:mpStr];
	}
	return mpStr.UTF8String;
#endif
}


/* -----------------------------------------------------------------------------
	While we’re in the Cocoa environment:
	Return the URL to the Application Support directory.
	Args:		--
	Return:	a URL
----------------------------------------------------------------------------- */

#if 1

std::filesystem::path ApplicationSupportFolder() {
#ifdef _WIN32
	auto path = std::filesystem::path(std::getenv("APPDATA")) / "Newton"; // On Windows
#elif defined(__APPLE__)
	auto path = std::filesystem::path(std::getenv("HOME")) / "Library" / "Application Support" / "Newton"; // On macOS
#else
	auto path = std::filesystem::path(std::getenv("HOME")) / ".local" / "share" / "Newton"; // On Linux
#endif
	try {
		std::filesystem::create_directories(path);
	} catch (const std::filesystem::filesystem_error& e) {
		std::cerr << "Can't create application support folder " << path << ": " << e.what() << std::endl;
	} catch (const std::exception& e) {
		std::cerr << "Error creating application support folder " << path << ": " << e.what() << std::endl;
	}
	return path;
}

#else

NSURL *
ApplicationSupportFolder(void)
{
	NSFileManager * fmgr = [NSFileManager defaultManager];
	NSURL * baseURL = [fmgr URLForDirectory:NSApplicationSupportDirectory inDomain:NSUserDomainMask appropriateForURL:nil create:YES error:nil];
	NSURL * appFolder = [baseURL URLByAppendingPathComponent:@"Newton"];
	// if folder doesn’t exist, create it
	[fmgr createDirectoryAtURL:appFolder withIntermediateDirectories:NO attributes:nil error:nil];
	return appFolder;
}

#endif

/* -----------------------------------------------------------------------------
	Return the path to the store backing file.
	Args:		inStoreName		*unique* name of the store
	Return:	C string			will be autoreleased by ARC
----------------------------------------------------------------------------- */

const char *
StoreBackingFile(const char * inStoreName)
{
#if 1
	static char buffer[2048];
	auto path = ApplicationSupportFolder() / inStoreName;
	strcpy(buffer, (char*)path.c_str());
	return buffer;
#else
	NSURL * url = [ApplicationSupportFolder() URLByAppendingPathComponent:[NSString stringWithUTF8String:inStoreName]];
	return url.fileSystemRepresentation;
#endif
}


#include "ROMData/NTK.Framework/Headers/NewtonErrors.h"

const std::map<int, std::string> error_LUT =
{
  { kMemErrBogusBlockType, "e.g. not free, direct or indirect" },
  { kMemErrUnalignedPointer, "pointer not aligned to 4-byte boundary" },
  { kMemErrPointerOutOfHeap, "pointer to outside of heap" },
  { kMemErrBogusInternalBlockType, "Unknown infrastructure type" },
  { kMemErrMisplacedFreeBlock, "free block where there shouldn't be one" },
  { kMemErrBadFreelistPointer, "free-list pointer points outside of heap" },
  { kMemErrFreelistPointerPointsAtJunk, "free-list pointer doesn't point at a free block" },
  { kMemErrBadForwardMarch, "Invalid block size" },
  { kMemErrBogusBlockSize, "forbidden bits set in block-size" },
  { kMemErrBlockSizeLessThanMinimum, "heap blocks have a certain minimum size..." },
  { kMemErrPreposterousBlockSize, "heap block too large (>2GB) probably don't" },
  { kMemErrBogusFreeCount, "total free is bigger than entire heap" },
  { kMemErrBadNilPointer, "Nil pointer where not allowed" },
  { kMemErrFreeSpaceDisagreement1, "tracked -vs- actual free-space is different" },
  { kMemErrFreeSpaceDisagreement2, "tracked -vs- linked free-space is different" },
  { kMemErrBadMasterPointer, "master pointer doesn't point back to handle block" },
  { kMemErrBadBlockDeltaSize, "bad block-size adjustment" },
  { kMemErrBadInternalBlockType, "possibly mangled internal block" },
  { kMemErrHeapCorruptErr, "The heap is invalid [apparently whacked?]" },
  { kMemErrExceptionGrokkingHeap, "caught an exception checking the heap [this is bad]" },
  { kMemErrBadHeapHeader, "Invalid heap header" },
  { kNSErrNotAFrameStore, "The PCMCIA card is not a data storage card" },
  { kNSErrOldStoreFormat, "Store format is too old to understand" },
  { kNSErrNewStoreFormat, "Store format is too new to understand" },
  { kNSErrStoreCorrupted, "Store is corrupted, can't recover" },
  { kNSErrObjectCorrupted, "Single object is corrupted, can't recover" },
  { kNSErrUnknownStreamFormat, "Object stream has unknown format version" },
  { kNSErrInvalidFaultBlock, "Fault block is invalid (probably from a removed store)" },
  { kNSErrNotAFaultBlock, "Not a fault block (internal error)" },
  { kNSErrNotASoupEntry, "Not a soup entry" },
  { kNSErrStoreNotRegistered, "Tried to remove a store that wasn't registered" },
  { kNSErrUnknownIndexType, "Soup index has an unknown type" },
  { kNSErrUnknownKeyStructure, "Soup index has an unknown key structure" },
  { kNSErrNoSuchIndex, "Soup index does not exist" },
  { kNSErrDuplicateSoupName, "A soup with this name already exists" },
  { kNSErrCantCopyToUnionSoup, "Tried to CopyEntries to a union soup" },
  { kNSErrInvalidSoup, "Soup is invalid (probably from a removed store)" },
  { kNSErrInvalidStore, "entry is invalid (probably from a removed store)" },
  { kNSErrInvalidEntry, "Entry is invalid (probably from a removed store)" },
  { kNSErrKeyHasWrongType, "Key does not have the type specified in the index" },
  { kNSErrStoreIsROM, "Store is in ROM" },
  { kNSErrDuplicateIndex, "Soup already has an index with this path" },
  { kNSErrInternalError, "Internal error--something unexpected happened" },
  { kNSErrCantRemoveUIdIndex, "Tried to RemoveIndex the _uniqueId index" },
  { kNSErrInvalidQueryType, "Query type missing or unknown" },
  { kNSErrIndexCorrupted, "Discovered index inconsistency" },
  { kNSErrInvalidTagsCount, "max tags count has been reached" },
  { kNSErrNoTags, "soup does not have tags (PlainSoupModifyTag, PlainSoupRemoveTags)" },
  { kNSErrInvalidTagSpec, "tagSpec frame is invalid" },
  { kNSErrWrongStoreVersion, "Store cannot handle the feature (e.g. large objects)" },
  { kNSErrInvalidSorting, "indexDesc requests an unknown sorting table" },
  { kNSErrInvalidUnion, "can not UnionSoup (different sorting tables)" },
  { kNSErrBadIndexDesc, "Bad index description" },
  { kNSErrVBOKey, "Soup entries keys can not be virtual binaries" },
  { kNSErrInvalidSoupName, "Soup name is too long" },
  { kNSErrObjectPointerOfNonPtr, "ObjectPtr of non-pointer" },
  { kNSErrBadMagicPointer, "Bad magic pointer" },
  { kNSErrEmptyPath, "Empty path" },
  { kNSErrBadSegmentInPath, "Invalid segment in path expression" },
  { kNSErrPathFailed, "Path failed" },
  { kNSErrOutOfBounds, "Index out of bounds (string or array)" },
  { kNSErrObjectsNotDistinct, "Source and dest must be different objects" },
  { kNSErrLongOutOfRange, "Long out of range" },
  { kNSErrSettingHeapSizeTwice, "Call SetObjectHeapSize only once, before InitObjects" },
  { kNSErrGcDuringGc, "GC during GC...this is bad!" },
  { kNSErrBadArgs, "Bad args" },
  { kNSErrStringTooBig, "String too big" },
  { kNSErrTFramesObjectPtrOfNil, "TFramesObjectPtr of NIL" },
  { kNSErrUnassignedTFramesObjectPtr, "unassigned TFramesObjectPtr" },
  { kNSErrObjectReadOnly, "Object is read-only" },
  { kNSErrReturn, "Return Error" },
  { kNSErrOutOfObjectMemory, "Ran out of heap memory" },
  { kNSErrDerefMagicPointer, "Magic pointers cannot be dereferenced in Fram" },
  { kNSErrNegativeLength, "Negative length" },
  { kNSErrOutOfRange, "Value out of range" },
  { kNSErrCouldntResizeLockedObject, "Couldn't resize locked object" },
  { kNSErrBadPackageRef, "Reference to deactivated package" },
  { kNSErrBadExceptionName, "Exception not a subexception of |evt.ex|" },
  { kNSErrBadStream, "Invalid item encountered in stream" },
  { kNSErrFuncInStream, "Function object encountered in stream" },
  { kNSErrNotAFrame, "Expected a frame" },
  { kNSErrNotAnArray, "Expected an array" },
  { kNSErrNotAString, "Expected a string" },
  { kNSErrNotAPointer, "Expected a pointer, array, or binary object" },
  { kNSErrNotANumber, "Expected a number" },
  { kNSErrNotAReal, "Expected a real" },
  { kNSErrNotAnInteger, "Expected an integer" },
  { kNSErrNotACharacter, "Expected a character" },
  { kNSErrNotABinaryObject, "Expected a binary object" },
  { kNSErrNotAPathExpr, "Expected a path expression (or a symbol or integer)" },
  { kNSErrNotASymbol, "Expected a symbol" },
  { kNSErrNotAFunction, "Expected a function" },
  { kNSErrNotAFrameOrArray, "Expected a frame or an array" },
  { kNSErrNotAnArrayOrNil, "Expected an array or NIL" },
  { kNSErrNotAStringOrNil, "Expected a string or NIL" },
  { kNSErrNotABinaryObjectOrNil, "Expected a binary object or NIL" },
  { kNSErrUnexpectedFrame, "Unexpected frame" },
  { kNSErrUnexpectedBinaryObject, "Unexpected binary object" },
  { kNSErrUnexpectedImmediate, "Unexpected immediate" },
  { kNSErrNotAnArrayOrString, "Expected an array or string" },
  { kNSErrNotAVBO, "Expected a vbo" },
  { kNSErrNotAPackage, "Expected a package" },
  { kNSErrNotNil, "Expected a NIL" },
  { kNSErrNotASymbolOrNil, "Expected NIL or a symbol" },
  { kNSErrNotTrueOrNil, "Expected NIL or True" },
  { kNSErrNotAnIntegerOrArray, "Expected an integer or an array" },
  { kNSErrNotAPlainString, "Expected a non-rich string" },
  { kNSErrNoREP, "could not open a listener window" },
  { kNSErrSyntaxError, "syntax error" },
  { kNSErrBadAssign, "cannot assign to a constant" },
  { kNSErrBadSubscriptTest, "cannot test for subscript existence; use length" },
  { kNSErrGlobalAndConstCollision, "cannot have a global variable and a global constant with the same name" },
  { kNSErrConstRedefined, "cannot redefine a constant" },
  { kNSErrVarAndConstCollision, "variable and constant have same name" },
  { kNSErrNonLiteralConst, "non-literal expression for constant initializer" },
  { kNSErrEOFInAString, "end if input inside a string" },
  { kNSErrBadEscape, "no escapes but \\u are allowed after \\u" },
  { kNSErrIllegalEscape, "no escapes but \\u are allowed after \\u" },
  { kNSErrBadHex, "only hex digits allowed within \\u" },
  { kNSErrLineRequired, "line required after #" },
  { kNSErrNumberRequired, "number required after #line" },
  { kNSErrSpaceRequired, "space required after number" },
  { kNSErr2HexDigitsRequired, "2 hex digits required" },
  { kNSErr4HexDigitsRequired, "4 hex digits required" },
  { kNSErrUnrecognized, "unrecognized token in input stream" },
  { kNSErrInvalidHex, "invalid hexadecimal imteger" },
  { kNSErrInvalidDecimal, "invalid decimal imteger" },
  { kNSErrBadPath, "expected '.' in path expression" },
  { kNSErrNumberTooLong, "buffer length exceeded" },
  { kNSErrHashForbidden, "#xxxx not allowed from NTK" },
  { kNSErrDigitRequired, "Decimal digit required after @" },
  { kNSErrNotInBreakLoop, "Not in a break loop" },
  { kNSErrTooManyArgs, "Too many args for a CFunction" },
  { kNSErrWrongNumberOfArgs, "Wrong number of args" },
  { kNSErrZeroForLoopIncr, "For loop by expression has value zero" },
  { kNSErrUndefinedBytecode, "Bytecode is unknown to interpreter" },
  { kNSErrNoCurrentException, "No current exception" },
  { kNSErrUndefinedVariable, "Undefined variable" },
  { kNSErrUndefinedGlobalFunction, "Undefined global function" },
  { kNSErrUndefinedMethod, "Undefined method" },
  { kNSErrMissingProtoForResend, "No _proto for inherited send" },
  { kNSErrNilContext, "Tried to access slot in NIL context" },
  { kNSErrBadCharForString, "The operation would make the (rich) string invalid" },
};

