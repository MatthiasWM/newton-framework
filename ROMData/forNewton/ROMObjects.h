/*
	File:		ROMObjects.h

	Contains:	Newton object system ROM Ref* object layout.
					This file is built automatically by ResMaker from the ROM image and RS symbols.
	Written by:	Newton Research Group, 2016.
*/

#if !defined(__ROMOBJECTS_H)
#define __ROMOBJECTS_H 1

#include <stdint.h>

/* A Ref-sized word of an object: a Ref (integer, immediate), a pointer Ref
   (an object's address + 1), or a C function's address. Initialized with
   address constants, so the data needs no code to set it up. */
typedef union ROMRef
{
	intptr_t		i;
	char *		p;
	void			(*f)(void);
} ROMRef;

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
_Static_assert(sizeof(ROMRef) == sizeof(void *), "a Ref is as wide as a pointer");
#endif

#define kROMRefSize		((intptr_t)sizeof(ROMRef))
#define kROMHeaderSize	(3 * kROMRefSize)
#define kROMFlagsBinary	((intptr_t)0x40 << 24)
#define kROMFlagsArray	((intptr_t)0x41 << 24)
#define kROMFlagsFrame	((intptr_t)0x43 << 24)

/* the size|flags word of an object */
#define NR_BINARY(_len)		{ .i = kROMHeaderSize + (_len) + kROMFlagsBinary }
#define NR_ARRAY(_len)		{ .i = kROMHeaderSize + (_len) * kROMRefSize + kROMFlagsArray }
#define NR_FRAMEMAP(_len)	{ .i = kROMHeaderSize + ((_len) + 1) * kROMRefSize + kROMFlagsArray }
#define NR_FRAME(_len)		{ .i = kROMHeaderSize + (_len) * kROMRefSize + kROMFlagsFrame }

/* Refs */
#define NR_0					{ .i = 0 }
#define NR_NIL				{ .i = 2 }
#define NR_REF(_r)			{ .i = (_r) }
#define NR_MAGIC(_n)		{ .i = ((intptr_t)(_n) << 2) + 3 }
#define NR_SYM(_label)		{ .p = (char *)&gROMSymbols._label + 1 }
#define NR_OBJ(_label)		{ .p = (char *)&gROMData._label + 1 }
#define NR_FUNC(_name)		{ .f = _name }

#endif	/* __ROMOBJECTS_H */
