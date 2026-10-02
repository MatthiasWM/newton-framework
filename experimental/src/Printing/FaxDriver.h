/*
	File:		FaxDriver.h

	Contains:	TFaxDriver, the fax as a dot printer driver: its bands go
				to the fax tool, a page at a time; TFaxDriverData, its
				connection to the fax tool (reconstructed: not in the
				published headers; layouts from the ROM's code, with
				newton-re's findings; names ours where Apple's table has
				none).
*/

#ifndef __FAXDRIVER_H
#define __FAXDRIVER_H

#ifndef __PRINTER_H
#include "Printing/Printer.h"
#endif

#ifndef __FAXTOOLINTERFACE_H
#include "Communications/Fax/FaxToolInterface.h"
#endif


/* The fax tool's answers: each kept, and the print job waiting on it let go
   on (620 bytes) */
class TFaxDriverData : public TFaxToolInterface
{
public:
					TFaxDriverData() : TFaxToolInterface('faxs', 'mods') { }

	virtual void	OpenSessionComplete(NewtonErr inErr, ULong inArg1, ULong inArg2, ULong inWidth, ULong inVerticalRes);
	virtual void	AcceptSessionComplete(NewtonErr, ULong, ULong, ULong) { }
	virtual void	CloseSessionComplete(NewtonErr inErr);
	virtual void	BeginPageComplete(NewtonErr inErr);
	virtual void	EndPageComplete(NewtonErr inErr);
	virtual void	PrintBandComplete(NewtonErr inErr);
	virtual void	GetBandComplete(NewtonErr, ULong, Boolean) { }
	virtual void	ConfirmReceivedPageComplete(NewtonErr, Boolean) { }

	TPrinter *		fPrinter;			// +23C the print job waiting
	Boolean			fWaiting;			// +240 Open is waiting for the session
	Boolean			fSessionOpen;		// +241 the session's answer came
	NewtonErr		fOpenErr;			// +244
	ULong			fOpenArg1;			// +248
	ULong			fOpenArg2;			// +24C
	ULong			fWidth;				// +250 dots a line
	ULong			fVerticalRes;		// +254 lines an inch (98 or 196)
	NewtonErr		fCloseErr;			// +258
	NewtonErr		fBeginPageErr;		// +25C
	NewtonErr		fEndPageErr;		// +260
	NewtonErr		fBandErr;			// +264
	Boolean			fBandPending;		// +268 a band is on its way
};


PROTOCOL TFaxDriver : public TDotPrinterDriver
{
public:
	PROTOCOL_IMPL_HEADER_MACRO(TFaxDriver);

	void			Delete(void);
	NewtonErr		Open(void);
	NewtonErr		Close(void);
	NewtonErr		OpenPage(void);
	NewtonErr		ClosePage(void);
	NewtonErr		ImageBand(PixelMap * inBand, const Rect * inMinRect);
	void			CancelJob(Boolean inAsync);
	PrProblemResolution	IsProblemResolved(void);
	void			GetPageInfo(PrPageInfo * outInfo);
	void			GetBandPrefs(DotPrinterPrefs * outPrefs);
	NewtonErr		FaxEndPage(long inPageCount);

	void			PrintBlankLines(long inCount);
	Boolean			ContinueIO(void);

	TFaxDriverData *	fData;			// +18
	TOptionArray *	fConfig;			// +1C
	TOptionArray *	fSessionOptions;	// +20
	Boolean			fPageOpen;			// +24
	NewtonErr		fError;				// +28
	long			fLinesSent;			// +2C of the page so far (-1 between pages)
	long			fPageLines;			// +30
};

#endif	/* __FAXDRIVER_H */
