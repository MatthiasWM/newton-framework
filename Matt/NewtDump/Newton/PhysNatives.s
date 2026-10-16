@ PhysNatives.s: newtdump's native functions on the Newton: read physical
@ memory (the ROM, PCMCIA cards) and keep a card powered. ARM (v4, the
@ StrongARM's: no Thumb, no bx), big-endian, position independent: the code
@ travels in a protocol extension (an NSOF object, no relocations), so our
@ own addresses come from the PC (adr), and the ROM is reached only through
@ its public jump table, virtual 0x01800000 + an entry's offset, the same in
@ every ROM build (names: newton-re src/armcpu/PublicJumpTable.cpp).
@
@ Physical memory is mapped as hardware drivers do it: a TUPhys (a kernel
@ object for a physical range) and a TLicenseeVAddress (a range of the
@ licensee domain, "LicD", 128 MB at 0xE0000000) that maps it there
@ (Map: RememberPhysMap and RememberPermMap, and the domain added to the
@ task's environment). Licensee addresses are handed out one after the
@ other and never given back (GetNextVirtualAddress), so the extension maps
@ each window once and keeps it (a global on the Newton).
@
@ Matt/tools/build_newtdump_natives.sh assembles this into PhysNatives.ns:
@ the code as hex and where each native starts; a native is a frame
@ {class: 'BinCFunction, code: <the binary>, numArgs, offset}.
@
@ A native is called as C: Ref F(RefArg rcvr, RefArg arg1, ...); r0 is the
@ receiver, r1... the arguments, each a RefArg: a pointer to a RefVar,
@ which holds a RefHandle*, which holds the Ref. The result is a Ref in r0.
@ An integer Ref is n << 2. Registers r4-r11 and sp are kept.
@
@ "@ native <name> <numArgs>" before an entry tells the build script.

        .syntax unified
        .arm
        .text
        .align  2

@ The state of a mapping: a binary of kStateSize bytes, made by the
@ extension: a TUPhys (a TUObject: its object id, a "created by us" flag)
@ and a TLicenseeVAddress (24 bytes: the phys object's id, the licensee
@ domain info, its domain, the virtual address, two flags).
        .equ    kPhys, 0                @ TUPhys
        .equ    kVAddress, 8            @ TLicenseeVAddress
        .equ    kVirtual, kVAddress + 16 @ its virtual address once mapped
        .equ    kStateSize, 32

        .equ    kReadOnly, 2            @ Perm: user read only

@ native PhysMap 3
@ PhysMap(state, firstPage, pageCount) -> 0, or an error: maps pageCount
@ pages of 4 KB of physical memory from firstPage * 4096 (pages, as a
@ NewtonScript integer has 30 bits: 0x4C000000 wouldn't fit), read only
@ and not cached (PCMCIA control registers must not be cached)
PhysMap:
        stmfd   sp!, {r4-r6, lr}
        sub     sp, sp, #4
        mov     r4, r1                  @ state (a RefArg)
        mov     r0, r2
        bl      IntOf
        mov     r5, r0, lsl #12         @ the physical address
        mov     r0, r3
        bl      IntOf
        mov     r6, r0, lsl #12         @ the size
        bl      StateOf                 @ r0 = the state's bytes (r4)
        mov     r1, #0                  @ start with empty objects
        mov     r2, #kStateSize
1:      subs    r2, r2, #4
        str     r1, [r0, r2]
        bne     1b
        mov     r1, #0                  @ cache: no (5th argument, on the stack)
        str     r1, [sp]
        mov     r1, r5                  @ base
        mov     r2, r6                  @ size
        mov     r3, #1                  @ read only
        bl      TUPhys_Init             @ TUPhys::Init(base, size, readOnly, cache)
        cmp     r0, #0
        bne     2f
        bl      StateOf
        ldr     r1, [r0, #kPhys]        @ the phys object's id
        add     r0, r0, #kVAddress
        bl      TLicenseeVAddress_ct    @ TLicenseeVAddress(physId), in place
        bl      StateOf
        add     r0, r0, #kVAddress
        mov     r1, #0                  @ not cached
        mov     r2, #kReadOnly
        bl      TLicenseeVAddress_Map   @ Map(cache, perm) -> error
2:      mov     r0, r0, lsl #2          @ the error as an integer
        add     sp, sp, #4
        ldmfd   sp!, {r4-r6, pc}

@ native PhysRead 4
@ PhysRead(state, offset, data, length) -> data, or the byte (an integer)
@ if all length bytes are the same (the desktop needs no more): copies
@ length bytes from offset in the mapped window into the binary data, as
@ 32-bit words (offset and length multiples of 4; PCMCIA memory reads as
@ words in the Newton's byte order). A bad address faults (the
@ extension's try reports it).
PhysRead:
        stmfd   sp!, {r4-r9, lr}
        mov     r4, r1                  @ state
        mov     r7, r3                  @ data (a RefArg)
        mov     r0, r2
        bl      IntOf
        mov     r5, r0                  @ offset
        ldr     r0, [sp, #28]           @ length: the 4th argument, on the stack
        bl      IntOf
        mov     r6, r0                  @ length
        bl      StateOf
        ldr     r8, [r0, #kVirtual]
        add     r8, r8, r5              @ the source
        ldr     r0, [r7]                @ data's Ref
        ldr     r0, [r0]
        bl      BinaryData
        mov     r9, r0                  @ the destination
        ldr     r2, [r8]                @ the first word: are all bytes like its first?
        and     r3, r2, #0xFF000000
        orr     r3, r3, r3, lsr #8
        orr     r3, r3, r3, lsr #16     @ its first byte, four times
        mov     r1, #1                  @ uniform so far
        mov     r12, #0
3:      cmp     r12, r6
        bhs     4f
        ldr     r2, [r8, r12]
        str     r2, [r9, r12]
        cmp     r2, r3
        movne   r1, #0
        add     r12, r12, #4
        b       3b
4:      cmp     r1, #0
        movne   r0, r3, lsr #24         @ all the same: the byte
        movne   r0, r0, lsl #2          @ as an integer
        ldreq   r0, [r7]                @ else the data
        ldreq   r0, [r0]
        ldmfd   sp!, {r4-r9, pc}

@ native PhysReadBytes 4
@ PhysReadBytes(state, offset, data, length) -> as PhysRead, with byte
@ reads: PCMCIA attribute memory (the CIS) answers only those (the ROM's
@ TCardCISIterator::ReadCIS reads it with ldrb; so does Einstein's card)
PhysReadBytes:
        stmfd   sp!, {r4-r9, lr}
        mov     r4, r1                  @ state
        mov     r7, r3                  @ data (a RefArg)
        mov     r0, r2
        bl      IntOf
        mov     r5, r0                  @ offset
        ldr     r0, [sp, #28]           @ length: the 4th argument, on the stack
        bl      IntOf
        mov     r6, r0                  @ length
        bl      StateOf
        ldr     r8, [r0, #kVirtual]
        add     r8, r8, r5              @ the source
        ldr     r0, [r7]                @ data's Ref
        ldr     r0, [r0]
        bl      BinaryData
        mov     r9, r0                  @ the destination
        ldrb    r3, [r8]                @ the first byte: are all like it?
        mov     r1, #1                  @ uniform so far
        mov     r12, #0
6:      cmp     r12, r6
        bhs     7f
        ldrb    r2, [r8, r12]
        strb    r2, [r9, r12]
        cmp     r2, r3
        movne   r1, #0
        add     r12, r12, #1
        b       6b
7:      cmp     r1, #0
        movne   r0, r3, lsl #2          @ all the same: the byte, as an integer
        ldreq   r0, [r7]                @ else the data
        ldreq   r0, [r0]
        ldmfd   sp!, {r4-r9, pc}

@ native CardPower 2
@ CardPower(socket, on) -> VccOn's result (an integer), or 0 for off:
@ VccOn(socket, 0) counts a user of the socket's Vcc and switches it on
@ for the first (waits until it is up, enables the bus); the ROM switches
@ a card off when it has no user and its timeout ran out. VccOff(socket)
@ ends our use.
CardPower:
        stmfd   sp!, {r4, lr}
        mov     r0, r1
        bl      IntOf
        mov     r4, r0                  @ socket
        ldr     r0, [r2]                @ on: nil (2) or not
        ldr     r0, [r0]
        cmp     r0, #2
        beq     5f
        mov     r0, r4
        mov     r1, #0
        bl      VccOn                   @ VccOn(socket, 0) -> bool
        and     r0, r0, #0xFF
        mov     r0, r0, lsl #2
        ldmfd   sp!, {r4, pc}
5:      mov     r0, r4
        bl      VccOff                  @ VccOff(socket)
        mov     r0, #0
        ldmfd   sp!, {r4, pc}

@ -- Helpers

@ StateOf() -> the bytes of the state binary (its RefArg in r4); a fresh
@ pointer each time: an allocation may move the binary
StateOf:
        ldr     r0, [r4]
        ldr     r0, [r0]
        b       BinaryData

@ IntOf(a RefArg) -> its integer; throws (RINT's error) if it isn't one
IntOf:
        ldr     r0, [r0]
        ldr     r0, [r0]
        tst     r0, #3
        bne     RINTError               @ doesn't return
        mov     r0, r0, asr #2
        mov     pc, lr

@ -- The ROM, through its public jump table: ldr pc, [pc, #-4] jumps to the
@ word that follows, 0x01800000 + the entry's offset

        .macro  ROM name, offset
\name:
        ldr     pc, [pc, #-4]
        .word   0x01800000 + \offset
        .endm

        ROM     BinaryData,             0x0828  @ BinaryData(long)
        ROM     RINTError,              0x07E8  @ _RINTError(long)
        ROM     TUPhys_Init,            0x1188  @ TUPhys::Init(unsigned long, unsigned long, unsigned char, unsigned char)
        ROM     TLicenseeVAddress_ct,   0x2554  @ TLicenseeVAddress::TLicenseeVAddress(unsigned long)
        ROM     TLicenseeVAddress_Map,  0x2AA8  @ TLicenseeVAddress::Map(unsigned char, Perm)
        ROM     VccOn,                  0x15A4  @ VccOn(int, unsigned char)
        ROM     VccOff,                 0x1598  @ VccOff(int)

        .ltorg
