/*
	File:		ADSPConnection.cc

	Contains:	An ADSP connection: opening it (the state machine a received
				control packet drives), its timers (probe, retry, send,
				forward reset), sending control packets, data and acks,
				processing acks, closing.

	ROM:		0x205180 (TADSPConnection::TADSPConnection) .. 0x206494 (after
				ExecuteState), MP2x00 US 2.1 (717006).
*/

#include "Communications/AppleTalk/ADSPConnection.h"


/*------------------------------------------------------------------------------
	Making and breaking.
------------------------------------------------------------------------------*/

TADSPConnection::TADSPConnection()
	: fFilterAddress(1), fRemoteAddress(1)
{
	fRefCon = 0;
	fOpenState = 0;
	fState = 0;
	f1E = 3;
	fSendCtl = 0;
	fUnackedSends = 0;
	fSendWindow = 1;
	f2C = 5;
	f28 = 0;
	f30 = 0;
	fSendTime = 0;
	fAckSeq = 0;
	f3C = 5;
	f40 = 1;
	f44 = 16;
	fSendPending = 0;
	fWindowDoubling = 0;
	fRetransmitted = 0;
	fAckPending = 0;
	fSendBlocked = 0;
	fAwaitingAck = 0;
	fFlushPending = 0;
	f48_24 = 0;
	fClientUpdate = 0;
	fOpenRetries = 10;
	fOpenInterval = 3;
	fProbeRetries = 20;
	fProbeInterval = 5;
	f98 = 15;
	fRetryInterval = 5;
	fRemoteAddress.Clear();
	fFilterAddress.Clear();
	fConnID = 0;
	fRemoteConnID = 0;
	fAckCountdown = 0;
	fStates = NULL;
}


TADSPConnection::~TADSPConnection()
{
	fProbeTimer.Stop();
	fRetryTimer.Stop();
	fSendTimer.Stop();
	fResetTimer.Stop();
}


/*------------------------------------------------------------------------------
	Whether a control packet is for this connection in its open state; the
	State it calls for.
------------------------------------------------------------------------------*/

#if 0
/* Not yet: identical but for three words (0x205428: the ROM loads
   fRemoteAddress.fFlags into r0 and the packet's into r1; here the other
   way round). Until the form is found, it stays generated assembler. */
Boolean
TADSPConnection::Match(ADSPHeader * inHeader, TAddress * inAddress, ADSPOpenConnInfo * inInfo, State ** outState)
{
	if (fOpenState < 1 || fOpenState > 4)
		return false;
	State * state = &fStates[((inHeader->descriptor & kADSPControlCode) - 1) * 4 + fOpenState - 1];
	*outState = state;
	UChar match = state->fMatch;
	if (match & kMatchNever)
		return false;
	if ((match & kMatchRemoteSocket) && fRemoteAddress.fFlags != inAddress->fFlags)
		return false;
	if ((match & kMatchFilterAddress) && !MatchFilterAddress(inAddress))
		return false;
	if ((match & kMatchRemoteAddress) && !MatchAddress(inAddress))
		return false;
	if ((match & kMatchDestConnID) && fConnID != inInfo->destConnID)
		return false;
	if ((match & kMatchSrcConnID) && fRemoteConnID != inHeader->srcConnID)
		return false;
	if ((match & kMatchNoDestConnID) && inInfo->destConnID != 0)
		return false;
	if ((match & kMatchNoSrcConnID) && inHeader->srcConnID != 0)
		return false;
	return true;
}
#endif


/*------------------------------------------------------------------------------
	A data packet arrived.
------------------------------------------------------------------------------*/

long
TADSPConnection::Read(TPacketMessage * inMessage, ADSPHeader * inHeader)
{
	TMemoryObject * data;
	data = inMessage->fData;
	UChar * p = (UChar *) data->GetPtr() + data->fOffset;
	long size = inMessage->fData->fSize;
	int eom = (inHeader->descriptor & kADSPEOM) != 0;
	ULong seq = (inHeader->firstByteSeqHi << 16) | inHeader->firstByteSeqLo;
	long result = fRecvBuffer->Putn(p, size, seq, eom);
	if (result == 0)
	{
		fAckCountdown = 32;
		ProcessAck(inHeader);
		fClientUpdate = 1;
		if (inHeader->descriptor & kADSPAckRequest)
		{
			fSendPending = 1;
			fAckPending = 1;
		}
	}
	else if (result == -1)
	{
		if (--fAckCountdown == 0)
			fSendCtl |= kSendRetransmitAdvice;
	}
	return result;
}


