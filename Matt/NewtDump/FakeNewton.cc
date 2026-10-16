/*
 File: FakeNewton.cc

 newtdump-test-newton: a Newton for testing newtdump without one. It
 connects to newtdump (MNP, the Newton's role, like Einstein's network
 client), docks (the Docking Protocol's handshake with the password check),
 takes the protocol extension ('pext') without running it, and answers the
 extension's requests ('NDmp': hello, map, read, power) itself, from image
 files at physical addresses: the ROM, and per socket the attribute memory,
 the common memory, and the control registers (card detect, Vcc). An
 address outside them is a bus error, as on a Newton. With --drop-after it
 breaks the connection after so many reads and connects again, to test
 newtdump's reconnecting.

 usage: newtdump-test-newton --target tcp-client:<port> [--rom <file>]
        [--attribute<n> <file>] [--common<n> <file>] [--machine <type>]
        [--password <p>] [--drop-after <reads>] [--cancel-after <reads>]
        [--bps <bits/s>] [--delay <ms>]
 */

#include "Matt/NewtDump/DockLink.h"
#include "Matt/NewtDump/NSOF.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

struct Region
{
  uint32_t base;
  std::vector<uint8_t> bytes;
};

struct Fake
{
  std::string target;
  std::string password;
  uint32_t machineType = 0x10003000;          // a MessagePad 2x00
  long dropAfter = 0;
  long cancelAfter = 0;                       // the user taps Disconnect after so many reads
  long bps = 0;
  long delayMs = 0;                           // before each answer (a slow Newton)
  std::vector<Region> regions;
  std::map<int, bool> cards;                  // socket -> a card is in it
  std::vector<std::pair<uint32_t, uint32_t>> maps;   // [base, size]
  int vcc[4] = { 0, 0, 0, 0 };
  long reads = 0;
  bool dropped = false;
};

std::vector<uint8_t> ReadFile(const std::string &inPath)
{
  FILE *f = fopen(inPath.c_str(), "rb");
  if (!f)
    throw std::runtime_error("can't read " + inPath);
  std::vector<uint8_t> bytes;
  uint8_t chunk[65536];
  size_t n;
  while ((n = fread(chunk, 1, sizeof(chunk), f)) > 0)
    bytes.insert(bytes.end(), chunk, chunk + n);
  fclose(f);
  return bytes;
}

// The memory at a physical address: the regions, the control registers of
// sockets with a card; false: a bus error
bool ReadPhysical(Fake &ioFake, uint32_t inAddress, uint32_t inLength, std::vector<uint8_t> &out)
{
  out.clear();
  for (uint32_t a = inAddress; a < inAddress + inLength; a += 4) {
    uint32_t socketBase = a & 0xF0000000;
    int socket = (int)((socketBase - 0x30000000) >> 28);
    if (socketBase >= 0x30000000 && socketBase <= 0x60000000 && (a & 0x0FFFFFFF) >= 0x0C000000) {
      // control registers: 0x1C00 pins (card detect clear if a card), 0x2800 Vcc
      uint32_t reg = a & 0x3FFF, value = 0;
      if (reg == 0x1C00)
        value = ioFake.cards[socket] ? 0x0430 : 0x000C;     // ready, 5 V; or no card
      else if (reg == 0x2800)
        value = ioFake.vcc[socket] > 0 ? 0x1C65 : 0x1C45;
      else if (reg == 0x2C00)
        value = 0x0001;
      for (int shift = 24; shift >= 0; shift -= 8)
        out.push_back((uint8_t)(value >> shift));
      continue;
    }
    bool found = false;
    for (const Region &r : ioFake.regions) {
      if (a >= r.base && a + 4 <= r.base + r.bytes.size()) {
        if (socketBase >= 0x30000000 && socketBase <= 0x60000000 && ioFake.vcc[socket] <= 0)
          return false;                                     // a card without power
        out.insert(out.end(), r.bytes.begin() + (a - r.base), r.bytes.begin() + (a - r.base) + 4);
        found = true;
        break;
      }
    }
    if (!found)
      return false;
  }
  return true;
}

nsof::Ref Error(long inCode, const std::string &inName)
{
  return nsof::Frame({{ "error", nsof::Integer(inCode) }, { "name", nsof::String(inName) }});
}

