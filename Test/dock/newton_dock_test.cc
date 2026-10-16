/*
	File:		newton_dock_test.cc

	Contains:	A test of the newton_dock library (the wire to a Newton, for
					NCX): MNP's CRC and framing, the Dock's DES, and two MNP links
					talking over TCP on this machine, one playing the Newton (as
					Einstein does) and one the desktop: link up, data both ways in
					many frames, disconnect. On Windows the link isn't there yet
					(NCX 3.1): it says so, and the rest is tested.
					Built as C++17, as NCX is. Prints "ok <what>" per check.

	Usage:	newton_dock_test
*/

#include "Matt/MNP.h"
#include "Matt/NTKTransport.h"
#include "Communications/DES.h"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#if !defined(_WIN32)
#include <unistd.h>
#endif

static int gFailed = 0;

static void
Check(bool inOK, const char * inWhat)
{
	printf("%s %s\n", inOK ? "ok     " : "FAIL   ", inWhat);
	if (!inOK)
		++gFailed;
}


/*------------------------------------------------------------------------------
	CRC and framing.
------------------------------------------------------------------------------*/

static void
TestFraming(void)
{
	// CRC-16/ARC's check value
	Check(MNPCrc((const uint8_t *)"123456789", 9) == 0xBB3D, "MNPCrc (CRC-16/ARC of \"123456789\" is BB3D)");

	// an LT with DLE (0x10) in its data: doubled on the wire, single again decoded
	std::vector<uint8_t> header = { 2, kMNPLinkTransfer, 7 };
	std::vector<uint8_t> data = { 'N', 0x10, 'C', 0x10, 0x10, 'X', 0x02, 0x03 };
	std::vector<uint8_t> wire = MNPEncodeFrame(header, data.data(), data.size());
	Check(wire.size() >= 3 && wire[0] == 0x16 && wire[1] == 0x10 && wire[2] == 0x02, "MNPEncodeFrame: SYN DLE STX");
	size_t dles = 0;
	for (size_t i = 3; i + 4 < wire.size(); ++i)
		dles += (wire[i] == 0x10);
	Check(dles == 6, "MNPEncodeFrame: DLE doubled in the data");

	MNPDecoder decoder;
	std::vector<uint8_t> expected(header);
	expected.insert(expected.end(), data.begin(), data.end());
	int frames = 0;
	bool same = false;
	for (int pass = 0; pass < 2; ++pass)		// twice: the decoder starts over by itself
		for (uint8_t b : wire)
			if (decoder.feed(b) == MNPDecoder::kFrame) {
				++frames;
				same = decoder.frame() == expected;
			}
	Check(frames == 2 && same, "MNPDecoder: the frame back, twice in a row");

	std::vector<uint8_t> bad(wire);
	bad[5] ^= 0x01;									// a bit flipped in the data
	decoder.reset();
	MNPDecoder::Result result = MNPDecoder::kMore;
	for (uint8_t b : bad)
		if ((result = decoder.feed(b)) != MNPDecoder::kMore)
			break;
	Check(result == MNPDecoder::kBadFrame, "MNPDecoder: a CRC error");
}


/*------------------------------------------------------------------------------
	DES.
------------------------------------------------------------------------------*/

static void
TestDES(void)
{
	// The textbook vector: key 133457799BBCDFF1, 0123456789ABCDEF -> 85E813540F0AB405.
	// The Newton's key schedule takes the key shifted right by a bit (its
	// keys come from DESCharToKey that way).
	SNewtNonce key = { 0x13345779u >> 1, 0x9BBCDFF1u >> 1 };
	SNewtNonce keys[16];
	DESKeySched(&key, keys);
	SNewtNonce data = { 0x01234567, 0x89ABCDEF };
	SNewtNonce * p = &data;
	DESEncode(keys, sizeof(SNewtNonce), &p);
	Check(data.hi == 0x85E81354 && data.lo == 0x0F0AB405, "DESEncode (the textbook vector)");
	p = &data;
	DESDecode(keys, sizeof(SNewtNonce), &p);
	Check(data.hi == 0x01234567 && data.lo == 0x89ABCDEF, "DESDecode");

	// CBC: three blocks there and back (the vector, ^ each block before it)
	SNewtNonce blocks[3] = { { 1, 2 }, { 3, 4 }, { 5, 6 } }, iv = { 0xCAFEBABE, 0xDEADBEEF }, chain = iv;
	p = blocks;
	DESCBCEncode(keys, sizeof(blocks), &p, &chain);
	bool changed = blocks[0].hi != 1 && blocks[2].lo != 6;
	chain = iv;
	p = blocks;
	DESCBCDecode(keys, sizeof(blocks), &p, &chain);
	Check(changed && blocks[0].hi == 1 && blocks[0].lo == 2 && blocks[1].hi == 3 && blocks[1].lo == 4
		&& blocks[2].hi == 5 && blocks[2].lo == 6 && p == blocks + 3, "DESCBCEncode, DESCBCDecode (3 blocks)");

	// The empty password's key (the Dock's default key, E40F7E9F0A362CFA,
	// shifted by a bit) and a password's answer to a nonce: golden values,
	// from macOS; DESCharToKey depends on byte order.
	UniChar empty[1] = { 0 };
	DESCharToKey(empty, &key);
	Check(key.hi == 0xF207BF4F && key.lo == 0x851B167D, "DESCharToKey (no password)");
	UniChar secret[] = { 's', 'e', 'c', 'r', 'e', 't', 0 };
	DESCharToKey(secret, &key);
	SNewtNonce nonce = { 0x01234567, 0x89ABCDEF };
	DESEncodeNonce(&key, &nonce);
	Check(key.hi == 0xCDFBC154 && key.lo == 0x1567C58D && nonce.hi == 0xA559988D && nonce.lo == 0xB3D4D8C2,
			"DESCharToKey, DESEncodeNonce (\"secret\")");
	DESDecodeNonce(&key, &nonce);
	Check(nonce.hi == 0x01234567 && nonce.lo == 0x89ABCDEF, "DESDecodeNonce");
}


