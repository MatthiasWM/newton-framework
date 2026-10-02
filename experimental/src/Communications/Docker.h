/*
	File:		Docker.h

	Contains:	The docking protocol's pipes (reconstructed: not in the
				published headers; the port's CEzPipeProtocol and
				CEzEndpointPipe, Apple's names).
*/

#ifndef __DOCKER_H
#define __DOCKER_H

#ifndef __PIPES_H
#include "Frames/Pipes.h"
#endif

#ifndef __NEWTONEXCEPTIONS_H
#include "NewtonExceptions.h"
#endif

DeclareException(exPipeException, exRootException);

enum ConnectionType
{
	kNoConnection,
	kADSPConnection,
	kMNPSerialConnection,
	kSerialConnection
};

/* An endpoint pipe made easy (TEndpointPipe, CBufferPipe in between) */
class TEzEndpointPipe : public CPipe
{
public:
				TEzEndpointPipe();

	long		Init(ConnectionType inType, char ** inOptions, ULong inTimeout);
	long		TearDown(void);
};

class TEzPipeProtocol
{
public:
	void		ProtocolInit(ULong inEventClass, ULong inEventId);
	void		SendDockerHeader(ULong inCommand, UChar inDone);

protected:
	TEzEndpointPipe *	fPipe;		// +00
	ULong				fEvtClass;	// +04
	ULong				fEvtId;		// +08
};

#endif	/* __DOCKER_H */
