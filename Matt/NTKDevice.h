/*
 File: NTKDevice.h
 newtc playing the Newton for NTK: the device side of the Toolkit protocol,
 so remote debugging can be tested without Einstein or a MessagePad.
 It follows the ROM's NTK nub (TNTKNub, PNTKInTranslator,
 PNTKOutTranslator; Matt/Toolkit Protocol.md section 2, and the
 differences of the port in NTK/NTK.cc listed in 2.7): it connects like
 Einstein's serial port (a TCP client), sends the MNP LR, 'cnnt', and
 Toolkit.pkg's 'dante' object, then runs newtc's REP with translators that
 read the desktop's commands and send text, results, objects, exceptions,
 and break loop entries and exits.
 */

#ifndef MATT_NTKDEVICE_H
#define MATT_NTKDEVICE_H

#include <string>

/** newtc -ntk-device <target>: connect to a desktop (e.g. tcp-client:3679,
    where newtc -ntk or NTK waits) and be its Newton until it sends 'term'
    or the link ends. 0 then, 1 if no connection could be made. The
    environment variable NEWTC_NTK_DEVICE_SESSIONS=<n>: n sessions one
    after the other (a Newton that stays on; its packages stay). */
int NTKDeviceRun(const std::string &inTarget);

#include "Frames/Objects.h"

/** The ROM's NTKAlive() and NTKSend(obj) (newtc has only stubs for them):
    true while -ntk-device is connected; send obj to the desktop as 'fobj'
    (nothing without a connection). */
extern "C" Ref FNTKDeviceAlive(RefArg rcvr);
extern "C" Ref FNTKDeviceSend(RefArg rcvr, RefArg inObject);

/** The ROM's GetPkgRef(name, store) and GetPkgRefInfo(pkgRef) for the
    packages -ntk-device installed (stubs in newtc otherwise): the ref is
    the package's name here; the info frame has title, size, numParts, and parts
    (each part's data, as the ROM gives for frame parts). nil for others. */
extern "C" Ref FNTKDeviceGetPkgRef(RefArg rcvr, RefArg inName, RefArg inStore);
extern "C" Ref FNTKDeviceGetPkgRefInfo(RefArg rcvr, RefArg inRef);

#endif // MATT_NTKDEVICE_H
