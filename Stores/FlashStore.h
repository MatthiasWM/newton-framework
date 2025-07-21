/*
	File:		FlashStore.h

	Contains:	Flash store declarations.
	
	Written by:	Newton Research Group, 2010.
*/

#if !defined(__FLASHSTORE_H)
#define __FLASHSTORE_H 1

#include "Store.h"
#include "Flash.h"
#include "FlashBlock.h"
#include "FlashCache.h"
#include "FlashTracker.h"
#include "CardEvents.h"

#define kRootDirId		 3
#define kTxnRecordId		23
#define kRootId			39
#define kFirstObjectId	49


/*------------------------------------------------------------------------------
	C F l a s h S t o r e
------------------------------------------------------------------------------*/

class CFlashStore : public CStore
{
public:
  // -- inherited from CProtocol
	PROTOCOL_IMPL_HEADER_MACRO(CFlashStore)
	CFlashStore *	make(void) override;
	void			destroy(void) override;
  // -- inherited from CStore
	NewtonErr	init(void * inStoreAddr, size_t inStoreSize, ULong inArg3, ArrayIndex inSocketNumber, ULong inFlags, void * inPSSInfo) override;
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

	bool			isSameStore(void * inAlienStoreData, size_t inAlienStoreSize) override;
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
  // -- end of protocol

// additional flash methods

	NewtonErr	newWithinTransaction(PSSId * outObjectId, size_t inSize, bool inArg3);
	NewtonErr	startTransaction(void);
	NewtonErr	doAbort(bool inArg);

	NewtonErr	vppOn(void);
	NewtonErr	vccOff(void);
	NewtonErr	vccOn(void);

	size_t		storeCapacity(void);
	size_t		avail(void);
	size_t		storeSlop(void);
	void			gc(void);

	ULong			nextLSN(void);
	NewtonErr	addObject(PSSId inObjectId, int inState, size_t inSize, CStoreObjRef & inObj, bool inArg4, bool inArg5);
	void			add(CStoreObjRef & inObj);
	void			remove(CStoreObjRef & inObj);
	ZAddr			translate(ZAddr inAddr);
	NewtonErr	validateIncomingPSSId(PSSId inObjectId);
	NewtonErr	lookup(PSSId inObjectId, int inState, CStoreObjRef & inObj);
	NewtonErr	setupForRead(PSSId inObjectId, CStoreObjRef & ioObj);
	NewtonErr	setupForModify(PSSId inObjectId, CStoreObjRef & ioObj, bool inArg3, bool inArg4);
	NewtonErr	basicRead(ZAddr inAddr, void * outBuf, size_t inLen);
	NewtonErr	basicWrite(ZAddr inAddr, void * inBuf, size_t inLen);
	NewtonErr	basicCopy(ZAddr inFromAddr, ZAddr inToAddr, size_t inLen);
	NewtonErr	zap(ZAddr inAddr, size_t inLen);
	NewtonErr	chooseWorkingBlock(size_t inMinSize, ZAddr inAddr);
	bool			isRangeVirgin(ZAddr inAddr, size_t inLen);


	NewtonErr	deleteTransactionRecord(void);
	NewtonErr	markCommitPoint(void);
	NewtonErr	doCommit(bool inArg);

	NewtonErr	transactionState(int * outState);
	NewtonErr	recoveryCheck(bool inArg);
	void			lowLevelRecovery(void);
	NewtonErr	mount(void);
	void			touchMe(void);
	void			notifyCompact(CFlashBlock * inBlock);

	void			initBlocks(void);
	NewtonErr	scanLogForLogicalBlocks(bool *);
	NewtonErr	scanLogForErasures(void);
	NewtonErr	scanLogForReservedBlocks(void);

	NewtonErr	addLogEntryToPhysBlock(ULong inIdent, size_t inSize, SFlashLogEntry * ioLogEntry, ZAddr inAddr, ZAddr * outAddr);
	NewtonErr	nextLogEntry(ZAddr inAddr, ZAddr * outAddr, ULong inType, void * inAlienStoreData);
	NewtonErr	zapLogEntry(ZAddr inAddr);
	ZAddr			findPhysWritable(ZAddr inStartAddr, ZAddr inEndAddr, size_t inSize);


	bool			isWriteProtected(void);

	bool			isErased(ZAddr inBlockAddr);
	bool			isErased(ZAddr inBlockAddr, size_t inBlockSize, ArrayIndex inNumOfBadBytesPermitted);
	NewtonErr	syncErase(ZAddr inBlockAddr);
	NewtonErr	startErase(ZAddr inBlockAddr);
	NewtonErr	eraseStatus(ZAddr inBlockAddr);
	NewtonErr	waitForEraseDone(void);
	void			calcAverageEraseCount(void);
	ArrayIndex	averageEraseCount(void);
	ULong			findUnusedPhysicalBlock(void);
	NewtonErr	bringVirginBlockOnline(ULong inUnusedBlock, ULong inBlock);

