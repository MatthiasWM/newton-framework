/*
	File:		Screen.cc

	Contains:	Display driver.

	Written by:	Newton Research Group.

	Newton (QuickDraw)
	all drawing is done into the screen�s GrafPort (bitmap)
	which is blitted to the LCD bitmap @ ~33fps (actually only dirty rect is blitted)

	Quartz
	all drawing is done into a window�s quartz context (presumably off-screen bitmap)
	the quartz compositor blends this context onto the display
so...
	we�ll just draw into the window�s quartz context
	at some point we could create a quartz context for each child view of the root, making app views more manageable
alternatively...
	create a CGLayer and draw into its context
		NSGraphicsContext * windowContext = window.graphicsContext;
		CGLayerRef qLayer = CGLayerCreateWithContext((CGContextRef)windowContext.graphicsPort, window.contentView.frame.size, NULL);
		CGContextRef qContext = CGLayerGetContext(qLayer);
		//draw into qContext
	screen task should blit that into window�s context
		void CGContextDrawLayerAtPoint(windowContext, CGPointZero, qLayer);
	although in reality we should probably
		StopDrawing() -> set needsDisplay on the contentView
		contentView:drawRect -> blit layer
	forFramework, must continue to draw into bitmap context
*/

#include "Quartz.h"

#include "Objects.h"
#include "NewtonGestalt.h"
#include "NewtGlobals.h"
#include "UserTasks.h"
#include "Semaphore.h"

#include "Screen.h"
#include "ScreenDriver.h"
#include "ViewFlags.h"
#include "QDDrawing.h"
#include "Tablet.h"

extern void SetEmptyRect(Rect * r);
extern void WeAreDirty(void);


/* -----------------------------------------------------------------------------
	P l a i n   C   F u n c t i o n   I n t e r f a c e
----------------------------------------------------------------------------- */

extern "C" {
Ref	FGetLCDContrast(RefArg inRcvr);
Ref	FSetLCDContrast(RefArg inRcvr, RefArg inContrast);
Ref	FGetOrientation(RefArg inRcvr);
Ref	FSetOrientation(RefArg inRcvr, RefArg inOrientation);
Ref	FLockScreen(RefArg inRcvr, RefArg inDoIt);
}


/*------------------------------------------------------------------------------
	D i s p l a y   D a t a
	Definition of the display: its PixelMap
------------------------------------------------------------------------------*/

const ScreenParams gScreenConstants =		// was qdConstants 00380BCC
{	{ 0, 5, 4, 0, 3, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },	// xShift[bitsPerPixel]
	{ 0, 3, 2, 0, 1 },	// depthShift
	{ 0x00000000, 0x00000001, 0x00000003, 0x0000000F, 0x000000FF, 0x0000FFFF, 0xFFFFFFFF },	// pixelMask
	{ 0, 0x1F, 0x0F, 0, 0x07, 0, 0, 0, 0x03, 0, 0, 0, 0, 0, 0, 0, 0x01, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },	// xMask
	{ 0, 1, 0, 2, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5, 0 } };	// maskIndex[bitsPerPixel]

/* -----------------------------------------------------------------------------
	D a t a
----------------------------------------------------------------------------- */

CGContextRef	quartz;

int				gScreenWidth;			// 0C104C58
int				gScreenHeight;			// 0C104C5C

struct
{
	CScreenDriver *		driver;			// +00	 actually CMainDisplayDriver protocol
	CScreenDriver *		auxDriver;		// +04
	int						f08;				// +08
	Rect						dirtyBounds;	// +0C 0C101A58
	CUTask *					updateTask;		// +14
	CUSemaphoreGroup *	semaphore;		// +18
	CUSemaphoreOpList *	f1C;				// acquire display
	CUSemaphoreOpList *	f20;				// release display
	CUSemaphoreOpList *	f24;
	CUSemaphoreOpList *	f28;
	CUSemaphoreOpList *	f2C;
	CUSemaphoreOpList *	f30;
	CUSemaphoreOpList *	f34;
	CUSemaphoreOpList *	f38;
	CULockingSemaphore *	lock;				// +3C
} gTheScreen;	// 0C101A4C


struct AlertScreenInfo
{
	CScreenDriver *	display;
	NativePixelMap		pixmap;
	int					orientation;
};

