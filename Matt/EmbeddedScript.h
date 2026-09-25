/*
 File: EmbeddedScript.h

 NewtonScript source files built into newtc.

 The CMake function embed_newtonscript() (CMakeLists.txt) turns a .ns file
 into a C++ file that defines an EmbeddedScript holding the file's name and
 text. newtc can then run it without the .ns file at run time, and the .ns
 file stays the one to read and edit.
 */

#ifndef MATT_EMBEDDED_SCRIPT_H
#define MATT_EMBEDDED_SCRIPT_H

struct EmbeddedScript
{
  const char *fileName;   ///< name of the .ns file, for error messages
  const char *source;     ///< its text, NUL terminated
};

/**
 \brief Compile and run a script, one top-level statement after the other,
   like `newtc -script` does.
 An exception stops the script; it is reported with the script's file name
 and line number.
 \return true if the script ran without an exception
 */
bool RunEmbeddedScript(const EmbeddedScript &script);

#endif // MATT_EMBEDDED_SCRIPT_H