long
TADSPConnection::ReadAttention(TPacketMessage * inMessage, ADSPHeader * inHeader)
{
	return 0;
}


void
TADSPConnection::RecvAttnComplete(TATAsyncMsg * inMessage)
{ }


void
TADSPConnection::UpDateClient(void)
{
	fClientUpdate = 0;
	fClientPort.Send(&fEventMessage, &fEvent, sizeof(TADSPEvent));
}


/*------------------------------------------------------------------------------
	Timers.
------------------------------------------------------------------------------*/

void
TADSPConnection::AttnExpired(TTimerMessage * inMessage)
{ }


void
TADSPConnection::ResetProbeTimer(void)
{
	fProbeTimer.Reset(fProbeInterval, kSeconds);
	fProbeRetries = 20;
}


void
TADSPConnection::ProbeExpired(TTimerMessage * inMessage)
{
	if (fState == 3 || fState == 4)
	{
		if (--fProbeRetries == 0)
			DoClose(-2);
		else
		{
			fProbeTimer.Reset(fProbeInterval, kSeconds);
			fSendCtl |= kSendProbe;
			CheckSend();
		}
	}
	else if (fState == 2 && (fOpenState == 2 || fOpenState == 3))
	{
		if (--fOpenRetries == 0)
			DoClose(-1);
		else
		{
			fProbeTimer.Reset(fOpenInterval, kSeconds);
			fSendCtl |= (fOpenState == 2) ? kSendOpenReq : kSendOpenAck;
			CheckSend();
		}
	}
}


void
TADSPConnection::FlushExpired(TTimerMessage * inMessage)
{
	if (fSendBuffer->fSendWindowSeq > fSendBuffer->fSendSeq)
	{
		fFlushPending = 1;
		CheckSend();
	}
}


void
TADSPConnection::Init(ULong inConnID, State * inStates)
{
	fProbeTimer.Init(((TAppWorld *) GetGlobals())->GetMyPort(), kADSPEventID, fProbeInterval, kSeconds, kADSPProbeTimer, inConnID);
	fRetryTimer.Init(((TAppWorld *) GetGlobals())->GetMyPort(), kADSPEventID, fRetryInterval, kSeconds, kADSPRetryTimer, inConnID);
	fSendTimer.Init(((TAppWorld *) GetGlobals())->GetMyPort(), kADSPEventID, 0, kSeconds, kADSPSendTimer, inConnID);
	fResetTimer.Init(((TAppWorld *) GetGlobals())->GetMyPort(), kADSPEventID, 0, kSeconds, kADSPResetTimer, inConnID);
	fConnID = inConnID;
	fStates = inStates;
	fEventMessage.Init(true);
	fEvent.fAEventClass = kADSPEventClass;
	fEvent.fAEventID = kADSPEventID;
	fEvent.fRefCon = fRefCon;
	fEvent.fResult = 0;
	fEvent.fType = kADSPUpdateEvent;
	fEvent.fConnID = fConnID;
}


void
TADSPConnection::RetryExpired(TTimerMessage * inMessage)
{
	if (!fAwaitingAck)
		return;
	fRetryTimer.Reset(fRetryInterval, kSeconds);
	fAwaitingAck = 0;
	fSendBuffer->Retransmit();
	fUnackedSends = 0;
	fWindowDoubling = 1;
	fRetransmitted = 1;
	if ((fSendWindow >>= 1) == 0)
		fSendWindow = 1;
	UpdateRetryIntervalAfterTimeout();
	CheckSend();
}


void
TADSPConnection::UpdateRetryIntervalAfterTimeout(void)
{
	fRetryInterval = 5;
}


void
TADSPConnection::ResetExpired(TTimerMessage * inMessage)
{
	fSendCtl |= kSendForwardReset;
	CheckSend();
}


/*------------------------------------------------------------------------------
	Sending.
------------------------------------------------------------------------------*/

void
TADSPConnection::CheckSend(void)
{
	fSendPending = 0;
	if (fState == 5)
		return;
	if (fSendCtl != 0)
		SendControl();
	while (CheckSendData())
		;
	if (fAckPending)
		SendDataAck();
}


