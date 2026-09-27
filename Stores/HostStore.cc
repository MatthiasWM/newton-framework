/*
	File:		HostStore.cc

	Contains:	A CStore in the host's memory, saved to a file. See
					HostStore.h.

	Written for newtc.
*/

#include "Objects.h"
#include "HostStore.h"
#include "OSErrors.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

// The store's root object (as CFlashStore's); new objects are numbered
// after it.
#define kHostRootId		39

// The file: "NEWTCSTO", a version, the root id, the next id, the number of
// objects, then each object: its id, its size, its bytes. Numbers are 32
// bits, little-endian.
static const char	kFileMagic[8] = { 'N','E','W','T','C','S','T','O' };
static const ULong	kFileVersion = 1;

static std::string	gHostStoreFile;
static CHostStore *	gHostStore = NULL;	// the one made, for SaveNow()


/* -----------------------------------------------------------------------------
	CHostStore implementation class info.
----------------------------------------------------------------------------- */

const CClassInfo *
CHostStore::classInfo(void)
{
	static CClassInfo _classInfo = {
		.fName = "CHostStore",
		.fInterface = "CStore",
		.fSignature = "\0",
		.fSizeofProc = []()->size_t { return sizeof(CHostStore); },
		.fAllocProc = []()->CProtocol* { return new CHostStore(); },
		.fFreeProc = [](CProtocol* p)->void { delete p; },
		.fVersion = 0,
		.fFlags = 0
	};
	return &_classInfo;
}

PROTOCOL_IMPL_SOURCE_MACRO(CHostStore)


void
CHostStore::SetFile(const char * inPath)
{
	gHostStoreFile = inPath ? inPath : "";
}


void
CHostStore::SaveNow(void)
{
	if (gHostStore)
		gHostStore->save();
}


CHostStore *
CHostStore::make(void)
{
	fNextId = kHostRootId + 1;
	fLockCount = 0;
	fLockROCount = 0;
	fInTransaction = false;
	fFormatted = false;
	fDirty = false;
	gHostStore = this;
	static bool atExit = false;
	if (!atExit) {
		atExit = true;
		atexit(SaveNow);
	}
	return this;
}


void
CHostStore::destroy(void)
{
	save();
	if (gHostStore == this)
		gHostStore = NULL;
}


NewtonErr
CHostStore::init(void * inStoreData, size_t inStoreSize, ULong inArg3, ArrayIndex inSocketNumber, ULong inFlags, void * inPSSInfo)
{
	fFormatted = load();
	return noErr;
}


NewtonErr
CHostStore::needsFormat(bool * outNeedsFormat)
{
	*outNeedsFormat = !fFormatted;
	return noErr;
}


NewtonErr
CHostStore::format(void)
{
	fObjects.clear();
	fJournal.clear();
	fSeparate.clear();
	fObjects[kHostRootId] = Bytes();		// an empty root object
	fNextId = kHostRootId + 1;
	fInTransaction = false;
	fLockCount = 0;
	fFormatted = true;
	fDirty = true;
	save();
	return noErr;
}


NewtonErr
CHostStore::getRootId(PSSId * outRootId)
{
	*outRootId = kHostRootId;
	return noErr;
}


/* -----------------------------------------------------------------------------
	Transactions: keep an object's contents before its first change.
----------------------------------------------------------------------------- */

void
CHostStore::willChange(PSSId inObjectId)
{
	fDirty = true;
	if (fLockCount == 0 || fSeparate.count(inObjectId) || fJournal.count(inObjectId))
		return;	// not in a transaction, or its contents before are kept already
	auto obj = fObjects.find(inObjectId);
	fJournal[inObjectId] = (obj == fObjects.end()) ? Original{false, Bytes()} : Original{true, obj->second};
	fInTransaction = true;
}


void
CHostStore::restore(PSSId inObjectId, const Original & inOriginal)
{
	if (inOriginal.exists)
		fObjects[inObjectId] = inOriginal.bytes;
	else
		fObjects.erase(inObjectId);
}


