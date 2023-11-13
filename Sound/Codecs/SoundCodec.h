/*
	File:		SoundCodec.h

	Contains:	Sound codec protocol definition.

	Written by:	Newton Research Group, 2007.
*/

#if !defined(__SOUNDCODEC_H)
#define __SOUNDCODEC_H 1

#include "Objects.h"
#include "Protocols.h"

/*------------------------------------------------------------------------------
	C S o u n d C o d e c
------------------------------------------------------------------------------*/

// data types
enum
{
	k8Bit = 8,
	k16Bit = 16
};

// compression types
enum
{
	kSampleStandard,		// uncompressed 8-bit samples
	kSampleMuLaw,			// µ-law compressed 8-bit samples
	kSampleLinear = 6		// uncompressed 16-bit samples
};

// parameters defining a codec
struct CodecBlock
{
	int			x00;
	void *		data;			// +04	source buffer
	size_t		dataSize;	// +08	source buffer length / number of samples for simpleSound
	int			dataType;	//	+0C
	int			comprType;	// +10
	float			sampleRate;	// +14	number of samples per second
	RefStruct *	frame;		// +18	original sndFrame
};


/*------------------------------------------------------------------------------
	C S o u n d C o d e c
	P-class interface.
	The protocol upon which all sound codecs must be based.
------------------------------------------------------------------------------*/

PROTOCOL CSoundCodec : public CProtocol
{
public:
	static CSoundCodec *	make(const char * inName);
	void			destroy(void) override;

	virtual NewtonErr	init(CodecBlock * inParms) = 0;
  virtual NewtonErr	reset(CodecBlock * inParms) = 0;
	virtual NewtonErr	produce(void * outBuf, size_t * ioBufSize, size_t * outDataSize, CodecBlock * ioParms) = 0;
	virtual NewtonErr	consume(const void * inBuf, size_t * ioBufSize, size_t * outDataSize, const CodecBlock * inParms) = 0;
	virtual void			start(void) = 0;
	virtual void			stop(int) = 0;
	virtual bool			bufferCompleted(void) = 0;
};

#endif	/* __SOUNDCODEC_H */