	ZAddr			offsetToLogs(void) const;

	size_t		bucketSize(void) const;
	size_t		bucketCount(void) const;

	PSSId			PSSIdFor(ArrayIndex inBlockNo, long inBlockOffset) const;
	ArrayIndex	objectNumberFor(PSSId inId) const;
	ArrayIndex	blockNumberFor(PSSId inId) const;
	CFlashBlock *	blockFor(PSSId inId) const;
	CFlashBlock *	blockForAddr(ZAddr inAddr) const;

private:
	friend struct StoreObjHeader;
	friend struct SDirEnt;
	friend class CStoreObjRef;
	friend class CFlashStoreLookupCache;
	friend class CFlashBlock;
	friend class CFlashIterator;

	bool			cardWPAlertProc(ULong inArg1, void * inArg2);
	NewtonErr	sendAlertMgrWPBitch(int inSelector);

	CFlash *				fFlash { nullptr };				// +10
	bool					fIsMounted { false };			// +14
	bool					fIsStoreRemovable { false };// +15
	bool					fIsInTransaction { false };	// +16
	bool					f17 { false };					// +17
	char *				fStoreAddr { nullptr };			// +18	base address of ROM store
	ArrayIndex			fSocketNo { 0 };			// +1C
	ArrayIndex			fLSN { 0 };					// +20	Log Sequence Number
	CFlashPhysBlock *	fPhysBlock { nullptr };			// +24
	CFlashBlock *		fWorkingBlock { nullptr };		// +28
	CFlashBlock **		fBlock { nullptr };				// +2C
	CFlashBlock *		fLogicalBlock { nullptr };		// +30
	CFlashStoreLookupCache *	fCache { nullptr };	// +34
	int					f38 { 0 };
	bool					fIsErasing { false };			// +3C
	bool					fIsSRAM { false };				// +3D
	bool					f3E { false };
	bool					fIsROM { false };				// +3F
	ULong					fEraseBlockAddr;	// +40
	ArrayIndex			fAvEraseCount { 0 };		// +44
	ULong					fDirtyBits { 0 };			// +48
	ULong					fVirginBits { 0 };		// +4C
	size_t				fBlockSize { 0 };			// +50
	ArrayIndex			fNumOfBlocks { 0 };		// +54
	ArrayIndex			fBlockSizeShift { 0 };	// +58
	ULong					fBlockSizeMask { 0 };	// +5C
	ULong					f60 { 0 };
	ULong					f64 { 0 };
	ULong					f68 { 0 };
	size_t				fBucketSize { 0 };		// +6C
	ArrayIndex			fBucketCount { 0 };		// +70
	int					fLockCount { 0 };			// +74
	CCardHandler *		fCardHandler { nullptr };		// +78
	CStoreObjRef *		fObjListTail { nullptr };		// +7C
	CStoreObjRef *		fObjListHead { nullptr };		// +80
	CFlashTracker *	fTracker { nullptr };			// +84
	CStoreDriver *		fStoreDriver { nullptr };		// +88
	SCompactState *	fCompact { nullptr };			// +8C
	bool					fIsFormatReqd { false };		// +90
	bool					fUseRAM { false };				// +91
	bool					f92 { false };
	bool					fIsFormatting { false };		// +94
	bool					f95 { false };
	bool					f96 { false };
	bool					fIsInternalFlash { false };	// +97
	long					fA0 { 0 };
	CStoreDriver		fA4 { 0 };
	int					fLockROCount { 0 };		// +D4
	long					fD8 { 0 };					// +D8	actually CStore *?
	bool					fE4 { false };
	bool					fE5 { false };
	ArrayIndex			fE8 { 0 };
	size_t				fCachedUsedSize { 0 };	// +EC
// size +F0
};


inline ULong			CFlashStore::offsetToLogs(void) const { return fBlockSize - (fIsSRAM ? 256 : 1024); }

inline size_t			CFlashStore::bucketSize(void) const { return fBucketSize; }
inline size_t			CFlashStore::bucketCount(void) const { return fBucketCount; }

inline PSSId			CFlashStore::PSSIdFor(ArrayIndex inBlockNo, long inBlockOffset) const { return (inBlockNo << f60) | inBlockOffset; }
inline ArrayIndex		CFlashStore::objectNumberFor(PSSId inId) const { return inId & ~(0x0FFFFFFF << f60); }
inline ArrayIndex		CFlashStore::blockNumberFor(PSSId inId) const { return inId >> f60; }
inline CFlashBlock *	CFlashStore::blockFor(PSSId inId) const { return fBlock[inId >> f60]; }
inline CFlashBlock *	CFlashStore::blockForAddr(ZAddr inAddr) const { return fBlock[inAddr >> fBlockSizeShift]; }

inline ArrayIndex		CFlashStore::nextLSN(void) { return ++fLSN; }


#endif	/* __FLASHSTORE_H */

