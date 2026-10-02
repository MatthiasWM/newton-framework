/*
	File:		VirtualMemory.h

	Contains:	Globals that live across reboot (reconstructed: not in the
				published headers; as the port's OS/VirtualMemory.h, but only
				what the ROM's code shows so far: its layout differs from the
				port's).
*/

#ifndef __VIRTUALMEMORY_H
#define __VIRTUALMEMORY_H

#ifndef __NEWTONTYPES_H
#include "NewtonTypes.h"
#endif

struct SGlobalsThatLiveAcrossReboot
{
	ULong		fMagicNumber;				// +000
	ULong		fPersistentDataInUse;		// +004
	ULong		fRebuildPageTracker;		// +008
	ULong		fRealTimeClockHackSavedTimeValue;	// +00C
	ULong		fWarmBootCount;				// +010
	NewtonErr	fRebootReason;				// +014
	void *		fMemObjDBIndexTable;		// +018
	ULong		fLastNodeId;				// +01C
	ULong		fNotYetKnown[75];			// +020 (patches, tablet calibration)

	// controlled by gCollectCPUStats
	ULong		fTimeAtColdBoot;			// +14C real time clock value at the last cold boot
	ULong		fProcessorOnTime;			// +150
	ULong		fScreenOnTime;				// +154
	ULong		fSerialOnTime;				// +158
	ULong		fSoundOnTime;				// +15C
};

extern SGlobalsThatLiveAcrossReboot	gGlobalsThatLiveAcrossReboot;	// 0C1061C4
extern ULong						gCollectCPUStats;				// 0C104F50

#endif	/* __VIRTUALMEMORY_H */
