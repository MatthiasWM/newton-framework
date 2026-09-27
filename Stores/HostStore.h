/*
	File:		HostStore.h

	Contains:	A CStore that keeps its objects in the host's memory and
					saves them to a file (newtc; replaces the flash store
					for the internal store).

	The Newton's internal store is a flash chip: CFlashStore keeps objects
	in a log-structured layout of blocks, with a flash driver below. newtc
	only needs what CStore promises: numbered objects of bytes, a root
	object, and transactions. Everything above (soups, entries, indexes,
	cursors, large binaries) stays Apple's.

	In memory: a map from PSSId to the object's bytes. In a file (if one is
	set, SetFile(); newtc -store): all objects, written to a temporary file
	that then replaces the old one, whenever a transaction commits and when
	newtc exits. Without a file the store starts empty every time.

	Transactions (as CFlashStore): changes while the store is locked
	(lockStore) belong to the transaction; the outermost unlockStore
	commits them, abort() takes them back. An object can also be in a
	transaction of its own (startTransactionAgainst, newWithinTransaction):
	separatelyAbort() takes back just that object, addToCurrentTransaction()
	joins it to the main one. Both keep the objects' previous contents; the
	file always gets the committed state.

	Written for newtc.
*/

#if !defined(__HOSTSTORE_H)
#define __HOSTSTORE_H 1

#include "Store.h"

#include <map>
#include <string>
#include <vector>

PROTOCOL CHostStore : public CStore
{
public:
	PROTOCOL_IMPL_HEADER_MACRO(CHostStore)

	/** The file the internal store is kept in (before the store is made);
	    empty: memory only. */
	static void			SetFile(const char * inPath);
	/** Save the store now if it changed (at exit, too). */
	static void			SaveNow(void);

	CHostStore *	make(void) override;
	void			destroy(void) override;

	NewtonErr	init(void * inStoreData, size_t inStoreSize, ULong inArg3, ArrayIndex inSocketNumber, ULong inFlags, void * inPSSInfo) override;
	NewtonErr	needsFormat(bool * outNeedsFormat) override;
	NewtonErr	format(void) override;
	NewtonErr	getRootId(PSSId * outRootId) override;
	NewtonErr	newObject(PSSId * outObjectId, size_t inSize) override;
	NewtonErr	eraseObject(PSSId inObjectId) override;
	NewtonErr	deleteObject(PSSId inObjectId) override;
	NewtonErr	setObjectSize(PSSId inObjectId, size_t inSize) override;
	NewtonErr	getObjectSize(PSSId inObjectId, size_t * outSize) override;
	NewtonErr	write(PSSId inObjectId, size_t inStartOffset, void * inBuffer, size_t inLength) override;
	NewtonErr	read(PSSId inObjectId, size_t inStartOffset, void * outBuffer, size_t inLength) override;
	NewtonErr	getStoreSize(size_t * outTotalSize, size_t * outUsedSize) override;
	NewtonErr	isReadOnly(bool * outIsReadOnly) override;
	NewtonErr	lockStore(void) override;
	NewtonErr	unlockStore(void) override;
	NewtonErr	abort(void) override;
	NewtonErr	idle(bool * outArg1, bool * outArg2) override;
	NewtonErr	nextObject(PSSId inObjectId, PSSId * outNextObjectId) override;
	NewtonErr	checkIntegrity(ULong * inArg1) override;
	NewtonErr	setBuddy(CStore * inStore) override;
	bool			ownsObject(PSSId inObjectId) override;
	VAddr			address(PSSId inObjectId) override;
	const char * storeKind(void) override;
	NewtonErr	setStore(CStore * inStore, ObjectId inEnvironment) override;
	bool			isSameStore(void * inData, size_t inSize) override;
	bool			isLocked(void) override;
	bool			isROM(void) override;
	NewtonErr	vppOff(void) override;
	NewtonErr	sleep(void) override;
	NewtonErr	newWithinTransaction(PSSId * outObjectId, size_t inSize) override;
	NewtonErr	startTransactionAgainst(PSSId inObjectId) override;
	NewtonErr	separatelyAbort(PSSId inObjectId) override;
	NewtonErr	addToCurrentTransaction(PSSId inObjectId) override;
	bool			inSeparateTransaction(PSSId inObjectId) override;
	NewtonErr	lockReadOnly(void) override;
	NewtonErr	unlockReadOnly(bool inReset) override;
	bool			inTransaction(void) override;
	NewtonErr	newObject(PSSId * outObjectId, void * inData, size_t inSize) override;
	NewtonErr	replaceObject(PSSId inObjectId, void * inData, size_t inSize) override;
	NewtonErr	calcXIPObjectSize(long inArg1, long inArg2, long * outArg3) override;
	NewtonErr	newXIPObject(PSSId * outObjectId, size_t inSize) override;
	NewtonErr	getXIPObjectInfo(PSSId inObjectId, unsigned long * outArg2, unsigned long * outArg3, unsigned long * outArg4) override;

private:
	typedef std::vector<unsigned char> Bytes;
	// An object's contents before a transaction changed it; absent: it
	// didn't exist.
	struct Original { bool exists; Bytes bytes; };

	void			willChange(PSSId inObjectId);
	void			restore(PSSId inObjectId, const Original & inOriginal);
	bool			load(void);
	bool			save(void);

	std::map<PSSId, Bytes>		fObjects;
	std::map<PSSId, Original>	fJournal;		// the main transaction
	std::map<PSSId, Original>	fSeparate;		// objects in transactions of their own
	PSSId			fNextId;
	int			fLockCount;
	int			fLockROCount;
	bool			fInTransaction;
	bool			fFormatted;
	bool			fDirty;
};

#endif	/* __HOSTSTORE_H */
