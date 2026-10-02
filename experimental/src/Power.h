/*
	File:		Power.h

	Contains:	Batteries and power (reconstructed: not in the published
				headers; the port has a Power.h). PowerPlantStatus's fields
				are named after the slots Apple's BatteryStatusHelper makes
				of them (batteryType, batteryVoltage, ...); the last ones
				are less certain.
*/

#ifndef __POWER_H
#define __POWER_H

#ifndef __NEWTONTYPES_H
#include "NewtonTypes.h"
#endif
#ifndef __AEVENTS_H
#include "UtilityClasses/AEvents.h"
#endif

class TUPort;

struct PowerPlantStatus
{
	long		fBatteryType;		// +00
	Fixed		fBatteryVoltage;	// +04
	long		fBatteryCapacity;	// +08 per cent
	long		fBatteryLow;		// +0C
	long		fBatteryDead;		// +10
	Fixed		fBatteryCurrent;	// +14
	long		fACPower;			// +18 0 no, 1 yes
	Fixed		fACVoltage;			// +1C
	long		fChargeState;		// +20
	Fixed		fChargeRate;		// +24
	Fixed		fChargeCurrent;		// +28
	long		fAmbientTemp;		// +2C -1: not known
	long		fBatteryTemp;		// +30 -1: not known
};

/* Requests to the power manager (GetPowerPort), and its replies: an event
   of class 'newt', id 'pg&e', a selector, then the selector's data */
enum
{
	kPowerPlantStatus = 4,
	kPowerPlantRawStatus = 5,
	kPowerPlantBatteryCount = 6,
	kPowerPlantSetBatteryType = 7
};

class TPowerPlantEvent : public TAEvent			// battery count
{
public:
	ULong		fSelector;
	long		fValue;
};

class TBatteryTypeEvent : public TAEvent		// set battery type
{
public:
	ULong		fSelector;
	long		fWhich;			// in the reply: the result
	long		fType;
};

class TBatteryStatusEvent : public TAEvent		// battery status
{
public:
	ULong		fSelector;		// in the reply: the result
	long		fWhich;
	PowerPlantStatus	fStatus;
};

TUPort *	GetPowerPort(void);
NewtonErr	GetBatteryStatus(long inWhich, PowerPlantStatus * outStatus, Boolean inRaw);
NewtonErr	SetBatteryType(long inWhich, long inType);
ULong		CyclePower(void);
ULong		TranslatePowerEvent(ULong inEvent);
ULong		SleepUntilNextWakeup(void);

#endif	/* __POWER_H */
