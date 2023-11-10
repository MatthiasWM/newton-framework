/*
	File:		StoreMonitorProto.s

	Contains:	CStoreMonitor p-class protocol interface.

	Written by:	Newton Research Group, 20014.
*/

#include "Stores/Store.h"
#include "Stores/StoreMonitor.h"

#include <cassert>

CStoreMonitor *  CStoreMonitor::make(const char * inName) {
  assert(0);
  return NULL;
}

void      CStoreMonitor::destroy(void) {
  assert(0);
}

NewtonErr  CStoreMonitor::init(CStore * inStore) {
  assert(0);
  return kNSErrInternalError;
}

NewtonErr  CStoreMonitor::needsFormat(bool * outNeedsFormat) {
  assert(0);
  return kNSErrInternalError;
}

NewtonErr  CStoreMonitor::format(void) {
  assert(0);
  return kNSErrInternalError;
}

NewtonErr  CStoreMonitor::getRootId(PSSId * outRootId) {
  assert(0);
  return kNSErrInternalError;
}

NewtonErr  CStoreMonitor::newObject(PSSId * outObjectId, size_t inSize) {
  assert(0);
  return kNSErrInternalError;
}

NewtonErr  CStoreMonitor::eraseObject(PSSId inObjectId) {
  assert(0);
  return kNSErrInternalError;
}

NewtonErr  CStoreMonitor::deleteObject(PSSId inObjectId) {
  assert(0);
  return kNSErrInternalError;
}

NewtonErr  CStoreMonitor::setObjectSize(PSSId inObjectId, size_t inSize) {
  assert(0);
  return kNSErrInternalError;
}

NewtonErr  CStoreMonitor::getObjectSize(PSSId inObjectId, size_t * outSize) {
  assert(0);
  return kNSErrInternalError;
}

NewtonErr  CStoreMonitor::write(PSSId inObjectId, size_t inStartOffset, void * inBuffer, size_t inLength) {
  assert(0);
  return kNSErrInternalError;
}

NewtonErr  CStoreMonitor::read(PSSId inObjectId, size_t inStartOffset, void * outBuffer, size_t inLength) {
  assert(0);
  return kNSErrInternalError;
}

NewtonErr  CStoreMonitor::getStoreSize(size_t * outTotalSize, size_t * outUsedSize) {
  assert(0);
  return kNSErrInternalError;
}

NewtonErr  CStoreMonitor::isReadOnly(bool * outIsReadOnly) {
  assert(0);
  return kNSErrInternalError;
}

NewtonErr  CStoreMonitor::lockStore(void) {
  assert(0);
  return kNSErrInternalError;
}

NewtonErr  CStoreMonitor::unlockStore(void) {
  assert(0);
  return kNSErrInternalError;
}

NewtonErr  CStoreMonitor::abort(void) {
  assert(0);
  return kNSErrInternalError;
}

NewtonErr  CStoreMonitor::idle(bool * outArg1, bool * outArg2) {
  assert(0);
  return kNSErrInternalError;
}

NewtonErr  CStoreMonitor::nextObject(PSSId inObjectId, PSSId * outNextObjectId) {
  assert(0);
  return kNSErrInternalError;
}

NewtonErr  CStoreMonitor::checkIntegrity(ULong * inArg1) {
  assert(0);
  return kNSErrInternalError;
}

NewtonErr  CStoreMonitor::newWithinTransaction(PSSId * outObjectId, size_t inSize) {
  assert(0);
  return kNSErrInternalError;
}

NewtonErr  CStoreMonitor::startTransactionAgainst(PSSId inObjectId) {
  assert(0);
  return kNSErrInternalError;
}

NewtonErr  CStoreMonitor::separatelyAbort(PSSId inObjectId) {
  assert(0);
  return kNSErrInternalError;
}

NewtonErr  CStoreMonitor::addToCurrentTransaction(PSSId inObjectId) {
  assert(0);
  return kNSErrInternalError;
}

NewtonErr  CStoreMonitor::lockReadOnly(void) {
  assert(0);
  return kNSErrInternalError;
}

NewtonErr  CStoreMonitor::unlockReadOnly(bool inReset) {
  assert(0);
  return kNSErrInternalError;
}

NewtonErr  CStoreMonitor::newObject(PSSId * outObjectId, void * inData, size_t inSize) {
  assert(0);
  return kNSErrInternalError;
}

NewtonErr  CStoreMonitor::replaceObject(PSSId inObjectId, void * inData, size_t inSize) {
  assert(0);
  return kNSErrInternalError;
}

NewtonErr  CStoreMonitor::newXIPObject(PSSId * outObjectId, size_t inSize) {
  assert(0);
  return kNSErrInternalError;
}



#if 0

#include "../Protocols/ProtoMacro.s"

