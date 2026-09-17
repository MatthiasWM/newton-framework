/*
	File:		DebugAPI.h

	Contains:	Interpreter debug function definitions.

	Written by:	Newton Research Group, 2013.
*/

#if !defined(__DEBUGAPI_H)
#define __DEBUGAPI_H 1

#include "Objects.h"
#include "Interpreter.h"


/* -----------------------------------------------------------------------------
	C N S D e b u g A P I
----------------------------------------------------------------------------- */

class CNSDebugAPI
{
public:
					CNSDebugAPI(CInterpreter * interpreter);
					~CNSDebugAPI();

	ArrayIndex	numStackFrames(void);
	VMState *	stackFrameAt(ArrayIndex index);
	bool			accurateStack(void);

	ArrayIndex	PC(ArrayIndex index);
	void			setPC(ArrayIndex index, ArrayIndex inPC);
	Ref			function(ArrayIndex index);
	void			setFunction(ArrayIndex index, RefArg inFunction);
	Ref			receiver(ArrayIndex index);
	void			setReceiver(ArrayIndex index, RefArg inRcvr);
	Ref			implementor(ArrayIndex index);
	void			setImplementor(ArrayIndex index, RefArg inImpl);
	Ref			locals(ArrayIndex index);

	Ref			getVar(ArrayIndex index, ArrayIndex inVarIndex);
	void			setVar(ArrayIndex index, ArrayIndex inVarIndex, RefArg inVar);
	Ref			findVar(ArrayIndex index, RefArg inSym);
	void			setFindVar(ArrayIndex index, RefArg inSym, RefArg inVar);

	ArrayIndex	numTemps(ArrayIndex index);
	Ref			tempValue(ArrayIndex index, ArrayIndex inTempIndex);
	void			setTempValue(ArrayIndex index, ArrayIndex inTempIndex, RefArg inTemp);

	void			Return(ArrayIndex index, RefArg);

private:
	ArrayIndex	stackStart(ArrayIndex index);

	CInterpreter * fInterpreter;
};


extern CNSDebugAPI *	NewNSDebugAPI(CInterpreter * interpreter);
extern void				DeleteNSDebugAPI(CNSDebugAPI * inDebugAPI);

/* -----------------------------------------------------------------------------
	Source-line lookup for a compiled function's line table -- see the
	implementation in DebugAPI.cc for the table format and how it's built
	(CFunctionState::noteLine() in CompilerSupport.cc, gated on the
	dbgKeepLineTable global var / newtc's -g flag). inFunc's `lineTable`
	slot 0 (the file) is an interned symbol, not a string -- see
	FindPCForLine() below, which relies on that for fast EQ() matching.
----------------------------------------------------------------------------- */
extern bool	FindSourceLine(RefArg inFunc, ArrayIndex inPC, RefVar & outFile, ArrayIndex & outLine);

/* -----------------------------------------------------------------------------
	The reverse lookup: given a source (file, line), find the compiled
	function and PC to set a breakpoint at -- e.g. to build the
	{programCounter:, instructions:, ...} frame SetBreakPoints() expects
	(see Frames/Interpreter.cc, "Breakpoints").

	Searches every function ever registered via RegisterDebugFunction()
	(every function compiled with dbgKeepLineTable set) for the one whose
	`lineTable` file matches inFile and whose recorded lines come closest
	to inLine without going under it -- i.e. this snaps *forward* to the
	next line that actually has code, the same way most source-level
	debuggers resolve a breakpoint set on a comment/blank/declaration-only
	line. outActualLine tells the caller what line it actually landed on,
	so a UI can report "breakpoint set at line N" honestly when N != inLine.

	Args:		inFile			a symbol, matched against each candidate
									function's own lineTable[0] with EQ()
				inLine			1-based source line to resolve
				outFunc			set to the matching function, if found
				outPC				set to the PC of the resolved statement
				outActualLine	set to the line that PC actually corresponds to
	Return:	true if some registered function's line table had an entry at
				or after inLine for that file; false otherwise (inFile was
				never compiled with line info, or inLine is past every
				registered function's last statement in that file).
----------------------------------------------------------------------------- */
extern void	InitDebugFunctionRegistry(void);
extern void	RegisterDebugFunction(RefArg inFunc);
extern bool	FindPCForLine(RefArg inFile, ArrayIndex inLine, RefVar & outFunc, ArrayIndex & outPC, ArrayIndex & outActualLine);

/* -----------------------------------------------------------------------------
	NewtonScript-callable: DbgAddBreakpoint(filename, line), DbgRemoveBreakpoint(ref).
	See the implementation in DebugAPI.cc; registered as globals in newtc.cc's init().
----------------------------------------------------------------------------- */
extern Ref	FDbgAddBreakpoint(RefArg inRcvr, RefArg inFilename, RefArg inLine);
extern Ref	FDbgRemoveBreakpoint(RefArg inRcvr, RefArg inBreakPoint);

#endif	/* __DEBUGAPI_H */
