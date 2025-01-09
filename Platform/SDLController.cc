


#define __MACMEMORY__
#include "SDLController.h"

#include "messagepad_SDL.h"
#include "Platform.h"
#include "NewtonTime.h"
#include "NewtonTypes.h"
#include "UserGlobals.h"

#include <mutex>

#define forLayerDrawing 1


extern CTime        GetClock(void);

extern "C" void    ResetNewton(void);
extern void            TimerInterruptHandler(void * inQueue);
extern void            PreemptiveTimerInterruptHandler(void * inQueue);

//dispatch_queue_t gNewtonQ;
//dispatch_queue_t gTimerQ;
//dispatch_source_t schedulerSource;
//dispatch_source_t alarmSource;
std::mutex gAlarmMutex;
bool gIsAlarmSet = false;
SDL_TimerID gActiveAlarm = 0;

ULong gPendingInterrupt = 0;

#define kIntSourceTimer            0x0020
//#define kIntSourceScheduler    0x0040
extern void    DispatchFakeInterrupt(uint32_t inSourceMask);
extern ULong gCPSR;

#define kTabScale 8.0

extern int gScreenHeight;

extern "C" void    PenDown(float inX, float inY);
extern "C" void    PenMoved(float inX, float inY);
extern "C" void    PenUp(void);

Uint32 platform_alarm_cb(Uint32, void*) 
{
    gAlarmMutex.lock();
    gActiveAlarm = 0;
    gIsAlarmSet = false;
    gAlarmMutex.unlock();

    if (FLAGTEST(gCPSR, kIRQDisable)) {
        gPendingInterrupt |= kIntSourceTimer;
    } else {
        // handle the timer interrupt
        putchar('!');
        DispatchFakeInterrupt(kIntSourceTimer);
    }
    return 0;
}
/**
 Host platform call to set an alarm in inDelta microseconds.
 \param inDelta[in] set alerm in microseconds after now
 \todo We have to check if alarms accumulate (unlikely), or if we replace a currently
    pending alarm with the new alarm time.
 */
void SetPlatformAlarm(int64_t inDelta)
{
#if 0
//    // set one-shot timer to fire in inDelta ms
//    dispatch_source_set_timer(alarmSource, dispatch_time(DISPATCH_TIME_NOW, inDelta * NSEC_PER_USEC), DISPATCH_TIME_FOREVER, 1000ull);
    if (!gIsAlarmSet)
    {
        gIsAlarmSet = true;
        printf("SetPlatformAlarm(inDelta=%lldms)\n", inDelta);
//        SDL_TimerID SDL_AddTimer(Uint32 interval, (ms)
//                                 SDL_TimerCallback callback,
//                                 void *param);
//        typedef Uint32 (SDLCALL * SDL_TimerCallback) (Uint32 interval, void *param);
//        dispatch_resume(alarmSource);
    }
#else
    gAlarmMutex.lock();
    if (gActiveAlarm) {
        printf("Clearing previous alarm\n");
        SDL_RemoveTimer(gActiveAlarm);
        gActiveAlarm = 0;
    }
    Uint32 delta_ms = (Uint32)(inDelta / 1000) + 1; // microseconds to milliseconds
    printf("SetPlatformAlarm(inDelta=%dms)\n", delta_ms);
    gActiveAlarm = SDL_AddTimer(delta_ms,
                                platform_alarm_cb,
                                nullptr);
    gIsAlarmSet = true;
    gAlarmMutex.unlock();
#endif

}


void
WeAreDirty(void)
{
    SDL_UserEvent user_event = {
        SDL_USEREVENT,
        SDL_GetTicks(),
        0,
        1,
        nullptr,
        nullptr
    };
    SDL_PushEvent((SDL_Event*)&user_event);
}


extern "C" void
ServicePendingInterrupts(void)
{
    if (gPendingInterrupt != 0)
    {
//        dispatch_async(gTimerQ, ^{ DispatchFakeInterrupt(gPendingInterrupt); });
        DispatchFakeInterrupt(gPendingInterrupt);
        gPendingInterrupt = 0;
    }
}





