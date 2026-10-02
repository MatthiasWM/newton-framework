/*
	File:		T4FaxLine.h

	Contains:	The fax tool's T.4 line codec (reconstructed: not in the
				published headers). TT4FaxLine decodes a received page:
				its modified Huffman (MH) code is put into a ring buffer as
				it arrives (AppendTo), read a bit at a time, least
				significant bit first, and decoded a scan line at a time
				into pixels, most significant bit first, a black pixel a 1
				(DecodeLine). EncodeT4 codes a scan line to be sent.

				Names ours where Apple's table has none (the fields, the
				parameters); the layout is the ROM's (0x34 bytes, the
				vtable pointer at +0).
*/

#ifndef __T4FAXLINE_H
#define __T4FAXLINE_H

#ifndef __NEWTON_H
#include "Newton.h"
#endif

#ifndef __NEWTONEXCEPTIONS_H
#include "NewtonExceptions.h"
#endif

/* The ring ran empty while a line was decoded (the data: an error code) */
DeclareException(exFaxBufOverrunException, exRootException);


/*------------------------------------------------------------------------------
	The decoder.
------------------------------------------------------------------------------*/

class TT4FaxLine
{
public:
	enum RunColor { kBlack = 0, kWhite = 1 };

					TT4FaxLine();
	virtual			~TT4FaxLine();

	void			Init(UChar * inRing, int inSize);
	void			Reset(void);
	Boolean			AppendTo(UChar ** ioData, int * ioCount, int * outAppended);
	Boolean			DecodeLine(UChar * outLine, int inLineBytes, int & outBytes, ULong inCatchOverrun);
	int				GetLength(void);
	Boolean			SkipPastEOL(void);

	Boolean			EmitBits(RunColor inColor, int inRun, int * outBytes);
	int				MHGetNextCode(RunColor inColor);
	int				GetNextBit(void);
	int				GetBits(int inCount);
	Boolean			DoMHDecodeLine(UChar * outLine, int inLineBytes, int & outBytes);

	UChar *			fRing;			// +04 the ring of received code
	int				fRingSize;		// +08
	UChar *			fRingEnd;		// +0C
	UChar *			fGet;			// +10 the byte last read
	UByte			fByte;			// +14 its bits not yet read
	int				fBitsInByte;	// +18
	Boolean			fLapped;		// +1C the writer is a lap ahead of the reader
	UChar *			fPut;			// +20 where the next byte goes
	UChar *			fLine;			// +24 the line being decoded into
	int				fLineBits;		// +28 free in fLineByte
	UByte			fLineByte;		// +2C
	UChar *			fLineEnd;		// +30
};


/*------------------------------------------------------------------------------
	The encoder.
------------------------------------------------------------------------------*/

int		EncodeT4(UChar * inLine, int inLineBytes, UChar * ioOut, int inOutSize, int inWidth, int inLeftMargin, int inMinBytes);
int		T4AddRTC(UChar * outCode);
void	writeCodeWord(UChar *& ioOut, UChar * inEnd, ULong inCode, ULong & ioBits, int & ioBitCount);
void	outputRun(UChar *& ioOut, UChar * inEnd, int inRun, UChar inColor, ULong & ioBits, int & ioBitCount);


/*------------------------------------------------------------------------------
	The code tables. A tree (kWhite_0.., kBlack_0..) is a heap of bytes, the
	root at 1, a node's children at 2n and 2n + 1: 0xFF an inner node, 0..63
	a terminating run, 0x40.. a make-up run of (n - 63) x 64, 0x68 an end of
	line, 0xFE not a code. A white code's first 4 bits choose its tree
	(kMajorIndexWhite), a black code's first bits as MHGetNextCode reads
	them. A code word (whiteCompleteTbl..) is its length << 16 | its bits,
	least significant first.
------------------------------------------------------------------------------*/

extern const UChar		kWhite_0[], kWhite_1[], kWhite_2[], kWhite_3[],
						kWhite_4[], kWhite_5[], kWhite_6[], kWhite_7[],
						kWhite_8[], kWhite_9[], kWhite_a[], kWhite_b[],
						kWhite_c[], kWhite_d[], kWhite_e[], kWhite_f[];
extern const UChar *	const kMajorIndexWhite[16];
extern const UChar		kBlack_0[], kBlack_1[], kBlack_2[], kBlack_3[];
extern const UChar *	const kMajorIndexBlack[4];

extern const ULong		whiteCompleteTbl[64];
extern const ULong		whiteMakeupTbl[29];
extern const ULong		blackCompleteTbl[64];
extern const ULong		blackMakeupTbl[29];

#endif	/* __T4FAXLINE_H */
