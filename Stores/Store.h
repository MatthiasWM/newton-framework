/*
	File:		CStore.h

	Contains:	Newton object store interface.

	Written by:	Newton Research Group.
*/

#if !defined(__STORE_H)
#define __STORE_H 1

#include "Protocols.h"
#include "PSSTypes.h"

/*------------------------------------------------------------------------------
	C S t o r e
	P-class interface.
------------------------------------------------------------------------------*/

PROTOCOL CStore : public CProtocol
{
// Implementations will need to consider:
//	PROTOCOL_IMPL_HEADER_MACRO(CStore);

//	CAPABILITIES( "LOBJ" )
// LOBJ says that this implementation is able to handle large binaries.
// If the store is mounted, this is implied somewhere so that it crashes if it wasn't declared.
// However, if the store is not registered on the PSSManager, the large binaries function
// will consider this store as non-valid.
public:
	static CStore *make(const char * inName);
  // -- inherited for CProtocol
	void			destroy(void) override;
  // -- added for CStore
	virtual NewtonErr	init(void * inStoreData, size_t inStoreSize, ULong inArg3, ArrayIndex inSocketNumber, ULong inFlags, void * inPSSInfo) = 0;
				// Initializes the store with the SPSSInfo (inPSSInfo)
				// Flags are used by TFlashStore to determine if the store is on SRAM, Flash card or Internal Flash
	virtual NewtonErr	needsFormat(bool * outNeedsFormat) = 0;
				// Tests if the store needs to be formatted. If so, a dialog will be shown to offer to format it.
	virtual NewtonErr	format(void) = 0;
				// Formats the card. Should create an empty object of the ID rootID.

	virtual NewtonErr	getRootId(PSSId * outRootId) = 0;
				// There is a root object from which every other object is linked.
  virtual NewtonErr	newObject(PSSId * outObjectId, size_t inSize) = 0;
				// Creates a new object. TFlashStore interface calls NewObject( NULL, inSize, outObjectID );
  virtual NewtonErr	eraseObject(PSSId inObjectId) = 0;
				// TFlashStore returns noErr without doing anything.
  virtual NewtonErr	deleteObject(PSSId inObjectId) = 0;
				// Removes the object
  virtual NewtonErr	setObjectSize(PSSId inObjectId, size_t inSize) = 0;
				// Changes the size of an object.
				// Returns kStoreErrObjectNotFound if the object cannot be found.
	virtual NewtonErr	getObjectSize(PSSId inObjectId, size_t * outSize) = 0;
				// Returns the size of an object.
				// Returns kStoreErrObjectNotFound if the object cannot be found.

	virtual NewtonErr	write(PSSId inObjectId, size_t inStartOffset, void * inBuffer, size_t inLength) = 0;
				// Writes the data for an object from inStartOffset for inLength bytes.
	virtual NewtonErr	read(PSSId inObjectId, size_t inStartOffset, void * outBuffer, size_t inLength) = 0;
				// Reads the data for an object from inStartOffset for inLength bytes.

  virtual NewtonErr	getStoreSize(size_t * outTotalSize, size_t * outUsedSize) = 0;
				// Returns the total size and the used size for the store.
	virtual NewtonErr	isReadOnly(bool * outIsReadOnly) = 0;
				// Physical ReadOnly
  virtual NewtonErr	lockStore(void) = 0;
  virtual NewtonErr	unlockStore(void) = 0;
				// There is normally a lock store counter.
				// This counter is increased with LockStore and decreased with UnlockStore.

  virtual NewtonErr	abort(void) = 0;
				// Aborts the current transaction.
  virtual NewtonErr	idle(bool * outArg1, bool * outArg2) = 0;
				// TFlashStore does nothing.

  virtual NewtonErr	nextObject(PSSId inObjectId, PSSId * outNextObjectId) = 0;
				// Suspect PSSIds are actually VAddrs
				// TPackageStore sets outNextObjectId to 0.
				// Both TFlashStore (which does not set *outObject) and TPackageStore return noErr.
				// This isn't really implemented anywhere.
  virtual NewtonErr	checkIntegrity(ULong * inArg1) = 0;
				//	Both TFlashStore and TPackageStore return noErr
				// This isn't really implemented anywhere.

  virtual NewtonErr	setBuddy(CStore * inStore) = 0;
				// TMuxStore calls inherited. Both TFlashStore and TPackageStore return noErr
  virtual bool			ownsObject(PSSId inObjectId) = 0;
				// TMuxStore only. Both TFlashStore and TPackageStore return true.
  virtual VAddr			address(PSSId inObjectId) = 0;
				// TMuxStore calls inherited. Both TFlashStore and TPackageStore return 0.
  virtual const char * storeKind(void) = 0;
				// Returns a CString representation of the store kind (e.g. "Internal", "Package")
	virtual NewtonErr	setStore(CStore * inStore, ObjectId inEnvironment) = 0;
				// TMuxStore only. Both TFlashStore and TPackageStore return noErr

	virtual bool isSameStore(void * inData, size_t inSize) = 0;
				// TPackageStore returns false. More complex with TFlashStore.
  virtual bool isLocked(void) = 0;
				// Returns (the counter of [Un]LockStore is not null)
	virtual bool isROM(void) = 0;
				// A ROM Flash Card is an application card. TPackageStore returns true.

// Power management
  virtual NewtonErr	vppOff(void) = 0;
				// TFlashStore powers off the card, whatever the power level is.
  virtual NewtonErr	sleep(void) = 0;
				// TMuxStore calls inherited. Both TFlashStore and TPackageStore return noErr

  virtual NewtonErr	newWithinTransaction(PSSId * outObjectId, size_t inSize) = 0;
				// TPackageStore calls NewObject.
				// Starts a transaction against this new object.
  virtual NewtonErr	startTransactionAgainst(PSSId inObjectId) = 0;
				// Start a transaction against the given object.
  virtual NewtonErr	separatelyAbort(PSSId inObjectId) = 0;
  virtual NewtonErr	addToCurrentTransaction(PSSId inObjectId) = 0;
  virtual bool			inSeparateTransaction(PSSId inObjectId) = 0;

  virtual NewtonErr	lockReadOnly(void) = 0;
				// TFlashStore increases a counter. TPackageStore returns noErr
  virtual NewtonErr	unlockReadOnly(bool inReset) = 0;
				// TFlashStore decreases a counter or resets it to zero if inReset. TPackageStore returns noErr
  virtual bool			inTransaction(void) = 0;

// The following entries are not in the jumptable, hence you can't call them (but the system may rely on them)

	virtual NewtonErr	newObject(PSSId * outObjectId, void * inData, size_t inSize) = 0;
				// TFlashStore::NewObject( long, unsigned long* ) calls this one with NULL as first arg
	virtual NewtonErr	replaceObject(PSSId inObjectId, void * inData, size_t inSize) = 0;

//	eXecute-In-Place
  virtual NewtonErr	calcXIPObjectSize(long inArg1, long inArg2, long * outArg3) = 0;
  virtual NewtonErr	newXIPObject(PSSId * outObjectId, size_t inSize) = 0;
  virtual NewtonErr	getXIPObjectInfo(PSSId inObjectId, unsigned long * outArg2, unsigned long * outArg3, unsigned long * outArg4) = 0;

};

#endif	/* __STORE_H */
