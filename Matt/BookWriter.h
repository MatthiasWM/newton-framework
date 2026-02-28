/*
 File:    NewtonPBookWriter.h

 Abstract:  Takes a Newton Ref describing the package and writes it in
 HTML format if it contains a book.

 Written by:  Matt, 2026.
 */

#ifndef __NEWTONBOOKWRITER_H
#define __NEWTONBOOKWRITER_H

#include <stdio.h>

#include "Newton.h"
#include "Objects.h"
#include "PackageParts.h"
#include "Frames/ObjHeader.h"

#include <string>
#include <memory>
#include <vector>
#include <map>

int writePackageBookToHTML(RefArg pkg, const std::string &filename);

#endif // !defined(__NEWTONBOOKWRITER_H)

