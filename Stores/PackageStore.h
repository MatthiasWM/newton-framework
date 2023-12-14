/*
	File:		PackageStore.h

	Contains:	Newton package store interface.
					The package store contains read-only data
					and is also known as a store part.

	Written by:	Newton Research Group.
*/

#if !defined(__PACKAGESTORE_H)
#define __PACKAGESTORE_H 1

#include "StoreWrapper.h"


/* -----------------------------------------------------------------------------
	P a c k a g e S t o r e D a t a
	A package store is a block of read-only data loaded from a package.
	The data is prefixed with a PackageStoreData struct that defines the objects
	within the store. An object’s id is an index into an array that gives the
	offset to the object within the package data block.
----------------------------------------------------------------------------- */

struct PackageStoreData
{
	PSSId		rootId;
	ULong		numOfObjects;
	ULong		offsetToObject[];
};


/* -----------------------------------------------------------------------------
	C P a c k a g e S t o r e
	A CStore protocol implementation.
----------------------------------------------------------------------------- */

PROTOCOL CPackageStore : public CStore
{
public:
    // -- inherited from CProtocol
	PROTOCOL_IMPL_HEADER_MACRO(CPackageStore)
	CPackageStore *	make(void) override;
	void			destroy(void) override;
    // -- inherited from CStore
	NewtonErr	init(void * inStoreData, size_t inStoreSize, ULong inArg3, ArrayIndex inSocketNumber, ULong inFlags, void * inArg6) override;
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
    //"    .long    __ZN13CPackageStore7addressEj - 4b  \n"
    //"    .long    __ZN13CPackageStore9storeKindEv - 4b  \n"
    //"    .long    __ZN13CPackageStore8setStoreEP6CStorej - 4b  \n"
    //"    .long    __ZN13CPackageStore11isSameStoreEPvm - 4b  \n"
    //"    .long    __ZN13CPackageStore8isLockedEv - 4b  \n"
    //"    .long    __ZN13CPackageStore5isROMEv - 4b  \n"
    //"    .long    __ZN13CPackageStore6vppOffEv - 4b  \n"
    //"    .long    __ZN13CPackageStore5sleepEv - 4b  \n"
    //"    .long    __ZN13CPackageStore20newWithinTransactionEPjm - 4b  \n"
    //"    .long    __ZN13CPackageStore23startTransactionAgainstEj - 4b  \n"
    //"    .long    __ZN13CPackageStore15separatelyAbortEj - 4b  \n"
    //"    .long    __ZN13CPackageStore23addToCurrentTransactionEj - 4b  \n"
    //"    .long    __ZN13CPackageStore21inSeparateTransactionEj - 4b  \n"
    //"    .long    __ZN13CPackageStore12lockReadOnlyEv - 4b  \n"
    //"    .long    __ZN13CPackageStore14unlockReadOnlyEb - 4b  \n"
    //"    .long    __ZN13CPackageStore13inTransactionEv - 4b  \n"
    //"    .long    __ZN13CPackageStore9newObjectEPjPvm - 4b  \n"
    //"    .long    __ZN13CPackageStore13replaceObjectEjPvm - 4b  \n"
    //"    .long    __ZN13CPackageStore17calcXIPObjectSizeEllPl - 4b  \n"
    //"    .long    __ZN13CPackageStore12newXIPObjectEPjm - 4b  \n"
    //"    .long    __ZN13CPackageStore16getXIPObjectInfoEjPmS0_S0_ - 4b  \n"
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
	PackageStoreData *	fStoreData;		// +10
	size_t					fStoreSize;		// +14
	int						fLockCounter;	// +18
};

#endif	/* __PACKAGESTORE_H */
