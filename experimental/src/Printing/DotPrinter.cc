/*
	File:		DotPrinter.cc

	Contains:	TDotPrinter, the imaging engine for dot matrix (bitmap)
				printer drivers: the page is drawn into bands, which go to
				the driver one at a time.

	ROM:		0x20D0F0 (TDotPrinter::Sizeof) .. 0x20E3F8 (after
				IsProblemResolved), MP2x00 US 2.1 (717006). Not yet written:
				OpenPage (0x20D7E4) and RepeatPage (0x20DEE0), still
				generated assembler.
*/

#include "Printing/Printer.h"
#include "QD/QDDrawing.h"
#include "Toolbox/FixedMath.h"
#include "NewtonMemory.h"


size_t
TDotPrinter::Sizeof(void)
{
	return sizeof(TDotPrinter);
}


NewtonErr
TDotPrinter::Constructor(char * inDriverName)
{
	fError = kPR_ERR_NotFound;
	fCancelled = false;
	fCancelHandled = false;
	fDriver = NULL;
	fDriver = (TDotPrinterDriver *) NewByName("TDotPrinterDriver", inDriverName);
	if (fDriver)
	{
		fError = noErr;
		PrintConnect * connect = new PrintConnect;
		fDriver->fConnect = connect;
		fDriver->fPrinter = this;
	}
	return fError;
}


void
TDotPrinter::SetPortraitOrientation(Boolean inPortrait)
{
	fDriver->fConnect->fPortrait = inPortrait;
}


/*------------------------------------------------------------------------------
	The smallest rectangle around the band's black pixels, in whole longs
	across.
------------------------------------------------------------------------------*/

#if 0
/* Not yet: the same algorithm, other registers and a 12-byte frame in the
   ROM (it keeps the first top and the long count on the stack). Until the
   form is found, it stays generated assembler. */
void
TDotPrinter::CalcMinBounds(const PixelMap * inBand, long inUnused, Rect * outBounds)
{
	long	top = inBand->bounds.top;
	long	firstTop = top;
	long	left = inBand->bounds.left;
	long	bottom = inBand->bounds.bottom;
	long	limit = inBand->bounds.right;
	long	right = (limit + 31) & ~31;
	long	rowBytes = inBand->rowBytes;
	long	rowLongs = rowBytes >> 2;
	long	bytes = rowBytes * (bottom - top);
	ULong *	bits = (ULong *) inBand->baseAddr;
	ULong *	p = bits;
	long	col = rowLongs;
	long	count;
	long	longs = bytes >> 2;

	for (count = longs; count > 0; count--)
	{
		if (*p++ != 0)
			goto found;
		if (--col == 0)
		{
			col = rowLongs;
			top++;
		}
	}
	right = bottom = left = top = 0;
	goto done;

found:
	p = (ULong *) ((char *) bits + bytes);
	col = rowLongs;
	for (count = longs; count > 0; count--)
	{
		if (*--p != 0)
			break;
		if (--col == 0)
		{
			col = rowLongs;
			bottom--;
		}
	}
	{
		ULong *	rowStart = (ULong *) ((char *) bits + rowBytes * (top - firstTop));
		ULong *	colStart = rowStart;
		long	i, j;
		for (i = rowLongs; i > 0; i--)
		{
			p = colStart++;
			for (j = bottom - top; j > 0; j--)
			{
				if (*p != 0)
				{
					left += (rowLongs - i) << 5;
					goto leftDone;
				}
				p += rowLongs;
			}
		}
leftDone:
		colStart = (ULong *) ((char *) rowStart + rowBytes);
		for (i = rowLongs; i > 0; i--)
		{
			p = --colStart;
			for (j = bottom - top; j > 0; j--)
			{
				if (*p != 0)
				{
					right -= (rowLongs - i) << 5;
					goto done;
				}
				p += rowLongs;
			}
		}
	}
done:
	if (limit < right)
		right = limit;
	SetRect(outBounds, left, top, right, bottom);
}
#endif


Boolean
TDotPrinter::TryAllocBands(char ** outBands, long inCount, long inSize)
{
	for (long i = 0; i < inCount; i++)
	{
		if ((outBands[i] = NewPtr(inSize)) == NULL)
		{
			for (long j = i - 1; j >= 0; j--)
				DisposPtr(outBands[j]);
			return false;
		}
	}
	return true;
}


NewtonErr
TDotPrinter::FaxEndPage(long inPageCount)
{
	if (fDriver->ClassInfo()->Version() >= 0x00020000)
		fError = fDriver->FaxEndPage(inPageCount);
	return fError;
}


