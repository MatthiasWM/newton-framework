/*
	File:		StoreProto.s

	Contains:	CStore p-class protocol interface.

	Written by:	Newton Research Group, 2009.
*/

#include "Stores/LargeObjectStore.h"

#include <assert.h>

CLrgObjStore *CLrgObjStore::make(const char * inName) {
  assert(0);
  return NULL;
}

void CLrgObjStore::destroy(void) {
  assert(0);
}

NewtonErr CLrgObjStore::init(void) {
  assert(0);
  return kNSErrInternalError;
}

NewtonErr CLrgObjStore::create(PSSId * outId, CStore * inStore, CPipe * inPipe, size_t inStreamSize, bool inReadOnly, const char * inCompanderName, void * inCompanderParms, size_t inCompanderParmSize, CLOCallback * inCallback) {
  assert(0);
  return kNSErrInternalError;
}

NewtonErr CLrgObjStore::createFromCompressed(PSSId * outId, CStore * inStore, CPipe * inPipe, size_t inStreamSize, bool inReadOnly, const char * inCompanderName, void * inCompanderParms, size_t inCompanderParmSize, CLOCallback * inCallback) {
  assert(0);
  return kNSErrInternalError;
}

NewtonErr CLrgObjStore::deleteObject(CStore * inStore, PSSId inId) {
  assert(0);
  return kNSErrInternalError;
}

NewtonErr CLrgObjStore::duplicate(PSSId * outId, CStore * inStore, PSSId inId, CStore * intoStore) {
  assert(0);
  return kNSErrInternalError;
}

NewtonErr CLrgObjStore::resize(CStore * inStore, PSSId inId, size_t inSize) {
  assert(0);
  return kNSErrInternalError;
}

size_t CLrgObjStore::storageSize(CStore * inStore, PSSId inId) {
  assert(0);
  return 0;
}

size_t CLrgObjStore::sizeOfStream(CStore * inStore, PSSId inId, bool inCompressed) {
  assert(0);
  return 0;
}

NewtonErr CLrgObjStore::backup(CPipe * inPipe, CStore * inStore, PSSId inId, bool inCompressed, CLOCallback * inCallback) {
  assert(0);
  return kNSErrInternalError;
}

// --------

CStore *CStore::make(const char * inName) {
  CProtocol *p = AllocInstanceByName("CStore", inName);
  if (p) p->make();
  return (CStore*)p;
}

void      CStore::destroy(void) {
  assert(0);
}

//NewtonErr  CStore::init(void * inStoreData, size_t inStoreSize, ULong inArg3, ArrayIndex inSocketNumber, ULong inFlags, void * inPSSInfo) {
//  assert(0);
//  return kNSErrInternalError;
//}

//NewtonErr  CStore::needsFormat(bool * outNeedsFormat) {
//  assert(0);
//  return kNSErrInternalError;
//}

//NewtonErr  CStore::format(void) {
//  assert(0);
//  return kNSErrInternalError;
//}

//NewtonErr  CStore::getRootId(PSSId * outRootId) {
//  assert(0);
//  return kNSErrInternalError;
//}

NewtonErr  CStore::newObject(PSSId * outObjectId, size_t inSize) {
  assert(0);
  return kNSErrInternalError;
}

NewtonErr  CStore::eraseObject(PSSId inObjectId) {
  assert(0);
  return kNSErrInternalError;
}

NewtonErr  CStore::deleteObject(PSSId inObjectId) {
  assert(0);
  return kNSErrInternalError;
}

NewtonErr  CStore::setObjectSize(PSSId inObjectId, size_t inSize) {
  assert(0);
  return kNSErrInternalError;
}

//NewtonErr  CStore::getObjectSize(PSSId inObjectId, size_t * outSize) {
//  assert(0);
//  return kNSErrInternalError;
//}

NewtonErr  CStore::write(PSSId inObjectId, size_t inStartOffset, void * inBuffer, size_t inLength) {
  assert(0);
  return kNSErrInternalError;
}

NewtonErr  CStore::read(PSSId inObjectId, size_t inStartOffset, void * outBuffer, size_t inLength) {
  assert(0);
  return kNSErrInternalError;
}

