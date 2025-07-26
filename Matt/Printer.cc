/*
 File:    ObjectPrinter.cc

 This class manages textual dividers and line indentation for text
 written to a std::ostream.

 Written by:  Matt, 2025.
 */

#include "Matt/Printer.h"


/**
 * @class PState
 * @brief A helper class that manages indents and dividing semicolons and commas.
 *
 * This works by deferring dividers, indents, and newlines until we actually print
 * the next chunk. That way, these parameters can be modified before they are
 * finally applied.
 *
 * @note This scheme should probably be ported to the ObjectPrinter class,
 *       so that it can be used for printing packages as well.
 *
 * @enum Type
 *   - bytecode: Formatting for bytecode output.
 *   - deep: Formatting for deep analysis output.
 *   - script: Formatting for script output.
 *
 */

Printer::Printer(std::ostream &oStream)
: out(oStream)
{
  // TODO: remove this when everything in ObjectPrinter and Decompiler uses Printer.out .
  std::ios::sync_with_stdio(true);
}