/*------------------------------------------------------------------------------
	Two links over TCP.
------------------------------------------------------------------------------*/

struct Side
{
	MNPLink					link;
	bool						up = false;
	bool						down = false;
	std::vector<uint8_t>	received;
};

// Take the events of both sides until inDone() or inSeconds pass.
template <typename Done>
static bool
Run(Side & inA, Side & inB, double inSeconds, Done inDone)
{
	auto end = std::chrono::steady_clock::now() + std::chrono::duration<double>(inSeconds);
	while (!inDone())
	{
		if (std::chrono::steady_clock::now() > end)
			return false;
		int fds[2] = { inA.link.notifyFd(), inB.link.notifyFd() };
		bool ready[2] = { false, false };
		NTKWaitReadable(fds, 2, 100, ready);
		for (Side * side : { &inA, &inB })
		{
			MNPLink::Event event;
			while (side->link.nextEvent(event))
			{
				if (event.kind == MNPLink::Event::kLinkUp)
					side->up = true;
				else if (event.kind == MNPLink::Event::kLinkDown)
					side->down = true;
				else if (event.kind == MNPLink::Event::kData)
					side->received.insert(side->received.end(), event.data.begin(), event.data.end());
			}
		}
	}
	return true;
}

static std::vector<uint8_t>
Pattern(size_t inSize, unsigned inSeed)
{
	std::vector<uint8_t> bytes(inSize);
	for (size_t i = 0; i < inSize; ++i)
		bytes[i] = (uint8_t)((i * 131 + inSeed) ^ (i >> 8));	// DLEs, SYNs, ETXs among them
	return bytes;
}

static void
TestLink(void)
{
	Side desktop, newton;
	std::string error;
#if defined(_WIN32)
	Check(!desktop.link.start("tcp:3679", error) && !error.empty(), "MNPLink: not on Windows yet, and says so");
	return;
#else
	std::string port = std::to_string(20000 + getpid() % 20000);
	bool started = desktop.link.start("tcp:" + port, error);
	Check(started, ("MNPLink: the desktop listens (tcp:" + port + ") " + error).c_str());
	newton.link.setRole(MNPLink::kNewton);
	started = started && newton.link.start("tcp-client:" + port, error);
	Check(started, ("MNPLink: the Newton connects (tcp-client:" + port + ") " + error).c_str());
	if (!started)
		return;

	bool up = Run(desktop, newton, 15, [&] { return desktop.up && newton.up; });
	Check(up, "MNPLink: the link comes up (LR, LA)");
	if (!up)
		return;

	// more than one LT each way (up to 256 bytes each), both at once
	std::vector<uint8_t> toNewton = Pattern(5000, 1), toDesktop = Pattern(3000, 2);
	desktop.link.send(toNewton);
	newton.link.send(toDesktop);
	bool arrived = Run(desktop, newton, 30, [&] {
		return newton.received.size() >= toNewton.size() && desktop.received.size() >= toDesktop.size(); });
	Check(arrived && newton.received == toNewton, "MNPLink: 5000 bytes to the Newton, in order");
	Check(arrived && desktop.received == toDesktop, "MNPLink: 3000 bytes to the desktop, in order");
	// the last LAs may still be on their way when the data is in
	bool acked = Run(desktop, newton, 10, [&] { return desktop.link.pending() == 0 && newton.link.pending() == 0; });
	Check(acked, "MNPLink: all acknowledged");

	newton.link.disconnect();
	bool down = Run(desktop, newton, 15, [&] { return desktop.down; });
	Check(down, "MNPLink: the desktop sees the Newton's LD");

	newton.link.stop();
	desktop.link.stop();
	Check(true, "MNPLink: both stop");
#endif
}


int
main(int argc, char * argv[])
{
	setvbuf(stdout, NULL, _IONBF, 0);	// a crash keeps what was printed
	TestFraming();
	TestDES();
	TestLink();
	printf("%s\n", gFailed ? "FAILED" : "all ok");
	return gFailed ? 1 : 0;
}
