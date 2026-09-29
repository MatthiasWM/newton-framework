/*
 File: NewtPlay.h

 NewtPlay: newtc as a Mac app that runs Newton packages (Phase 11, see
 Matt/CLAUDE.md). The CMake target NewtPlay builds NewtPlay.app from
 newtc's sources with NEWTC_NEWTPLAY. When it starts, Arguments() decides
 what it does:

   one argument, a file          run that package (`NewtPlay game.pkg`)
   no arguments (the Finder)     run the package in its own bundle
                                 (Contents/Resources/*.nspkg, .newtonpkg,
                                 .pkg: a self-contained app, 11.4), else let
                                 the user choose one (the file chooser; the
                                 splash window comes in 11.3)
   anything else                 newtc, as always

 A package runs as `newtc -store <store> -pkg <file> -run`, with its store
 in ~/Library/Application Support/NewtPlay/<package name>/, the same
 wherever the package comes from. A file that isn't a Newton package (it
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

/** Whether the file is a Newton package, and its name (the package's own,
    e.g. "Battleship:ATOW"; empty if it has none). */
bool ReadPackageName(const std::string & inPath, std::string * outName);

/** Where a package's store is: ~/Library/Application Support/NewtPlay/
    <name>/<name>.store (the name made safe for a file name; the folder is
    made). Empty if there is no home folder. */
std::string StorePath(const std::string & inPackageName);

} // namespace newtplay

#endif // MATT_NEWTPLAY_H
