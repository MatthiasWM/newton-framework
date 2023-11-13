/*
	File:		CompressionProto.s

	Contains:	CCompressor, CCallbackCompressor and
					CDecompressor, CCallbackDecompressor p-class protocol interfaces.

	Written by:	Newton Research Group, 2009.
*/

#include "Compression/Compression.h"
#include "StoreCompander.h"

#include <cassert>

NewtonErr CCompressor::init(void*) {
  assert(0);
  return kNSErrInternalError;
}

void      CCompressor::destroy(void) {
  assert(0);
}

NewtonErr  CCompressor::compress(size_t * outSize, void * inDstBuf, size_t inDstLen, void * inSrcBuf, size_t inSrcLen) {
  assert(0);
  return kNSErrInternalError;
}

// --------

CDecompressor *  CDecompressor::make(const char * inName) {
  assert(0);
  return nullptr;
}

void      CDecompressor::destroy(void) {
  assert(0);
}

//NewtonErr  CDecompressor::init(void *) {
//  assert(0);
//  return kNSErrInternalError;
//}

NewtonErr  CDecompressor::decompress(size_t * outSize, void * inDstBuf, size_t inDstLen, void * inSrcBuf, size_t inSrcLen) {
  assert(0);
  return kNSErrInternalError;
}

// --------

CStoreCompander *  CStoreCompander::make(const char * inName) {
  assert(0);
  return nullptr;
}

void      CStoreCompander::destroy(void) {
  assert(0);
}

NewtonErr  CStoreCompander::init(CStore * inStore, PSSId inRootId, PSSId inParmsId, bool inShared, bool inArg5) {
  assert(0);
  return kNSErrInternalError;
}

size_t    CStoreCompander::blockSize(void) {
  assert(0);
  return 0;
}

NewtonErr  CStoreCompander::read(size_t inOffset, char * outBuf, size_t inBufLen, VAddr inBaseAddr) {
  assert(0);
  return kNSErrInternalError;
}

NewtonErr  CStoreCompander::write(size_t inOffset, char * inBuf, size_t inBufLen, VAddr inBaseAddr) {
  assert(0);
  return kNSErrInternalError;
}

NewtonErr  CStoreCompander::doTransactionAgainst(int, ULong) {
  assert(0);
  return kNSErrInternalError;
}

bool      CStoreCompander::isReadOnly(void) {
  assert(0);
  return true;
}

// --------

CStoreDecompressor *  CStoreDecompressor::make(const char * inName) {
  assert(0);
  return nullptr;
}

void      CStoreDecompressor::destroy(void) {
  assert(0);
}

NewtonErr  CStoreDecompressor::init(CStore * inStore, PSSId inParmsId, char * inLZWBuffer) {
assert(0);
return kNSErrInternalError;
}

NewtonErr  CStoreDecompressor::read(PSSId inObjId, char * outBuf, size_t inBufLen, VAddr inBaseAddr) {
  assert(0);
  return kNSErrInternalError;
}

// --------

CCallbackCompressor *CCallbackCompressor::make(const char * inName) {
  assert(0);
  return nullptr;
}

//void      CCallbackCompressor::destroy(void) {
//  assert(0);
//}

NewtonErr  CCallbackCompressor::init(void *) {
  assert(0);
  return kNSErrInternalError;
}

void      CCallbackCompressor::reset(void) {
  assert(0);
}

//NewtonErr  CCallbackCompressor::writeChunk(void * inSrcBuf, size_t inSrcLen) {
//  assert(0);
//  return kNSErrInternalError;
//}

//NewtonErr  CCallbackCompressor::flush(void) {
//  assert(0);
//  return kNSErrInternalError;
//}

// --------

CCallbackDecompressor *CCallbackDecompressor::make(const char * inName) {
  assert(0);
  return nullptr;
}

//void      CCallbackDecompressor::destroy(void) {
//  assert(0);
//}

NewtonErr  CCallbackDecompressor::init(void *) {
  assert(0);
  return kNSErrInternalError;
}

void      CCallbackDecompressor::reset(void) {
  assert(0);
}

NewtonErr  CCallbackDecompressor::readChunk(void * inDstBuf, size_t * outBufLen, bool * outUnderflow) {
  assert(0);
  return kNSErrInternalError;
}




#if 0

#include "../Protocols/ProtoMacro.s"

/* ---------------------------------------------------------------- */

		.globl	__ZN11CCompressor4makeEPKc
		.globl	__ZN11CCompressor7destroyEv
		.globl	__ZN11CCompressor4initEPv
		.globl	__ZN11CCompressor8compressEPmPvmS1_m
		.globl	__ZN11CCompressor23estimatedCompressedSizeEPvm

		.text
		.align	2

CCompressor_name:
		.asciz	"CCompressor"
		.align	2

__ZN11CCompressor4makeEPKc:
		New CCompressor_name
		Dispatch 2
__ZN11CCompressor7destroyEv:
		Delete 3
__ZN11CCompressor4initEPv:
		Dispatch 4
__ZN11CCompressor8compressEPmPvmS1_m:
		Dispatch 5
