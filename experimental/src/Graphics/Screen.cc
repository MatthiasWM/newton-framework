/*
	File:		Screen.cc

	Contains:	Screen orientation natives.

	ROM:		0x202AE4..0x202C6C (FGetOrientation, FSetOrientation,
				SetOrientation), MP2x00 US 2.1 (717006). The file holds more
				around them (UpdateGestalt, FSetLCDContrast, FRegisterGestalt,
				FReplaceGestalt before; SetScreenSize, the tablet bypass after):
				still generated assembler.
*/

#include "Frames/objects.h"
#include "OS600/NewtonGestalt.h"
#include "Graphics/Screen.h"
#include "Frames/NewtGlobals.h"
#include "Recognition/Tablet.h"

extern "C" {
Ref		FGetOrientation(RefArg inRcvr);
Ref		FSetOrientation(RefArg inRcvr, RefArg inOrientation);
}


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