// The extension's answers (Extension.ns)
nsof::Ref Answer(Fake &ioFake, const nsof::Ref &inRequest)
{
  nsof::Ref op = inRequest ? inRequest->slot("op") : nullptr;
  std::string name = op && op->kind == nsof::Value::kSymbol ? op->text : "";
  auto integer = [&](const char *inSlot) -> int64_t {
    nsof::Ref v = inRequest->slot(inSlot);
    if (!v || !v->isInteger())
      throw std::runtime_error(std::string("no ") + inSlot);
    return v->integer;
  };
  if (name == "hello")
    return nsof::Frame({{ "version", nsof::Integer(1) }, { "maps", nsof::Integer((long)ioFake.maps.size()) }});
  if (name == "map") {
    uint32_t base = (uint32_t)integer("page") * 4096, size = (uint32_t)integer("pages") * 4096;
    for (size_t i = 0; i < ioFake.maps.size(); i++)
      if (ioFake.maps[i].first == base && ioFake.maps[i].second == size)
        return nsof::Frame({{ "map", nsof::Integer((long)i) }});
    ioFake.maps.push_back({ base, size });
    return nsof::Frame({{ "map", nsof::Integer((long)ioFake.maps.size() - 1) }});
  }
  if (name == "read") {
    int64_t map = integer("map");
    if (map < 0 || map >= (int64_t)ioFake.maps.size())
      return Error(-48201, "evt.ex.fr.intrp");
    uint32_t offset = (uint32_t)integer("offset"), length = (uint32_t)integer("length");
    if (offset + length > ioFake.maps[map].second)
      return Error(-10035, "evt.ex.bus");
    std::vector<uint8_t> bytes;
    if (!ReadPhysical(ioFake, ioFake.maps[map].first + offset, length, bytes))
      return Error(-10035, "evt.ex.bus");
    ioFake.reads++;
    bool uniform = true;
    for (uint8_t b : bytes)
      uniform = uniform && b == bytes[0];
    if (uniform && !bytes.empty())
      return nsof::Integer(bytes[0]);
    auto v = std::make_shared<nsof::Value>();
    v->kind = nsof::Value::kBinary;
    v->cls = "data";
    v->bytes = bytes;
    return v;
  }
  if (name == "power") {
    int socket = (int)integer("socket");
    nsof::Ref on = inRequest->slot("on");
    if (on && !on->isNil()) {
      ioFake.vcc[socket & 3]++;
      return nsof::Frame({{ "result", nsof::Integer(1) }});
    }
    ioFake.vcc[socket & 3]--;
    return nsof::Frame({{ "result", nsof::Integer(0) }});
  }
  return Error(-1, "unknown request");
}

DockPacket Expect(DockLink &ioLink, uint32_t inCommand)
{
  DockPacket p;
  for (;;) {
    if (!ioLink.receive(p, 30))
      throw std::runtime_error("no '" + DockCommandName(inCommand) + "' (" + ioLink.why() + ")");
    if (p.command == inCommand)
      return p;
    if (p.command == DockCommand("disc"))
      throw std::runtime_error("the desktop ended the connection");
  }
}

