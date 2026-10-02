/*
	File:		TAPIOptions.cc

	Contains:	The telephony (TAPI) options' constructors.

	ROM:		0x206494 (TCMOTAPIHold::TCMOTAPIHold) .. 0x20684C (after
				TCMOTAPIService::TCMOTAPIService), MP2x00 US 2.1 (717006).
*/

#include "Communications/TAPIOptions.h"


TCMOTAPIHold::TCMOTAPIHold()
	: TOption(kOptionType)
{
	SetLabel(kCMOTAPIHold);
	SetLength(sizeof(TCMOTAPIHold) - sizeof(TOption));
}


TCMOTAPIUnhold::TCMOTAPIUnhold()
	: TOption(kOptionType)
{
	SetLabel(kCMOTAPIUnhold);
	SetLength(sizeof(TCMOTAPIUnhold) - sizeof(TOption));
}


TCMOTAPISpeaker::TCMOTAPISpeaker()
	: TOption(kOptionType)
{
	SetLabel(kCMOTAPISpeaker);
	SetLength(sizeof(TCMOTAPISpeaker) - sizeof(TOption));
	fSpeakerOn = true;
}


TCMOHandsetManagement::TCMOHandsetManagement()
	: TOption(kOptionType)
{
	SetLabel(kCMOHandsetManagement);
	SetLength(sizeof(TCMOHandsetManagement) - sizeof(TOption));
	fHandsetOn = true;
}


TCMOTAPIOutGoing::TCMOTAPIOutGoing()
	: TOption(kOptionType)
{
	SetLabel(kCMOTAPIOutGoing);
	SetLength(sizeof(TCMOTAPIOutGoing) - sizeof(TOption));
}


TCMOTAPIAnswer::TCMOTAPIAnswer()
	: TOption(kOptionType)
{
	SetLabel(kCMOTAPIAnswer);
	SetLength(sizeof(TCMOTAPIAnswer) - sizeof(TOption));
}


TCMOTAPIDisconnect::TCMOTAPIDisconnect()
	: TOption(kOptionType)
{
	SetLabel(kCMOTAPIDisconnect);
	SetLength(sizeof(TCMOTAPIDisconnect) - sizeof(TOption));
}


TCMOTAPIForward::TCMOTAPIForward()
	: TOption(kOptionType)
{
	SetLabel(kCMOTAPIForward);
	SetLength(sizeof(TCMOTAPIForward) - sizeof(TOption));
}


TCMOTAPIForwardClear::TCMOTAPIForwardClear()
	: TOption(kOptionType)
{
	SetLabel(kCMOTAPIForwardClear);
	SetLength(sizeof(TCMOTAPIForwardClear) - sizeof(TOption));
}


TCMOTAPITransfer::TCMOTAPITransfer()
	: TOption(kOptionType)
{
	SetLabel(kCMOTAPITransfer);
	SetLength(sizeof(TCMOTAPITransfer) - sizeof(TOption));
}


TCMOTAPISendDigit::TCMOTAPISendDigit()
	: TOption(kOptionType)
{
	SetLabel(kCMOTAPISendDigit);
	SetLength(sizeof(TCMOTAPISendDigit) - sizeof(TOption));
}


TCMOTAPIService::TCMOTAPIService()
	: TOption(kOptionType)
{
	SetLabel(kCMOTAPIService);
	SetLength(sizeof(TCMOTAPIService) - sizeof(TOption));
	fHold = false;
	fTransfer = false;
	fForward = false;
	fSpeaker = true;
}
