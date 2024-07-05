/*
	File:		MemMgr.h

	Contains:	Private interfaces to memory management functions.

	Written by:	Newton Research Group.
*/

#if !defined(__MEMMGR_H)
#define __MEMMGR_H

#include "VirtualMemory.h"
#include "NewtonMemory.h"
#include "Semaphore.h"
#include "MemObjManager.h"
#include "SafeHeap.h"


struct MasterPtr
{
	Ptr			ptr;
	MasterPtr *	next;
};
typedef MasterPtr * MPPtr;


struct FakeBlock
{
	Ptr		ptr;
	unsigned	size : 30;
	unsigned	flags : 2;
};

struct SHeap;

union Block
{
	// all blocks use the first two bits as flags
	unsigned char	flags;			// +00

	// free blocks are doubly-linked
	struct {
		Size			size;				// +00
		Block *		next;				// +04
		Block *		prev;				// +08
	} free;

	// in-use blocks have extra info
	struct {
		unsigned char	flags;		// +00	-10
		unsigned char	delta;		// +01	-0F
		unsigned char	busy;			// +02	-0E
		unsigned char	type;			// +03	-0D
		Size				size;			// +04	including this header
		void *			link;			// +08	direct: heap to which this block belongs
											//			indirect: master pointer
		union {
			ObjectId		owner;
			ULong			name;			// +0C
		};
	} inuse;
};

/* Block flags
	kInUse		0x80
	kPrivate		0x10
	k?????		0x04
	kIndirect	0x02
	kDirect		0x01
*/

typedef void (*VoidProcPtr)(void);
typedef bool (*SizeProcPtr)(Size);
typedef bool (*ExtnProcPtr)(VAddr, Size);
typedef void (*BlocProcPtr)(Block*, Block*);


struct SHeap
{
	VAddr			start;			// +00
	VAddr			end;				// +04
	ULong			tag;				// +08	'skia'
	SHeap *		fixedHeap;		// +0C	pointers heap (fixed items); 'safe' if safe heap
	SHeap *		relocHeap;		// +10	handles heap (relocatable items)
	SWiredHeapDescr *	wiredHeap;		// +14	wired pointers heap
	void *		refCon;			// +18
	Size			free;				// +1C	free amount
	Block *		freeList;		// +20	chain of free blocks
	Block *		freeListTail;	// +24	reverse chain
	Size			size;				// +28	max heap size
	Size			extent;			// +2C	allocated heap size - starts off at 1 page
	Size			prevMaxExtent;	// +30
	Size			maxExtent;		// +34	largest extent recorded in VM heap (heap can shrink)
	Size			pageSize;		// +38	extent units
	bool			isVMBacked;		// +3C
	int			mpBlockSize;	// +40	number of master pointers per allocation
	MPPtr			freeMPList;		// +44
	Block *		x48;				// +48	last used?
	BlocProcPtr	x4C;
	SizeProcPtr	x50;
	VoidProcPtr	x54;
	ExtnProcPtr	x58;
	int			x5C;
	int			seed;				// +60
	CULockingSemaphore *	x64;	// +64	skia semaphore
	int			x68;				// +68	space lost to fragmentation?
	int			x6C;				// +6C
	int			x70;				// +70
	int			x74;				// +74
	int			x78;				// +78
	int			x7C;				// +7C
	int			x80;				// +80
	int			x84;				// +84
	int			x88;				// +88
	int			x8C;				// +8C
	int			x90;				// +90
	int			x94;				// +94
	SHeap *		masterPtrHeap;	// +98	MP heap
	SHeap *		x9C;				// +9C	SP heap
	int			xA0;
	int			xA4;
	Ptr			xA8;
	long			xAC;
	int			xB0;
	int			freeMPCount;	// +B4	number of free master pointers remaining
	int			xB8;
//	size +BC
};


/*------------------------------------------------------------------------------
	I n l i n e s
------------------------------------------------------------------------------*/

#define HeapPtr(p) ((SHeap*)p)

inline Heap		GetFixedHeap(Heap inHeap) { return HeapPtr(inHeap)->fixedHeap; }
inline void		SetFixedHeap(Heap inHeap, Heap inFixedHeap) { HeapPtr(inHeap)->fixedHeap = HeapPtr(inFixedHeap); }
inline Heap		GetRelocHeap(Heap inHeap) { return HeapPtr(inHeap)->relocHeap; }
inline void		SetRelocHeap(Heap inHeap, Heap inRelocHeap) { HeapPtr(inHeap)->relocHeap = HeapPtr(inRelocHeap); }
inline Heap		GetMPHeap(Heap inHeap) { return HeapPtr(inHeap)->masterPtrHeap; }
inline void		SetMPHeap(Heap inHeap, Heap inMPHeap) { HeapPtr(inHeap)->masterPtrHeap = HeapPtr(inMPHeap); }
inline Heap		GetSPHeap(Heap inHeap) { return HeapPtr(inHeap)->x9C; }
inline SWiredHeapDescr *	GetWiredHeap(Heap inHeap) { return HeapPtr(inHeap)->wiredHeap; }
inline void						SetWiredHeap(Heap inHeap, SWiredHeapDescr * inWiredHeap) { HeapPtr(inHeap)->wiredHeap = inWiredHeap; }