AlertScreenInfo gAlertScreenInfo;		// 0C105EF0



/* -----------------------------------------------------------------------------
	F u n c t i o n   P r o t o t y p e s
----------------------------------------------------------------------------- */

static void	SetupScreen(void);
static void	SetupScreenPixelMap(void);

void	SetScreenInfo(void);
void	SetAlertScreenInfo(AlertScreenInfo * info);

void	InitScreenTask(void);
void	ScreenUpdateTask(void * inTask, size_t inSize, ObjectId inArg);
extern "C" void	UpdateHardwareScreen(void);
void	BlitToScreens(NativePixelMap * inPixmap, Rect * inSrcBounds, Rect * inDstBounds, int inTransferMode);


#pragma mark -
/*------------------------------------------------------------------------------
	Set up the Quartz graphics system.
	Args:		--
	Return:  --
------------------------------------------------------------------------------*/

extern "C" void
SetupQuartzContext(CGContextRef inContext)
{
	quartz = inContext;
#if 1
	CGContextTranslateCTM(quartz, 10.0, 20.0);
#else
	CGContextTranslateCTM(quartz, 10.0, 20.0+480.0);	// gScreenHeight
	CGContextScaleCTM(quartz, 1.0, -1.0);
#endif
}


#if defined(forFramework)
void
InitDrawing(CGContextRef inContext, int inScreenHeight)
{
	quartz = inContext;
	gScreenHeight = inScreenHeight;
}
#endif


#pragma mark -
/*------------------------------------------------------------------------------
	Initialize the screen.
	Args:		--
	Return:	--
------------------------------------------------------------------------------*/

void
InitScreen(void)
{
#if !defined(forFramework)
	// initialize the driver(s)
	SetupScreen();
	gTheScreen.driver->screenSetup();
	gTheScreen.driver->powerInit();

	// initialize the QD pixmap
	SetupScreenPixelMap();
	if (qdGlobals.pixelMap.baseAddr == NULL)
	{
	// allocate enough memory for either screen orientation
		ScreenInfo info;
		int			landscapePix, portraitPix, screenPix;
		int			pixAlignment, screenBytes;

		gTheScreen.driver->getScreenInfo(&info);
		pixAlignment = 8 * 8 / info.depth;
		landscapePix = (((info.width + pixAlignment - 1) & -pixAlignment) / 8) * info.height;
		portraitPix = (((info.height + pixAlignment - 1) & -pixAlignment) / 8) * info.width;
		screenPix = (landscapePix > portraitPix) ? landscapePix : portraitPix;
		screenBytes = screenPix * info.depth;
		qdGlobals.pixelMap.baseAddr = NewPtr(screenBytes);
		memset(qdGlobals.pixelMap.baseAddr, 0, screenBytes);
		SetScreenInfo();
	}

	SetEmptyRect(&gTheScreen.dirtyBounds);
    //o SetEmptyRect(gScreenDirtyRect);

	InitScreenTask();
#endif
}


/*------------------------------------------------------------------------------
	Set up the screen driver(s).
	Args:		--
	Return:	--
------------------------------------------------------------------------------*/

static void
SetupScreen(void)
{
	gTheScreen.driver = (CScreenDriver *)MakeByName("CScreenDriver", "CMainDisplayDriver");
	gTheScreen.auxDriver = NULL;
	gTheScreen.f08 = 0;
}


/*------------------------------------------------------------------------------
	Set up the screen�s pixmap.
	Args:		--
	Return:	--
------------------------------------------------------------------------------*/

static void
SetupScreenPixelMap(void)
{
	ScreenInfo info;
	int			pixAlignment;

	gTheScreen.driver->getScreenInfo(&info);
	pixAlignment = 8 * 8 / info.depth;
	qdGlobals.pixelMap.rowBytes = (((info.width + pixAlignment - 1) & -pixAlignment) * info.depth) / 8;
	SetRect(&qdGlobals.pixelMap.bounds, 0, 0, info.width, info.height);
	qdGlobals.pixelMap.pixMapFlags = kPixMapPtr + info.depth;
	qdGlobals.pixelMap.deviceRes.h = info.resolution.h;
	qdGlobals.pixelMap.deviceRes.v = info.resolution.v;
	qdGlobals.pixelMap.grayTable = NULL;

	SetScreenInfo();
}