#if 0

// Replace all the following code with C++ code

#define forLayerDrawing 1


extern CTime		GetClock(void);

extern "C" void	ResetNewton(void);
extern void			TimerInterruptHandler(void * inQueue);
extern void			PreemptiveTimerInterruptHandler(void * inQueue);

dispatch_queue_t gNewtonQ;
dispatch_queue_t gTimerQ;
dispatch_source_t schedulerSource;
dispatch_source_t alarmSource;
BOOL gIsAlarmSet = NO;
ULong gPendingInterrupt = 0;

#define kIntSourceTimer			0x0020
#define kIntSourceScheduler	0x0040
extern void	DispatchFakeInterrupt(uint32_t inSourceMask);
extern ULong gCPSR;

void
SetPlatformAlarm(int64_t inDelta)
{
	// set one-shot timer to fire in inDelta ms
	dispatch_source_set_timer(alarmSource, dispatch_time(DISPATCH_TIME_NOW, inDelta * NSEC_PER_USEC), DISPATCH_TIME_FOREVER, 1000ull);
	if (!gIsAlarmSet)
	{
		gIsAlarmSet = YES;
printf("SetPlatformAlarm(inDelta=%lldms)\n", inDelta);
		dispatch_resume(alarmSource);
	}
}

/*------------------------------------------------------------------------------
	U s e r   I n t e r f a c e
--------------------------------------------------------------------------------
	Start up the Newton simulation environment.
	Interrupts based on 2 timers

		scheduler: PreemptiveTimerInterruptHandler
			request schedule; current task = nil; set interrupt in next 20ms;
				can do this b/c IRQ is (largely) disabled during SWI
		->	set up 20ms(1s for testing) recurring timer to DispatchFakeInterrupt(kIntSourceTimer);
			restore IRQ enable in SWI
			if IRQ disabled when PreemptiveTimerInterruptHandler fires, queue it; run it when IRQ re-enabled

		timer: TimerInterruptHandler
			gTimerEngine->alarm(); calls back expired items in timer queue;
		->	SetAlarm() should SetPlatformAlarm()
				reset one-shot timer w/ given future delta
			when timer expires, DispatchFakeInterrupt(kIntSourceTimer);

// print stack trace
//	NSLog(@"%@", [NSThread callStackSymbols]);
------------------------------------------------------------------------------*/
MPWindowController * wc;

@implementation MPController

- (void) applicationDidFinishLaunching: (NSNotification *)notification
{
	[wc setupDrawing];

	gNewtonQ = dispatch_queue_create("org.newton.messagepad", NULL);
	gTimerQ = dispatch_queue_create("org.newton.messagepad.timer", NULL);

	// set up scheduler interrupt
	schedulerSource = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, gTimerQ);
	dispatch_source_set_event_handler(schedulerSource,
	^{
		if (FLAGTEST(gCPSR, kIRQDisable))
			gPendingInterrupt |= kIntSourceScheduler;
		else
		{
			// handle the scheduler interrupt
//			putchar('>');
			DispatchFakeInterrupt(kIntSourceScheduler);
//			GetPlatformDriver()->timerInterruptHandler();
		}
	});

	// start the scheduler interrupt
	dispatch_source_set_timer(schedulerSource, dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_MSEC*500), NSEC_PER_MSEC*500, 1000ull);	// every second
	dispatch_resume(schedulerSource);

	// set up timer interrupt
	alarmSource = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, gTimerQ);
	dispatch_source_set_event_handler(alarmSource,
	^{
		if (FLAGTEST(gCPSR, kIRQDisable))
			gPendingInterrupt |= kIntSourceTimer;
		else
		{
			// handle the timer interrupt
//			putchar('!');
			DispatchFakeInterrupt(kIntSourceTimer);
//			gIsAlarmSet = NO;
		}
	});


	// GO!!
	dispatch_async(gNewtonQ, ^{ ResetNewton(); });
}

@end

extern "C" void
ServicePendingInterrupts(void)
{
	if (gPendingInterrupt != 0)
	{
		dispatch_async(gTimerQ, ^{ DispatchFakeInterrupt(gPendingInterrupt); });
		gPendingInterrupt = 0;
	}
}


