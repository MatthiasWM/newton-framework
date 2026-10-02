/*
	File:		Printer.h

	Contains:	TPrinter, the imaging engine a print job draws through, and
				TDotPrinter, the one for dot matrix (bitmap) drivers
				(reconstructed: not in the published headers; layouts from
				the ROM's code, with newton-re's findings; names ours where
				Apple's table has none).
*/

#ifndef __PRINTER_H
#define __PRINTER_H

#ifndef __DOTDRIVERS_H
#include "Printing/DotDrivers.h"
#endif

class TPseudoSyncState;

PROTOCOL TPrinter : public TProtocol
{
public:
	void			OpenPort(const PrPageInfo & inInfo);
	void			ClosePort(void);
	GrafPort *		GetPrinterPort(void);
	GrafPort *		GetPort(void);
	ScalerInfo *	GetScalerInfo(void);
	void			SetScalerInfo(const PrPageInfo & inInfo);
	void			SetupConnect(PrintConnect * outConnect, RefArg inConnectInfo);
	void			DoUserAbort(void);
	Boolean			CheckUserAbort(void);

	NewtonErr			fError;				// +10 what the last call came to
	Boolean				fCancelled;			// +14
	Boolean				fCancelHandled;		// +15
	TPseudoSyncState *	fBlocked;			// +18
	long				fReserved[3];		// +1C
	ScalerInfo			fScaler;			// +28
	PrintPort			fPort;				// +40
};


PROTOCOL TDotPrinter : public TPrinter
{
public:
	PROTOCOL_IMPL_HEADER_MACRO(TDotPrinter);

	NewtonErr		Constructor(char * inDriverName);
	void			Delete(void);
	NewtonErr		Open(RefArg inConnectInfo);
	NewtonErr		Close(void);
	NewtonErr		OpenPage(void);
	NewtonErr		ClosePage(void);
	Boolean			RepeatPage(void);
	void			CancelJob(Boolean inAsync);
	PrProblemResolution	IsProblemResolved(void);
	void			SetPortraitOrientation(Boolean inPortrait);
	NewtonErr		FaxEndPage(long inPageCount);

	void			CalcMinBounds(const PixelMap * inBand, long inUnused, Rect * outBounds);
	Boolean			TryAllocBands(char ** outBands, long inCount, long inSize);

	TDotPrinterDriver *	fDriver;			// +098
	DotPrinterPrefs	fPrefs;					// +09C
	PixelMap		fBands[2];				// +0A8
	long			fBandHeight72;			// +0E0
	long			fBandHeight;			// +0E4
	Rect			fPageBand;				// +0E8
	Rect			fBandRect;				// +0F0
	Point			fPatOffset;				// +0F8
	Point			fPatOffset2;			// +0FC
	long			fBandCount;				// +100
	long			fCurBand;				// +104
	RgnHandle		fClip;					// +108
	RgnHandle		fVis;					// +10C
	long			fBandSize;				// +110
	PixelMap		fMask;					// +114
	PixelMap		fPattern;				// +130
	PixelMap		fTurned;				// +14C
	Ptr				fMaskBits;				// +168
	PatternHandle	fScalePat;				// +16C
	long			fScalePatAlign;			// +170
	PrintPort		fPhantom;				// +174
};

/* Whether a printer package is running */
extern Boolean	gSCPDevicePackageBusy;

Boolean		SetupScalingBottlenecks(GrafPort * inPort);
void		TearDownScalingBottlenecks(GrafPort * inPort);
void		PrintPatchpoint(void);

#endif	/* __PRINTER_H */
