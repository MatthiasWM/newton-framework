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

NewtonErr	GetBatteryStatus(long inWhich, PowerPlantStatus * outStatus, Boolean inRaw);
ULong		CyclePower(void);
ULong		TranslatePowerEvent(ULong inEvent);
ULong		SleepUntilNextWakeup(void);

#endif	/* __POWER_H */