//NewtonErr  CStore::getStoreSize(size_t * outTotalSize, size_t * outUsedSize) {
//  assert(0);
//  return kNSErrInternalError;
//}

//NewtonErr  CStore::isReadOnly(bool * outIsReadOnly) {
//  assert(0);
//  return kNSErrInternalError;
//}

//NewtonErr  CStore::lockStore(void) {
//  assert(0);
//  return kNSErrInternalError;
//}

//NewtonErr  CStore::unlockStore(void) {
//  assert(0);
//  return kNSErrInternalError;
//}

NewtonErr  CStore::abort(void) {
  assert(0);
  return kNSErrInternalError;
}

NewtonErr  CStore::idle(bool * outArg1, bool * outArg2) {
  assert(0);
  return kNSErrInternalError;
}

NewtonErr  CStore::nextObject(PSSId inObjectId, PSSId * outNextObjectId) {
  assert(0);
  return kNSErrInternalError;
}

NewtonErr  CStore::checkIntegrity(ULong * inArg1) {
  assert(0);
  return kNSErrInternalError;
}

NewtonErr  CStore::setBuddy(CStore * inStore) {
  assert(0);
  return kNSErrInternalError;
}

bool      CStore::ownsObject(PSSId inObjectId) {
  assert(0);
  return false;
}

VAddr      CStore::address(PSSId inObjectId) {
  assert(0);
  return 0;
}

const char * CStore::storeKind(void) {
  assert(0);
  return nullptr;
}

//NewtonErr  CStore::setStore(CStore * inStore, ObjectId inEnvironment) {
//  assert(0);
//  return kNSErrInternalError;
//}

//bool      CStore::isSameStore(void * inData, size_t inSize) {
//  assert(0);
//  return false;
//}

//bool      CStore::isLocked(void) {
//  assert(0);
//  return false;
//}

//bool      CStore::isROM(void) {
//  assert(0);
//  return false;
//}

NewtonErr  CStore::vppOff(void) {
  assert(0);
  return kNSErrInternalError;
}

NewtonErr  CStore::sleep(void) {
  assert(0);
  return kNSErrInternalError;
}

NewtonErr  CStore::newWithinTransaction(PSSId * outObjectId, size_t inSize) {
  assert(0);
  return kNSErrInternalError;
}

NewtonErr  CStore::startTransactionAgainst(PSSId inObjectId) {
  assert(0);
  return kNSErrInternalError;
}

NewtonErr  CStore::separatelyAbort(PSSId inObjectId) {
  assert(0);
  return kNSErrInternalError;
}

NewtonErr  CStore::addToCurrentTransaction(PSSId inObjectId) {
  assert(0);
  return kNSErrInternalError;
}

bool      CStore::inSeparateTransaction(PSSId inObjectId) {
  assert(0);
  return false;
}

NewtonErr  CStore::lockReadOnly(void) {
  assert(0);
  return kNSErrInternalError;
}

NewtonErr  CStore::unlockReadOnly(bool inReset) {
  assert(0);
  return kNSErrInternalError;
}

bool      CStore::inTransaction(void) {
  assert(0);
  return false;
}

NewtonErr  CStore::newObject(PSSId * outObjectId, void * inData, size_t inSize) {
  assert(0);
  return kNSErrInternalError;
}

//NewtonErr  CStore::replaceObject(PSSId inObjectId, void * inData, size_t inSize) {
//  assert(0);
//  return kNSErrInternalError;
//}

NewtonErr  CStore::calcXIPObjectSize(long inArg1, long inArg2, long * outArg3) {
  assert(0);
  return kNSErrInternalError;
}

NewtonErr  CStore::newXIPObject(PSSId * outObjectId, size_t inSize) {
  assert(0);
  return kNSErrInternalError;
}

NewtonErr  CStore::getXIPObjectInfo(PSSId inObjectId, unsigned long * outArg2, unsigned long * outArg3, unsigned long * outArg4) {
  assert(0);
  return kNSErrInternalError;
}



#if 0

#include "../Protocols/ProtoMacro.s"