/*------------------------------------------------------------------------------
	Set up the screen info record.
	Args:		--
	Return:	--
------------------------------------------------------------------------------*/

void
SetScreenInfo(void)
{
	AlertScreenInfo info;

	info.display = gTheScreen.driver;

	if (qdGlobals.pixelMap.baseAddr)
		info.pixmap = qdGlobals.pixelMap;	// sic
	info.pixmap.rowBytes = qdGlobals.pixelMap.rowBytes;
	info.pixmap.bounds.top = qdGlobals.pixelMap.bounds.top;
	info.pixmap.bounds.bottom = qdGlobals.pixelMap.bounds.bottom;
	info.pixmap.bounds.left = qdGlobals.pixelMap.bounds.left;
	info.pixmap.bounds.right = qdGlobals.pixelMap.bounds.right;
	info.pixmap.pixMapFlags = qdGlobals.pixelMap.pixMapFlags;
	info.pixmap.deviceRes = qdGlobals.pixelMap.deviceRes;
	info.pixmap.grayTable = qdGlobals.pixelMap.grayTable;

	info.orientation = gTheScreen.driver->getFeature(4);
	
	SetAlertScreenInfo(&info);
}

void
SetAlertScreenInfo(AlertScreenInfo * info)
{
	// the original does field-by-field copy
	gAlertScreenInfo = *info;
}


#pragma mark -

/*------------------------------------------------------------------------------
	Set the physical orientation of the screen.
	Args:		inOrientation	the orientation
	Return:	--
------------------------------------------------------------------------------*/
Ref
FSetOrientation(RefArg inRcvr, RefArg inOrientation)
{
	SetOrientation(RINT(inOrientation));
	return NILREF;
}


void
SetOrientation(int inOrientation)
{
	SetGrafInfo(kGrafOrientation, inOrientation);
#if !defined(forFramework)
	TabSetOrientation(inOrientation);
#endif
#if 0
	GetGrafInfo(kGrafPixelMap, &gScreenPort.portBits);
	gScreenPort.portRect = gScreenPort.portBits.bounds;
	InitPortRgns(&gScreenPort);
#endif

#if !defined(forFramework)
	NewtGlobals *	newt;
/*	if ((newt = GetNewtGlobals()) != NULL)
	{
		GrafPtr	newtGraf = newt->graf;
		GetGrafInfo(kGrafPixelMap, newtGraf);
		newtGraf->portRect = newtGraf->portBits.bounds;
		InitPortRgns(newtGraf);
	}
*/
	CUGestalt				gestalt;
	GestaltSystemInfo		sysInfo;
	gestalt.gestalt(kGestalt_SystemInfo, &sysInfo, sizeof(sysInfo));
	bool	isWideScreenHardware = (sysInfo.fScreenWidth > sysInfo.fScreenHeight);
	if (inOrientation == kLandscape || inOrientation == kLandscapeFlip)
	{
		if (isWideScreenHardware)
		{
			gScreenWidth = sysInfo.fScreenWidth;
			gScreenHeight = sysInfo.fScreenHeight;
		}
		else
		{
			gScreenWidth = sysInfo.fScreenHeight;
			gScreenHeight = sysInfo.fScreenWidth;
		}
	}
	else
	{
		if (isWideScreenHardware)
		{
			gScreenWidth = sysInfo.fScreenHeight;
			gScreenHeight = sysInfo.fScreenWidth;
		}
		else
		{
			gScreenWidth = sysInfo.fScreenWidth;
			gScreenHeight = sysInfo.fScreenHeight;
		}
	}
#endif
}


Ref
FGetOrientation(RefArg inRcvr)
{
	int	grafOrientation;
	GetGrafInfo(kGrafOrientation, &grafOrientation);
	return MAKEINT(grafOrientation);
}


Ref
FGetLCDContrast(RefArg inRcvr)
{
	int	contrast;
	GetGrafInfo(kGrafContrast, &contrast);
	return MAKEINT(contrast);
}


Ref
FSetLCDContrast(RefArg inRcvr, RefArg inContrast)
{
	SetGrafInfo(kGrafContrast, RINT(inContrast));
	return NILREF;
}