void
TDotPrinter::Delete(void)
{
	if (fDriver)
	{
		PrintConnect * connect = fDriver->fConnect;
		if (connect)
			delete connect;
		fDriver->Delete();
	}
	gSCPDevicePackageBusy = false;
}


/*------------------------------------------------------------------------------
	Opening the job: the driver, the port, the bands.
------------------------------------------------------------------------------*/

#if 0
/* Not yet: identical but for twelve words, the registers of the band
   count, the row bytes and the size (0x20D55C), and two struct copies with
   source and destination registers swapped. Until the form is found, it
   stays generated assembler. */
NewtonErr
TDotPrinter::Open(RefArg inConnectInfo)
{
	if (!CheckUserAbort())
	{
		SetupConnect(fDriver->fConnect, inConnectInfo);
		do
		{
			gSCPDevicePackageBusy = true;
			fError = fDriver->Open();
			if (fError == kPR_ERR_Busy)
			{
				PrReleaseControl(10 * kSeconds, this);
				CheckUserAbort();
			}
		} while (fError == kPR_ERR_Busy);

		if (fError == noErr)
		{
			PrPageInfo	info;
			fDriver->GetPageInfo(&info);
			OpenPort(info);
			GrafPort * port = GetPrinterPort();
			port->portBits.pixMapFlags |= kPixMapDevDotPrint;
			SetupScalingBottlenecks(port);

			char *	bands[4];
			fDriver->GetBandPrefs(&fPrefs);
			fBandCount = fPrefs.asyncBanding ? 2 : 1;
			long count = fBandCount + 2;
			long rowBytes = ((GetScalerInfo()->toRect.right + 31) & ~31) >> 3;
			long height = fPrefs.optimumBand;
			long size;
			while (fPrefs.minBand <= height)
			{
				size = height * rowBytes;
				if (TryAllocBands(bands, count, size))
					break;
				height >>= 1;
			}
			if (bands[0] == NULL)		// may be stale or not set (BUGS.md B10)
			{
				fDriver->Close();
				return fError = kPR_ERR_NewtonError;
			}
			fBandSize = size;
			SetRect(&fBandRect, 0, 0, GetScalerInfo()->toRect.right, (short) height);
			fBands[0].baseAddr = bands[0];
			fBands[0].rowBytes = rowBytes;
			fBands[0].bounds = fBandRect;
			fBands[0].pixMapFlags = kPixMapPtr + kPixMapDevDotPrint + kOneBitDepth;
			fBands[0].deviceRes.v = 0;
			fBands[0].deviceRes.h = 0;
			fBands[0].grayTable = NULL;
			if (fBandCount == 2)
			{
				fBands[1] = fBands[0];
				fBands[1].baseAddr = bands[3];
			}
			else
				fBands[1].baseAddr = NULL;
			fBandHeight = height;
			fBandHeight72 = (short) ((FixedDivide(height << 16, GetScalerInfo()->scaleRatios.y) + 0x8000) >> 16);
			fCurBand = 0;
			fClip = NewRgn();
			fVis = NewRgn();
			fMask = fBands[0];
			fPattern = fBands[0];
			fMask.baseAddr = bands[1];
			fMaskBits = bands[1];
			fPattern.baseAddr = bands[2];
			fScalePat = GetStdPattern(blackPat);
			fPhantom.prObject = this;
			GrafPort * phantom = &fPhantom.port;
			::OpenPort(phantom);
			phantom->portBits = fBands[0];
			phantom->portRect = fBands[0].bounds;
		}
	}
	return fError;
}
#endif


NewtonErr
TDotPrinter::Close(void)
{
	fError = fDriver->Close();
	::ClosePort(&fPhantom.port);
	if (fBands[0].baseAddr)
	{
		DisposPtr(fBands[0].baseAddr);
		if (fBandCount > 1)
			DisposPtr(fBands[1].baseAddr);
		DisposPtr(fMaskBits);
		DisposPtr(fPattern.baseAddr);
	}
	DisposeRgn(fClip);
	DisposeRgn(fVis);
	TearDownScalingBottlenecks(GetPort());
	ClosePort();
	return fError;
}


NewtonErr
TDotPrinter::ClosePage(void)
{
	fError = fDriver->ClosePage();
	CheckUserAbort();
	return fError;
}


void
TDotPrinter::CancelJob(Boolean inAsync)
{
	fDriver->CancelJob(inAsync);
}


PrProblemResolution
TDotPrinter::IsProblemResolved(void)
{
	return fDriver->IsProblemResolved();
}
