/*
	File:		ObjectSystemCore.cc

	Contains:	The Newton object system on its own: its globals, the stubs the
					framework needs instead of an OS, and InitObjectSystemCore(), the
					first part of InitObjectSystem() (ObjectSystem.cc): task globals,
					protocols, ROM domain, stores, objects, translators, NTK's
					translator protocols, the REP's streams, Unicode, the internal
					store. No REP context, graphics or ROM scripts, and no ROM
					functions (ROMFunctions.c).
					This is what the newton_ncx library initializes (NCX).

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
#include "ROMResources.h"
#include "Interpreter.h"
#include "REP.h"

#if defined(forFramework)
#define kBundleId CFSTR("org.newton.objects")
#else
#define kBundleId CFSTR("org.newton.messagepad")
#endif

#define kNSDictData CFSTR("DictData")

extern void			StartupProtocolRegistry(void);
extern NewtonErr	InitROMDomainManager(void);
extern NewtonErr	RegisterROMDomainManager(void);
extern NewtonErr	InitPSSManager(ObjectId inEnvId, ObjectId inDomId);
extern void			InitObjects(void);
extern void			InitTranslators(void);
extern Ref			InitUnicode(void);
extern void			NTKInit(void);
extern void			REPInitGlobals(void);
extern void			InitExternal(void);
extern PInTranslator *	gREPin;
extern POutTranslator *	gREPout;

extern "C" void	InitObjectSystemCore(void);
			void		MakeROMResources(void);


/*------------------------------------------------------------------------------
	D a t a
------------------------------------------------------------------------------*/

char *			gDictData = NULL;
TaskGlobals		gMyTaskGlobals;
NewtGlobals		gMyNewtGlobals;

#if defined(forFramework)
// Stubs
const Ref *		gTagCache;
NewtGlobals *	gNewtGlobals;
void *			gCurrentGlobals;
void *			GetGlobals(void)  { return gCurrentGlobals; }


int		GetCPUMode(void)	{ return kUserMode; }
void		Wait(ULong inMilliseconds) { }
// Ticks: 60 a second, as on a Newton (was ms / 20, 50 a second: B25)
ULong		GetTicks(void) {
	CTime now(GetGlobalTime());
	return ULong((unsigned long long)now.convertTo(kMilliseconds) * 60 / 1000);
}

// FAddDeferredSend, FAddDeferredCall: Host/Timers.cc

// RefStack allocates a new stack, but we don't want the whole VM system.
// On a Newton the stack grows (virtual memory) up to inMaxSize; nothing
// checks its bounds, so allocate all of it (it was 4 KB: 512 values; an
// array literal of 1200 elements ran over it, bug B17).
extern "C" NewtonErr	NewStack(ObjectId inDomainId, size_t inMaxSize, ObjectId inOwnerId, VAddr * outTopOfStack, VAddr * outBottomOfStack)
{
	*outBottomOfStack = (VAddr)malloc(inMaxSize);
	*outTopOfStack = *outBottomOfStack + inMaxSize - 1;
	return noErr;
}

// RefStack frees its stack, but we don't want the whole VM system
extern "C" NewtonErr	FreePagedMem(VAddr inAddressInArea)
{
	free((void *)inAddressInArea);
	return noErr;
}

#if 1
/*------------------------------------------------------------------------------
	S t u b s
	The framework has no view system -- provide stubs.
	Needed only for IA.CC : ParseUtter
------------------------------------------------------------------------------*/

CRootView * gRootView;

void		CRootView::update(Rect * inRect) { }


// FSetValue, FRefreshViews: Host/ViewMethods.cc

#endif
#endif	// forFramework


/*------------------------------------------------------------------------------
	I n i t i a l i z a t i o n
------------------------------------------------------------------------------*/

static bool		gObjectSystemCoreInitialized = false;

void
InitObjectSystemCore(void)
{
	if (gObjectSystemCoreInitialized)
		return;
#if defined(forFramework)
	// Simulate OsBoot() sequence

	// Set up task globals
	gMyTaskGlobals.fExceptionGlobals.firstCatch = NULL;
	gMyTaskGlobals.fTaskId = 0;
	gMyTaskGlobals.fErrNo = noErr;
	gMyTaskGlobals.fMemErr = noErr;
	gMyTaskGlobals.fTaskName = 'newt';
	gMyTaskGlobals.fStackTop = 0;
	gMyTaskGlobals.fStackBottom = 0;
	gMyTaskGlobals.fCurrentHeap = 0;
	gMyTaskGlobals.fDefaultHeapDomainId = 0;
	gCurrentGlobals = &gMyTaskGlobals + 1;	// globals are AFTER task globals

	//	UserBoot task
	StartupProtocolRegistry();
	InitROMDomainManager();

	//	InitialKSRVTask task
	RegisterROMDomainManager();

	// CLoader::theMain -> UserMain -> CNewtWorld::mainConstructor()
	SetNewtGlobals(&gMyNewtGlobals);

	InitPSSManager(0, 0);

	InitObjects();
	InitTranslators();			// register flatten/unflatten etc

	NTKInit();						// the REP's translators (null, stdio)
	gREPin = InitREPIn();
	gREPout = InitREPOut();
	REPInitGlobals();				// vars, functions, printDepth (InitObjectSystem's REPInit prompts)

	InitUnicode();					// encodings, sort tables
	InitExternal();				// NSOF precedents, the internal store, large objects
#endif
	gObjectSystemCoreInitialized = true;
}


/*------------------------------------------------------------------------------
	Simulate NewtonScript ROM resources by loading from disk.
------------------------------------------------------------------------------*/

void
MakeROMResources(void)
{
#if defined(forDarkStar)
//	Load recognition/IA dictionaries.
	CFBundleRef newtBundle = CFBundleGetBundleWithIdentifier(kBundleId);
	CFURLRef		url = CFBundleCopyResourceURL(newtBundle, kNSDictData, NULL, NULL);
	CFStringRef dictPath = CFURLCopyFileSystemPath(url, kCFURLPOSIXPathStyle);
	const char * dictStr = CFStringGetCStringPtr(dictPath, kCFStringEncodingMacRoman);
	size_t dictSize;

	FILE * fp = fopen(dictStr, "r");

	CFRelease(dictPath);
	CFRelease(url);

	if (fp != NULL) {
		fseek(fp, 0, SEEK_END);
		dictSize = ftell(fp);
		fseek(fp, 0, SEEK_SET);
		gDictData = (char *)malloc(dictSize);
		fread(gDictData, dictSize, 1, fp);
		fclose(fp);
	}
#endif
}


