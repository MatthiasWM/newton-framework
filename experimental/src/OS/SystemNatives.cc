/*
	File:		SystemNatives.cc

	Contains:	NewtonScript natives for the system: the serial number,
				batteries and power, the backlight, Gestalt, the screen's
				contrast and orientation, the tablet, heap statistics.
				(The original file's name is not known; newton-re calls it
				SystemNatives.)

	ROM:		The file is 0x20171C (FGetSerialNumber) .. 0x203DE8 (after
				FBatteryStatus), MP2x00 US 2.1 (717006). Here so far:
				0x202AE4 (FGetOrientation) .. 0x202F08 (after
				FResetPowerStats); the rest is still generated assembler.
*/

#include "Frames/objects.h"
#include "OS600/NewtonGestalt.h"
#include "Graphics/Screen.h"
#include "Frames/NewtGlobals.h"
#include "Recognition/Tablet.h"
#include "OS/VirtualMemory.h"
#include "Frames/RSSymbols.h"

extern "C" {
Ref		FGetOrientation(RefArg inRcvr);
Ref		FSetOrientation(RefArg inRcvr, RefArg inOrientation);
Ref		FStartBypassTablet(RefArg inRcvr);
Ref		FStopBypassTablet(RefArg inRcvr);
Ref		FInsertTabletSample(RefArg inRcvr, RefArg inX, RefArg inY, RefArg inZ, RefArg inTime);
Ref		FTabletBufferEmpty(RefArg inRcvr);
Ref		FEnablePowerStats(RefArg inRcvr, RefArg inEnable);
Ref		FGetPowerStats(RefArg inRcvr);
Ref		FResetPowerStats(RefArg inRcvr);
}
Ref		FGetLCDContrast(RefArg inRcvr);


/*------------------------------------------------------------------------------
	Return the screen's orientation.
	Args:		inRcvr			the receiver
	Return:		an integer: kPortrait .. kLandscapeFlip
------------------------------------------------------------------------------*/

Ref
FGetOrientation(RefArg inRcvr)
{
	long	orientation;
	GetGrafInfo(4, &orientation);		// kGrafOrientation
	return MAKEINT(orientation);
}


/*------------------------------------------------------------------------------
	Set the screen's orientation.
	Args:		inRcvr			the receiver
				inOrientation	an integer
	Return:		nil
------------------------------------------------------------------------------*/

Ref
FSetOrientation(RefArg inRcvr, RefArg inOrientation)
{
	SetOrientation(RINT(inOrientation));
	return NILREF;
}


/*------------------------------------------------------------------------------
	Set the screen's orientation: the display, the tablet, the screen's
	GrafPort and NewtonScript's, and the screen size.
	Args:		inOrientation	kPortrait .. kLandscapeFlip
	Return:		--
------------------------------------------------------------------------------*/

void
SetOrientation(long inOrientation)
{
	SetGrafInfo(kGrafOrientation, inOrientation);
	TabSetOrientation(inOrientation);

	GrafPtr	port = &gGrafPort;
	GetGrafInfo(kGrafPixelMap, port);
	port->portRect = port->portBits.bounds;
	InitPortRgns(port);

	if (gNewtGlobals != NULL)
	{
		GetGrafInfo(kGrafPixelMap, gNewtGlobals->graf);
		gNewtGlobals->graf->portRect = gNewtGlobals->graf->portBits.bounds;
		InitPortRgns(gNewtGlobals->graf);
	}

	TUGestalt			gestalt;
	TGestaltSystemInfo	info;
	gestalt.Gestalt(kGestalt_SystemInfo, &info, sizeof(info));
	if (inOrientation == kLandscape || inOrientation == kLandscapeFlip)
	{
		screenWidth = (info.fScreenWidth > info.fScreenHeight) ? info.fScreenWidth : info.fScreenHeight;
		screenHeight = (info.fScreenWidth < info.fScreenHeight) ? info.fScreenWidth : info.fScreenHeight;
	}
	else
	{
		screenWidth = (info.fScreenWidth < info.fScreenHeight) ? info.fScreenWidth : info.fScreenHeight;
		screenHeight = (info.fScreenWidth > info.fScreenHeight) ? info.fScreenWidth : info.fScreenHeight;
	}
}


