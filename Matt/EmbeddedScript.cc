/*
 File: EmbeddedScript.cc

 Run NewtonScript source files built into newtc. See EmbeddedScript.h.
 */

#include "Matt/EmbeddedScript.h"

#include "Frames/Frames.h"
#include "Frames/Funcs.h"
#include "Frames/Interpreter.h"
#include "Frames/Compiler/InputStreams.h"
#include "Frames/Compiler/Compiler.h"
#include "ROMResources.h"
#include "REPTranslators.h"

bool RunEmbeddedScript(const EmbeddedScript &script)
{
  volatile bool ok = true;   // volatile: changed in newton_catch (longjmp)
  RefVar src(MakeStringFromCString(script.source));
  RefVar codeBlock;
  CStringInputStream stream(src);
  stream.setFileName(script.fileName);
  CCompiler compiler(&stream, true);

  newton_try
  {
    while (!stream.end())
    {
      codeBlock = compiler.compile();
      if (NOTNIL(codeBlock))
        InterpretBlock(codeBlock, RA(NILREF));
    }
  }
  newton_catch(exRefException)
  {
    RefVar data(*(RefStruct *)CurrentException()->data);
    if (IsFrame(data) && ISNIL(GetFrameSlot(data, SYMA(filename))))
    {
      SetFrameSlot(data, SYMA(filename), MakeStringFromCString(script.fileName));
      SetFrameSlot(data, SYMA(lineNumber), MAKEINT(compiler.lineNo()));
    }
    gREPout->exceptionNotify(CurrentException());
    ok = false;
  }
  newton_catch_all
  {
    gREPout->exceptionNotify(CurrentException());
    ok = false;
  }
  end_try;

  return ok;
}
