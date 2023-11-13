/*
	File:		Compression.cc

	Contains:	Arithmetic compression initialization.

	Written by:	Newton Research Group, 2007.
*/

#include "Newton.h"
#include "Compression/Compression.h"


PROTOCOL CArithmeticCompressor : public CCallbackCompressor
	PROTOCOLVERSION(1.0)
{
public:
	PROTOCOL_IMPL_HEADER_MACRO(CArithmeticCompressor)

	CArithmeticCompressor *	make(void) override;
	void			destroy(void) override;

	NewtonErr	init(void *) override;
	void			reset(void) override;
	NewtonErr	writeChunk(void * inSrcBuf, size_t inSrcLen) override;
	NewtonErr	flush(void) override;

private:
/*	NewtonErr	narrowRegion(int);
	NewtonErr	startModel(void);
	NewtonErr	updateModel(int);
	NewtonErr	startOutputtingBits(void);
	NewtonErr	pushOutBits(void);
	NewtonErr	writeByte(unsigned char);
	NewtonErr	flushBits(void);
	NewtonErr	cleanup(void);
*/};


PROTOCOL CArithmeticDecompressor : public CCallbackDecompressor
	PROTOCOLVERSION(1.0)
{
public:
	PROTOCOL_IMPL_HEADER_MACRO(CArithmeticDecompressor)

	CArithmeticDecompressor *	make(void) override;
	void			destroy(void) override;

	NewtonErr	init(void *) override;
	void			reset(void) override;
	NewtonErr	readChunk(void * inDstBuf, size_t * outBufLen, bool * outArg3) override;

private:
/*	NewtonErr	narrowRegion(int);
	NewtonErr	startModel(void);
	NewtonErr	updateModel(int);
	NewtonErr	startReadingBits(void);
	NewtonErr	discardBits(void);
	NewtonErr	readByte(void);
	NewtonErr	findSymbol(void);
	NewtonErr	cleanup(void);
*/};


void
InitArithmeticCompression(void)
{
#if 0
	CArithmeticCompressor::classInfo()->registerProtocol();
	CArithmeticDecompressor::classInfo()->registerProtocol();
#endif
}