/* ---------------------------------------------------------------- */

		.globl	__ZN6CStore4makeEPKc
		.globl	__ZN6CStore7destroyEv
		.globl	__ZN6CStore4initEPvmjjjS0_
		.globl	__ZN6CStore11needsFormatEPb
		.globl	__ZN6CStore6formatEv
		.globl	__ZN6CStore9getRootIdEPj
		.globl	__ZN6CStore9newObjectEPjm
		.globl	__ZN6CStore11eraseObjectEj
		.globl	__ZN6CStore12deleteObjectEj
		.globl	__ZN6CStore13setObjectSizeEjm
		.globl	__ZN6CStore13getObjectSizeEjPm
		.globl	__ZN6CStore5writeEjmPvm
		.globl	__ZN6CStore4readEjmPvm
		.globl	__ZN6CStore12getStoreSizeEPmS0_
		.globl	__ZN6CStore10isReadOnlyEPb
		.globl	__ZN6CStore9lockStoreEv
		.globl	__ZN6CStore11unlockStoreEv
		.globl	__ZN6CStore5abortEv
		.globl	__ZN6CStore4idleEPbS0_
		.globl	__ZN6CStore10nextObjectEjPj
		.globl	__ZN6CStore14checkIntegrityEPj
		.globl	__ZN6CStore8setBuddyEPS_
		.globl	__ZN6CStore10ownsObjectEj
		.globl	__ZN6CStore7addressEj
		.globl	__ZN6CStore9storeKindEv
		.globl	__ZN6CStore8setStoreEPS_j
		.globl	__ZN6CStore11isSameStoreEPvm
		.globl	__ZN6CStore8isLockedEv
		.globl	__ZN6CStore5isROMEv
		.globl	__ZN6CStore6vppOffEv
		.globl	__ZN6CStore5sleepEv
		.globl	__ZN6CStore20newWithinTransactionEPjm
		.globl	__ZN6CStore23startTransactionAgainstEj
		.globl	__ZN6CStore15separatelyAbortEj
		.globl	__ZN6CStore23addToCurrentTransactionEj
		.globl	__ZN6CStore21inSeparateTransactionEj
		.globl	__ZN6CStore12lockReadOnlyEv
		.globl	__ZN6CStore14unlockReadOnlyEb
		.globl	__ZN6CStore13inTransactionEv
		.globl	__ZN6CStore9newObjectEPjPvm
		.globl	__ZN6CStore13replaceObjectEjPvm
		.globl	__ZN6CStore17calcXIPObjectSizeEllPl
		.globl	__ZN6CStore12newXIPObjectEPjm
		.globl	__ZN6CStore16getXIPObjectInfoEjPmS0_S0_

		.text
		.align	2

CStore_name:
		.asciz	"CStore"
		.align	2

__ZN6CStore4makeEPKc:
		New CStore_name
		Dispatch 2
__ZN6CStore7destroyEv:
		Delete 3
__ZN6CStore4initEPvmjjjS0_:
		Dispatch 4
__ZN6CStore11needsFormatEPb:
		Dispatch 5
__ZN6CStore6formatEv:
		Dispatch 6
__ZN6CStore9getRootIdEPj:
		Dispatch 7
__ZN6CStore9newObjectEPjm:
		Dispatch 8
__ZN6CStore11eraseObjectEj:
		Dispatch 9
__ZN6CStore12deleteObjectEj:
		Dispatch 10
__ZN6CStore13setObjectSizeEjm:
		Dispatch 11
__ZN6CStore13getObjectSizeEjPm:
		Dispatch 12
__ZN6CStore5writeEjmPvm:
		Dispatch 13
__ZN6CStore4readEjmPvm:
		Dispatch 14
__ZN6CStore12getStoreSizeEPmS0_:
		Dispatch 15
__ZN6CStore10isReadOnlyEPb:
		Dispatch 16
__ZN6CStore9lockStoreEv:
		Dispatch 17
__ZN6CStore11unlockStoreEv:
		Dispatch 18
__ZN6CStore5abortEv:
		Dispatch 19
__ZN6CStore4idleEPbS0_:
		Dispatch 20
