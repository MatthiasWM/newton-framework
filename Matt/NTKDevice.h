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
    or the link ends. 0 then, 1 if no connection could be made. */
int NTKDeviceRun(const std::string &inTarget);

#endif // MATT_NTKDEVICE_H