/*------------------------------------------------------------------------------
	Set info about the screen.
	Args:		inSelector		the type of info
				inInfo			the info
	Return:	error code
------------------------------------------------------------------------------*/

void
SetGrafInfo(int inSelector, int inValue)
{
	switch (inSelector)
	{
	case kGrafContrast:
		if (gTheScreen.driver)
			gTheScreen.driver->setFeature(0, inValue);
/*		CADC *	adc = GetADCObject();
		if (adc)
			adc->getSample(10, 1000, ContrastTempSample, NULL);
*/		break;

	case kGrafOrientation:
		if (gTheScreen.driver)
			gTheScreen.driver->setFeature(4, inValue);
		SetupScreenPixelMap();
		memset(PixelMapBits(&qdGlobals.pixelMap), 0, (qdGlobals.pixelMap.bounds.bottom - qdGlobals.pixelMap.bounds.top) * qdGlobals.pixelMap.rowBytes);
		break;

	case kGrafBacklight:
		if (gTheScreen.driver)
			gTheScreen.driver->setFeature(2, inValue);
		break;
	}
}


/*------------------------------------------------------------------------------
	Return info about the screen.
	Args:		inSelector		the type of info required
				outInfo			pointer to result
	Return:	error code
------------------------------------------------------------------------------*/

NewtonErr
GetGrafInfo(int inSelector, void * outInfo)
{
	NewtonErr err = noErr;

	switch (inSelector)
	{
	case kGrafPixelMap:
		*(NativePixelMap *)outInfo = qdGlobals.pixelMap;
		break;

	case kGrafResolution:
		*(Point *)outInfo = qdGlobals.pixelMap.deviceRes;
		break;

	case kGrafPixelDepth:
		*(ULong *)outInfo = PixelDepth(&qdGlobals.pixelMap);
		break;

	case kGrafContrast:
		if (gTheScreen.driver)
			*(int*)outInfo = gTheScreen.driver->getFeature(0);
		else
		{
			*(int*)outInfo = 0;
			err = -1;
		}
		break;

	case kGrafOrientation:
		if (gTheScreen.driver)
			*(int*)outInfo = gTheScreen.driver->getFeature(4);
		else
		{
			*(int*)outInfo = 2;	// 1 in the original, but we prefer portrait
			err = -1;
		}
		break;

	case kGrafBacklight:
		if (gTheScreen.driver)
			*(int*)outInfo = gTheScreen.driver->getFeature(2);
		else
		{
			*(int*)outInfo = 0;	// no display, no backlight
			err = -1;
		}
		break;

	case 6:
		if (gTheScreen.driver)
			*(int*)outInfo = gTheScreen.driver->getFeature(5);
		else
		{
			*(int*)outInfo = 10;
			err = -1;
		}
		break;

	case kGrafScreen:
		if (gTheScreen.driver)
			gTheScreen.driver->getScreenInfo((ScreenInfo *)outInfo);
		else
			err = -1;
		break;
	default:
		err = -1;
	}

	return err;
}

#pragma mark -

