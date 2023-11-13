/*
	File:		Compression.h

	Contains:	Compression declarations.

	Written by:	Newton Research Group.
*/

#if !defined(__COMPRESSION_H)
#define __COMPRESSION_H 1

#include "Protocols.h"

/*------------------------------------------------------------------------------
	C C o m p r e s s o r
	P-class interface.
------------------------------------------------------------------------------*/

PROTOCOL CCompressor : public CProtocol
{
public:
	static CCompressor *	make(const char * inName);
	void			destroy(void) override;
	virtual NewtonErr	init(void *) = 0;
	virtual NewtonErr	compress(size_t * outSize, void * inDstBuf, size_t inDstLen, void * inSrcBuf, size_t inSrcLen) = 0;
	virtual size_t estimatedCompressedSize(void * inSrcBuf, size_t inSrcLen) = 0;
};


/*------------------------------------------------------------------------------
	C C a l l b a c k C o m p r e s s o r
	P-class interface.
------------------------------------------------------------------------------*/

/** Pure virtual class */
PROTOCOL CCallbackCompressor : public CProtocol
{
public:
	//static CCallbackCompressor *	make(const char * inName);
  // -- new methods for CCallbackCompressor
	virtual NewtonErr	init(void *) = 0;
	virtual void			reset(void) = 0;
	virtual NewtonErr	writeChunk(void * inSrcBuf, size_t inSrcLen) = 0;
	virtual NewtonErr	flush(void) = 0;
};


/*------------------------------------------------------------------------------
	C D e c o m p r e s s o r
	P-class interface.
------------------------------------------------------------------------------*/

/** Pure virtual class. */
PROTOCOL CDecompressor : public CProtocol
{
public:
  // -- inherited from CProtocol
	void			destroy(void) override = 0;
  // -- new for CDecompressor
	virtual NewtonErr	init(void *) = 0;
	virtual NewtonErr	decompress(size_t * outSize, void * inDstBuf, size_t inDstLen, void * inSrcBuf, size_t inSrcLen) = 0;
};


/*------------------------------------------------------------------------------
	C C a l l b a c k D e c o m p r e s s o r
	P-class interface.
------------------------------------------------------------------------------*/

/** Pure virtual class. */
PROTOCOL CCallbackDecompressor : public CProtocol
{
public:
  // -- inherited from CProtocol
	virtual void destroy(void) = 0;
  // -- new for CCallbackDecompressor
	virtual NewtonErr	init(void *) = 0;
	virtual void			reset(void) = 0;
	virtual NewtonErr	readChunk(void * inDstBuf, size_t * outBufLen, bool * outUnderflow) = 0;
};


/*------------------------------------------------------------------------------
	P u b l i c   I n t e r f a c e
------------------------------------------------------------------------------*/

void			InitializeCompression(void);
NewtonErr	InitializeStoreDecompressors(void);
NewtonErr	GetSharedLZObjects(CCompressor ** outCompressor, CDecompressor ** outDecompressor, char ** outBuffer, size_t * outBufLen);
void			ReleaseSharedLZObjects(CCompressor * inCompressor, CDecompressor * inDecompressor, char * inBuffer);

#endif	/* __COMPRESSION_H */
