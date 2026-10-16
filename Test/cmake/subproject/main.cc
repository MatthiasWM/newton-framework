// A program that uses newton_ncx as NCX will (C++17, the library's include
// directories and definitions from its target): initialize the object
// system, round-trip a frame through NSOF.
#include "Objects.h"
#include "Pipes.h"
#include <cstdio>
#include <vector>

extern "C" void	InitObjectSystemCore(void);
void	FlattenRef(RefArg inRef, CPipe & inPipe);
long	FlattenRefSize(RefArg inRef);
Ref	UnflattenRef(CPipe & inPipe);

int
main()
{
	InitObjectSystemCore();
	RefVar frame(AllocateFrame());
	SetFrameSlot(frame, MakeSymbol("answer"), MAKEINT(42));
	long size = FlattenRefSize(frame);
	std::vector<char> buf(size);
	CPtrPipe out;
	out.init(buf.data(), size, false, NULL);
	FlattenRef(frame, out);
	CPtrPipe in;
	in.init(buf.data(), size, false, NULL);
	RefVar back(UnflattenRef(in));
	bool ok = RINT(GetFrameSlot(back, MakeSymbol("answer"))) == 42;
	printf("round trip %s\n", ok ? "ok" : "FAILED");
	return ok ? 0 : 1;
}
