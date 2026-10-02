/*
	File:		Screen.h

	Contains:	The screen: its GrafPort, size and orientation
				(reconstructed: not in the published headers; as the port's
				Graphics/Screen.h).
*/

#ifndef __SCREEN_H
#define __SCREEN_H

#ifndef __NEWTQD_H
#include "QD/NewtQD.h"
#endif

/* GetGrafInfo, SetGrafInfo selectors */
enum
{
	kGrafPixelMap,
	kGrafResolution,
	kGrafPixelDepth,
	kGrafContrast,
	kGrafOrientation,
	kGrafBacklight,
	kGrafInfo6,
	kGrafScreen
};

enum ScreenOrientation
{
	kPortrait,
	kLandscape,
	kPortraitFlip,
	kLandscapeFlip
};

long	GetGrafInfo(long inSelector, void * outInfo);
long	SetGrafInfo(long inSelector, long inValue);
void	SetOrientation(long inOrientation);

/* QuickDraw (the port has it in Graphics/NewtQD.h) */
void	InitPortRgns(GrafPtr inPort);

extern GrafPort		gGrafPort;		// 0C1067CC, the screen's
extern long			screenWidth;	// 0C104C58
extern long			screenHeight;	// 0C104C5C

#endif	/* __SCREEN_H */