inline VAddr	GetHeapStart(Heap inHeap) { return HeapPtr(inHeap)->start; }
inline VAddr	GetHeapEnd(Heap inHeap) { return HeapPtr(inHeap)->end; }
inline Size		GetHeapSize(Heap inHeap) { return HeapPtr(inHeap)->end - HeapPtr(inHeap)->start; }
inline VAddr	GetHeapExtent(Heap inHeap) { return HeapPtr(inHeap)->extent; }
inline void		SetHeapExtentUnits(Heap inHeap, long inUnits) { HeapPtr(inHeap)->pageSize = inUnits; }
inline Size		GetHeapReleasable(Heap inHeap) { return HeapPtr(inHeap)->prevMaxExtent - HeapPtr(inHeap)->extent; }

inline void		SetHeapIsVMBacked(Heap inHeap) { HeapPtr(inHeap)->isVMBacked = true; }

inline bool		IsSkiaHeap(Heap inHeap) { return HeapPtr(inHeap)->tag == 'skia'; }

/*------------------------------------------------------------------------------
	D a t a
------------------------------------------------------------------------------*/

extern ULong		gNewtConfig;
extern bool			gOSIsRunning;

extern Heap			gSkiaHeapBase;
extern Heap			gKernelHeap;

extern ArrayIndex	gHandlesUsed;
extern ArrayIndex	gPtrsUsed;
extern ArrayIndex	gSavedHandlesUsed;
extern ArrayIndex	gSavedPtrsUsed;

extern bool			sMMBreak_Enabled;
extern Size			sMMBreak_LT;
extern Size			sMMBreak_GE;
extern ArrayIndex	sMMBreak_OnCCHash;
extern ArrayIndex	sMMBreak_OnCount;
extern Ptr			sMMBreak_AddressIn;
extern Ptr			sMMBreak_AddressOut;

extern ArrayIndex	gMemMgrCallCounter;
extern bool			sMMTagBlocksWithCCHash;

/*------------------------------------------------------------------------------
	H e a p   D e b u g   F u n c t i o n   P r o t o t y p e s
------------------------------------------------------------------------------*/

extern NewtonErr CheckHeap(Heap inHeap, void ** outWhereSmashed);
extern void			ReportSmashedHeap(const char * inDoingWhat, NewtonErr inErr, void * inWhere);
extern void			ReportMemMgrTrap(ULong inName);
extern ULong		HashCallChain(void);

/*------------------------------------------------------------------------------
	F u n c t i o n   P r o t o t y p e s
------------------------------------------------------------------------------*/

extern void		CreatePrivateBlock(Block * ioBlock, int inType);

extern Block *	NewBlock(Size inSize);

extern Ptr		NewDirectBlock(Size inSize);
extern void		DisposeDirectBlock(Ptr inBlock);
extern Size		GetDirectBlockSize(Ptr inBlock);
extern Ptr		SetDirectBlockSize(Ptr inBlock, Size inSize);

extern Handle	NewIndirectBlock(Size inSize);
extern void		DisposeIndirectBlock(Handle inBlock);
extern Size		GetIndirectBlockSize(Handle inBlock);
extern Ptr		SetIndirectBlockSize(Handle inBlock, Size inSize);

extern Handle	NewFakeIndirectBlock(void * inAddr, Size inSize);
extern Size		GetFakeIndirectBlockSize(Handle inBlock);
extern bool		IsFakeIndirectBlock(Handle inBlock);

extern Size		GetBlockPhysicalSize(void * inPtr);
extern Heap		GetBlockParent(void * inPtr);
extern UChar	GetBlockType(void * inPtr);
extern UChar	GetBlockBusy(void * inPtr);
extern void		SetBlockBusy(void * inPtr, UChar inBusyState);
extern void		IncrementBlockBusy(void * inPtr);
extern void		DecrementBlockBusy(void * inPtr);
extern UChar	GetBlockDelta(void * inPtr);
extern UChar	GetBlockFlags(void * inPtr);

extern void		ClearMemory(Ptr inMem, Size inSize);

// in Heap.cc
extern Heap		GetCurrentHeap(void);
extern void		SetCurrentHeap(Heap inHeap);

extern CULockingSemaphore *  GetHeapSemaphore(Heap inHeap);


#endif /* __MEMMGR_H */

