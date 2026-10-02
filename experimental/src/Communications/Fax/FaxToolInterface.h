/*
	File:		FaxToolInterface.h

	Contains:	TFaxToolInterface, a client's connection to the fax tool
				(reconstructed: not in the published headers). A client
				derives from it and gets each request's result through its
				...Complete functions. The virtual functions in the order
				of the ROM's vtable (0x1E798); the 552 bytes after the event
				handler's are the requests and replies the interface sends
				and keeps (the ROM's constructor at 0xB9794), not spelled
				out yet. Names ours where Apple's table has none.
*/

#ifndef __FAXTOOLINTERFACE_H
#define __FAXTOOLINTERFACE_H

#ifndef __AEVENTHANDLER_H
#include "UtilityClasses/AEventHandler.h"
#endif

#ifndef __OPTIONARRAY_H
#include "CommAPI/OptionArray.h"
#endif


class TFaxToolInterface : public TAEventHandler
{
public:
					TFaxToolInterface(ULong inServiceId, ULong inToolId);
	virtual			~TFaxToolInterface();

	virtual	Boolean	AETestEvent(TAEvent * inEvent);
	virtual	void	AECompletionProc(TUMsgToken * inToken, ULong * inSize, TAEvent * inEvent);
	virtual	void	IdleProc(TUMsgToken * inToken, ULong * inSize, TAEvent * inEvent);

	virtual void	OpenSession(TOptionArray * inOptions, UChar * inPhoneNumber, ULong inLength, UChar inAsync);
	virtual void	OpenSessionComplete(NewtonErr inErr, ULong inArg1, ULong inArg2, ULong inWidth, ULong inVerticalRes) = 0;
	virtual void	AcceptSession(TOptionArray * inOptions, UChar inAsync);
	virtual void	AcceptSessionComplete(NewtonErr inErr, ULong inArg1, ULong inWidth, ULong inVerticalRes) = 0;
	virtual void	CloseSession(UChar inAsync);
	virtual void	CloseSessionComplete(NewtonErr inErr) = 0;
	virtual void	BeginPage(UChar inAsync);
	virtual void	BeginPageComplete(NewtonErr inErr) = 0;
	virtual void	EndPage(UChar inAsync, UChar inLastPage);
	virtual void	EndPageComplete(NewtonErr inErr) = 0;
	virtual void	PrintBand(UChar * inBits, ULong inLines, ULong inRowBytes, ULong inLineBytes, UChar inAsync);
	virtual void	PrintBandContinue(NewtonErr inErr, UChar inAsync);
	virtual void	PrintBandComplete(NewtonErr inErr) = 0;
	virtual void	GetBand(UChar * outBits, ULong inSize, UChar inAsync);
	virtual void	GetBandComplete(NewtonErr inErr, ULong inLines, Boolean inLastBand) = 0;
	virtual void	ConfirmReceivedPage(UChar inGood, UChar inAsync);
	virtual void	ConfirmReceivedPageComplete(NewtonErr inErr, Boolean inLastPage) = 0;
	virtual void	ContinueClose(void);
	virtual void	PostBind(UChar inAsync);
	virtual void	PostConnect(UChar inAsync);
	virtual NewtonErr	DoInit(TOptionArray * inOptions);
	virtual void	InitConnect(UChar * inPhoneNumber, ULong inLength);
	virtual void	CleanUpAfterConnect(void);

	NewtonErr		Init(TOptionArray * inConfig, ULong inServiceId, ULong inClientId);
	void			SetDefaultConfig(TOptionArray * ioConfig, ULong inFlags);
	void			SetDefaultOptions(TOptionArray * ioOptions);
	void			SetFaxOptions(TOptionArray * ioOptions, UChar inSend);
	void			SetMinScanLineTime(ULong inTime);

	char			fState[552];		// +014 .. +23C
};

#endif	/* __FAXTOOLINTERFACE_H */
