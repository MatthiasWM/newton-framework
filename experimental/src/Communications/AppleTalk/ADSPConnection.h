/*
	File:		ADSPConnection.h

	Contains:	An ADSP (AppleTalk Data Stream Protocol) connection: the
				connection's state machine, its timers, sending control
				packets and data (reconstructed: not in the published
				headers).

				Names ours where Apple's table has none; layouts as the ROM's
				code uses them. The ARM610 has no halfword loads: a UShort
				field is read from its word and shifted.
*/

#ifndef __ADSPCONNECTION_H
#define __ADSPCONNECTION_H

#ifndef __APPLETALK_H
#include "Communications/AppleTalk/AppleTalk.h"
#endif

#ifndef __AEVENTS_H
#include "UtilityClasses/AEvents.h"
#endif


/*------------------------------------------------------------------------------
	The packets' headers.
------------------------------------------------------------------------------*/

/* The ADSP header (13 bytes on the wire): its 32-bit sequence numbers lie
   on halfword boundaries; the halfwords are bitfields (written as words) */
struct ADSPHeader
{
	unsigned	srcConnID : 16;			// +00
	unsigned	firstByteSeqHi : 16;	// +02
	unsigned	firstByteSeqLo : 16;	// +04
	unsigned	nextRecvSeqHi : 16;		// +06
	unsigned	nextRecvSeqLo : 16;		// +08
	unsigned	recvWindow : 16;		// +0A
	int			descriptor : 8;			// +0C
};

/* descriptor */
#define kADSPControl		0x80
#define kADSPAckRequest		0x40
#define kADSPEOM			0x20
#define kADSPAttention		0x10
#define kADSPControlCode	0x0F

/* control codes */
#define kADSPProbeOrAck		0
#define kADSPOpenConnReq	1
#define kADSPOpenConnAck	2
#define kADSPOpenConnReqAck	3
#define kADSPOpenConnDeny	4
#define kADSPCloseAdvice	5
#define kADSPForwardReset	6
#define kADSPForwardResetAck 7
#define kADSPRetransmitAdvice 8

/* What follows the header in an open connection packet */
struct ADSPOpenConnInfo
{
	unsigned	version : 16;		// +00
	unsigned	destConnID : 16;	// +02
	ULong		attnSendSeq;		// +04
};


/*------------------------------------------------------------------------------
	The state machine: a State for each control code and open state.
------------------------------------------------------------------------------*/

struct State
{
	UChar		fMatch;				// +00 what a packet must match (kMatch...)
	UChar		fActions;			// +01 what to do (kAction...)
	UChar		fSend;				// +02 control packets to send (fSendCtl)
	UChar		fNewOpenState;		// +03
	UChar		fNewState;			// +04
};

/* State.fMatch */
#define kMatchRemoteSocket		0x01
#define kMatchRemoteAddress		0x02
#define kMatchDestConnID		0x04
#define kMatchSrcConnID			0x08
#define kMatchNoDestConnID		0x10
#define kMatchNoSrcConnID		0x20
#define kMatchFilterAddress		0x40
#define kMatchNever				0x80

/* State.fActions */
#define kActionOpened			0x01
#define kActionSetRemote		0x02
#define kActionResetTrans		0x04
#define kActionClose			0x10


/*------------------------------------------------------------------------------
	The events sent to the connection's client.
------------------------------------------------------------------------------*/

#define kADSPEventClass			'newt'
#define kADSPEventID			'adsp'

#define kADSPCloseEvent			0x0801
#define kADSPOpenEvent			0x0802
#define kADSPUpdateEvent		0x0804

class TATEvent : public TAEvent
{
public:
				TATEvent()			{ fAEventClass = kADSPEventClass; fRefCon = 0; fResult = 0; }

	long		fResult;			// +08
	ULong		fRefCon;			// +0C
};

class TADSPEvent : public TATEvent
{
public:
				TADSPEvent()		{ fAEventID = kADSPEventID; fType = kADSPUpdateEvent; }

	ULong		fType;				// +10
	ULong		fConnID;			// +14
};

class TADSPOpenEvent : public TATEvent
{
public:
				TADSPOpenEvent() : fRemoteAddress(1) { }

	ULong		fType;				// +10
	ULong		fConnID;			// +14
	ULong		fRemoteConnID;		// +18
	TAddress	fRemoteAddress;		// +1C
	ULong		fSendSeq;			// +28
	ULong		f2C;				// +2C
};


/*------------------------------------------------------------------------------
	The connection's buffers.
------------------------------------------------------------------------------*/

class TADSPSendBuffer
{
public:
	long		Getn(void * outData, long * ioSize, ULong * outEOM);
	long		DataCount(int * outEOM);
	long		Ack(ULong inSeq, ULong inWindow);
	void		Retransmit(void);

	UChar		fReserved[76];
	ULong		f4C;				// +4C
	ULong		fSendSeq;			// +50 the next byte's
	ULong		f54;				// +54
	ULong		fSendWindowSeq;		// +58 the last byte the remote can take
};

class TADSPRecvBuffer
{
public:
	long		Putn(const void * inData, long inSize, ULong inSeq, int inEOM);
	ULong		RecvWdw(void);

