/*
	File:		FaxDriver.cc

	Contains:	TFaxDriver, the fax as a dot printer driver: Open starts the
				fax tool and dials, each band imaged goes to the tool as the
				rows with anything in them (blank lines stand for the rest),
				a page is framed by a quarter inch of blank lines.
				TFaxDriverData keeps the tool's answers.

	ROM:		0x20F044 (TFaxDriver::Sizeof) .. 0x20FAE8 (after ContinueIO),
				and its constant data 0x378BE4..0x378C20, MP2x00 US 2.1
				(717006). Not in place yet (#if 0, see each): GetPageInfo
				(and its templates), ImageBand.
*/

#include "Printing/FaxDriver.h"
#include "Printing/PrintErrors.h"
#include "Communications/CommErrors.h"
#include "Communications/Fax/FaxOptions.h"
#include "Communications/ModemOptions.h"
#include "Frames/ROMResources.h"
#include "Frames/RSSymbols.h"
#include "Utilities/Unicode.h"
#include "CLibrary/string.h"


size_t
TFaxDriver::Sizeof(void)
{
	return sizeof(TFaxDriver);
}


/*------------------------------------------------------------------------------
	Opening: the fax tool started, its options from the connection frame,
	the number dialled; the job waits until the session is there.
------------------------------------------------------------------------------*/

NewtonErr
TFaxDriver::Open(void)
{
	RefVar	connectInfo(fConnect->fConnectInfo);

	fData = NULL;
	fConfig = NULL;
	fSessionOptions = NULL;
	fPageOpen = false;
	fLinesSent = -1;
	fError = noErr;
	if ((fConfig = new TOptionArray) == NULL)
		return kPR_ERR_NewtonError;
	fConfig->Init();
	if ((fSessionOptions = new TOptionArray) == NULL)
		return kPR_ERR_NewtonError;
	fSessionOptions->Init();
	fData = new TFaxDriverData;
	if (fData == NULL)
		return kPR_ERR_NewtonError;

	fData->fPrinter = fPrinter;
	fData->fWaiting = false;
	fData->fSessionOpen = false;
	fData->fBandPending = false;
	if (fError == noErr)
	{
		fData->SetDefaultConfig(fConfig, 0);
		NewtonErr err = fData->Init(fConfig, 'faxs', 'newt');
		if (fError == noErr)
			fError = err;
		if (fError == noErr)
		{
			fData->SetDefaultOptions(fSessionOptions);

			TCMOModemDialing dialing;
			SetDialingOptionsFromPrefs(&dialing);

			RefVar	phoneRef(GetFrameSlot(connectInfo, SYMA(phonenumber)));
			char	phone[256];
			if (ISNIL(phoneRef))
			{
				phone[0] = ' ';
				phone[1] = 0;
			}
			else
			{
				DataPtr	phoneStr = DataPtr(phoneRef);
				ConvertFromUnicode((UniChar *) (char *) phoneStr, phone, kMacRomanEncoding, 0x7FFFFFFF);
			}

			RefVar	manual(GetFrameSlot(connectInfo, SYMA(manualdialing)));
			dialing.fManualDial = NOTNIL(manual);
			if (dialing.fManualDial)
			{
				dialing.fDetectDialTone = false;
				dialing.fDetectBusy = false;
			}
			fSessionOptions->AppendOption(&dialing);

			RefVar	value(GetFrameSlot(connectInfo, SYMA(localid)));
			if (NOTNIL(value))
			{
				TCMOFaxLocalId localId;
				localId.SetOpCode(opSetRequired);
				ConvertFromUnicode(GetCString(value), localId.fId, kMacRomanEncoding, 20);
				fSessionOptions->AppendOption(&localId);
			}

			value = GetFrameSlot(connectInfo, SYMA(faxresolution));
			if (NOTNIL(value))
			{
				ULong resolution = 2;
				if (EQ(value, SYMA(normal)))
					resolution = 1;
				TOptionIterator iter(fSessionOptions);
				TCMOFaxPageSetUp * setUp = (TCMOFaxPageSetUp *) iter.FindOption(kCMOFaxPageSetUp);
				if (setUp)
					setUp->fResolution = resolution;
			}

			TCMOFaxRemoteId remoteId;
			remoteId.SetOpCode(opGetCurrent);
			fSessionOptions->AppendOption(&remoteId);

			if (fError != kPR_ERR_UserCancel)
			{
				fData->OpenSession(fSessionOptions, (UChar *) phone, strlen(phone), true);
				fData->fWaiting = true;
				PrReleaseControl(kTimeOutImmediate, fPrinter);
				fData->fWaiting = false;
				if (fError != kPR_ERR_UserCancel && fError == noErr)
				{
					TOptionIterator iter(fSessionOptions);
					TCMOFaxRemoteId * remote = (TCMOFaxRemoteId *) iter.FindOption(kCMOFaxRemoteId);
					if (remote)
					{
						UniChar idStr[22];
						ConvertToUnicode(remote->fId, idStr, kMacRomanEncoding, 20);
						SetFrameSlot(connectInfo, SYMA(remoteid), MakeString(idStr));
					}
				}
			}
		}
	}
	return fError;
}