/* ---------------------------------------------------------------- */

		.globl	__ZN13CStoreMonitor4makeEPKc
		.globl	__ZN13CStoreMonitor7destroyEv
		.globl	__ZN13CStoreMonitor4initEP6CStore
		.globl	__ZN13CStoreMonitor11needsFormatEPb
		.globl	__ZN13CStoreMonitor6formatEv
		.globl	__ZN13CStoreMonitor9getRootIdEPj
		.globl	__ZN13CStoreMonitor9newObjectEPjm
		.globl	__ZN13CStoreMonitor11eraseObjectEj
		.globl	__ZN13CStoreMonitor12deleteObjectEj
		.globl	__ZN13CStoreMonitor13setObjectSizeEjm
		.globl	__ZN13CStoreMonitor13getObjectSizeEjPm
		.globl	__ZN13CStoreMonitor5writeEjmPvm
		.globl	__ZN13CStoreMonitor4readEjmPvm
		.globl	__ZN13CStoreMonitor12getStoreSizeEPmS0_
		.globl	__ZN13CStoreMonitor10isReadOnlyEPb
		.globl	__ZN13CStoreMonitor9lockStoreEv
		.globl	__ZN13CStoreMonitor11unlockStoreEv
		.globl	__ZN13CStoreMonitor5abortEv
		.globl	__ZN13CStoreMonitor4idleEPbS0_
		.globl	__ZN13CStoreMonitor10nextObjectEjPj
		.globl	__ZN13CStoreMonitor14checkIntegrityEPj
		.globl	__ZN13CStoreMonitor20newWithinTransactionEPjm
		.globl	__ZN13CStoreMonitor23startTransactionAgainstEj
		.globl	__ZN13CStoreMonitor15separatelyAbortEj
		.globl	__ZN13CStoreMonitor23addToCurrentTransactionEj
		.globl	__ZN13CStoreMonitor12lockReadOnlyEv
		.globl	__ZN13CStoreMonitor14unlockReadOnlyEb
		.globl	__ZN13CStoreMonitor9newObjectEPjPvm
		.globl	__ZN13CStoreMonitor13replaceObjectEjPvm
		.globl	__ZN13CStoreMonitor12newXIPObjectEPjm

		.text
		.align	2

MonitorGlue

CStoreMonitor_name:
		.asciz	"CStoreMonitor"
		.align	2

__ZN13CStoreMonitor4makeEPKc:
		New CStoreMonitor_name
		ret
__ZN13CStoreMonitor7destroyEv:
		MonDelete 1
__ZN13CStoreMonitor4initEP6CStore:
		MonDispatch 2
__ZN13CStoreMonitor11needsFormatEPb:
		MonDispatch 3
__ZN13CStoreMonitor6formatEv:
		MonDispatch 4
__ZN13CStoreMonitor9getRootIdEPj:
		MonDispatch 5
__ZN13CStoreMonitor9newObjectEPjm:
		MonDispatch 6
__ZN13CStoreMonitor11eraseObjectEj:
		MonDispatch 7
__ZN13CStoreMonitor12deleteObjectEj:
		MonDispatch 8
__ZN13CStoreMonitor13setObjectSizeEjm:
		MonDispatch 9
__ZN13CStoreMonitor13getObjectSizeEjPm:
		MonDispatch 10
__ZN13CStoreMonitor5writeEjmPvm:
		MonDispatch 11
__ZN13CStoreMonitor4readEjmPvm:
		MonDispatch 12
__ZN13CStoreMonitor12getStoreSizeEPmS0_:
		MonDispatch 13
__ZN13CStoreMonitor10isReadOnlyEPb:
		MonDispatch 14
__ZN13CStoreMonitor9lockStoreEv:
		MonDispatch 15
__ZN13CStoreMonitor11unlockStoreEv:
		MonDispatch 16
__ZN13CStoreMonitor5abortEv:
		MonDispatch 17
__ZN13CStoreMonitor4idleEPbS0_:
		MonDispatch 18
__ZN13CStoreMonitor10nextObjectEjPj:
		MonDispatch 19
__ZN13CStoreMonitor14checkIntegrityEPj:
		MonDispatch 20
__ZN13CStoreMonitor20newWithinTransactionEPjm:
		MonDispatch 21
__ZN13CStoreMonitor23startTransactionAgainstEj:
		MonDispatch 22
__ZN13CStoreMonitor15separatelyAbortEj:
		MonDispatch 23
__ZN13CStoreMonitor23addToCurrentTransactionEj:
		MonDispatch 24
__ZN13CStoreMonitor12lockReadOnlyEv:
		MonDispatch 25
__ZN13CStoreMonitor14unlockReadOnlyEb:
		MonDispatch 26
__ZN13CStoreMonitor9newObjectEPjPvm:
		MonDispatch 27
__ZN13CStoreMonitor13replaceObjectEjPvm:
		MonDispatch 28
__ZN13CStoreMonitor12newXIPObjectEPjm:
		MonDispatch 29

#endif