__ZN11CCompressor23estimatedCompressedSizeEPvm:
		Dispatch 6

/* ---------------------------------------------------------------- */

		.globl	__ZN19CCallbackCompressor4makeEPKc
		.globl	__ZN19CCallbackCompressor7destroyEv
		.globl	__ZN19CCallbackCompressor4initEPv
		.globl	__ZN19CCallbackCompressor5resetEv
		.globl	__ZN19CCallbackCompressor10writeChunkEPvm
		.globl	__ZN19CCallbackCompressor5flushEv

		.text
		.align	2

CCallbackCompressor_name:
		.asciz	"CCallbackCompressor"
		.align	2

__ZN19CCallbackCompressor4makeEPKc:
		New CCallbackCompressor_name
		Dispatch 2
__ZN19CCallbackCompressor7destroyEv:
		Delete 3
__ZN19CCallbackCompressor4initEPv:
		Dispatch 4
__ZN19CCallbackCompressor5resetEv:
		Dispatch 5
__ZN19CCallbackCompressor10writeChunkEPvm:
		Dispatch 6
__ZN19CCallbackCompressor5flushEv:
		Dispatch 7

/* ---------------------------------------------------------------- */

		.globl	__ZN13CDecompressor4makeEPKc
		.globl	__ZN13CDecompressor7destroyEv
		.globl	__ZN13CDecompressor4initEPv
		.globl	__ZN13CDecompressor10decompressEPmPvmS1_m

		.text
		.align	2

CDecompressor_name:
		.asciz	"CDecompressor"
		.align	2

__ZN13CDecompressor4makeEPKc:
		New CDecompressor_name
		Dispatch 2
__ZN13CDecompressor7destroyEv:
		Delete 3
__ZN13CDecompressor4initEPv:
		Dispatch 4
__ZN13CDecompressor10decompressEPmPvmS1_m:
		Dispatch 5

/* ---------------------------------------------------------------- */

		.globl	__ZN21CCallbackDecompressor4makeEPKc
		.globl	__ZN21CCallbackDecompressor7destroyEv
		.globl	__ZN21CCallbackDecompressor4initEPv
		.globl	__ZN21CCallbackDecompressor5resetEv
		.globl	__ZN21CCallbackDecompressor9readChunkEPvPmPb

		.text
		.align	2

CCallbackDecompressor_name:
		.asciz	"CCallbackDecompressor"
		.align	2

__ZN21CCallbackDecompressor4makeEPKc:
		New CCallbackDecompressor_name
		Dispatch 2
__ZN21CCallbackDecompressor7destroyEv:
		Delete 3
__ZN21CCallbackDecompressor4initEPv:
		Dispatch 4
__ZN21CCallbackDecompressor5resetEv:
		Dispatch 5
__ZN21CCallbackDecompressor9readChunkEPvPmPb:
		Dispatch 6

/* ---------------------------------------------------------------- */

		.globl	__ZN18CStoreDecompressor4makeEPKc
		.globl	__ZN18CStoreDecompressor7destroyEv
		.globl	__ZN18CStoreDecompressor4initEP6CStorejPc
		.globl	__ZN18CStoreDecompressor4readEjPcmm

		.text
		.align	2

CStoreDecompressor_name:
		.asciz	"CStoreDecompressor"
		.align	2

__ZN18CStoreDecompressor4makeEPKc:
		New CStoreDecompressor_name
		Dispatch 2
__ZN18CStoreDecompressor7destroyEv:
		Delete 3
__ZN18CStoreDecompressor4initEP6CStorejPc:
		Dispatch 4
__ZN18CStoreDecompressor4readEjPcmm:
		Dispatch 5

/* ---------------------------------------------------------------- */

		.globl	__ZN15CStoreCompander4makeEPKc
		.globl	__ZN15CStoreCompander7destroyEv
		.globl	__ZN15CStoreCompander4initEP6CStorejjbb
		.globl	__ZN15CStoreCompander9blockSizeEv
		.globl	__ZN15CStoreCompander4readEmPcmm
		.globl	__ZN15CStoreCompander5writeEmPcmm
		.globl	__ZN15CStoreCompander20doTransactionAgainstEij
		.globl	__ZN15CStoreCompander10isReadOnlyEv

		.text
		.align	2

CStoreCompander_name:
		.asciz	"CStoreCompander"
		.align	2

__ZN15CStoreCompander4makeEPKc:
		New CStoreCompander_name
		Dispatch 2
__ZN15CStoreCompander7destroyEv:
		Delete 3
__ZN15CStoreCompander4initEP6CStorejjbb:
		Dispatch 4
__ZN15CStoreCompander9blockSizeEv:
		Dispatch 5
__ZN15CStoreCompander4readEmPcmm:
		Dispatch 6
__ZN15CStoreCompander5writeEmPcmm:
		Dispatch 7
__ZN15CStoreCompander20doTransactionAgainstEij:
		Dispatch 8
__ZN15CStoreCompander10isReadOnlyEv:
		Dispatch 9

#endif
