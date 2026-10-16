/*
	File:		ObjectSystem.cc

	Contains:	Newton object system framework initialization.

	Written by:	Newton Research Group.
*/

#include "RootView.h"
#include "Unimplemented.h"

#ifdef __APPLE__

#include <CoreFoundation/CoreFoundation.h>

#endif

#include "Objects.h"
#include "ObjHeader.h"
#include "Ref32.h"
#include "PackageParts.h"
#include "Globals.h"
#include "NewtWorld.h"
#include "Iterators.h"
#include "REP.h"
#include "ROMResources.h"
#include "Interpreter.h"

#define forDebug 1
//#define forExperiment 1

#if defined(forFramework)
#define kBundleId CFSTR("org.newton.objects")
#else
#define kBundleId CFSTR("org.newton.messagepad")
#endif

#define kNSDictData CFSTR("DictData")

#if defined(forNTK)
#define kNSDataPkg CFSTR("NTKConstData.newtonpkg")
#else
#define kNSDataPkg CFSTR("NSConstData.newtonpkg")
#endif


extern PInTranslator *	gREPin;
extern POutTranslator *	gREPout;

extern void			ResetFromResetSwitch(void);
extern void			UserInit(void);
extern void			StartupProtocolRegistry(void);
extern NewtonErr	InitROMDomainManager(void);
extern NewtonErr	RegisterROMDomainManager(void);
extern NewtonErr	InitPSSManager(ObjectId inEnvId, ObjectId inDomId);
extern void			InitializeCompression(void);
extern void			InitObjects(void);
extern void			InitTranslators(void);
extern void			NTKInit(void);
extern Ref			InitUnicode(void);
extern void			InitExternal(void);
extern void			InitGraf(void);
extern void			InitScriptGlobals(void);
extern NewtonErr	InitInternationalUtils(void);
extern void			InitDictionaries(void);
extern void			RunInitScripts(void);
extern Ref			InitDarkStar(RefArg inRcvr, RefArg inOptions);

/*------------------------------------------------------------------------------
	F u n c t i o n   P r o t o t y p e s
------------------------------------------------------------------------------*/

extern "C" {
#ifdef __APPLE__
void	NSLog(CFStringRef format, ...);
#endif
void  InitObjectSystem(void);
void  InitObjectSystemCore(void);	// ObjectSystemCore.cc
void  InstallROMFunctions(void);		// ROMData/forNewton/ROMFunctions.c
}

static void			InitBuiltInFunctions(void);
static void			LoadDictionaries(void);


/*------------------------------------------------------------------------------
	D a t a
	(the object system's globals and the framework's stubs: ObjectSystemCore.cc)
------------------------------------------------------------------------------*/

int				gFramesInitialized = false;


/*------------------------------------------------------------------------------
	O b j e c t   S y s t e m   I n i t i a l i z a t i o n
	Everything newtc and the MessagePad need: the object system
	(InitObjectSystemCore), then NTK, the REP, graphics, the script globals,
	the ROM's init scripts.
------------------------------------------------------------------------------*/
extern "C" Ref	GetProtoVariable(RefArg context, RefArg name, bool * exists = NULL);

void
InitObjectSystem(void)
{
	if (!gFramesInitialized)
	{
#if defined(forFramework)
	// the plain C functions of the ROM's function frames (ROMData/forNewton/ROMFunctions.c)
		InstallROMFunctions();
	// task globals, protocols, ROM domain, stores, objects, translators,
	// the REP's translator protocols and streams, Unicode, the internal store
	// (ObjectSystemCore.cc)
		InitObjectSystemCore();

		ResetREPIdler();
		REPInit();

		InitGraf();

	// CNotebook::initToolbox()
		InitScriptGlobals();
		InitInternationalUtils();
#if defined(forDarkStar)
		InitDictionaries();		//	should be gRecognitionManager.init(2);
#endif
		RunInitScripts();

//		Override debug print defaults set in init script
		DefGlobalVar(SYMA(printDepth), MAKEINT(7));

#if defined(forDarkStar)
		InitDarkStar(RA(gFunctionFrame), RA(NILREF));

	// CNotebook::init()
	// stub up the root view -- can't gRootView->init(SYS_rootProto, NULL) w/o bringing in a world of stuff we don't really want
		gRootView = (CRootView *)calloc(0xA0, 1);
		gRootView->fContext.h = AllocateRefHandle(AllocateFrame());
		gRootView->fContext.h->stackPos = 0;

		gRootView->fContext = Clone(RA(rootContext));	// must make it mutable
		SetFrameSlot(gRootView->fContext, SYMA(_proto), RA(viewRoot));
		RefVar vwChildren(GetFrameSlot(RA(viewRoot), SYMA(viewChildren)));
		FOREACH(vwChildren, child)
			RefVar contextTag(GetFrameSlot(child, SYMA(preAllocatedContext)));
			if (NOTNIL(contextTag))
			{
				RefVar mutableChild(Clone(child));
				SetFrameSlot(gRootView->fContext, contextTag, mutableChild);
				if (SymbolCompareLexRef(contextTag, SYMA(assistant)) == 0)
				{
					RefVar assistLine(AllocateFrame());
					SetFrameSlot(assistLine, SYMA(entryLine), AllocateFrame());
					SetFrameSlot(mutableChild, SYMA(assistLine), assistLine);
				}
			}
		END_FOREACH
#endif
#else
		ResetFromResetSwitch();
#endif
		gFramesInitialized = true;
	}
}


/*------------------------------------------------------------------------------
	Load PNG image.
	The busy box, caret, etc are now PNG not PICT.
	Args:		--
	Return:	--
------------------------------------------------------------------------------*/

#ifdef __APPLE__

#include <CoreGraphics/CoreGraphics.h>

CGImageRef
LoadPNG(const char * inName)
{
	CFBundleRef newtBundle = CFBundleGetBundleWithIdentifier(kBundleId);
	CFStringRef name = CFStringCreateWithCString(NULL, inName, kCFStringEncodingASCII);
	CFURLRef url = CFBundleCopyResourceURL(newtBundle, name, CFSTR("png"), NULL);
	CFRelease(name);
	CGDataProviderRef dataProvider = CGDataProviderCreateWithURL(url);
	CFRelease(url);
	CGImageRef png = CGImageCreateWithPNGDataProvider(dataProvider, NULL, false, kCGRenderingIntentDefault);
	CFRelease(dataProvider);
	return png;
}

#endif