void
TADSPConnection::SendControl(void)
{
	ADSPHeader			header;
	ADSPOpenConnInfo	info;
	ULong				sent = 0;
	TWriteChain			chain;
	TWriteElement		infoElement;

	header.descriptor = 0;
	if (fSendCtl & (kSendOpenReq | kSendOpenReqAck | kSendOpenAck | kSendOpenDeny))
	{
		info.version = 0x0100;
		info.destConnID = fRemoteConnID;
		info.attnSendSeq = 0;
		if (fSendCtl & kSendOpenReq)
		{
			sent = kSendOpenReq;
			header.descriptor = kADSPControl | kADSPOpenConnReq;
		}
		else if (fSendCtl & kSendOpenReqAck)
		{
			sent = kSendOpenReqAck;
			header.descriptor = kADSPControl | kADSPOpenConnAck;
		}
		else if (fSendCtl & kSendOpenAck)
		{
			sent = kSendOpenAck;
			header.descriptor = kADSPControl | kADSPOpenConnReqAck;
		}
		else
		{
			header.srcConnID = 0;
			sent = kSendOpenDeny;
			header.descriptor = kADSPControl | kADSPOpenConnDeny;
		}
		if (fSendCtl & (kSendOpenReq | kSendOpenAck))
			fProbeTimer.Reset(fOpenInterval, kSeconds);
		infoElement.Init(&info, sizeof(ADSPOpenConnInfo), 2);
		chain.Add(&infoElement);
	}
	else if (fSendCtl & kSendCloseAdvice)
	{
		fState = 5;
		sent = kSendCloseAdvice;
		header.descriptor = kADSPControl | kADSPCloseAdvice;
	}
	else if (fSendCtl & kSendProbe)
	{
		sent = kSendProbe;
		header.descriptor = kADSPControl | kADSPAckRequest | kADSPProbeOrAck;
	}
	else if (fSendCtl & kSendForwardReset)
	{
		sent = kSendForwardReset;
		header.descriptor = kADSPControl | kADSPForwardReset;
		fResetTimer.Reset(fRetryInterval, kSeconds);
	}
	else if (fSendCtl & kSendForwardResetAck)
	{
		sent = kSendForwardResetAck;
		header.descriptor = kADSPControl | kADSPForwardResetAck;
	}
	else if (fSendCtl & kSendRetransmitAdvice)
	{
		sent = kSendRetransmitAdvice;
		header.descriptor = kADSPControl | kADSPRetransmitAdvice;
	}
	fSendCtl &= ~sent;
	PrepHeader(&header);
	{
		TWriteElement headerElement;
		headerElement.Init(&header, 13, 2);
		chain.Add(&headerElement);
		WriteSocket(&fRemoteAddress, &chain, 7);
	}
}


void
TADSPConnection::SendDataAck(void)
{
	if (fRecvBuffer->RecvWdw() > 0)
	{
		fAckPending = 0;
		ADSPHeader		header;
		TWriteChain		chain;
		TWriteElement	element;
		header.descriptor = kADSPControl | kADSPProbeOrAck;
		PrepHeader(&header);
		element.Init(&header, 13, 2);
		chain.Add(&element);
		WriteSocket(&fRemoteAddress, &chain, 7);
	}
}


#if 0
/* Not yet: identical but for seven words, two register choices (0x205D60:
   the ROM builds firstByteSeqHi in r2, here r1; 0x205DB4: the ROM keeps
   ++fUnackedSends in r1 and fSendWindow in r2, here the other way round).
   Until the form is found, it stays generated assembler. */
