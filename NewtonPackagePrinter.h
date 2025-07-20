/*
 File:    NewtonPackagePrinter.h

 Prints a NewtonScript object tree that resembles a Package into a
 source file that can be recompiled into the same Package.

 Written by:  Matt, 2025.
 */

#if !defined(__NEWTONPACKAGEPRINTER_H)
#define __NEWTONPACKAGEPRINTER_H 1

#include "Frames/ObjHeader.h"

void printPackage(Ref package);

#endif  /* __NEWTONPACKAGEPRINTER_H */
