/*
	File:		NewtcStubs.cc

	Contains:	What ResMaker needs of newtc.cc to link newtc's sources without it.
					ResMaker never calls these.
*/

#include "Objects.h"
#include <string>

std::string currentFileName = "<undefined>";	// Matt/Decompiler.cc

bool
installPackage(RefArg package, bool inOpenApp, RefVar * outParts)	// Matt/NTKDevice.cc
{
	return false;
}
