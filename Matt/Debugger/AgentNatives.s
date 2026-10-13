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
@ and sp are kept.
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

@ native NumStackFrames 0
@ stack:NumStackFrames() -> the number of frames (0 is the oldest)
NumStackFrames:
        stmfd   sp!, {lr}
        bl      ApiOf
        bl      TNSDebugAPI_NumStackFrames
        mov     r0, r0, lsl #2          @ an integer Ref
        ldmfd   sp!, {pc}

@ native Function 1
@ stack:Function(frame) -> the function running in that frame
Function:
        stmfd   sp!, {r4, r5, lr}
        mov     r4, r1
        bl      ApiOf
        mov     r5, r0
        mov     r0, r4
        bl      IntOf
        mov     r1, r0
        mov     r0, r5
        bl      TNSDebugAPI_Function
        ldmfd   sp!, {r4, r5, pc}

@ -- Helpers

@ ApiOf(rcvr, a RefArg) -> the TNSDebugAPI* in rcvr.nsDebugAPI
ApiOf:
        stmfd   sp!, {r4, lr}
        ldr     r4, [r0]
        ldr     r4, [r4]                @ the receiver's Ref
        adr     r0, nsDebugAPIName
        bl      Intern                  @ the symbol
        mov     r1, r0
        mov     r0, r4
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
        ROM     GetGInterpreter,                0x1EE8  @ GetGInterpreter()
        ROM     TInterpreter_SetBreakPoints,    0x1FF4  @ TInterpreter::SetBreakPoints(const RefVar&)
        ROM     TInterpreter_EnableBreakPoints, 0x20C0  @ TInterpreter::EnableBreakPoints(unsigned char)
        ROM     AllocateCObjectBinary,          0x27E0  @ AllocateCObjectBinary(void*, void(*)(void*), ...)
        ROM     TNSDebugAPI_NumStackFrames,     0x27F0  @ TNSDebugAPI::NumStackFrames()
        ROM     TNSDebugAPI_Function,           0x27F4  @ TNSDebugAPI::Function(long)
        ROM     NewNSDebugAPI,                  0x2838  @ NewNSDebugAPI(TInterpreter*)

        .ltorg