Boolean
TADSPConnection::CheckSendData(void)
{
	Boolean more = false;
	if (fState == 3 || fState == 4)
	{
		int eom;
		long count;
		count = fSendBuffer->DataCount(&eom);
		if (count > 0
		 && fSendBuffer->fSendWindowSeq > fSendBuffer->fSendSeq
		 && fUnackedSends < fSendWindow
		 && !fAwaitingAck)
		{
			if (fFlushPending || eom != 0 || count >= 16)
			{
				ULong			seq;
				UChar			data[572];
				ADSPHeader		header;
				TWriteChain		chain;
				long result = fSendBuffer->Getn(data, &count, &seq);
				if (result == -3)
				{
					fSendBlocked = 1;
					fWindowDoubling = 1;
				}
				else if (result == -4)
					fSendBlocked = 1;
				fAckPending = 0;
				if (!fSendBlocked && result != -4)
					more = true;
				header.srcConnID = fConnID;
				header.firstByteSeqHi = seq >> 16;
				header.firstByteSeqLo = seq;
				if (eom)
					header.descriptor = kADSPEOM;
				else
					header.descriptor = 0;
				if (fSendBlocked || ++fUnackedSends >= fSendWindow)
				{
					fSendBlocked = 0;
					header.descriptor |= kADSPAckRequest;
					fAwaitingAck = 1;
					fSendTime = GetGlobalTime();
					fAckSeq = fSendBuffer->fSendSeq;
					fRetryTimer.Start();
				}
				header.nextRecvSeqHi = fRecvBuffer->fRecvSeq >> 16;
				header.nextRecvSeqLo = fRecvBuffer->fRecvSeq;
				header.recvWindow = fRecvBuffer->RecvWdw();
				{
					TWriteElement dataElement;
					dataElement.Init(data, count, 2);
					chain.Add(&dataElement);
					{
						TWriteElement headerElement;
						headerElement.Init(&header, 13, 2);
						chain.Add(&headerElement);
						WriteSocket(&fRemoteAddress, &chain, 7);
					}
				}
			}
			else
				fSendTimer.Start();
		}
	}
	return more;
}
#endif


long
TADSPConnection::UpdateConnection(TUMsgToken * inToken, TAppleTalkMessage * inMessage)
{
	CheckSend();
	return 0;
}


long
TADSPConnection::NotifyListener(ADSPHeader * inHeader, TAddress * inAddress, ADSPOpenConnInfo * inInfo)
{
	return 0;
}


void
TADSPConnection::ForwdReset(ADSPHeader * inHeader)
{
	ProcessAck(inHeader);
}


void
TADSPConnection::OpenComplete(void)
{
	TADSPOpenEvent event;
	event.fAEventID = kADSPEventID;
	event.fType = kADSPOpenEvent;
	event.fRefCon = fRefCon;
	event.fResult = 0;
	event.fConnID = fConnID;
	event.fRemoteConnID = fRemoteConnID;
	event.fRemoteAddress = fRemoteAddress;
	event.fSendSeq = fSendBuffer->fSendSeq;
	event.f2C = 0;
	fToken.ReplyRPC(&event, sizeof(TADSPOpenEvent), 0);
}


void
TADSPConnection::ForwdResetAck(ADSPHeader * inHeader)
{
	ProcessAck(inHeader);
}


void
TADSPConnection::Abort(long inResult)
{ }


void
TADSPConnection::NotifyUser(void)
{ }


void
TADSPConnection::PrepHeader(ADSPHeader * ioHeader)
{
	ioHeader->srcConnID = fConnID;
	ULong sendSeq = fSendBuffer->fSendSeq;
	ioHeader->firstByteSeqHi = sendSeq >> 16;
	ioHeader->firstByteSeqLo = sendSeq;
	ULong recvSeq = fRecvBuffer->fRecvSeq;
	ioHeader->nextRecvSeqHi = recvSeq >> 16;
	ioHeader->nextRecvSeqLo = recvSeq;
	ioHeader->recvWindow = fRecvBuffer->RecvWdw();
}


void
TADSPConnection::ProcessAck(ADSPHeader * inHeader)
{
	ULong seq = (inHeader->nextRecvSeqHi << 16) | inHeader->nextRecvSeqLo;
	if (fAwaitingAck && fAckSeq <= seq)
	{
		fAwaitingAck = 0;
		fUnackedSends = 0;
		fRetryTimer.Stop();
		if (fRetransmitted)
			fRetransmitted = 0;
		else
		{
			UpdateRetryIntervalAfterAck();
			if (!fWindowDoubling && fSendWindow < 50)
				fSendWindow++;
			fWindowDoubling = 0;
		}
	}
	long result = fSendBuffer->Ack(seq, inHeader->recvWindow);
	if (inHeader->descriptor & kADSPAckRequest)
	{
		fSendPending = 1;
		fAckPending = 1;
	}
	if (result == -2)
	{
		fSendPending = 1;
		fClientUpdate = 1;
	}
}


void
TADSPConnection::UpdateRetryIntervalAfterAck(void)
{
	fRetryInterval = 5;
}


/*------------------------------------------------------------------------------
	Addresses.
------------------------------------------------------------------------------*/