__ZN6CStore10nextObjectEjPj:
		Dispatch 21
__ZN6CStore14checkIntegrityEPj:
		Dispatch 22
__ZN6CStore8setBuddyEPS_:
		Dispatch 23
__ZN6CStore10ownsObjectEj:
		Dispatch 24
__ZN6CStore7addressEj:
		Dispatch 25
__ZN6CStore9storeKindEv:
		Dispatch 26
__ZN6CStore8setStoreEPS_j:
		Dispatch 27
__ZN6CStore11isSameStoreEPvm:
		Dispatch 28
__ZN6CStore8isLockedEv:
		Dispatch 29
__ZN6CStore5isROMEv:
		Dispatch 30
__ZN6CStore6vppOffEv:
		Dispatch 31
__ZN6CStore5sleepEv:
		Dispatch 32
__ZN6CStore20newWithinTransactionEPjm:
		Dispatch 33
__ZN6CStore23startTransactionAgainstEj:
		Dispatch 34
__ZN6CStore15separatelyAbortEj:
		Dispatch 35
__ZN6CStore23addToCurrentTransactionEj:
		Dispatch 36
__ZN6CStore21inSeparateTransactionEj:
		Dispatch 37
__ZN6CStore12lockReadOnlyEv:
		Dispatch 38
__ZN6CStore14unlockReadOnlyEb:
		Dispatch 39
__ZN6CStore13inTransactionEv:
		Dispatch 40
__ZN6CStore9newObjectEPjPvm:
		Dispatch 41
__ZN6CStore13replaceObjectEjPvm:
		Dispatch 42
__ZN6CStore17calcXIPObjectSizeEllPl:
		Dispatch 43
__ZN6CStore12newXIPObjectEPjm:
		Dispatch 44
__ZN6CStore16getXIPObjectInfoEjPmS0_S0_:
		Dispatch 45

/* ---------------------------------------------------------------- */

		.globl	__ZN12CLrgObjStore4makeEPKc
		.globl	__ZN12CLrgObjStore7destroyEv
		.globl	__ZN12CLrgObjStore4initEv
		.globl	__ZN12CLrgObjStore6createEPjP6CStoreP5CPipembPKcPvmP11CLOCallback
		.globl	__ZN12CLrgObjStore20createFromCompressedEPjP6CStoreP5CPipembPKcPvmP11CLOCallback
		.globl	__ZN12CLrgObjStore12deleteObjectEP6CStorej
		.globl	__ZN12CLrgObjStore9duplicateEPjP6CStorejS2_
		.globl	__ZN12CLrgObjStore6resizeEP6CStorejm
		.globl	__ZN12CLrgObjStore11storageSizeEP6CStorej
		.globl	__ZN12CLrgObjStore12sizeOfStreamEP6CStorejb
		.globl	__ZN12CLrgObjStore6backupEP5CPipeP6CStorejbP11CLOCallback

		.text
		.align	2

CLrgObjStore_name:
		.asciz	"CLrgObjStore"
		.align	2

__ZN12CLrgObjStore4makeEPKc:
		New CLrgObjStore_name
		Dispatch 2
__ZN12CLrgObjStore7destroyEv:
		Delete 3
__ZN12CLrgObjStore4initEv:
		Dispatch 4
__ZN12CLrgObjStore6createEPjP6CStoreP5CPipembPKcPvmP11CLOCallback:
		Dispatch 5
__ZN12CLrgObjStore20createFromCompressedEPjP6CStoreP5CPipembPKcPvmP11CLOCallback:
		Dispatch 6
__ZN12CLrgObjStore12deleteObjectEP6CStorej:
		Dispatch 7
__ZN12CLrgObjStore9duplicateEPjP6CStorejS2_:
		Dispatch 8
__ZN12CLrgObjStore6resizeEP6CStorejm:
		Dispatch 9
__ZN12CLrgObjStore11storageSizeEP6CStorej:
		Dispatch 10
__ZN12CLrgObjStore12sizeOfStreamEP6CStorejb:
		Dispatch 11
__ZN12CLrgObjStore6backupEP5CPipeP6CStorejbP11CLOCallback:
		Dispatch 12

#endif

