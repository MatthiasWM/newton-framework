/*
 File: HostCompat.h

 The few POSIX functions newtc's own code uses, also on Windows (MSVC):
 case-insensitive compares, big-endian conversion (HostToBig16/32: htons,
 htonl, which windows.h declares too), the
 canonical path of a file, a FILE that reads from memory.
 */

#ifndef HOSTCOMPAT_H
#define HOSTCOMPAT_H

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>

#if defined(_WIN32)
#include <stdlib.h>

inline int strcasecmp(const char * a, const char * b) { return _stricmp(a, b); }
inline int strncasecmp(const char * a, const char * b, size_t n) { return _strnicmp(a, b, n); }

// Windows runs little-endian only
inline uint16_t HostToBig16(uint16_t v) { return _byteswap_ushort(v); }
inline uint32_t HostToBig32(uint32_t v) { return _byteswap_ulong(v); }
#else
#include <arpa/inet.h>

inline uint16_t HostToBig16(uint16_t v) { return htons(v); }
inline uint32_t HostToBig32(uint32_t v) { return htonl(v); }
#endif

/** The path with symbolic links and . and .. resolved (realpath()); the
    path as it is if the file isn't there. */
inline std::string HostCanonicalPath(const std::string & inPath)
{
  if (inPath.empty())
    return inPath;
  std::error_code err;
  std::filesystem::path path = std::filesystem::canonical(std::filesystem::path(inPath), err);
  return err ? inPath : path.string();
}

/** A FILE that reads inText (fmemopen(): inText must stay as it is until
    the FILE is closed; Windows: a temporary file with a copy). Closed with
    fclose(); NULL if it can't be made. */
inline FILE * HostTextFile(const std::string & inText)
{
#if defined(_WIN32)
  FILE * file = tmpfile();
  if (file != nullptr) {
    fwrite(inText.data(), 1, inText.size(), file);
    rewind(file);
  }
  return file;
#else
  return fmemopen((void *)inText.data(), inText.size(), "r");
#endif
}

#endif // HOSTCOMPAT_H
