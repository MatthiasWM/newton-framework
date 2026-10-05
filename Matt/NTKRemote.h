/*
 File: NTKRemote.h
 A remote Newton for NewtonScript: functions on top of NTKInspector, so the
 remote debugger can be written in NewtonScript like the local one
 (Matt/Debugger/DAP.ns). One connection at a time.
 What the Newton sends on its own (text, exceptions, break loops, objects,
 the end of the connection) goes to a handler frame, set with
 NTKSetHandler(frame), whose methods are called while a function below
 waits, or in NTKPoll():
   :Connected(peer)  :Disconnected(reason)  :Text(string)
   :Exception(name, data)      data: the frame (eref), the message (estr),
                               or the error code (eerr)
   :BreakLoop(entered)         true for 'eext', nil for 'bext'
   :Object(command, object)    'fobj' or 'fstk' (command as a string)
   :Result(error, command)     a 'rslt' nobody waits for (e.g. after 'lscb')
 Without a handler (or a method), it is printed like newtc -ntk prints it.
 */

#ifndef MATT_NTKREMOTE_H
#define MATT_NTKREMOTE_H

#include "Frames/Objects.h"

// NewtonScript functions, registered in newtc.cc:
//   NTKOpen(target) -> nil, or an error message (see NTKTransport.h)
//   NTKWaitConnected(seconds) -> true when connected, nil after the time
//   NTKIsConnected() -> true while the Toolkit connection is up
//   NTKCall(fn) -> what fn returns on the Newton ('code'); waits at most
//       ntkTimeout seconds (default 30); throws if the connection is lost
//       or there is no answer
//   NTKEvaluate(fn) -> nil; 'lscb': the Newton's REP runs fn, its output
//       comes to the handler
//   NTKInstallPackage(binary) -> 0, or the Newton's error ('pkg ')
//   NTKDeletePackage(name) -> 0, or the Newton's error ('pkgX')
//   NTKPoll(seconds) -> true if anything happened (handled), within seconds
//   NTKSetHandler(frame) -> nil
//   NTKClose() -> nil; 'term', and the link ends
Ref FNTKOpen(RefArg rcvr, RefArg inTarget);
Ref FNTKWaitConnected(RefArg rcvr, RefArg inSeconds);
Ref FNTKIsConnected(RefArg rcvr);
Ref FNTKCall(RefArg rcvr, RefArg inFunction);
Ref FNTKEvaluate(RefArg rcvr, RefArg inFunction);
Ref FNTKInstallPackage(RefArg rcvr, RefArg inPackage);
Ref FNTKDeletePackage(RefArg rcvr, RefArg inName);
Ref FNTKPoll(RefArg rcvr, RefArg inSeconds);
Ref FNTKSetHandler(RefArg rcvr, RefArg inHandler);
Ref FNTKClose(RefArg rcvr);

//   NTKCallAsync(fn) -> a number: 'code' sent, not waited for;
//   NTKCallResult(number) -> {value: result} once answered (taken), else nil
//   NTKWaitAny(seconds) -> 'dap (a DAP request waits), 'ntk (the Newton's
//       events handled), nil (nothing within seconds)
//   NTKCompileFile(path) -> [codeBlock]: the whole file compiled here as
//       one top-level block inside a try (an exception is returned as
//       {|DAP error|: ex}), to run with 'code'; throws on errors (printed
//       like -script)
Ref FNTKCallAsync(RefArg rcvr, RefArg inFunction);
Ref FNTKCallResult(RefArg rcvr, RefArg inNumber);
Ref FNTKWaitAny(RefArg rcvr, RefArg inSeconds);
Ref FNTKCompileFile(RefArg rcvr, RefArg inPath);

//   NTKMakePackage(frame) -> the package (a binary of class 'package)
//   NTKPackageName(package) -> its name (for 'pkgX'), nil if no package
//   NTKLibrary() -> the global NTKRemote (Matt/Debugger/Remote.ns: install,
//       replace, NS Debug Tools and the agent on the Newton), loaded once
Ref FNTKMakePackage(RefArg rcvr, RefArg inPackage);
Ref FNTKPackageName(RefArg rcvr, RefArg inPackage);
Ref FNTKLibrary(RefArg rcvr);

//   LoadDataFile(fileName, class) -> the file's bytes as a binary object of
//       that class (NTK's function, for build scripts); throws if unreadable
Ref FLoadDataFile(RefArg rcvr, RefArg inFileName, RefArg inClass);

#endif // MATT_NTKREMOTE_H
