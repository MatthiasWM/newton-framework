/*
 File: JSON.h

 JSON <-> NewtonScript objects, for the Debug Adapter Protocol (and later the
 Language Server Protocol).

 JSON                     NewtonScript
 object {"a": 1}          frame {a: 1} (keys become symbols; slot order kept)
 array  [1, 2]            array [1, 2]
 string "text"            string (UTF-8 <-> UTF-16, \uXXXX escapes incl.
                          surrogate pairs; a \u0000 ends the string)
 number 12, -3            integer (if it fits, else real; 62 bits on 64-bit)
 number 1.5, 1e3          real
 true                     true
 false, null              nil

 Writing: nil becomes false, a symbol or a character becomes a string,
 non-ASCII characters are written as \uXXXX. Anything else (functions,
 other binaries, ...) throws. Errors throw an exception with a message.

 Note: symbols are case-insensitive and keep the spelling they were first
 created with, JSON keys are case-sensitive. A key is written with the
 spelling of its symbol.
 */

#ifndef MATT_JSON_H
#define MATT_JSON_H

#include "Frames/Objects.h"
#include <string>

/** Parse JSON text (UTF-8) into a NewtonScript object. Throws on errors. */
Ref ParseJSON(const char *text, size_t length);

/** Write a NewtonScript object as JSON text (ASCII, compact). Throws if it
    contains something JSON can't express. */
std::string ToJSON(RefArg obj);

/** A C string as a JSON string literal (with quotes). Bytes >= 0x80 are
    taken as Latin-1. For the text newtc prints, which is ASCII. */
std::string QuoteJSON(const std::string &text);

/** A NewtonScript string (UTF-16) as UTF-8. */
std::string UTF8FromString(RefArg str);

// NewtonScript functions, registered in newtc.cc:
//   JSONParse(string) -> object, JSONStringify(object) -> string
extern "C" Ref FJSONParse(RefArg rcvr, RefArg inString);
extern "C" Ref FJSONStringify(RefArg rcvr, RefArg inObject);

#endif // MATT_JSON_H