/* -----------------------------------------------------------------------------
	M P W i n d o w C o n t r o l l e r
----------------------------------------------------------------------------- */
extern CGContextRef quartz;

extern void SetUpFakeTablet(void);


@implementation MPWindowController
- (void)windowDidLoad {
	wc = self;
}

- (void)setupDrawing {
	CGContextRef windowContext = self.window.graphicsContext.CGContext;
#if defined(forLayerDrawing)
	//create a CGLayer and draw into its context
	MPView * theView = (MPView *)self.window.contentView;
	CGLayerRef drgLayer = CGLayerCreateWithContext(windowContext, theView.frame.size, NULL);
	theView.qContext = windowContext;
	theView.qLayer = drgLayer;
	quartz = CGLayerGetContext(drgLayer);
#else
	// for drawing direct into the window:
	quartz = windowContext;
#endif

#if defined(forOriginTopLeft)
	CGContextTranslateCTM(quartz, 0.0, 480.0);	// gScreenHeight not set yet
	CGContextScaleCTM(quartz, 1.0, -1.0);
	CGContextSetTextMatrix(quartz, CGAffineTransformMake(1.0, 0.0, 0.0, -1.0, 0.0, 0.0));
#else
	CGContextTranslateCTM(quartz, 0.0, 0.0);
	CGContextScaleCTM(quartz, 1.0, 1.0);

//	CGContextSetTextMatrix(quartz, CGAffineTransformIdentity);	// done every time we DrawUnicodeText

//	CGContextSetAllowsAntialiasing(quartz, true);

//	CGContextSetAllowsFontSmoothing(quartz, true);
//	CGContextSetShouldSmoothFonts(quartz, false);	//improves draw quality of text

//	CGContextSetAllowsFontSubpixelPositioning(quartz, true);
//	CGContextSetShouldSubpixelPositionFonts(quartz, true);

//	CGContextSetAllowsFontSubpixelQuantization(quartz, true);
//	CGContextSetShouldSubpixelQuantizeFonts(quartz, true);

#endif
	// draw into quartz context
	// StopDrawing() -> WeAreDirty() -> set needsDisplay on the contentView
	SetUpFakeTablet();
}
@end


/* -----------------------------------------------------------------------------
	M P V i e w
----------------------------------------------------------------------------- */
#define kTabScale 8.0

extern int gScreenHeight;

extern "C" void	PenDown(float inX, float inY);
extern "C" void	PenMoved(float inX, float inY);
extern "C" void	PenUp(void);


void
WeAreDirty(void)
{
#if 0
    ((MPView *)wc.window.contentView).needsDisplay = true;
#else
    dispatch_async(dispatch_get_main_queue(), ^{
        ((MPView *)wc.window.contentView).needsDisplay = true;
    });
#endif
}


@implementation MPView

#if defined(forLayerDrawing)
- (void)drawRect:(NSRect)dirtyRect
{
	if (self.qContext) {
//printf("-[MPView drawRect:]\n");
		CGContextDrawLayerAtPoint(self.qContext, CGPointZero, self.qLayer);
	}
}
#endif

- (BOOL)isOpaque
{ return true; }

- (BOOL)acceptsFirstResponder
{ return true; }

- (BOOL)mouseDownCanMoveWindow
{ return false; }


- (void)mouseDown:(NSEvent *)inEvent
{
	NSPoint penLoc = [self convertPoint: [inEvent locationInWindow] fromView: nil];
	PenDown((penLoc.x) * kTabScale, (gScreenHeight-penLoc.y) * kTabScale);
}


- (void)mouseDragged:(NSEvent *)inEvent
{
	NSPoint penLoc = [self convertPoint: [inEvent locationInWindow] fromView: nil];
	PenMoved((penLoc.x) * kTabScale, (gScreenHeight-penLoc.y) * kTabScale);
}


- (void)mouseUp:(NSEvent *)inEvent
{
	PenUp();
}

@end

#endif