NewtonErr
CHostStore::lockStore(void)
{
	fLockCount++;
	return noErr;
}


NewtonErr
CHostStore::unlockStore(void)
{
	if (fLockCount == 1 && fInTransaction) {
		// commit
		fJournal.clear();
		fInTransaction = false;
		fLockCount = 0;
		save();
	} else if (fLockCount > 0)
		fLockCount--;
	return noErr;
}


NewtonErr
CHostStore::abort(void)
{
	if (fInTransaction) {
		for (auto & entry : fJournal)
			restore(entry.first, entry.second);
		fJournal.clear();
		fInTransaction = false;
	}
	fLockCount = 0;
	return noErr;
}


bool
CHostStore::inTransaction(void)
{
	return fInTransaction;
}


NewtonErr
CHostStore::newWithinTransaction(PSSId * outObjectId, size_t inSize)
{
	NewtonErr err = newObject(outObjectId, inSize);
	if (err == noErr) {
		fJournal.erase(*outObjectId);
		fSeparate[*outObjectId] = Original{false, Bytes()};
	}
	return err;
}


NewtonErr
CHostStore::startTransactionAgainst(PSSId inObjectId)
{
	auto obj = fObjects.find(inObjectId);
	if (obj == fObjects.end())
		return kStoreErrObjectNotFound;
	if (!fSeparate.count(inObjectId)) {
		auto kept = fJournal.find(inObjectId);
		if (kept != fJournal.end()) {		// its contents before the main transaction
			fSeparate[inObjectId] = kept->second;
			fJournal.erase(kept);
		} else
			fSeparate[inObjectId] = Original{true, obj->second};
	}
	return noErr;
}


NewtonErr
CHostStore::separatelyAbort(PSSId inObjectId)
{
	auto kept = fSeparate.find(inObjectId);
	if (kept == fSeparate.end())
		return kStoreErrObjectNotFound;
	restore(inObjectId, kept->second);
	fSeparate.erase(kept);
	fDirty = true;
	return noErr;
}


NewtonErr
CHostStore::addToCurrentTransaction(PSSId inObjectId)
{
	lockStore();		// as CFlashStore: the caller unlocks
	auto kept = fSeparate.find(inObjectId);
	if (kept != fSeparate.end()) {
		if (!fJournal.count(inObjectId))
			fJournal[inObjectId] = kept->second;
		fSeparate.erase(kept);
	} else if (!fObjects.count(inObjectId))
		return kStoreErrObjectNotFound;
	fInTransaction = true;
	return noErr;
}


bool
CHostStore::inSeparateTransaction(PSSId inObjectId)
{
	return fSeparate.count(inObjectId) != 0;
}


/* -----------------------------------------------------------------------------
	Objects.
----------------------------------------------------------------------------- */

NewtonErr
CHostStore::newObject(PSSId * outObjectId, size_t inSize)
{
	return newObject(outObjectId, NULL, inSize);
}


NewtonErr
CHostStore::newObject(PSSId * outObjectId, void * inData, size_t inSize)
{
	inSize &= ~0x80000000;		// CFlashStore: a flag in the size
	PSSId id = fNextId++;
	willChange(id);
	Bytes & bytes = fObjects[id];
	bytes.assign(inSize, 0);
	if (inData)
		memcpy(bytes.data(), inData, inSize);
	*outObjectId = id;
	return noErr;
}


NewtonErr
CHostStore::replaceObject(PSSId inObjectId, void * inData, size_t inSize)
{
	NewtonErr err = setObjectSize(inObjectId, inSize);
	if (err == noErr)
		err = write(inObjectId, 0, inData, inSize & ~0x80000000);
	return err;
}


NewtonErr
CHostStore::eraseObject(PSSId inObjectId)
{
	return noErr;
}


NewtonErr
CHostStore::deleteObject(PSSId inObjectId)
{
	if (!fObjects.count(inObjectId))
		return kStoreErrObjectNotFound;
	willChange(inObjectId);
	fObjects.erase(inObjectId);
	return noErr;
}