void
InitScreenTask(void)
{
#if !defined(forFramework)
	if (gTheScreen.semaphore == NULL)
	{
		CUSemaphoreGroup *	semGroup;	// size +08
		semGroup = new CUSemaphoreGroup;
		gTheScreen.semaphore = semGroup;
		semGroup->init(3);

		CULockingSemaphore *	semLock;		// size +0C
		semLock = new CULockingSemaphore;
		gTheScreen.lock = semLock;
		semLock->init();

		CUSemaphoreOpList *	semList;		// size +08
		semList = new CUSemaphoreOpList;
		gTheScreen.f24 = semList;
		semList->init(1, MAKESEMLISTITEM(2,1));

		semList = new CUSemaphoreOpList;
		gTheScreen.f28 = semList;
		semList->init(1, MAKESEMLISTITEM(2,-1));

		semList = new CUSemaphoreOpList;
		gTheScreen.f38 = semList;
		semList->init(3, MAKESEMLISTITEM(2,-1), MAKESEMLISTITEM(2,0), MAKESEMLISTITEM(2,1));

		semList = new CUSemaphoreOpList;
		gTheScreen.f2C = semList;
		semList->init(1, MAKESEMLISTITEM(1,1));

		semList = new CUSemaphoreOpList;
		gTheScreen.f1C = semList;
		semList->init(2, MAKESEMLISTITEM(0,0), MAKESEMLISTITEM(0,1));

		semList = new CUSemaphoreOpList;
		gTheScreen.f20 = semList;
		semList->init(1, MAKESEMLISTITEM(0,-1));

		semList = new CUSemaphoreOpList;
		gTheScreen.f30 = semList;
		semList->init(4, MAKESEMLISTITEM(0,0), MAKESEMLISTITEM(0,1), MAKESEMLISTITEM(1,-1), MAKESEMLISTITEM(2,0));

		semList = new CUSemaphoreOpList;
		gTheScreen.f34 = semList;
		semList->init(1, MAKESEMLISTITEM(0,-1));

// we don�t use this frame-buffering scheme
#if defined(correct) || defined(NFW_USE_SDL)
		CUTask * task = new CUTask;
		gTheScreen.updateTask = task;
		task->init(ScreenUpdateTask, 4*KByte, 0, NULL, kScreenTaskPriority, 'scrn');

		gTheScreen.driver->powerOn();
		task->start();
#endif
	}
#endif
}


void
ScreenUpdateTask(void * inTask, size_t inSize, ObjectId inArg)
{
#if defined(correct) || defined(NFW_USE_SDL)
	CTime		nextUpdateTime;
//	CADC *	adc = GetADCObject();

	for (;;)
	{
        putchar('+');
		gTheScreen.semaphore->semOp(gTheScreen.f30, kWaitOnBlock);
		gTheScreen.lock->acquire(kWaitOnBlock);

        nextUpdateTime = TimeFromNow(30*kMilliseconds);
//        nextUpdateTime = TimeFromNow(30*kMilliseconds*100);
		UpdateHardwareScreen();

		gTheScreen.lock->release();
		gTheScreen.semaphore->semOp(gTheScreen.f34, kWaitOnBlock);

		if (GetGlobalTime() > nextUpdateTime)
		{
			nextUpdateTime = TimeFromNow(30*kMilliseconds);
/*			if (adc)
				adc->getSample(10, 1000, ContrastTempSample, NULL);
			else
				adc = GetADCObject();
*/		}

		SleepTill(&nextUpdateTime);
	}
#endif
}


/*------------------------------------------------------------------------------
	Update the display.
	Args:		--
	Return:	--
------------------------------------------------------------------------------*/

#if 1

void
UpdateHardwareScreen(void)
{
	Rect r;
//    printf("UpdateHardwareScreen...\n");

	// only bother if the dirty area is actually on-screen
	if (SectRect(&gTheScreen.dirtyBounds, &qdGlobals.pixelMap.bounds, &r))
		BlitToScreens(&qdGlobals.pixelMap, &r, &r, modeCopy);
	// it's no longer dirty
	SetEmptyRect(&gTheScreen.dirtyBounds);
}

#else

void UpdateHardwareScreen(void)
{
    // qdGlobals: 0x0C107D88
    // Initialised in 0x002E4388: InitGraf(void) (struct is 60 bytes)
    Rect r;
    if (SectRect(gScreenDirtyRect, qdGlobals.d12, r)) {
        BlitToScreen(qdGlobals.d04, r, r, modeCopy);
    }
}

#endif

/*------------------------------------------------------------------------------
	Blit a pixmap to the display.
	If there�s an external display connected, blit to that too.
	Args:		inPixmap			the pix map to be blitted
				inSrcBounds		bounds  of inPixmap to use
				inDstBounds		bounds to blit into
				inTransferMode
	Return:	--
------------------------------------------------------------------------------*/

void
BlitToScreens(NativePixelMap * inPixmap, Rect * inSrcBounds, Rect * inDstBounds, int inTransferMode)
{
	gTheScreen.driver->blit(inPixmap, inSrcBounds, inDstBounds, inTransferMode);

	if (gTheScreen.auxDriver)
		gTheScreen.auxDriver->blit(inPixmap, inSrcBounds, inDstBounds, inTransferMode);

#if !defined(forFramework)
	WeAreDirty();
#endif
}

