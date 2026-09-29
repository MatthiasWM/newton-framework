/*
 File: NewtPlay.h

 NewtPlay: newtc as a Mac app that runs Newton packages (Phase 11, see
 Matt/CLAUDE.md). The CMake target NewtPlay builds NewtPlay.app from
 newtc's sources with NEWTC_NEWTPLAY. When it starts, Arguments() decides
 what it does:

   one argument, a file          run that package (`NewtPlay game.pkg`)
   -make-shortcut <package>      make a shortcut or an app for it (see
   -make-app <package>           MakeBundle(); prints its path)
   no arguments (the Finder)     run the package in its own bundle
                                 (its Contents/Resources: a .nspkg, .newtonpkg,
                                 .pkg: an app made by MakeBundle), else the
                                 splash window (run, make a shortcut or an
                                 app, the history)
   anything else                 newtc, as always

 A package runs as `newtc -store <store> -pkg <file> -run`, with its store
 in ~/Library/Application Support/NewtPlay/<package name>/, a store per
 version of the package (the package's version number): the same wherever
 the package comes from, and versions don't read each other's data
 (nBattleship 2.5 saves settings 1.4 can't read; both are Battleship:ATOW). A file that isn't a Newton package (it
 starts with "package0" or "package1") gets a polite no.
 */

#ifndef MATT_NEWTPLAY_H
#define MATT_NEWTPLAY_H

#include <string>
#include <vector>

namespace newtplay {

/** The newtc arguments (argv[0] first) for how NewtPlay was started. Empty:
    nothing to run (the user cancelled, or the file isn't a Newton package:
    the user was told). */
std::vector<std::string> Arguments(int argc, char ** argv);

/** After the package's start (newtc's main, before its event loop): if its
    app didn't open a window, tell the user why: its error (empty: none,
    "nothing to show"). After an error the user may start it again with new
    data: its store is kept aside as <store>.old-<date>-<time>, and NewtPlay
    starts again with the package (the store may be from another version of
    the package: nBattleship 2.5's settings stop 1.4). Nothing to do if no
    package runs (newtc) or a window is open. */
void CheckStarted(const std::string & inError);

/** Whether the file is a Newton package, its name (the package's own, e.g.
    "Battleship:ATOW"; empty if it has none), and its version (the
    header's version number: nBattleship 1.4 has 1, 2.5 has 8). */
bool ReadPackageName(const std::string & inPath, std::string * outName, unsigned long * outVersion = nullptr);

/** Where a version of a package keeps its store:
    ~/Library/Application Support/NewtPlay/<name>/<name>-v<version>.store
    (the name made safe for a file name; the folder is made). Empty if there
    is no home folder. */
std::string StorePath(const std::string & inPackageName, unsigned long inVersion);

/** A shortcut (inApp false) or an app (inApp true) for a package (11.4,
    Matt/NewtPlayMake.cc): <package's folder>/<package file's name>.app,
    next to the package (for a package in a shortcut or an app: next to
    that). Both have the package (as .nspkg), its icon (the package's own,
    its pixels big on a green rounded square), an Info.plist of their own, and an ad hoc
    signature. A shortcut's program is a script that runs the package with
    NewtPlay (`open -b org.newton-framework.NewtPlay`); an app's is a copy
    of NewtPlay's, which runs the package in its bundle. A bundle there
    already (a shortcut, an app, anything) is replaced if the user says so
    (NEWTPLAY_TEST_ANSWER: 0 Cancel, 1 Replace). outBundle: its path;
    false: not made, with outError why (empty: the user cancelled). */
bool MakeBundle(const std::string & inPackage, bool inApp, std::string * outBundle, std::string * outError);

/** Show a file in the Finder (`open -R`). */
void ShowInFinder(const std::string & inPath);

} // namespace newtplay

#endif // MATT_NEWTPLAY_H
