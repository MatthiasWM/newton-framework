/*
	File:		StoreCompander.h

	Contains:	Store compression protocol.

	Written by:	Newton Research Group, 2007.
*/

#if !defined(__STORECOMPANDER_H)
#define __STORECOMPANDER_H 1

#include "Newton.h"
#include "Store.h"
#include "StoreRootObjects.h"
#include "Compression/Compression.h"
#include "Relocation.h"


/*------------------------------------------------------------------------------
	C S t o r e D e c o m p r e s s o r
	P-class interface.
------------------------------------------------------------------------------*/

/** Pure virtual class */
PROTOCOL CStoreDecompressor : public CProtocol
{
public:
  // -- inherited from CProtocol
	void			destroy(void) override = 0;
  // -- new for CStoreDecompressor
	virtual NewtonErr	init(CStore * inStore, PSSId inParmsId, char * inLZWBuffer = NULL) = 0;	// original has no inLZWBuffer
	virtual NewtonErr	read(PSSId inObjId, char * outBuf, size_t inBufLen, VAddr inBaseAddr) = 0;
};


/*------------------------------------------------------------------------------
	C S t o r e C o m p a n d e r
	P-class interface.
------------------------------------------------------------------------------*/

/** Pure virtual class. */
PROTOCOL CStoreCompander : public CProtocol
{
public:
//	static CStoreCompander *	make(const char * inName);
  // -- inherited from CProtocol
	void			destroy(void) override;
  // -- added for CStoreCompander
	virtual NewtonErr	init(CStore * inStore, PSSId inRootId, PSSId inParmsId, bool inShared, bool inArg5) = 0;
	virtual size_t		blockSize(void) = 0;
	virtual NewtonErr	read(size_t inOffset, char * outBuf, size_t inBufLen, VAddr inBaseAddr) = 0;
	virtual NewtonErr	write(size_t inOffset, char * inBuf, size_t inBufLen, VAddr inBaseAddr) = 0;
	virtual NewtonErr	doTransactionAgainst(int, ULong) = 0;
	virtual bool			isReadOnly(void) = 0;
};


PROTOCOL CStoreCompanderWrapper : public CStoreCompander
{
public:
    // -- inherited from CProtocol
	PROTOCOL_IMPL_HEADER_MACRO(CStoreCompanderWrapper)
	CStoreCompanderWrapper *	make(void) override;
	void			destroy(void) override;
    // -- inherited from CStoreCompander
	NewtonErr	init(CStore * inStore, PSSId inRootId, PSSId inParmsId, bool inShared, bool inArg5) override;
	size_t		blockSize(void) override;
	NewtonErr	read(size_t inOffset, char * outBuf, size_t inBufLen, VAddr inBaseAddr) override;
	NewtonErr	write(size_t inOffset, char * inBuf, size_t inBufLen, VAddr inBaseAddr) override;
	NewtonErr	doTransactionAgainst(int, ULong) override;
	bool			isReadOnly(void) override;
    // -- end of protocol

    NewtonErr  init(CStore * inStore, char * inCompanderName, PSSId inRootId, PSSId inParmsId, char * inLZWBuffer = NULL);  // original has no inLZWBuffer

private:
//										// +00	CProtocol fields
	CStore *		fStore;			// +10
	PSSId			fRootId;			// +14
	PSSId			fChunksId;		// +18
	CStoreDecompressor *	fDecompressor;	// +1C
	char *		fCompanderName;	// +20
};


#endif	/* __STORECOMPANDER_H */
