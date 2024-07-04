/*
	File:		Voyager.cc

	Contains:	Platform-specific implementation.

	Written by:	Newton Research Group, 2010.
*/

#include "UserGlobals.h"
#include "Platform.h"


void
RegisterVoyagerMiscIntf(void)
{
#if defined(correct)
	CVoyagerMiscIntfImpl::classInfo->registerProtocol();
#endif
}


/* ----------------------------------------------------------------
	CVoyagerPlatform implementation class info.
	Not fully implemented -- just to see what Newt does.
---------------------------------------------------------------- */

const CClassInfo *
CVoyagerPlatform::classInfo(void)
{
    static CClassInfo _classInfo = {
        .fName = "CVoyagerPlatform",
        .fInterface = "CPlatformDriver",
        .fSignature = "\0",
        .fSizeofProc = []()->size_t { return sizeof(CVoyagerPlatform); },
        .fAllocProc = []()->CProtocol* { return new CVoyagerPlatform(); },
        .fFreeProc = [](CProtocol* p)->void { delete p; },
        .fVersion = 0,
        .fFlags = 0
    };
    return &_classInfo;
#if 0
__asm__ (
CLASSINFO_BEGIN
"		.long		0			\n"
"		.long		1f - .	\n"
"		.long		2f - .	\n"
"		.long		3f - .	\n"
"		.long		4f - .	\n"
"		.long		5f - .	\n"
"		.long		__ZN16CVoyagerPlatform6sizeOfEv - 0b	\n"
"		.long		0			\n"
"		.long		0			\n"
"		.long		__ZN16CVoyagerPlatform4makeEv - 0b	\n"
"		.long		__ZN16CVoyagerPlatform7destroyEv - 0b	\n"
"		.long		0			\n"
"		.long		0			\n"
"		.long		0			\n"
"		.long		6f - 0b	\n"
"1:	.asciz	\"CVoyagerPlatform\"	\n"
"2:	.asciz	\"CPlatformDriver\"	\n"
"3:	.byte		0			\n"
"		.align	2			\n"
"4:	.long		0			\n"
"		.long		__ZN16CVoyagerPlatform9classInfoEv - 4b	\n"
"		.long		__ZN16CVoyagerPlatform4makeEv - 4b	\n"
"		.long		__ZN16CVoyagerPlatform7destroyEv - 4b	\n"
"		.long		__ZN16CVoyagerPlatform4initEv - 4b	\n"
"		.long		__ZN16CVoyagerPlatform16backlightTriggerEv - 4b	\n"
"		.long		__ZN16CVoyagerPlatform28registerPowerSwitchInterruptEv - 4b	\n"
"		.long		__ZN16CVoyagerPlatform23enableSysPowerInterruptEv - 4b	\n"
"		.long		__ZN16CVoyagerPlatform16interruptHandlerEv - 4b	\n"
"		.long		__ZN16CVoyagerPlatform21timerInterruptHandlerEv - 4b	\n"
"		.long		__ZN16CVoyagerPlatform18resetZAPStoreCheckEv - 4b	\n"
"		.long		__ZN16CVoyagerPlatform13powerOnSystemEv - 4b	\n"
"		.long		__ZN16CVoyagerPlatform11pauseSystemEv - 4b	\n"
"		.long		__ZN16CVoyagerPlatform14powerOffSystemEv - 4b	\n"
"		.long		__ZN16CVoyagerPlatform16powerOnSubsystemEj - 4b	\n"
"		.long		__ZN16CVoyagerPlatform17powerOffSubsystemEj - 4b	\n"
"		.long		__ZN16CVoyagerPlatform21powerOffAllSubsystemsEv - 4b	\n"
"		.long		__ZN16CVoyagerPlatform19translatePowerEventEj - 4b	\n"
"		.long		__ZN16CVoyagerPlatform18getPCMCIAPowerSpecEjPj - 4b	\n"
"		.long		__ZN16CVoyagerPlatform18powerOnDeviceCheckEh - 4b	\n"
"		.long		__ZN16CVoyagerPlatform17setSubsystemPowerEjj - 4b	\n"
"		.long		__ZN16CVoyagerPlatform17getSubsystemPowerEjPj - 4b	\n"
CLASSINFO_END
);
#endif
}

PROTOCOL_IMPL_SOURCE_MACRO(CVoyagerPlatform)


CVoyagerPlatform *
CVoyagerPlatform::make(void)
{
	return this;
}


void
CVoyagerPlatform::destroy(void)
{ }


void
CVoyagerPlatform::init(void)
{}


void
CVoyagerPlatform::backlightTrigger(void)
{}

void
CVoyagerPlatform::registerPowerSwitchInterrupt(void)
{}

void
CVoyagerPlatform::enableSysPowerInterrupt(void)
{}


void
CVoyagerPlatform::interruptHandler(void)
{
	samplePowerSwitchStateMachine();
}


void
CVoyagerPlatform::timerInterruptHandler(void)
{
	if (f34 == 1)
		f34 = 2;
	else if (f34 == 2)
		f34 = 3;
	samplePowerSwitchStateMachine();
}


bool
CVoyagerPlatform::resetZAPStoreCheck(void)
{ return false; }


NewtonErr
CVoyagerPlatform::powerOnSystem(void)
{ return noErr; }


NewtonErr
CVoyagerPlatform::pauseSystem(void)
{
#if defined(correct)
	ULong x = g0F110000;
	g0F110000 = MASKCLEAR(x, 1);
	if (FLAGTEST(gDebuggerBits, kDebuggerIsPresent))
		g0F110400 |= 1;
	g0F110000 = x;
#endif
	return noErr;
}


NewtonErr
CVoyagerPlatform::powerOffSystem(void)
{
	f36 = true;
#if defined(correct)
	DebuggerPowerCycleProc(true);
	SaveCPUStateAndStopSystem();
	DebuggerPowerCycleProc(false);
#endif
	f36 = false;
	return noErr;
}


void
CVoyagerPlatform::powerOnSubsystem(ULong)
{}

void
CVoyagerPlatform::powerOffSubsystem(ULong)
{}

void
CVoyagerPlatform::powerOffAllSubsystems(void)
{}


void
CVoyagerPlatform::translatePowerEvent(ULong)
{}

void
CVoyagerPlatform::getPCMCIAPowerSpec(ULong, ULong *)
{}

void
CVoyagerPlatform::powerOnDeviceCheck(unsigned char)
{}

void
CVoyagerPlatform::setSubsystemPower(ULong, ULong)
{}

void
CVoyagerPlatform::getSubsystemPower(ULong, ULong *)
{}


void
CVoyagerPlatform::samplePowerSwitchStateMachine(void)
{}
