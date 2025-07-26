
/*
 File:    MattsDecompiler.h

 Decompile a NewtonScript function.

 Written by:  Matt, 2025.
 */

#if !defined(__MATTSDECOMPILER_H)
#define __MATTSDECOMPILER_H 1

#include "Matt/ObjectPrinter.h"

class ObjectPrinter;

NewtonErr mDecompile(Ref ref, ObjectPrinter &printer, bool debugAST);


#endif  /* __MATTSDECOMPILER_H */
