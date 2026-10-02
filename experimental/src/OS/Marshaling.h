/*
	File:		Marshaling.h

	Contains:	NewtonScript arguments to C parameter blocks (reconstructed:
				not in the published headers; the functions are in the port's
				OS/Marshaling.cc).
*/

#ifndef __MARSHALING_H
#define __MARSHALING_H

#ifndef __OBJECTS_H
#include "Frames/objects.h"
#endif

NewtonErr	MarshalArgumentSize(RefArg inArgs, RefArg inTypes, ULong * outSize, int inCount);
NewtonErr	MarshalArguments(RefArg inArgs, RefArg inTypes, void ** outBlock, int inCount);
NewtonErr	MarshalArguments(RefArg inArgs, RefArg inTypes, void * ioBlock, ULong inSize, int inCount);

#endif	/* __MARSHALING_H */
