/*
	File:		SerialNumber.h

	Contains:	The serial number chip (reconstructed: not in the published
				headers; as the port's SerialNumber.h, with Apple's names).
*/

#ifndef __SERIALNUMBER_H
#define __SERIALNUMBER_H

#ifndef __NEWTONTYPES_H
#include "NewtonTypes.h"
#endif

class TSerialNumberROM
{
public:
	NewtonErr	GetSystemSerialNumber(ULong * outSerialNumber);
};

TSerialNumberROM *	GetSerialNumberROMObject(void);

#endif	/* __SERIALNUMBER_H */