NewtonErr
CHostStore::setObjectSize(PSSId inObjectId, size_t inSize)
{
	inSize &= ~0x80000000;
	auto obj = fObjects.find(inObjectId);
	if (obj == fObjects.end())
		return kStoreErrObjectNotFound;
	if (obj->second.size() != inSize) {
		willChange(inObjectId);
		fObjects[inObjectId].resize(inSize, 0);
	}
	return noErr;
}


NewtonErr
CHostStore::getObjectSize(PSSId inObjectId, size_t * outSize)
{
	auto obj = fObjects.find(inObjectId);
	if (obj == fObjects.end())
		return kStoreErrObjectNotFound;
	*outSize = obj->second.size();
	return noErr;
}


NewtonErr
CHostStore::write(PSSId inObjectId, size_t inStartOffset, void * inBuffer, size_t inLength)
{
	auto obj = fObjects.find(inObjectId);
	if (obj == fObjects.end())
		return kStoreErrObjectNotFound;
	if (inStartOffset > obj->second.size() || inStartOffset + inLength > obj->second.size())
		return kStoreErrObjectOverRun;
	willChange(inObjectId);
	memcpy(fObjects[inObjectId].data() + inStartOffset, inBuffer, inLength);
	return noErr;
}


NewtonErr
CHostStore::read(PSSId inObjectId, size_t inStartOffset, void * outBuffer, size_t inLength)
{
	auto obj = fObjects.find(inObjectId);
	if (obj == fObjects.end())
		return kStoreErrObjectNotFound;
	size_t size = obj->second.size();
	if (inStartOffset > size)
		return kStoreErrObjectOverRun;
	if (inStartOffset + inLength > size) {	// as CFlashStore: what there is, and an error
		memcpy(outBuffer, obj->second.data() + inStartOffset, size - inStartOffset);
		return kStoreErrObjectOverRun;
	}
	memcpy(outBuffer, obj->second.data() + inStartOffset, inLength);
	return noErr;
}


NewtonErr
CHostStore::getStoreSize(size_t * outTotalSize, size_t * outUsedSize)
{
	size_t used = 0;
	for (auto & obj : fObjects)
		used += obj.second.size() + 16;
	*outUsedSize = used;
	*outTotalSize = used + 64 * 1024 * 1024;	// plenty: the host's memory
	return noErr;
}


/* -----------------------------------------------------------------------------
	The file: the committed state (what a transaction changed, as it was).
----------------------------------------------------------------------------- */

static void
PutLong(FILE * inFile, ULong inValue)
{
	unsigned char b[4] = { (unsigned char)inValue, (unsigned char)(inValue >> 8),
								  (unsigned char)(inValue >> 16), (unsigned char)(inValue >> 24) };
	fwrite(b, 1, 4, inFile);
}


static bool
GetLong(FILE * inFile, ULong * outValue)
{
	unsigned char b[4];
	if (fread(b, 1, 4, inFile) != 4)
		return false;
	*outValue = b[0] | (b[1] << 8) | (b[2] << 16) | ((ULong)b[3] << 24);
	return true;
}


bool
CHostStore::save(void)
{
	if (gHostStoreFile.empty() || !fDirty || !fFormatted)
		return true;
	// the committed state: objects as they were before open transactions
	std::map<PSSId, const Bytes *> committed;
	for (auto & obj : fObjects)
		committed[obj.first] = &obj.second;
	for (auto * kept : { &fJournal, &fSeparate })
		for (auto & entry : *kept) {
			if (entry.second.exists)
				committed[entry.first] = &entry.second.bytes;
			else
				committed.erase(entry.first);
		}
	std::string temp = gHostStoreFile + ".tmp";
	FILE * file = fopen(temp.c_str(), "wb");
	if (file == NULL) {
		fprintf(stderr, "newtc: can't write the store %s\n", temp.c_str());
		return false;
	}
	fwrite(kFileMagic, 1, sizeof(kFileMagic), file);
	PutLong(file, kFileVersion);
	PutLong(file, kHostRootId);
	PutLong(file, fNextId);
	PutLong(file, (ULong)committed.size());
	for (auto & obj : committed) {
		PutLong(file, obj.first);
		PutLong(file, (ULong)obj.second->size());
		fwrite(obj.second->data(), 1, obj.second->size(), file);
	}
	bool ok = fclose(file) == 0 && rename(temp.c_str(), gHostStoreFile.c_str()) == 0;
	if (ok)
		fDirty = false;
	else
		fprintf(stderr, "newtc: can't write the store %s\n", gHostStoreFile.c_str());
	return ok;
}