/*------------------------------------------------------------------------------
	The page: 200 dots an inch, as fine as the session agreed (196 lines an
	inch, else 98: half as many lines), letter or A4.
------------------------------------------------------------------------------*/

#if 0
/* Not yet: identical but for four words (0x20F520: the ROM copies the
   first template with destination r2 and source r1, here the other way
   round; the wait loop's form moves them: with braces the three others are
   right). Until the form is found, it stays generated assembler, with its
   templates (0x378BE4..0x378C14). */
void
TFaxDriver::GetPageInfo(PrPageInfo * outInfo)
{
	Boolean		isA4 = EQ(fConnect->fPaperSize, SYMA(a4));
	PrPageInfo	letterFine = { { 200 << 16, 200 << 16 }, { 2050, 1650 } };
	PrPageInfo	letterNormal = { { 200 << 16, 100 << 16 }, { 1025, 1650 } };
	PrPageInfo	a4Fine = { { 200 << 16, 200 << 16 }, { 2188, 1596 } };
	PrPageInfo	a4Normal = { { 200 << 16, 100 << 16 }, { 1094, 1596 } };
	PrPageInfo *	info;

	while (!fData->fSessionOpen)		// read once (BUGS.md B11)
		{ }
	if (fData->fVerticalRes == 196)
		info = isA4 ? &a4Fine : &letterFine;
	else if (fData->fVerticalRes == 98)
		info = isA4 ? &a4Normal : &letterNormal;
	else
		info = isA4 ? &a4Normal : &letterNormal;
	fPageLines = info->printerPageSize.v;
	*outInfo = *info;
}
#endif


void
TFaxDriver::GetBandPrefs(DotPrinterPrefs * outPrefs)
{
	DotPrinterPrefs	prefs = { 25, 25, true, true };
	*outPrefs = prefs;
}


/*------------------------------------------------------------------------------
	Blank lines: a band without bits.
------------------------------------------------------------------------------*/

void
TFaxDriver::PrintBlankLines(long inCount)
{
	if (ContinueIO())
	{
		if (fData->fBandPending)
			PrReleaseControl(kTimeOutImmediate, fPrinter);
		fData->fBandPending = true;
		fData->PrintBand(NULL, inCount, 0, 0, true);
		fLinesSent += inCount;
	}
}


void
TFaxDriver::Delete(void)
{
	delete fData;
	delete fConfig;
	delete fSessionOptions;
}


/*------------------------------------------------------------------------------
	T F a x D r i v e r D a t a
------------------------------------------------------------------------------*/

void
TFaxDriverData::OpenSessionComplete(NewtonErr inErr, ULong inArg1, ULong inArg2, ULong inWidth, ULong inVerticalRes)
{
	fWaiting = false;
	fSessionOpen = true;
	fOpenErr = inErr;
	fOpenArg1 = inArg1;
	fOpenArg2 = inArg2;
	fWidth = inWidth;
	fVerticalRes = inVerticalRes;
	PrRegainControl(fPrinter);
}


void
TFaxDriverData::CloseSessionComplete(NewtonErr inErr)
{
	fCloseErr = inErr;
}


void
TFaxDriverData::BeginPageComplete(NewtonErr inErr)
{
	fBeginPageErr = inErr;
}


void
TFaxDriverData::EndPageComplete(NewtonErr inErr)
{
	fEndPageErr = inErr;
}


void
TFaxDriverData::PrintBandComplete(NewtonErr inErr)
{
	fBandPending = false;
	fBandErr = inErr;
	PrRegainControl(fPrinter);
}


