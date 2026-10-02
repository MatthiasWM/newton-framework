/*
	File:		AppleTalk.h

	Contains:	The AppleTalk stack's common types, as the ADSP connection uses
				them (reconstructed: not in the published headers): addresses,
				the chains of write elements a packet is sent from, the
				message timers, the messages the stack's world passes around.

				Names ours where Apple's table has none (fields, parameters,
				enum values); layouts as the ROM's code uses them, the parts
				no code here touches kept as reserved bytes.
*/

#ifndef __APPLETALK_H
#define __APPLETALK_H

#ifndef __NEWTON_H
#include "Newton.h"
#endif

#ifndef __USERPORTS_H
#include "OS600/UserPorts.h"
#endif

#ifndef __NEWTONTIME_H
#include "NewtonTime.h"
#endif


/*------------------------------------------------------------------------------
	An AppleTalk address: network, node, socket.
------------------------------------------------------------------------------*/

class TAddress
{
public:
					TAddress(UChar inType);

	void			operator=(const TAddress & inAddress);
	void			SetAddress(ULong inZone, UShort inNet, UChar inNode, UChar inSocket);
	void			Clear(void)			{ SetAddress(0, 0, 0, 0); fFlags = 0; fZone = 0; }

	UChar			fType;			// +00
	UChar			fFlags;			// +01
	ULong			fZone;			// +04
	UShort			fNet;			// +08
	UChar			fNode;			// +0A
	UChar			fSocket;		// +0B
};


/*------------------------------------------------------------------------------
	A packet is written from a chain of elements, each a piece of memory.
------------------------------------------------------------------------------*/

class TWriteElement
{
public:
					TWriteElement();
	virtual			~TWriteElement();

	void			Init(void * inData, ULong inSize, UChar inFlags);

	UChar			fReserved[20];	// +04
};


class TWriteChain
{
public:
					TWriteChain();
					~TWriteChain()		{ Destroy(); }

	void			Add(TWriteElement * inElement);
	void			Destroy(void);

	UChar			fReserved[12];
};


long			WriteSocket(TAddress * inAddress, TWriteChain * inChain, UChar inType);


/*------------------------------------------------------------------------------
	Memory a received packet is in.
------------------------------------------------------------------------------*/

class TMemoryObject
{
public:
	void *			GetPtr(void);

	UChar			fReserved[40];
	long			fOffset;		// +28 the data's, from GetPtr()
	long			fSize;			// +2C
};


/*------------------------------------------------------------------------------
	Messages.
------------------------------------------------------------------------------*/

class TAppleTalkMessage;
class TTimerMessage;

class TPacketMessage
{
public:
	UChar			fReserved[40];
	TMemoryObject *	fData;			// +28 the packet
};


/* An asynchronous message from the stack's pool (TAppleTalkWorld::NewMessage) */
class TATAsyncMsg
{
public:
	long			Send(TUPort * inPort, ULong inSize, ULong inTimeout, ULong inMsgType);

	UChar			fReserved[128];
	UChar			fData[1];		// +80 the message's content
};


/*------------------------------------------------------------------------------
	A timer that sends a message to a port when it expires.
------------------------------------------------------------------------------*/

enum TimerType
{
	kADSPSendTimer = 8,
	kADSPRetryTimer = 9,
	kADSPProbeTimer = 10,
	kADSPResetTimer = 12
};

class TMessageTimer
{
public:
					TMessageTimer();

	void			Init(TUPort * inPort, ULong inName, ULong inInterval, TimeUnits inUnits, TimerType inType, ULong inRefCon);
	void			Start(void);
	void			Stop(void);
	void			Reset(void);
	void			Reset(ULong inInterval, TimeUnits inUnits);

	TUAsyncMessage	fMessage;		// +00
	UChar			fReserved[44];	// +10
};


/*------------------------------------------------------------------------------
	The tasks' worlds (GetGlobals()).
------------------------------------------------------------------------------*/

class TAppWorld
{
public:
	TUPort *		GetMyPort(void);
};

class TAppleTalkWorld : public TAppWorld
{
public:
	TATAsyncMsg *	NewMessage(void);
};

#endif	/* __APPLETALK_H */