	UChar		fReserved[76];
	ULong		fRecvSeq;			// +4C the next byte expected
};


/*------------------------------------------------------------------------------
	T A D S P C o n n e c t i o n
------------------------------------------------------------------------------*/

/* fSendCtl: control packets to send */
#define kSendProbe				0x0001
#define kSendOpenReq			0x0002
#define kSendOpenReqAck			0x0004
#define kSendOpenAck			0x0008
#define kSendOpenDeny			0x0010
#define kSendCloseAdvice		0x0020
#define kSendForwardReset		0x0040
#define kSendForwardResetAck	0x0080
#define kSendRetransmitAdvice	0x0100

class TADSPConnection
{
public:
				TADSPConnection();
				~TADSPConnection();

	void		Init(ULong inConnID, State * inStates);
	Boolean		Match(ADSPHeader * inHeader, TAddress * inAddress, ADSPOpenConnInfo * inInfo, State ** outState);
	long		ExecuteState(State * inState, TAddress * inAddress, ADSPHeader * inHeader, ADSPOpenConnInfo * inInfo);
	long		Read(TPacketMessage * inMessage, ADSPHeader * inHeader);
	long		ReadAttention(TPacketMessage * inMessage, ADSPHeader * inHeader);
	void		RecvAttnComplete(TATAsyncMsg * inMessage);
	void		UpDateClient(void);
	long		UpdateConnection(TUMsgToken * inToken, TAppleTalkMessage * inMessage);
	long		NotifyListener(ADSPHeader * inHeader, TAddress * inAddress, ADSPOpenConnInfo * inInfo);

	void		AttnExpired(TTimerMessage * inMessage);
	void		ResetProbeTimer(void);
	void		ProbeExpired(TTimerMessage * inMessage);
	void		FlushExpired(TTimerMessage * inMessage);
	void		RetryExpired(TTimerMessage * inMessage);
	void		ResetExpired(TTimerMessage * inMessage);
	void		UpdateRetryIntervalAfterTimeout(void);
	void		UpdateRetryIntervalAfterAck(void);

	void		CheckSend(void);
	void		SendControl(void);
	void		SendDataAck(void);
	Boolean		CheckSendData(void);
	void		PrepHeader(ADSPHeader * ioHeader);
	void		ProcessAck(ADSPHeader * inHeader);

	void		ForwdReset(ADSPHeader * inHeader);
	void		ForwdResetAck(ADSPHeader * inHeader);
	void		ResetTrans(ADSPHeader * inHeader);
	void		OpenComplete(void);
	void		DoClose(long inResult);
	void		DoCloseAdvice(ADSPHeader * inHeader);
	void		Abort(long inResult);
	void		NotifyUser(void);
	Boolean		MatchFilterAddress(TAddress * inAddress);
	Boolean		MatchAddress(TAddress * inAddress);

	ULong				fRefCon;			// +000 the client's, in its events
	TUPort				fClientPort;		// +004
	TUMsgToken			fToken;				// +00C the client's request
	UChar				fState;				// +01C 3 open, 4 closing, 5 closed
	UChar				fOpenState;			// +01D 1..4
	UChar				f1E;				// +01E
	ULong				fSendCtl;			// +020 control packets to send (kSend...)
	UChar				fUnackedSends;		// +024
	UChar				fSendWindow;		// +025 packets sent before an ack is asked for
	ULong				f28;				// +028
	ULong				f2C;				// +02C
	ULong				f30;				// +030
	ULong				fSendTime;			// +034 of the packet that asked for an ack
	ULong				fAckSeq;			// +038 the sequence it is acked with
	ULong				f3C;				// +03C
	ULong				f40;				// +040
	ULong				f44;				// +044
	unsigned			fSendPending : 1;	// +048
	unsigned			fWindowDoubling : 1;
	unsigned			fRetransmitted : 1;
	unsigned			fAckPending : 1;
	unsigned			fSendBlocked : 1;
	unsigned			fAwaitingAck : 1;
	unsigned			fFlushPending : 1;
	unsigned			f48_24 : 1;
	unsigned			fClientUpdate : 1;
	UChar				fOpenRetries;		// +04C
	ULong				fOpenInterval;		// +050 seconds
	UChar				fProbeRetries;		// +054
	ULong				fProbeInterval;		// +058 seconds
	TMessageTimer		fProbeTimer;		// +05C
	UChar				f98;				// +098
	ULong				fRetryInterval;		// +09C seconds
	TMessageTimer		fRetryTimer;		// +0A0
	TMessageTimer		fSendTimer;			// +0DC
	TMessageTimer		fResetTimer;		// +118
	TAddress			fFilterAddress;		// +154 whom a listener takes
	TAddress			fRemoteAddress;		// +160
	ULong				fConnID;			// +16C ours
	ULong				fRemoteConnID;		// +170
	TADSPSendBuffer *	fSendBuffer;		// +174
	long				fAckCountdown;		// +178
	TADSPRecvBuffer *	fRecvBuffer;		// +17C
	TADSPEvent			fEvent;				// +180 to the client
	TUAsyncMessage		fEventMessage;		// +198
	State *				fStates;			// +1A8 [control code - 1][open state - 1]
};

#endif	/* __ADSPCONNECTION_H */
