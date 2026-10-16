/*
	File:		SerialNumber.cc

	Contains:	ROM serial number emulation.
					We use the Mac serial number; which may actually include alpha characters.

	Written by:	Newton Research Group, 2014.
*/

#include "SerialNumber.h"
#include "OSErrors.h"

#include <stdio.h>
#include <string.h>

/* -----------------------------------------------------------------------------
	The host's serial number as text, for the Newton's: the Mac's (12
	alphanumeric characters), Linux's machine id; none on Windows (the
	internal store's signature is 0 then, as for any error).
----------------------------------------------------------------------------- */
#ifdef __APPLE__

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>

/* -----------------------------------------------------------------------------
	From https://developer.apple.com/library/mac/technotes/tn1103/_index.html
----------------------------------------------------------------------------- */
// Returns the serial number as a CFString.
// It is the caller's responsibility to release the returned CFString when done with it.

void
CopySerialNumber(CFStringRef *serialNumber)
{
	if (serialNumber != NULL)
	{
		*serialNumber = NULL;

		io_service_t platformExpert = IOServiceGetMatchingService(kIOMasterPortDefault,
																					 IOServiceMatching("IOPlatformExpertDevice"));

		if (platformExpert)
		{
			CFTypeRef serialNumberAsCFString = IORegistryEntryCreateCFProperty(platformExpert,
																									CFSTR(kIOPlatformSerialNumberKey),
																									kCFAllocatorDefault, 0);
			if (serialNumberAsCFString)
				*serialNumber = (CFStringRef)serialNumberAsCFString;

			IOObjectRelease(platformExpert);
		}
	}
}


static bool
HostSerialNumber(char * outStr, size_t inSize)
{
	CFStringRef serNo;
	CopySerialNumber(&serNo);
	if (serNo == NULL)
		return false;
	bool ok = CFStringGetCString(serNo, outStr, inSize, kCFStringEncodingASCII);
	CFRelease(serNo);
	return ok;
}

#else

static bool
HostSerialNumber(char * outStr, size_t inSize)
{
	FILE * fp = fopen("/etc/machine-id", "r");	// 32 hex digits (systemd); not on Windows
	if (fp == NULL)
		return false;
	bool ok = fgets(outStr, (int)inSize, fp) != NULL;
	fclose(fp);
	outStr[strcspn(outStr, "\r\n")] = 0;
	return ok && outStr[0] != 0;
}

#endif


/* -----------------------------------------------------------------------------
	P u b l i c   I n t e r f a c e
----------------------------------------------------------------------------- */

CSerialNumberROM gSerialNumberROM;

CSerialNumberROM *
GetSerialNumberROMObject(void)
{ return &gSerialNumberROM; }


/* -----------------------------------------------------------------------------
	C S e r i a l N u m b e r R O M
----------------------------------------------------------------------------- */

void
CSerialNumberROM::init(void)
{ /* not required */ }


int
CSerialNumberROM::nextChar(char * inStr)
{
	int ch = inStr[fIndex++];
	if (fIndex >= kSerialNumberBufferLength)
		fIndex = 0;
	return ch;
}


int
CSerialNumberROM::nextXDigit(char * inStr)
{
	int ch = nextChar(inStr);
	return ch & 0x0F;
}


NewtonErr
CSerialNumberROM::getSystemSerialNumber(SerialNumber * outSerialNumber)
{
	char str[kSerialNumberBufferLength] = { 0 };
	if (HostSerialNumber(str, kSerialNumberBufferLength))
	{
		fIndex = 0;
		for (ArrayIndex j = 0; j < 2; j++)	// 2 ULongs => 64-bit serial number
		{
			ULong word = 0;
			for (ArrayIndex i = 0; i < 8; ++i)	// 8 nybls / ULong
			{
				word = (word << 4) | nextXDigit(str);
			}
			outSerialNumber->word[j] = CANONICAL_LONG(word);
		}
		return noErr;
	}
	return kOSErrCouldNotCreateObject;
}


/* -----------------------------------------------------------------------------
	P l a i n   C   I n t e r f a c e
----------------------------------------------------------------------------- */
#include "Objects.h"
#include "RSSymbols.h"
extern "C" Ref		FGetSerialNumber(RefArg inRcvr);

Ref
FGetSerialNumber(RefArg inRcvr)
{
//	RefVar huh(inRcvr);	// sic
	RefVar serno(AllocateBinary(SYMA(serialNumber), sizeof(SerialNumber)));
	
	NewtonErr err = GetSerialNumberROMObject()->getSystemSerialNumber((SerialNumber *)BinaryData(serno));
	if (err == noErr)
		return serno;
	return NILREF;
}
