/*
	File:		SimpleStoreCompander.h

	Contains:	Simple store compression protocol.
					Simple => actually no compression at all.

	Written by:	Newton Research Group, 2007.
*/

#if !defined(__SIMPLESTORECOMPANDER_H)
#define __SIMPLESTORECOMPANDER_H 1

#include "StoreCompander.h"

/*------------------------------------------------------------------------------
	C S i m p l e S t o r e D e c o m p r e s s o r
------------------------------------------------------------------------------*/

PROTOCOL CSimpleStoreDecompressor : public CStoreDecompressor
{
public:
    // -- inherited from CProtocol
	PROTOCOL_IMPL_HEADER_MACRO(CSimpleStoreDecompressor)
	CSimpleStoreDecompressor *	make(void) override;
	void			destroy(void) override;
    // -- inherited from CStoreDecompressor
	NewtonErr		init(CStore * inStore, PSSId inParmsId, char * inLZWBuffer = NULL) override;	// original has no inLZWBuffer
	NewtonErr		read(PSSId inObjId, char * outBuf, size_t inBufLen, VAddr inBaseAddr) override;
    // -- end of protocol

private:
	char *				f10;
	CStore *				fStore;
};


/*------------------------------------------------------------------------------
	C S i m p l e R e l o c S t o r e D e c o m p r e s s o r
------------------------------------------------------------------------------*/

PROTOCOL CSimpleRelocStoreDecompressor : public CStoreDecompressor
{
public:
    // -- inherited from CProtocol
	PROTOCOL_IMPL_HEADER_MACRO(CSimpleRelocStoreDecompressor)
	CSimpleRelocStoreDecompressor *	make(void) override;
	void			destroy(void) override;
    // -- inherited from CStoreDecompressor
	NewtonErr		init(CStore * inStore, PSSId inParmsId, char * inLZWBuffer = NULL) override;	// original has no inLZWBuffer
	NewtonErr		read(PSSId inObjId, char * outBuf, size_t inBufLen, VAddr inBaseAddr) override;
    // -- end of protocol

private:
	CStore *				fStore;
};


/*------------------------------------------------------------------------------
	C S i m p l e S t o r e C o m p a n d e r
------------------------------------------------------------------------------*/

PROTOCOL CSimpleStoreCompander : public CStoreCompander
{
public:
    // -- inherited from CProtocol
	PROTOCOL_IMPL_HEADER_MACRO(CSimpleStoreCompander)
	CSimpleStoreCompander *	make(void) override;		// constructor
	void			destroy(void) override;				// destructor
    // -- inherited from CStoreCompander
	NewtonErr	init(CStore * inStore, PSSId inRootId, PSSId inParmsId, bool inShared, bool inArg5) override;
	size_t		blockSize(void) override;
	NewtonErr	read(size_t inOffset, char * outBuf, size_t inBufLen, VAddr inBaseAddr) override;
	NewtonErr	write(size_t inOffset, char * inBuf, size_t inBufLen, VAddr inBaseAddr) override;
	NewtonErr	doTransactionAgainst(int, ULong) override;
	bool			isReadOnly(void) override;
    // -- end of protocol

private:
//												// +00	CProtocol fields
	CStore *				fStore;			// +10
	PSSId					fRootId;			// +14
	PSSId					fChunksId;		// +18
	bool					fIsReadOnly;	// +1C
};


#endif	/* __SIMPLESTORECOMPANDER_H */