#if 0
/* Not yet: the ROM tests the node with a copy (teq r2, #0; movne r3, r2;
   ldrneb r2, ...; teqne r3, r2), one instruction more than any form tried
   here. Until the form is found, it stays generated assembler. */
Boolean
TADSPConnection::MatchFilterAddress(TAddress * inAddress)
{
	TAddress * filter = &fFilterAddress;
	if ((filter->fNode == 0 || filter->fNode == inAddress->fNode)
	 && (filter->fSocket == 0 || filter->fSocket == inAddress->fSocket)
	 && (filter->fNet == 0 || inAddress->fNet == 0 || filter->fNet == inAddress->fNet))
		return true;
	return false;
}
#endif


#if 0
/* Not yet: identical but for two words (0x2061E4: the ROM loads the
   remote node into r2 and the packet's into r3; written either way round,
   the node or the socket comes out swapped). Until the form is found, it
   stays generated assembler. */
Boolean
TADSPConnection::MatchAddress(TAddress * inAddress)
{
	TAddress * remote = &fRemoteAddress;
	if (inAddress->fNode == remote->fNode && remote->fSocket == inAddress->fSocket
	 && (remote->fNet == 0 || inAddress->fNet == 0 || remote->fNet == inAddress->fNet))
		return true;
	return false;
}
#endif


/*------------------------------------------------------------------------------
	Closing.
------------------------------------------------------------------------------*/

void
TADSPConnection::DoClose(long inResult)
{
	if (inResult == -2)
	{
		TATAsyncMsg * msg = ((TAppleTalkWorld *) GetGlobals())->NewMessage();
		TADSPEvent * event = (TADSPEvent *) msg->fData;
		event->fType = kADSPCloseEvent;
		event->fResult = 0;
		event->fAEventClass = kADSPEventClass;
		event->fAEventID = kADSPEventID;
		event->fRefCon = fRefCon;
		event->fConnID = fConnID;
		msg->Send(&fClientPort, sizeof(TADSPEvent), 0, 0);
	}
	else
	{
		TADSPOpenEvent event;
		event.fType = kADSPOpenEvent;
		event.fResult = inResult;
		event.fAEventID = kADSPEventID;
		event.fRefCon = fRefCon;
		fToken.ReplyRPC(&event, sizeof(TADSPOpenEvent), inResult);
	}
}


void
TADSPConnection::DoCloseAdvice(ADSPHeader * inHeader)
{
	ProcessAck(inHeader);
	UChar openState = fOpenState;
	fState = 5;
	Abort(-2);
	NotifyUser();
	if (openState == 4)
		DoClose(-2);
}


void
TADSPConnection::ResetTrans(ADSPHeader * inHeader)
{
	if (inHeader)
		ProcessAck(inHeader);
	fRetryTimer.Stop();
	fSendBuffer->Retransmit();
	fUnackedSends = 0;
	fAwaitingAck = 0;
	fSendPending = 1;
}


/*------------------------------------------------------------------------------
	Do what a matched State says.
------------------------------------------------------------------------------*/

long
TADSPConnection::ExecuteState(State * inState, TAddress * inAddress, ADSPHeader * inHeader, ADSPOpenConnInfo * inInfo)
{
	long result = 0;
	ULong seq = (inHeader->nextRecvSeqHi << 16) | inHeader->nextRecvSeqLo;
	fOpenState = inState->fNewOpenState;
	fState = inState->fNewState;
	if (inState->fActions & kActionSetRemote)
	{
		TADSPSendBuffer * buffer = fSendBuffer;
		ULong window = inHeader->recvWindow;
		buffer->f54 = seq;
		buffer->f4C = seq;
		buffer->fSendSeq = seq;
		buffer->fSendWindowSeq = seq + window;
		fRemoteConnID = inHeader->srcConnID;
		fRemoteAddress = *inAddress;
	}
	if (inState->fActions & kActionClose)
		DoClose(-3);
	if (inState->fActions & kActionResetTrans)
		ResetTrans(NULL);
	if (inState->fSend)
	{
		fSendCtl |= inState->fSend;
		fSendPending = 1;
	}
	if (inState->fActions & kActionOpened)
	{
		if (fOpenState == 4)
			fProbeTimer.Stop();
		OpenComplete();
		if (fOpenState == 4)
			fProbeTimer.Reset();
	}
	return result;
}