// One docking session: true if the desktop ended it ('disc'), false if we
// dropped it (--drop-after)
bool Session(Fake &ioFake)
{
  DockLink link(MNPLink::kNewton);
  link.setThrottle(ioFake.bps);
  std::string error;
  if (!link.start(ioFake.target, error))
    throw std::runtime_error(error);
  if (!link.waitLinkUp(30))
    throw std::runtime_error("no link to the desktop");

  // the Newton's side of the handshake
  link.sendLong(DockCommand("rtdk"), 9);
  Expect(link, DockCommand("dock"));
  std::vector<uint32_t> info(17, 0);
  info[0] = 0x12345678;          // fNewtonID
  info[1] = 0x01000000;          // fManufacturer
  info[2] = ioFake.machineType;
  info[3] = 0x00020002;          // ROM 2.2 (NewtonOS 2.1)
  info[5] = 4 << 20;             // RAM
  std::vector<uint8_t> name;
  for (int shift = 24; shift >= 0; shift -= 8)
    name.push_back((uint8_t)((uint32_t)(info.size() * 4) >> shift));
  for (uint32_t v : info)
    for (int shift = 24; shift >= 0; shift -= 8)
      name.push_back((uint8_t)(v >> shift));
  for (char c : std::string("Test Newton")) {
    name.push_back(0);
    name.push_back((uint8_t)c);
  }
  name.push_back(0);
  name.push_back(0);
  link.send(DockCommand("name"), name);
  DockPacket desktopInfo = Expect(link, DockCommand("dinf"));
  DockNonce desktopNonce{ desktopInfo.longData(8), desktopInfo.longData(12) };
  DockNonce ours = DockRandomNonce();
  link.sendLongs(DockCommand("ninf"), { 10, ours.hi, ours.lo });
  Expect(link, DockCommand("wicn"));
  link.sendLong(DockCommand("dres"), 0);
  Expect(link, DockCommand("stim"));
  DockNonce password = DockEncryptNonce(desktopNonce, ioFake.password);
  link.sendLongs(DockCommand("pass"), { password.hi, password.lo });
  DockPacket answer;
  for (;;) {
    if (!link.receive(answer, 30))
      throw std::runtime_error("no password from the desktop");
    if (answer.command == DockCommand("pass"))
      break;
    if (answer.command == DockCommand("pwbd"))         // the user would type it again
      link.sendLongs(DockCommand("pass"), { password.hi, password.lo });
    if (answer.command == DockCommand("dres"))
      throw std::runtime_error("the desktop refused the password");
  }
  DockNonce expected = DockEncryptNonce(ours, ioFake.password);
  if (answer.longData(0) != expected.hi || answer.longData(4) != expected.lo)
    throw std::runtime_error("the desktop's password answer is wrong");
  fprintf(stderr, "test newton: docked\n");

  for (;;) {
    DockPacket p;
    if (!link.receive(p, 120))
      throw std::runtime_error("the desktop went quiet (" + link.why() + ")");
    if (p.command == DockCommand("disc")) {
      link.stop();
      return true;
    }
    if (p.command == DockCommand("pext")) {
      link.sendLong(DockCommand("dres"), 0);
    } else if (p.command == DockCommand("NDmp")) {
      if (ioFake.delayMs > 0)
        std::this_thread::sleep_for(std::chrono::milliseconds(ioFake.delayMs));
      nsof::Ref request = nsof::Read(p.data.data(), p.data.size());
      nsof::Ref result = Answer(ioFake, request);
      if (ioFake.cancelAfter > 0 && ioFake.reads >= ioFake.cancelAfter) {
        // as the ROM (0x00094450): 'opca', then everything is thrown away
        // until 'ocaa' or 'disc'; the session ends
        link.send(DockCommand("opca"));
        for (;;) {
          DockPacket q;
          if (!link.receive(q, 30))
            throw std::runtime_error("no 'ocaa' after 'opca'");
          if (q.command == DockCommand("ocaa") || q.command == DockCommand("disc")) {
            fprintf(stderr, "test newton: cancel acknowledged ('%s')\n", DockCommandName(q.command).c_str());
            break;
          }
        }
        link.send(DockCommand("disc"));
        double until = 0;
        while (link.pending() > 0 && until++ < 20) {
          DockPacket q;
          link.receive(q, 0.1);
        }
        link.stop();
        return true;
      }
      if (ioFake.dropAfter > 0 && !ioFake.dropped && ioFake.reads >= ioFake.dropAfter) {
        ioFake.dropped = true;
        fprintf(stderr, "test newton: dropping the connection after %ld reads\n", ioFake.reads);
        link.stop();
        return false;
      }
      // as the ROM's WriteCommand: an integer as a 4-byte long, else NSOF
      if (result->isInteger())
        link.sendLong(DockCommand("NDrs"), (uint32_t)result->integer);
      else
        link.sendRef(DockCommand("NDrs"), result);
    }
    // dsnc, opdn, others: nothing to answer
  }
}

} // namespace


int main(int argc, char **argv)
{
  Fake fake;
  try {
    for (int i = 1; i < argc; i++) {
      std::string a = argv[i];
      auto value = [&]() -> std::string {
        if (i + 1 >= argc)
          throw std::runtime_error(a + " needs a value");
        return argv[++i];
      };
      if (a == "--target") fake.target = value();
      else if (a == "--rom") fake.regions.push_back({ 0, ReadFile(value()) });
      else if (a.rfind("--attribute", 0) == 0 || a.rfind("--common", 0) == 0) {
        bool common = a.rfind("--common", 0) == 0;
        int socket = atoi(a.c_str() + (common ? 8 : 11));
        uint32_t base = 0x30000000 + (uint32_t)socket * 0x10000000 + (common ? 0x08000000 : 0);
        fake.regions.push_back({ base, ReadFile(value()) });
        fake.cards[socket] = true;
      }
      else if (a == "--machine") fake.machineType = (uint32_t)strtoul(value().c_str(), nullptr, 0);
      else if (a == "--password") fake.password = value();
      else if (a == "--drop-after") fake.dropAfter = atol(value().c_str());
      else if (a == "--cancel-after") fake.cancelAfter = atol(value().c_str());
      else if (a == "--bps") fake.bps = atol(value().c_str());
      else if (a == "--delay") fake.delayMs = atol(value().c_str());
      else throw std::runtime_error("unknown option " + a);
    }
    if (fake.target.empty())
      throw std::runtime_error("--target tcp-client:<port>");
    // as a Newton that stays on: one session, or two with --drop-after
    for (int attempt = 0; attempt < 20; attempt++) {
      try {
        if (Session(fake))
          break;
      } catch (std::exception &e) {
        if (fake.dropped && attempt < 19) {   // newtdump may not be listening yet
          std::this_thread::sleep_for(std::chrono::milliseconds(200));
          continue;
        }
        throw;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(300));   // dropped: connect again
    }
  } catch (std::exception &e) {
    fprintf(stderr, "newtdump-test-newton: %s\n", e.what());
    return 1;
  }
  return 0;
}
