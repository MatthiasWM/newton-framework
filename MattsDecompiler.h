
/*
 File:    MattsDecompiler.h

 Decompile a NewtonScript function.

 Written by:  Matt, 2025.
 */

#if !defined(__MATTSDECOMPILER_H)
#define __MATTSDECOMPILER_H 1

#include "NewtonPackagePrinter.h"

class NewtonPackagePrinter;

NewtonErr mDecompile(Ref ref, NewtonPackagePrinter &printer);


#endif  /* __MATTSDECOMPILER_H */
