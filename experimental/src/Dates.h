/*
	File:		Dates.h

	Contains:	Dates: the date and time now, as strings (reconstructed: not in
				the published headers; the port's CDate, Apple's name).
*/

#ifndef __DATES_H
#define __DATES_H

#ifndef __NEWTON_H
#include "Newton.h"
#endif

#ifndef __OBJECTS_H
#include "Frames/objects.h"
#endif

/* date and time elements */
#define kIncludeAllElements		0

class TDate
{
public:
				TDate();

	void		SetCurrentTime(void);
	void		ShortDateString(ULong inStrSpec, UniChar * outStr, ULong inStrSize);
	void		TimeString(ULong inStrSpec, UniChar * outStr, ULong inStrSize);

	ULong		fYear;				// +00
	ULong		fMonth;				// +04
	ULong		fDay;				// +08
	ULong		fHour;				// +0C
	ULong		fMinute;			// +10
	ULong		fSecond;			// +14
	ULong		fDayOfWeek;			// +18
	RefStruct	fLongDateFormat;	// +1C
	RefStruct	fShortDateFormat;	// +20
	RefStruct	fTimeFormat;		// +24
};

#endif	/* __DATES_H */