/*------------------------------------------------------------------------------
	Closing, pages.
------------------------------------------------------------------------------*/

NewtonErr
TFaxDriver::Close(void)
{
	if (ContinueIO() && fPageOpen)
	{
		fData->EndPage(false, true);
		fError = fData->fEndPageErr;
	}
	if (ContinueIO() && fData->fBandPending)
		PrReleaseControl(kTimeOutImmediate, fPrinter);
	fData->CloseSession(false);
	if (fError == noErr)
		fError = fData->fCloseErr;
	return fError;
}


NewtonErr
TFaxDriver::OpenPage(void)
{
	if (ContinueIO())
	{
		fData->BeginPage(false);
		fError = fData->fBeginPageErr;
		fPageOpen = true;
	}
	fLinesSent = 0;
	PrintBlankLines(fData->fVerticalRes / 4);
	return fError;
}


NewtonErr
TFaxDriver::FaxEndPage(long inPageCount)
{
	if (inPageCount && ContinueIO() && fPageOpen)
	{
		fData->EndPage(false, false);
		if (fError == noErr)
			fError = fData->fEndPageErr;
	}
	return fError;
}


NewtonErr
TFaxDriver::ClosePage(void)
{
	PrintBlankLines(fData->fVerticalRes / 4);
	fLinesSent = -1;
	return fError;
}


/*------------------------------------------------------------------------------
	A band: blank lines above and below its black rows, the rows sent.
------------------------------------------------------------------------------*/

#if 0
/* Not yet: identical but for four words (0x20F938: for above the ROM
   loads the band's top into r1 and the black's top into r0, here the
   other way round; declaring the locals at the top, in this order, put the
   two subtractions before it right). Until the form is found, it stays
   generated assembler. */
NewtonErr
TFaxDriver::ImageBand(PixelMap * inBand, const Rect * inMinRect)
{
	long	above;
	long	bandLines;
	ULong	lineBytes;
	Ptr		bits;
	long	blank;
	long	lines;

	lineBytes = fData->fWidth >> 3;
	bandLines = inBand->bounds.bottom - inBand->bounds.top;
	lines = inMinRect->bottom - inMinRect->top;

	if (ContinueIO())
	{
		if (lines <= 0)
			blank = bandLines;
		else
		{
			bits = inBand->baseAddr;
			above = inMinRect->top - inBand->bounds.top;
			if (above)
			{
				PrintBlankLines(above);
				bits += above * inBand->rowBytes;
			}
			if (ContinueIO() && fData->fBandPending)
				PrReleaseControl(kTimeOutImmediate, fPrinter);
			if (fData->fBandErr)
				return 1;
			if (ContinueIO())
			{
				fData->fBandPending = true;
				fData->PrintBand((UChar *) bits, lines, inBand->rowBytes, lineBytes, true);
				fLinesSent += lines;
			}
			if ((blank = inBand->bounds.bottom - inMinRect->bottom) == 0)
				goto done;
		}
		PrintBlankLines(blank);
	}
done:
	return fError;
}
#endif


/*------------------------------------------------------------------------------
	Cancelling: a page under way is finished with blank lines (an inch at
	most); a job waiting for its session gives up.
------------------------------------------------------------------------------*/

void
TFaxDriver::CancelJob(Boolean inAsync)
{
	if (fData->fSessionOpen && !inAsync)
	{
		if (fLinesSent > 0)
		{
			ULong	lines = fPageLines - fLinesSent;
			if (fData->fVerticalRes < lines)
				lines = fData->fVerticalRes;
			PrintBlankLines(lines);
		}
	}
	else if (inAsync && fData->fWaiting)
	{
		fData->CloseSession(false);
		fError = kPR_ERR_UserCancel;
		PrRegainControl(fPrinter);
		return;
	}
	fError = kPR_ERR_UserCancel;
}


PrProblemResolution
TFaxDriver::IsProblemResolved(void)
{
	if (fError)
		return kPrProblemNotFixed;
	return kPrProblemNotFixed;
}


Boolean
TFaxDriver::ContinueIO(void)
{
	Boolean result = false;
	if (fError == noErr || fError == kPR_ERR_UserCancel || fError == kFaxToolErrTransmissionFailed)
		result = true;
	return result;
}
