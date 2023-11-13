/*
	File:		StoreMonitor.h

	Contains:	Newton store monitor interface.

	Written by:	Newton Research Group, 2010.
*/

#if !defined(__STOREMONITOR_H)
#define __STOREMONITOR_H 1

#include "Protocols.h"
#include "PSSTypes.h"

/*------------------------------------------------------------------------------
	C S t o r e M o n i t o r
	P-class interface.

	It’s not the full store protocol.
------------------------------------------------------------------------------*/

MONITOR CStoreMonitor : public CProtocol
{
public:
	static CStoreMonitor *	make(const char * inName);
  // -- inherited from CProtocol
	void			destroy(void) override;
  // -- added to CStoreMonitor
	virtual NewtonErr	init(CStore * inStore) = 0;
  virtual NewtonErr	needsFormat(bool * outNeedsFormat) = 0;
  virtual NewtonErr	format(void) = 0;

  virtual NewtonErr	getRootId(PSSId * outRootId) = 0;
  virtual NewtonErr	newObject(PSSId * outObjectId, size_t inSize) = 0;
  virtual NewtonErr	eraseObject(PSSId inObjectId) = 0;
  virtual NewtonErr	deleteObject(PSSId inObjectId) = 0;
  virtual NewtonErr	setObjectSize(PSSId inObjectId, size_t inSize) = 0;
  virtual NewtonErr	getObjectSize(PSSId inObjectId, size_t * outSize) = 0;

  virtual NewtonErr	write(PSSId inObjectId, size_t inStartOffset, void * inBuffer, size_t inLength) = 0;
  virtual NewtonErr	read(PSSId inObjectId, size_t inStartOffset, void * outBuffer, size_t inLength) = 0;

  virtual NewtonErr	getStoreSize(size_t * outTotalSize, size_t * outUsedSize) = 0;
  virtual NewtonErr	isReadOnly(bool * outIsReadOnly) = 0;
  virtual NewtonErr	lockStore(void) = 0;
  virtual NewtonErr	unlockStore(void) = 0;

  virtual NewtonErr	abort(void) = 0;
  virtual NewtonErr	idle(bool * outArg1, bool * outArg2) = 0;

  virtual NewtonErr	nextObject(PSSId inObjectId, PSSId * outNextObjectId) = 0;
  virtual NewtonErr	checkIntegrity(ULong * inArg1) = 0;

  virtual NewtonErr	newWithinTransaction(PSSId * outObjectId, size_t inSize) = 0;
  virtual NewtonErr	startTransactionAgainst(PSSId inObjectId) = 0;
  virtual NewtonErr	separatelyAbort(PSSId inObjectId) = 0;
  virtual NewtonErr	addToCurrentTransaction(PSSId inObjectId) = 0;

  virtual NewtonErr	lockReadOnly(void) = 0;
  virtual NewtonErr	unlockReadOnly(bool inReset) = 0;

  virtual NewtonErr	newObject(PSSId * outObjectId, void * inData, size_t inSize) = 0;
  virtual NewtonErr	replaceObject(PSSId inObjectId, void * inData, size_t inSize) = 0;

  virtual NewtonErr	newXIPObject(PSSId * outObjectId, size_t inSize) = 0;
};

#endif	/* __STOREMONITOR_H */