#pragma mark -

void
StartDrawing(NativePixelMap * inPixmap, const Rect * inBounds)
{
	if (inPixmap == NULL)
#if defined(correct)
		inPixmap = &GetCurrentPort()->portBits;
#else
		inPixmap = &qdGlobals.pixelMap;
#endif

#if !defined(forFramework)
	if (PixelMapBits(inPixmap) == PixelMapBits(&qdGlobals.pixelMap))
		// drawing direct onto screen
		gTheScreen.semaphore->semOp(gTheScreen.f24, kWaitOnBlock);
#endif
}


void
StopDrawing(NativePixelMap * inPixmap, const Rect * inBounds)
{
	if (inPixmap == NULL)
#if defined(correct)
		inPixmap = &GetCurrentPort()->portBits;
#else
		inPixmap = &qdGlobals.pixelMap;
#endif

	if (PixelMapBits(inPixmap) == PixelMapBits(&qdGlobals.pixelMap))
	{
		// drawing direct onto screen
		if (inBounds)
		{
			Rect	bbox = *inBounds;
			OffsetRect(&bbox, -inPixmap->bounds.left, -inPixmap->bounds.top);
			UnionRect(&bbox, &gTheScreen.dirtyBounds, &gTheScreen.dirtyBounds);
		}

#if !defined(forFramework)
		if (gTheScreen.semaphore->semOp(gTheScreen.f38, kNoWaitOnBlock) == 0)
		{
			gTheScreen.semaphore->semOp(gTheScreen.f1C, kWaitOnBlock);
			UpdateHardwareScreen();
			gTheScreen.lock->release();
			gTheScreen.semaphore->semOp(gTheScreen.f20, kWaitOnBlock);
		}
		gTheScreen.semaphore->semOp(gTheScreen.f28, kNoWaitOnBlock);
#endif
	}
}


void
QDStartDrawing(NativePixelMap * inPixmap, Rect * inBounds)
{
	if (inPixmap == NULL)
#if defined(correct)
		inPixmap = &GetCurrentPort()->portBits;
#else
		inPixmap = &qdGlobals.pixelMap;
#endif

#if !defined(forFramework)
	if (PixelMapBits(inPixmap) == PixelMapBits(&qdGlobals.pixelMap))
		//	we�re drawing to the display so acquire a lock
		gTheScreen.lock->acquire(kWaitOnBlock);
#endif
}


void
QDStopDrawing(NativePixelMap * inPixmap, Rect * inBounds)
{
	if (inPixmap == NULL)
#if defined(correct)
		inPixmap = &GetCurrentPort()->portBits;
#else
		inPixmap = &qdGlobals.pixelMap;
#endif

	if (PixelMapBits(inPixmap) == PixelMapBits(&qdGlobals.pixelMap))
	{
		bool	isAnythingToDraw = !EmptyRect(&gTheScreen.dirtyBounds);
		if (inBounds)
		{
			Rect	bbox = *inBounds;
			OffsetRect(&bbox, -inPixmap->bounds.left, -inPixmap->bounds.top);
			UnionRect(&bbox, &gTheScreen.dirtyBounds, &gTheScreen.dirtyBounds);
		}

#if !defined(forFramework)
		if (!isAnythingToDraw)
			gTheScreen.semaphore->semOp(gTheScreen.f2C, kWaitOnBlock);
		//	we�re drawing to the display so release the lock
		gTheScreen.lock->release();
printf("QDStopDrawing() -- ");
		WeAreDirty();
#endif
	}
}


void
BlockLCDActivity(bool doIt)
{
#if !defined(forFramework)
	gTheScreen.semaphore->semOp(doIt ? gTheScreen.f1C : gTheScreen.f20, kWaitOnBlock);
#endif
}


void
ReleaseScreenLock(void)
{
#if !defined(forFramework)
	while (gTheScreen.semaphore->semOp(gTheScreen.f28, kNoWaitOnBlock) == 0)
		;
#endif
}


Ref
FLockScreen(RefArg inRcvr, RefArg inDoIt)
{
	if (NOTNIL(inDoIt))
		StartDrawing(NULL, NULL);
	else
		StopDrawing(NULL, NULL);
	return NILREF;
}

