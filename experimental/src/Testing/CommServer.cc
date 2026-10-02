/*
	File:		CommServer.cc

	Contains:	TCommServer, the test agent's connection to a test server on
				the desktop: connecting through an endpoint pipe, reading and
				writing chunks padded to 4 bytes.

	ROM:		0x209654 (TCommServer::TCommServer) .. 0x209E84 (after
				SendChunk), MP2x00 US 2.1 (717006).
*/

#include "Testing/CommServer.h"
#include "Utilities/Unicode.h"
#include "NewtonMemory.h"
#include "NewtErrors.h"


TCommServer::TCommServer(UChar inArg1, UChar inArg2, UChar * inType)
{
	f0C = inArg2;
	f0D = inArg1;
	Ustrcpy(fATType, (UniChar *) inType);
	fEvtLength = 0;
	fIsBusy = false;
}


TCommServer::~TCommServer()
{ }


/*------------------------------------------------------------------------------
	Writing: pad to the stream's alignment.
------------------------------------------------------------------------------*/

void
TCommServer::Pad(ULong inLength)
{
	ULong delta = inLength & (kStreamAlignment - 1);
	if (delta > 0)
	{
		ULong length = kStreamAlignment - delta;
		ULong padding = 0;
		newton_try
		{
			fPipe->WriteChunk(&padding, length, false);
		}
		newton_catch(exPipeException)
		{
			TestPipeExceptionHandler((long) CurrentException()->data);
		}
		end_try;
	}
}


/*------------------------------------------------------------------------------
	Reading.
------------------------------------------------------------------------------*/

ULong
TCommServer::GetResponse(long * outLength)
{
	ULong		header[4];
	long		length = 2 * sizeof(ULong);
	Boolean		isEOF;

	newton_try
	{
		fPipe->ReadChunk(&header[0], length, isEOF);
	}
	newton_catch(exPipeException)
	{
		TestPipeExceptionHandler((long) CurrentException()->data);
	}
	end_try;

	if (header[0] != 'newt' || header[1] != kTestServerEventId)
		return 0;
	ReadChunk(&header[2], 2 * sizeof(ULong), false);
	fEvtLength = header[3];
	if (outLength)
		*outLength = fEvtLength;
	return header[2];
}


Boolean
TCommServer::ReadChunk(void * outBuf, long inLength, UChar inDone)
{
	Boolean		isEOF;
	long		length = inLength;

	newton_try
	{
		fPipe->ReadChunk(outBuf, length, isEOF);
	}
	newton_catch(exPipeException)
	{
		TestPipeExceptionHandler((long) CurrentException()->data);
	}
	end_try;

	if (inDone)
		FlushPadding(inLength);
	return isEOF;
}


long
TCommServer::ReadString(char * outStr, ULong inLength, UChar inDone)
{
	long		err = noErr;
	ULong		i = 0;
	Boolean		isEOS = false;

	for ( ; i < inLength && !isEOS && err == noErr; i++)
	{
		Boolean isEOF = ReadChunk(outStr + i, 1, false);
		if (outStr[i] == 0)
			isEOS = true;
		if (isEOF)
			err = -2;
		else if (i == inLength - 1)
			err = -3;
		if (err)
			outStr[i] = 0;
	}
	if (inDone)
		FlushPadding(i);
	return err;
}


void
TCommServer::FlushPadding(ULong inLength)
{
	long length = inLength & (kStreamAlignment - 1);
	if (length > 0)
	{
		char		padding[kStreamAlignment];
		Boolean		isEOF;
		length = kStreamAlignment - length;
		newton_try
		{
			fPipe->ReadChunk(padding, length, isEOF);
		}
		newton_catch(exPipeException)
		{
			TestPipeExceptionHandler((long) CurrentException()->data);
		}
		end_try;
	}
}


void
TCommServer::TestPipeExceptionHandler(long inErr)
{
	fError = inErr;
}


void
TCommServer::SetBusy(UChar inBusy)
{
	fIsBusy = inBusy;
}


UChar
TCommServer::IsBusy(void)
{
	return fIsBusy;
}


/*------------------------------------------------------------------------------
	Connecting.
------------------------------------------------------------------------------*/

void
TCommServer::SetTestServerName(char * inEntity, char * inZone)
{
	if (inEntity)
		Ustrcpy(fATEntity, (UniChar *) inEntity);
	if (inZone)
		Ustrcpy(fATZone, (UniChar *) inZone);
	else
		Ustrcpy(fATZone, (UniChar *) "\0*\0\0");
}


#if 0
/* Not yet: identical but for four words (0x209BC4: for the '@' the ROM
   keeps the address in r1 and the character in r0; for the ':' before it,
   as here, the other way round). Until the form is found, it stays
   generated assembler. */
long
TCommServer::ConnectToTestServer(TEzEndpointPipe ** outPipe)
{
	UniChar			addrStr[96];
	ConnectionType	connType = kSerialConnection;
	char **			options;

	*outPipe = NULL;
	if (fATEntity[0] == 0)
		return -1;
	fPipe = new TEzEndpointPipe;
	if (fPipe == NULL)
		return -2;
	if (fATEntity[0] == '*' && fATEntity[2] == 0)
	{
		if (fATEntity[1] >= '0' && fATEntity[1] < '9')
			connType = (ConnectionType) (fATEntity[1] - '0');
		options = NewHandle(4 * sizeof(UniChar));
		*(UniChar *) *options = 0;		// NewHandle not checked (BUGS.md B8)
	}
	else
	{
		connType = kADSPConnection;
		for (ULong i = 0; i < 96; i++)
			addrStr[i] = 0;
		Ustrcpy(addrStr, fATEntity);
		addrStr[Ustrlen(addrStr)] = ':';
		Ustrcat(addrStr, fATType);
		addrStr[Ustrlen(addrStr)] = '@';
		Ustrcat(addrStr, fATZone);
		if ((options = NewHandle(sizeof(addrStr))) == NULL)
			return -3;
		Ustrcpy((UniChar *) *options, addrStr);
	}
	newton_try
	{
		fPipe->Init(connType, options, 0x0D2F0000);
	}
	newton_catch(exPipeException)
	{
		TestPipeExceptionHandler((long) CurrentException()->data);
	}
	end_try;

	ProtocolInit('newt', kTestAgentEventId);
	*outPipe = fPipe;
	return noErr;
}
#endif


long
TCommServer::DisconnectFromTestServer(void)
{
	long err;
	if (fPipe == NULL)
		return -1;
	newton_try
	{
		err = fPipe->TearDown();
	}
	newton_catch(exPipeException)
	{
		TestPipeExceptionHandler((long) CurrentException()->data);
	}
	end_try;

	delete fPipe;
	fPipe = NULL;
	return err;
}


/*------------------------------------------------------------------------------
	Writing.
------------------------------------------------------------------------------*/

void
TCommServer::SendCommandHeader(ULong inCommand, UChar inDone)
{
	SendDockerHeader(inCommand, inDone);
}


void
TCommServer::SendChunk(void * inBuf, long inLength, UChar inDone)
{
	newton_try
	{
		*fPipe << inLength;
		fPipe->WriteChunk(inBuf, inLength, false);
	}
	newton_catch(exPipeException)
	{
		TestPipeExceptionHandler((long) CurrentException()->data);
	}
	end_try;

	Pad(inLength);
	if (inDone)
	{
		newton_try
		{
			fPipe->FlushWrite();
		}
		newton_catch(exPipeException)
		{
			TestPipeExceptionHandler((long) CurrentException()->data);
		}
		end_try;
	}
}