/*------------------------------------------------------------------------------
	Set the screen's size: nothing to do.
	Args:		inWidth
				inHeight
	Return:		--
------------------------------------------------------------------------------*/

void
SetScreenSize(long inWidth, long inHeight)
{ }


/*------------------------------------------------------------------------------
	Tablet bypass: samples come from NewtonScript instead of the tablet.
------------------------------------------------------------------------------*/

Ref
FStartBypassTablet(RefArg inRcvr)
{
	return MAKEINT(StartBypassTablet());
}


Ref
FStopBypassTablet(RefArg inRcvr)
{
	return MAKEINT(StopBypassTablet());
}


/*------------------------------------------------------------------------------
	Put a sample into the tablet buffer.
	Args:		inRcvr
				inX, inY		the point
				inZ				the pressure
				inTime			its time stamp
	Return:		an error code, as an integer
------------------------------------------------------------------------------*/

Ref
FInsertTabletSample(RefArg inRcvr, RefArg inX, RefArg inY, RefArg inZ, RefArg inTime)
{
	ULong	sample = (RINT(inX) << 21) | ((RINT(inY) & 0x3FFF) << 7) | (RINT(inZ) & 0x0F);
	return MAKEINT(InsertTabletSample(sample, RINT(inTime)));
}


Ref
FTabletBufferEmpty(RefArg inRcvr)
{
	return TabletBufferEmpty() ? TRUEREF : NILREF;
}


/*------------------------------------------------------------------------------
	Return the screen's contrast.
	Args:		inRcvr
	Return:		an integer
------------------------------------------------------------------------------*/

Ref
FGetLCDContrast(RefArg inRcvr)
{
	long	contrast;
	GetGrafInfo(kGrafContrast, &contrast);
	return MAKEINT(contrast);
}


/*------------------------------------------------------------------------------
	Power statistics: how long the processor, the screen, serial and sound
	have been on, collected while gCollectCPUStats is set.
------------------------------------------------------------------------------*/

Ref
FEnablePowerStats(RefArg inRcvr, RefArg inEnable)
{
	gCollectCPUStats = NOTNIL(inEnable);
	return MAKEBOOLEAN(gCollectCPUStats);
}


Ref
FGetPowerStats(RefArg inRcvr)
{
	RefVar	stats(Clone(RA(canonicalpowerstats)));
	SetFrameSlot(stats, SYMA(timeatcoldboot), MAKEINT(gGlobalsThatLiveAcrossReboot.fTimeAtColdBoot - 0xA76C6BBC));
	SetFrameSlot(stats, SYMA(processorofftime), MAKEINT(gGlobalsThatLiveAcrossReboot.fProcessorOnTime));
	SetFrameSlot(stats, SYMA(screenontime), MAKEINT(gGlobalsThatLiveAcrossReboot.fScreenOnTime));
	SetFrameSlot(stats, SYMA(serialontime), MAKEINT(gGlobalsThatLiveAcrossReboot.fSerialOnTime));
	SetFrameSlot(stats, SYMA(soundontime), MAKEINT(gGlobalsThatLiveAcrossReboot.fSoundOnTime));
	return stats;
}


Ref
FResetPowerStats(RefArg inRcvr)
{
	gGlobalsThatLiveAcrossReboot.fProcessorOnTime = 0;
	gGlobalsThatLiveAcrossReboot.fScreenOnTime = 0;
	gGlobalsThatLiveAcrossReboot.fSerialOnTime = 0;
	gGlobalsThatLiveAcrossReboot.fSoundOnTime = 0;
	return NILREF;
}
