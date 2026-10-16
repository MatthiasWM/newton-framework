// A program that uses newton_ncx and newton_dock as NCX will (C++17, the
// libraries' include directories and definitions from their targets):
// initialize the object system, round-trip a frame through NSOF, frame a
// message for MNP.
#include "Objects.h"
#include "Pipes.h"
#include "Matt/MNP.h"
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
	bool crc = MNPCrc((const uint8_t *)"123456789", 9) == 0xBB3D;
	printf("MNP %s\n", crc ? "ok" : "FAILED");
	return ok && crc ? 0 : 1;
}
