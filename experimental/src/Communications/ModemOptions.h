/*
	File:		ModemOptions.h

	Contains:	The modem tool's options (reconstructed: not in the published
				headers). Layouts from the ROM's constructors and uses, with
				newton-re's findings; names ours.
*/

#ifndef __MODEMOPTIONS_H
#define __MODEMOPTIONS_H

#ifndef __OPTIONARRAY_H
#include "CommAPI/OptionArray.h"
#endif


#define kCMOModemDialing		'mdo '


/* 'mdo ': how to dial (32 bytes) */
class TCMOModemDialing : public TOption
{
public:
					TCMOModemDialing();

	Boolean			fSpeakerOn;			// +0C
	Boolean			fDetectDialTone;	// +0D
	Boolean			fDetectBusy;		// +0E
	Boolean			fToneDialing;		// +0F
	Boolean			fManualDial;		// +10
	UChar			fSpeakerVolume;		// +11
	UChar			fWaitForCarrier;	// +12  seconds
	UChar			fBlindDialDelay;	// +13  seconds
	UChar			fCommaDelay;		// +14  seconds
	UChar			fRingsToAnswer;		// +15
	long			fCountry;			// +18
	Boolean			fCellular;			// +1C
};


/* The user's modem preferences into a dialing option */
void		SetDialingOptionsFromPrefs(TCMOModemDialing * ioOption);

#endif	/* __MODEMOPTIONS_H */
