/*
	File:		TAPIOptions.h

	Contains:	The telephony (TAPI) options a comm tool takes: hold, answer,
				dial out, transfer, the speaker, the handset (reconstructed:
				not in the published headers).

				Names ours where Apple's table has none (fields, labels).
*/

#ifndef __TAPIOPTIONS_H
#define __TAPIOPTIONS_H

#ifndef __OPTIONARRAY_H
#include "CommAPI/OptionArray.h"
#endif

#define kCMOTAPIHold				'hold'
#define kCMOTAPIUnhold				'unho'
#define kCMOTAPISpeaker				'tasp'
#define kCMOHandsetManagement		'hsmn'
#define kCMOTAPIOutGoing			'outg'
#define kCMOTAPIAnswer				'answ'
#define kCMOTAPIDisconnect			'disc'
#define kCMOTAPIForward				'forw'
#define kCMOTAPIForwardClear		'focr'
#define kCMOTAPITransfer			'tran'
#define kCMOTAPISendDigit			'sdgt'
#define kCMOTAPIService				'taps'

class TCMOTAPIHold : public TOption
{
public:
				TCMOTAPIHold();
};

class TCMOTAPIUnhold : public TOption
{
public:
				TCMOTAPIUnhold();
};

class TCMOTAPISpeaker : public TOption
{
public:
				TCMOTAPISpeaker();

	Boolean		fSpeakerOn;			// +0C
};

class TCMOHandsetManagement : public TOption
{
public:
				TCMOHandsetManagement();

	Boolean		fHandsetOn;			// +0C
};

class TCMOTAPIOutGoing : public TOption
{
public:
				TCMOTAPIOutGoing();
};

class TCMOTAPIAnswer : public TOption
{
public:
				TCMOTAPIAnswer();
};

class TCMOTAPIDisconnect : public TOption
{
public:
				TCMOTAPIDisconnect();
};

class TCMOTAPIForward : public TOption
{
public:
				TCMOTAPIForward();
};

class TCMOTAPIForwardClear : public TOption
{
public:
				TCMOTAPIForwardClear();
};

class TCMOTAPITransfer : public TOption
{
public:
				TCMOTAPITransfer();
};

class TCMOTAPISendDigit : public TOption
{
public:
				TCMOTAPISendDigit();
};

class TCMOTAPIService : public TOption
{
public:
				TCMOTAPIService();

	Boolean		fHold;				// +0C
	Boolean		fTransfer;			// +0D
	Boolean		fForward;			// +0E
	Boolean		fSpeaker;			// +0F
};

#endif	/* __TAPIOPTIONS_H */
