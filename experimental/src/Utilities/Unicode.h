/*
	File:		Unicode.h

	Contains:	Converting Unicode text (reconstructed: not in the published
				headers).
*/

#ifndef __UNICODE_H
#define __UNICODE_H

#ifndef __NEWTON_H
#include "Newton.h"
#endif

/* encodings */
#define kMacRomanEncoding		1

void	ConvertFromUnicode(const UniChar * inStr, void * outStr, long inEncoding, long inSize);

extern "C"
{
UniChar *	Ustrcpy(UniChar * outStr, const UniChar * inStr);
UniChar *	Ustrcat(UniChar * ioStr, const UniChar * inStr);
long		Ustrlen(const UniChar * inStr);
}

#endif	/* __UNICODE_H */
