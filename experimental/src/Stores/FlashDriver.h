/*
	File:		FlashDriver.h

	Contains:	Flash chips and their drivers (reconstructed: not in the
				published headers; the port has a Stores/FlashDriver.h).
				Layouts as the ROM's code uses them; names ours where
				Apple's table has none.
*/

#ifndef __FLASHDRIVER_H
#define __FLASHDRIVER_H

#ifndef __NEWTONTYPES_H
#include "NewtonTypes.h"
#endif

/* What a driver finds out about a chip (Identify) */
struct SFlashChipInformation
{
	ULong		fManufacturer;		// +00 0x89 Intel, 0xB0 Sharp
	ULong		fDevice;			// +04
	ULong		fVppKind;			// +08 2 Intel, 1 Sharp
	ULong		fWidth;				// +0C lanes one chip takes: 1 (x8) or 2 (x16)
	ULong		fChipSize;			// +10 bytes
	ULong		fBlockSize;			// +14 bytes of an erase block
};

#endif	/* __FLASHDRIVER_H */
