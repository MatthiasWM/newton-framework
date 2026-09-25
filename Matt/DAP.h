/*
 File: DAP.h

 The Debug Adapter Protocol (DAP) for `newtc -dap`: the C++ part.

 C++ only moves messages; the protocol itself is NewtonScript
 (Matt/Debugger/DAP.ns). A DAP message is a header plus JSON:

   Content-Length: 58\r\n
   \r\n
   {"seq":1,"type":"request","command":"initialize",...}

 DAPReceive() reads one message from stdin and returns it as a frame
 (Matt/JSON.h), DAPSend(frame) writes one to stdout and adds the "seq"
 number. In -dap mode stdout carries nothing but DAP messages: everything
 else newtc writes to stdout goes to stderr instead.

 The REP translators PDAPOutTranslator and PDAPInTranslator (like
 PHammerOut/InTranslator for NTK's Inspector) replace the stdio ones:
 whatever NewtonScript prints (Print, Write, exceptions, the break loop)
 becomes a DAP "output" event, and a break loop reads DAP requests instead
 of lines of NewtonScript.
 */

#ifndef MATT_DAP_H
#define MATT_DAP_H

#include "Frames/Objects.h"

/** Reserve stdout for DAP messages; from now on anything else written to
    stdout goes to stderr. Call before anything is printed in -dap mode. */
void DAPStartIO(void);

/** Replace the REP translators (gREPin, gREPout) with the DAP ones. */
void DAPInstallTranslators(void);

/** Number of exceptions reported so far (they are sent as "stderr" output);
    newtc uses it for the exit code of the program. */
int DAPExceptionCount(void);

// NewtonScript functions, registered in newtc.cc:
//   DAPReceive() -> the next message as a frame, nil at end of input
//   DAPSend(frame) -> nil; writes the frame as a message, adds "seq"
//   DAPExit(code) -> doesn't return; flushes and quits newtc
extern "C" Ref FDAPReceive(RefArg rcvr);
extern "C" Ref FDAPSend(RefArg rcvr, RefArg inMessage);
extern "C" Ref FDAPExit(RefArg rcvr, RefArg inCode);

#endif // MATT_DAP_H
