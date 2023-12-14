/*
	File:		MuxStore.h

	Contains:	Newton mux store interface.

	Written by:	Newton Research Group.
*/

#if !defined(__MUXSTORE_H)
#define __MUXSTORE_H 1

#include "Store.h"
#include "StoreMonitor.h"
#include "Semaphore.h"

class CMuxStoreMonitor;

/*------------------------------------------------------------------------------
	C M u x S t o r e
	A mux store is a wrapper around a CStore that allows thread-safe access.
------------------------------------------------------------------------------*/

PROTOCOL CMuxStore : public CStore
{
public:
    // -- inherited from CProtocol
	PROTOCOL_IMPL_HEADER_MACRO(CMuxStore)
	CMuxStore *	make(void) override;
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
	VAddr			address(PSSId inObjectId) override;
	const char * storeKind(void) override;
	NewtonErr	setStore(CStore * inStore, PSSId inEnvironment) override;

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

	CStore *		getStore(void) const;

private:
	NewtonErr	acquireLock(void);
	NewtonErr	releaseLock(void);

	CStore *					fStore;			// +10
	CMuxStoreMonitor *	fMonitor;		// +14
	CULockingSemaphore *	fLock;			// +18
};

inline CStore *	CMuxStore::getStore(void) const	{ return fStore; }
inline NewtonErr	CMuxStore::acquireLock(void)		{ return fLock->acquire(kWaitOnBlock); }
inline NewtonErr	CMuxStore::releaseLock(void)		{ return fLock->release(); }


/*------------------------------------------------------------------------------
	C M u x S t o r e M o n i t o r
	The mux store monitor allows thread-safe access to a CStore.
------------------------------------------------------------------------------*/

MONITOR CMuxStoreMonitor : public CStoreMonitor
{
public:
    // -- inherited from CProtocol
	PROTOCOL_IMPL_HEADER_MACRO(CMuxStoreMonitor)
	CMuxStoreMonitor *	make(void) override;
	void			destroy(void) override;
    // -- inherited from CStoreMonitor
	NewtonErr	init(CStore * inStore) override;
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

	NewtonErr	newWithinTransaction(PSSId * outObjectId, size_t inSize) override;
	NewtonErr	startTransactionAgainst(PSSId inObjectId) override;
	NewtonErr	separatelyAbort(PSSId inObjectId) override;
	NewtonErr	addToCurrentTransaction(PSSId inObjectId) override;

	NewtonErr	lockReadOnly(void) override;
	NewtonErr	unlockReadOnly(bool inReset) override;

	NewtonErr	newObject(PSSId * outObjectId, void * inData, size_t inSize) override;
	NewtonErr	replaceObject(PSSId inObjectId, void * inData, size_t inSize) override;

	NewtonErr	newXIPObject(PSSId * outObjectId, size_t inSize) override;
    // -- end of protocol
    
private:
	CStore *		fStore;	//+10
//size+20
};


#endif	/* __MUXSTORE_H */