bool
CHostStore::load(void)
{
	if (gHostStoreFile.empty())
		return false;
	FILE * file = fopen(gHostStoreFile.c_str(), "rb");
	if (file == NULL)
		return false;			// a new store
	char magic[sizeof(kFileMagic)];
	ULong version, rootId, nextId, count;
	bool ok = fread(magic, 1, sizeof(magic), file) == sizeof(magic)
			 && memcmp(magic, kFileMagic, sizeof(magic)) == 0
			 && GetLong(file, &version) && version == kFileVersion
			 && GetLong(file, &rootId) && rootId == kHostRootId
			 && GetLong(file, &nextId) && GetLong(file, &count);
	for (ULong i = 0; ok && i < count; ++i) {
		ULong id, size;
		ok = GetLong(file, &id) && GetLong(file, &size);
		if (ok) {
			Bytes & bytes = fObjects[id];
			bytes.resize(size);
			ok = fread(bytes.data(), 1, size, file) == size;
		}
	}
	fclose(file);
	if (!ok) {
		fprintf(stderr, "newtc: %s is not a newtc store; starting a new one\n", gHostStoreFile.c_str());
		fObjects.clear();
		return false;
	}
	fNextId = nextId;
	return true;
}


/* -----------------------------------------------------------------------------
	The rest: what a store in memory doesn't need.
----------------------------------------------------------------------------- */

NewtonErr CHostStore::isReadOnly(bool * outIsReadOnly) { *outIsReadOnly = false; return noErr; }
NewtonErr CHostStore::idle(bool * outArg1, bool * outArg2) { *outArg1 = false; *outArg2 = false; return noErr; }
NewtonErr CHostStore::nextObject(PSSId inObjectId, PSSId * outNextObjectId) { *outNextObjectId = 0; return noErr; }
NewtonErr CHostStore::checkIntegrity(ULong * inArg1) { return noErr; }
NewtonErr CHostStore::setBuddy(CStore * inStore) { return noErr; }
bool CHostStore::ownsObject(PSSId inObjectId) { return true; }
VAddr CHostStore::address(PSSId inObjectId) { return 0; }
const char * CHostStore::storeKind(void) { return "Internal"; }
NewtonErr CHostStore::setStore(CStore * inStore, ObjectId inEnvironment) { return noErr; }
bool CHostStore::isSameStore(void * inData, size_t inSize) { return false; }
bool CHostStore::isLocked(void) { return fLockCount > 0; }
bool CHostStore::isROM(void) { return false; }
NewtonErr CHostStore::vppOff(void) { return noErr; }
NewtonErr CHostStore::sleep(void) { return noErr; }
NewtonErr CHostStore::lockReadOnly(void) { fLockROCount++; return noErr; }
NewtonErr CHostStore::unlockReadOnly(bool inReset) { if (inReset) fLockROCount = 0; else if (fLockROCount > 0) fLockROCount--; return noErr; }
NewtonErr CHostStore::calcXIPObjectSize(long inArg1, long inArg2, long * outArg3) { return kOSErrXIPNotPossible; }
NewtonErr CHostStore::newXIPObject(PSSId * outObjectId, size_t inSize) { return kOSErrXIPNotPossible; }
NewtonErr CHostStore::getXIPObjectInfo(PSSId inObjectId, unsigned long * outArg2, unsigned long * outArg3, unsigned long * outArg4) { return kOSErrXIPNotPossible; }
