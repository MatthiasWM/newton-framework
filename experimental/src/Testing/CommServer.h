/*
	File:		CommServer.h

	Contains:	TCommServer, the test agent's connection to a test server on
				the desktop (reconstructed: not in the published headers; the
				port's CCommServer, Apple's names).
*/

#ifndef __COMMSERVER_H
#define __COMMSERVER_H

#ifndef __DOCKER_H
#include "Communications/Docker.h"
#endif

#define kTestServerEventId		'tsvr'
#define kTestAgentEventId		'tagt'

#define kStreamAlignment		4

class TCommServer : public TEzPipeProtocol
{
public:
				TCommServer(UChar inArg1, UChar inArg2, UChar * inType);
				~TCommServer();

	void		SetTestServerName(char * inEntity, char * inZone);
	long		ConnectToTestServer(TEzEndpointPipe ** outPipe);
	long		DisconnectFromTestServer(void);

	void		SetBusy(UChar inBusy);
	UChar		IsBusy(void);

	ULong		GetResponse(long * outLength);
	Boolean		ReadChunk(void * outBuf, long inLength, UChar inDone);
	long		ReadString(char * outStr, ULong inLength, UChar inDone);
	void		FlushPadding(ULong inLength);

	void		SendCommandHeader(ULong inCommand, UChar inDone);
	void		SendChunk(void * inBuf, long inLength, UChar inDone);
	void		Pad(ULong inLength);

	void		TestPipeExceptionHandler(long inErr);

private:
	UChar		f0C;					// +0C
	UChar		f0D;					// +0D
	long		fEvtLength;				// +10
	UniChar		fATType[32+1];			// +14
	UniChar		fATEntity[32+1];		// +56
	UniChar		fATZone[32+1];			// +98
	long		fError;					// +DC
	UChar		fIsBusy;				// +E0
};

#endif	/* __COMMSERVER_H */
