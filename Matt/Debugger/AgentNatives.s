@ AgentNatives.s: the agent's native functions (plan step 3.18b), so a
@ Newton needs no NS Debug Tools.pkg. ARM (v4, the StrongARM's), big-endian,
@ position independent: a package has no relocations, so our own addresses
@ come from the PC (adr) and the ROM is reached only through its public
@ jump table, virtual 0x01800000 + an entry's offset, the same in every ROM
@ build (Matt, 2026-10-13: ROMs in other languages; Toolkit Protocol.md
@ 7.4). No addresses of a particular ROM build.
@
@ Matt/tools/build_agent_natives.sh assembles this into
@ Matt/Debugger/AgentNatives.ns (the code as hex and where each native
@ starts), which the agent package uses: a native is a frame {class:
@ 'BinCFunction, code: <the binary>, numArgs, offset}.
@
@ A native is called as C: Ref F(RefArg rcvr, RefArg arg1, ...); r0 is the
@ receiver, r1... the arguments, each a RefArg: a pointer to a RefVar,
@ which holds a RefHandle*, which holds the Ref. The result is a Ref in r0.
@ Refs: an integer is n << 2, nil is 2, true is 0x1A. Registers r4-r11
@ and sp are kept. A Ref is read from its RefArg just before it is used:
@ a call that allocates (Intern) may move objects (NSDT does the same).
@
@ The StrongARM has no Thumb (ARMv4, not v4T; Matt): no Thumb code and no
@ bx; return with mov pc, lr or ldm ..., pc. -march=armv4 makes the
@ assembler refuse both.
@
@ "@ native <name> <numArgs>" before an entry tells the build script.

        .syntax unified
        .arm
        .text
        .align  2

@ -- Natives: the interpreter's breakpoints (as NSDInstallBreakPoints and
@ NSDEnableBreakPoints of NS Debug Tools)

@ native InstallBreakPoints 1
@ InstallBreakPoints(list) -> the list before: {programCounter:
@ [{instructions, programCounter, ...}, ...]} or nil
InstallBreakPoints:
        stmfd   sp!, {r4, lr}
        mov     r4, r1                  @ the list (a RefArg)
        bl      GetGInterpreter
        mov     r1, r4
        bl      TInterpreter_SetBreakPoints
        ldmfd   sp!, {r4, pc}

@ native EnableBreakPoints 1
@ EnableBreakPoints(flag) -> whether they were enabled (true or nil)
EnableBreakPoints:
        stmfd   sp!, {r4, lr}
        ldr     r4, [r1]                @ RefVar -> RefHandle*
        ldr     r4, [r4]                @ -> the Ref
        subs    r4, r4, #2              @ nil: 0
        movne   r4, #1
        bl      GetGInterpreter
        mov     r1, r4
        bl      TInterpreter_EnableBreakPoints
        tst     r0, #0xFF               @ a bool (unsigned char)
        moveq   r0, #2
        movne   r0, #0x1A
        ldmfd   sp!, {r4, pc}

@ -- The debug API (as NSDMakeNSDebugAPI and the methods of NSDSelfFuncs):
@ the stack object is a frame whose slot nsDebugAPI holds a CObject
@ binary with the ROM's TNSDebugAPI for the current interpreter.

@ native MakeNSDebugAPI 0
@ MakeNSDebugAPI() -> a CObject binary; the garbage collector deletes the
@ TNSDebugAPI through the jump table (not through this package, which may be
@ gone by then: a new agent)
MakeNSDebugAPI:
        stmfd   sp!, {lr}
        bl      GetGInterpreter
        bl      NewNSDebugAPI
        ldr     r1, =0x0180283C         @ DeleteNSDebugAPI(TNSDebugAPI*), its jump table entry
        mov     r2, #0                  @ no mark proc
        mov     r3, #0                  @ no update proc
        bl      AllocateCObjectBinary
        ldmfd   sp!, {pc}

@ native AccurateStack 0
@ stack:AccurateStack() -> whether the PCs are accurate (breakpoints on)
AccurateStack:
        stmfd   sp!, {lr}
        bl      ApiOf
        bl      TNSDebugAPI_AccurateStack
        tst     r0, #0xFF               @ a bool
        moveq   r0, #2
        movne   r0, #0x1A
        ldmfd   sp!, {pc}

@ native NumStackFrames 0
@ stack:NumStackFrames() -> the number of frames (0 is the oldest)
NumStackFrames:
        stmfd   sp!, {lr}
        bl      ApiOf
        bl      TNSDebugAPI_NumStackFrames
        mov     r0, r0, lsl #2          @ an integer Ref
        ldmfd   sp!, {pc}

@ The methods, by their arguments (frame: a stack frame, 0 the oldest;
@ RefArgs as they came): the TNSDebugAPI call with the integers converted,
@ its result as it is (a Ref), as an integer (_INT), or nil (setters).

        @ name(frame) -> call(api, frame)
        .macro  METHOD_I name, call
\name:
        stmfd   sp!, {r4, r5, lr}
        mov     r4, r1                  @ frame
        bl      ApiOf
        mov     r5, r0                  @ the TNSDebugAPI*
        mov     r0, r4
        bl      IntOf
        mov     r1, r0
        mov     r0, r5
        bl      \call
        ldmfd   sp!, {r4, r5, pc}
        .endm

        @ name(frame) -> call(api, frame) as an integer Ref
        .macro  METHOD_I_INT name, call
\name:
        stmfd   sp!, {r4, r5, lr}
        mov     r4, r1                  @ frame
        bl      ApiOf
        mov     r5, r0
        mov     r0, r4
        bl      IntOf
        mov     r1, r0
        mov     r0, r5
        bl      \call
        mov     r0, r0, lsl #2
        ldmfd   sp!, {r4, r5, pc}
        .endm

        @ name(frame, index) -> call(api, frame, index)
        .macro  METHOD_II name, call
\name:
        stmfd   sp!, {r4, r5, r6, lr}
        mov     r4, r1                  @ frame
        mov     r5, r2                  @ index
        bl      ApiOf
        mov     r6, r0
        mov     r0, r5
        bl      IntOf
        mov     r5, r0
        mov     r0, r4
        bl      IntOf
        mov     r1, r0
        mov     r2, r5
        mov     r0, r6
        bl      \call
        ldmfd   sp!, {r4, r5, r6, pc}
        .endm

        @ name(frame, index, value) -> nil; call(api, frame, index, value)
        .macro  METHOD_IIV name, call
\name:
        stmfd   sp!, {r4, r5, r6, r7, lr}
        mov     r4, r1                  @ frame
        mov     r5, r2                  @ index
        mov     r7, r3                  @ value (a RefArg: const RefVar&)
        bl      ApiOf
        mov     r6, r0
        mov     r0, r5
        bl      IntOf
        mov     r5, r0
        mov     r0, r4
        bl      IntOf
        mov     r1, r0
        mov     r2, r5
        mov     r3, r7
        mov     r0, r6
        bl      \call
        mov     r0, #2
        ldmfd   sp!, {r4, r5, r6, r7, pc}
        .endm

        @ name(frame, symbol) -> call(api, frame, symbol)
        .macro  METHOD_IS name, call
\name:
        stmfd   sp!, {r4, r5, r6, lr}
        mov     r4, r1                  @ frame
        mov     r5, r2                  @ symbol (a RefArg)
        bl      ApiOf
        mov     r6, r0
        mov     r0, r4
        bl      IntOf
        mov     r1, r0
        mov     r2, r5
        mov     r0, r6
        bl      \call
        ldmfd   sp!, {r4, r5, r6, pc}
        .endm

        @ name(frame, symbol, value) -> nil; call(api, frame, symbol, value)
        .macro  METHOD_ISV name, call
\name:
        stmfd   sp!, {r4, r5, r6, r7, lr}
        mov     r4, r1                  @ frame
        mov     r5, r2                  @ symbol
        mov     r7, r3                  @ value
        bl      ApiOf
        mov     r6, r0
        mov     r0, r4
        bl      IntOf
        mov     r1, r0
        mov     r2, r5
        mov     r3, r7
        mov     r0, r6
        bl      \call
        mov     r0, #2
        ldmfd   sp!, {r4, r5, r6, r7, pc}
        .endm

        @ name(frame, value) -> nil; call(api, frame, value as an integer)
        .macro  METHOD_IIN name, call
\name:
        stmfd   sp!, {r4, r5, r6, lr}
        mov     r4, r1                  @ frame
        mov     r5, r2                  @ the integer
        bl      ApiOf
        mov     r6, r0
        mov     r0, r5
        bl      IntOf
        mov     r5, r0
        mov     r0, r4
        bl      IntOf
        mov     r1, r0
        mov     r2, r5
        mov     r0, r6
        bl      \call
        mov     r0, #2
        ldmfd   sp!, {r4, r5, r6, pc}
        .endm

@ native Function 1
@ stack:Function(frame) -> the function running in that frame
        METHOD_I        Function, TNSDebugAPI_Function

@ native ProgramCounter 1
@ stack:ProgramCounter(frame) -> its PC, -1 for a native function
        METHOD_I_INT    ProgramCounter, TNSDebugAPI_PC

@ native SetProgramCounter 2
@ stack:SetProgramCounter(frame, pc) -> nil; it goes on there
        METHOD_IIN      SetProgramCounter, TNSDebugAPI_SetPC

@ native Receiver 1
@ stack:Receiver(frame) -> its self
        METHOD_I        Receiver, TNSDebugAPI_Receiver

@ native Implementor 1
@ stack:Implementor(frame) -> where its method was found
        METHOD_I        Implementor, TNSDebugAPI_Implementor

@ native GetVar 2
@ stack:GetVar(frame, index) -> an argument or local (arguments first)
        METHOD_II       GetVar, TNSDebugAPI_GetVar

@ native SetVar 3
@ stack:SetVar(frame, index, value) -> nil
        METHOD_IIV      SetVar, TNSDebugAPI_SetVar

@ native FindVar 2
@ stack:FindVar(frame, symbol) -> a variable by name (from its locals)
        METHOD_IS       FindVar, TNSDebugAPI_FindVar

@ native SetFindVar 3
@ stack:SetFindVar(frame, symbol, value) -> nil
        METHOD_ISV      SetFindVar, TNSDebugAPI_SetFindVar

@ native NumTemps 1
@ stack:NumTemps(frame) -> how many values it has pushed (temporaries)
        METHOD_I_INT    NumTemps, TNSDebugAPI_NumTemps

@ native TempValue 2
@ stack:TempValue(frame, index) -> a temporary (0 the deepest)
        METHOD_II       TempValue, TNSDebugAPI_TempValue

@ native SetTempValue 3
@ stack:SetTempValue(frame, index, value) -> nil
        METHOD_IIV      SetTempValue, TNSDebugAPI_SetTempValue

@ -- Printing (as NSDRefToHexString)

@ native RefToHexString 1
@ RefToHexString(obj) -> "#<its Ref in hex>", e.g. "#4410A1" (names objects
@ that have no name)
RefToHexString:
        stmfd   sp!, {lr}
        sub     sp, sp, #16             @ room for "#" and 8 digits
        ldr     r2, [r1]
        ldr     r2, [r2]                @ the Ref
        adr     r1, hexFormat
        mov     r0, sp
        bl      sprintf
        mov     r0, sp
        bl      MakeString
        add     sp, sp, #16
        ldmfd   sp!, {pc}

hexFormat:
        .asciz  "#%lX"
        .align  2

@ -- Helpers

@ ApiOf(rcvr, a RefArg) -> the TNSDebugAPI* in rcvr.nsDebugAPI
ApiOf:
        stmfd   sp!, {r4, lr}
        mov     r4, r0                  @ the receiver (a RefArg)
        adr     r0, nsDebugAPIName
        bl      Intern                  @ the symbol (may allocate: the receiver's Ref after it)
        mov     r1, r0
        ldr     r0, [r4]
        ldr     r0, [r0]
        bl      GetFrameSlotRef         @ rcvr.nsDebugAPI
        bl      BinaryData              @ a CObject binary: the object
        ldmfd   sp!, {r4, pc}

@ IntOf(a RefArg) -> its integer; throws (RINT's error) if it isn't one
IntOf:
        ldr     r0, [r0]
        ldr     r0, [r0]
        tst     r0, #3
        bne     RINTError               @ doesn't return
        mov     r0, r0, asr #2
        mov     pc, lr

nsDebugAPIName:
        .asciz  "nsDebugAPI"
        .align  2

@ -- The ROM, through its public jump table: ldr pc, [pc, #-4] jumps to the
@ word that follows, 0x01800000 + the entry's offset (names: newton-re
@ src/armcpu/PublicJumpTable.cpp)

        .macro  ROM name, offset
\name:
        ldr     pc, [pc, #-4]
        .word   0x01800000 + \offset
        .endm

        ROM     BinaryData,                     0x0828  @ BinaryData(long)
        ROM     GetFrameSlotRef,                0x092C  @ GetFrameSlotRef(long, long)
        ROM     Intern,                         0x094C  @ Intern(char*)
        ROM     RINTError,                      0x07E8  @ _RINTError(long)
        ROM     MakeString,                     0x099C  @ MakeString(const char*)
        ROM     sprintf,                        0x0160  @ sprintf(char*, const char*, ...)
        ROM     GetGInterpreter,                0x1EE8  @ GetGInterpreter()
        ROM     TInterpreter_SetBreakPoints,    0x1FF4  @ TInterpreter::SetBreakPoints(const RefVar&)
        ROM     TInterpreter_EnableBreakPoints, 0x20C0  @ TInterpreter::EnableBreakPoints(unsigned char)
        ROM     AllocateCObjectBinary,          0x27E0  @ AllocateCObjectBinary(void*, void(*)(void*), ...)
        ROM     TNSDebugAPI_NumStackFrames,     0x27F0  @ TNSDebugAPI::NumStackFrames()
        ROM     TNSDebugAPI_Function,           0x27F4  @ TNSDebugAPI::Function(long)
        ROM     TNSDebugAPI_AccurateStack,      0x27EC  @ TNSDebugAPI::AccurateStack()
        ROM     TNSDebugAPI_PC,                 0x27FC  @ TNSDebugAPI::PC(long)
        ROM     TNSDebugAPI_SetPC,              0x2800  @ TNSDebugAPI::SetPC(long, long)
        ROM     TNSDebugAPI_Receiver,           0x2804  @ TNSDebugAPI::Receiver(long)
        ROM     TNSDebugAPI_Implementor,        0x280C  @ TNSDebugAPI::Implementor(long)
        ROM     TNSDebugAPI_GetVar,             0x2818  @ TNSDebugAPI::GetVar(long, long)
        ROM     TNSDebugAPI_SetVar,             0x281C  @ TNSDebugAPI::SetVar(long, long, const RefVar&)
        ROM     TNSDebugAPI_FindVar,            0x2820  @ TNSDebugAPI::FindVar(long, const RefVar&)
        ROM     TNSDebugAPI_SetFindVar,         0x2824  @ TNSDebugAPI::SetFindVar(long, const RefVar&, const RefVar&)
        ROM     TNSDebugAPI_NumTemps,           0x2828  @ TNSDebugAPI::NumTemps(long)
        ROM     TNSDebugAPI_TempValue,          0x282C  @ TNSDebugAPI::TempValue(long, long)
        ROM     TNSDebugAPI_SetTempValue,       0x2830  @ TNSDebugAPI::SetTempValue(long, long, const RefVar&)
        ROM     NewNSDebugAPI,                  0x2838  @ NewNSDebugAPI(TInterpreter*)

        .ltorg
