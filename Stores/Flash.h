/*
	File:		Flash.h

	Contains:	Newton flash memory interface.

	Written by:	Newton Research Group, 2010.
*/

#if !defined(__FLASH_H)
#define __FLASH_H 1

#include "PSSTypes.h"
#include "Protocols.h"
#include "VirtualMemory.h"
#include "FlashDriver.h"

class CCardSocket;
class CCardPCMCIA;

/* -----------------------------------------------------------------------------
	C U L o c k i n g S e m a p h o r e G r a b b e r
----------------------------------------------------------------------------- */
class CULockingSemaphore;

class CULockingSemaphoreGrabber
{
public:
enum eNonBlockOption
{
	kOption0,
	kOption1
};
			CULockingSemaphoreGrabber(CULockingSemaphore * inSemaphore);
			CULockingSemaphoreGrabber(CULockingSemaphore * inSemaphore, eNonBlockOption inOption);
			~CULockingSemaphoreGrabber();

	void	doAcquire(CULockingSemaphore * inSemaphore);

private:
	CULockingSemaphore * fSemaphore;
};


/*------------------------------------------------------------------------------
	C F l a s h
	P-class interface.
	Interface to the flash hardware, typically on a PCMCIA card, but also internal.
	Implementations are TFlashAMD, TFlashSeries2, TNewInternalFlash.
------------------------------------------------------------------------------*/

PROTOCOL CFlash : public CProtocol
{
public:
	static CFlash *	make(const char * inName);
	virtual void			destroy(void) = 0;

	virtual NewtonErr	read(ZAddr inAddr, size_t inLength, char * inBuffer) = 0;
	virtual NewtonErr	write(ZAddr inAddr, size_t inLength, char * inBuffer) = 0;

	virtual NewtonErr	erase(ZAddr) = 0;
  virtual void			suspendErase(ULong, ULong, ULong) = 0;
  virtual void			resumeErase(ULong) = 0;

	virtual void			deepSleep(ULong) = 0;
	virtual void			wakeup(ULong) = 0;
	virtual NewtonErr	status(ULong) = 0;

	virtual void			resetCard(void) = 0;
	virtual void acknowledgeReset(void) = 0;
	virtual void			getPhysResource(void) = 0;
	virtual void			registerClientInfo(ULong) = 0;

  virtual void			getWriteProtected(bool * outWP) = 0;
  virtual void			getWriteErrorAddress(void) = 0;
	virtual ULong getAttributes(void) = 0;
  virtual ULong			getDataOffset(void) = 0;
	virtual size_t getTotalSize(void) = 0;
  virtual size_t		getGroupSize(void) = 0;
	virtual size_t getEraseRegionSize(void) = 0;

	virtual ULong			getChipsPerGroup(void) = 0;
	virtual ULong			getBlocksPerPartition(void) = 0;

  virtual ULong			getMaxConcurrentVppOps(void) = 0;
  virtual ULong			getEraseRegionCurrent(void) = 0;
  virtual ULong			getWriteRegionCurrent(void) = 0;
  virtual ULong			getEraseRegionTime(void) = 0;
  virtual ULong			getWriteAccessTime(void) = 0;
  virtual ULong			getReadAccessTime(void) = 0;

	virtual ULong			getVendorInfo(void) = 0;
	virtual int			getSocketNumber(void) = 0;

  virtual ULong			vppStatus(void) = 0;
  virtual ULong			vppRisingTime(void) = 0;

  virtual void			flashSpecific(ULong, void*, ULong) = 0;

  virtual NewtonErr	initialize(CCardSocket*, CCardPCMCIA*, ULong, ULong) = 0;
  virtual NewtonErr	suspendService(void) = 0;
  virtual NewtonErr	resumeService(CCardSocket*, CCardPCMCIA*, ULong) = 0;

  virtual NewtonErr	copy(ZAddr inFromAddr, ZAddr inToAddr, size_t inLength) = 0;
  virtual bool isVirgin(ZAddr inAddr, size_t inLength) = 0;
};


/* -----------------------------------------------------------------------------
	C N e w I n t e r n a l F l a s h
	P-class implementation.
----------------------------------------------------------------------------- */
typedef NewtonErr (CFlashRange::*FlashRWProcPtr)(VAddr inAddr, size_t inLength, char * inBuffer);

PROTOCOL CNewInternalFlash : public CFlash
{
public:
    // inherited from CProtocol
	PROTOCOL_IMPL_HEADER_MACRO(CNewInternalFlash)
	CNewInternalFlash *	make(void) override;
	void			destroy(void) override;

// ---- initialization
	enum eInitHWOption
	{
		kHWNoMMU,
		kHWUseMMU
	};
	enum eCheckEraseOption
	{
		kEraseAsync,
		kEraseSync
	};

    // -- inherited from CFlash

    //"    .long    __ZN17CNewInternalFlash9vppStatusEv - 4b  \n"
    //"    .long    __ZN17CNewInternalFlash13vppRisingTimeEv - 4b  \n"
    //"    .long    __ZN17CNewInternalFlash13flashSpecificEjPvj - 4b  \n"
    //"    .long    __ZN17CNewInternalFlash10initializeEP11CCardSocketP11CCardPCMCIAjj - 4b  \n"
    //"    .long    __ZN17CNewInternalFlash14suspendServiceEv - 4b  \n"
    //"    .long    __ZN17CNewInternalFlash13resumeServiceEP11CCardSocketP11CCardPCMCIAj - 4b  \n"
    //"    .long    __ZN17CNewInternalFlash4copyEjjm - 4b  \n"
    //"    .long    __ZN17CNewInternalFlash8isVirginEjm - 4b  \n"
    NewtonErr  read(ZAddr inAddr, size_t inLength, char * inBuffer) override;
    NewtonErr  write(ZAddr inAddr, size_t inLength, char * inBuffer) override;

    NewtonErr  erase(ZAddr) override;
    void      suspendErase(ULong, ULong, ULong) override;
    void      resumeErase(ULong) override;

    void      deepSleep(ULong) override;
    void      wakeup(ULong) override;
    NewtonErr  status(ULong) override;

    void      resetCard(void) override;
    void      acknowledgeReset(void) override;
    void      getPhysResource(void) override;
    void      registerClientInfo(ULong) override;

    void      getWriteProtected(bool * outWP) override;
    void      getWriteErrorAddress(void) override;
    ULong      getAttributes(void) override;
    ULong      getDataOffset(void) override;
    size_t    getTotalSize(void) override;
    size_t    getGroupSize(void) override;
    size_t    getEraseRegionSize(void) override;

    ULong      getChipsPerGroup(void) override;
    ULong      getBlocksPerPartition(void) override;

    ULong      getMaxConcurrentVppOps(void) override;
    ULong      getEraseRegionCurrent(void) override;
    ULong      getWriteRegionCurrent(void) override;
    ULong      getEraseRegionTime(void) override;
    ULong      getWriteAccessTime(void) override;
    ULong      getReadAccessTime(void) override;

    ULong      getVendorInfo(void) override;
    int      getSocketNumber(void) override;

    ULong      vppStatus(void) override;
    ULong      vppRisingTime(void) override;

    void      flashSpecific(ULong, void*, ULong) override;

    NewtonErr  initialize(CCardSocket*, CCardPCMCIA*, ULong, ULong) override;
    NewtonErr  suspendService(void) override;
    NewtonErr  resumeService(CCardSocket*, CCardPCMCIA*, ULong) override;

    NewtonErr  copy(ZAddr inFromAddr, ZAddr inToAddr, size_t inLength) override;
    bool      isVirgin(ZAddr inAddr, size_t inLength) override;
    // -- end of protocol

	NewtonErr	init(CMemoryAllocator * inAllocator);
	NewtonErr	internalInit(CMemoryAllocator * inAllocator, eInitHWOption inOptions);
	NewtonErr	initializeState(CMemoryAllocator * inAllocator, eInitHWOption inOptions);

	void			clobber(void);
	void			cleanUp(void);

private:
	friend class CFlashRange;
	NewtonErr	readPhysical(ZAddr inAddr, size_t inLength, char * inBuffer);
	NewtonErr	writePhysical(ZAddr inAddr, size_t inLength, char * inBuffer);
	NewtonErr	readWrite(FlashRWProcPtr inRW, ZAddr inAddr, size_t inLength, char * inBuffer);
	NewtonErr	readWritePhysical(FlashRWProcPtr inRW, ZAddr inAddr, size_t inLength, char * inBuffer);
	NewtonErr	setupVirtualMappings(void);
	NewtonErr	gatherBlockMappingInfo(ULong & ioArg1, ULong & ioArg2, ULong & ioArg3, ULong & ioArg4);
	NewtonErr	flashAllowedLocations(bool * outArg1, bool * outArg2);
	void			avoidConflictWithRExInIOSpace(bool * outBank);
	void			alignAndMapVMRange(VAddr& ioAddr, PAddr inPhysAddr, size_t inSize, bool inCacheable, Perm inPerm);
	NewtonErr	configureFlashBank(VAddr& ioBankAddr, VAddr& inROAddr, VAddr& inRWAddr);
	NewtonErr	configureIOBank(VAddr& ioBankAddr, VAddr& inROAddr, VAddr& inRWAddr);
	NewtonErr	addFlashRange(CFlashRange * inRange, VAddr& ioBankAddr, VAddr& inROAddr, VAddr& inRWAddr, PAddr inPhysAddr);
	NewtonErr	findRange(PAddr inAddr, CFlashRange *& outRange);

	bool			checkFor4LaneFlash(VAddr, SFlashChipInformation&, CFlashDriver*&);
	bool			checkFor2LaneFlash(VAddr, SFlashChipInformation&, CFlashDriver*&, eMemoryLane);
	bool			checkFor1LaneFlash(VAddr, SFlashChipInformation&, CFlashDriver*&, eMemoryLane);
	NewtonErr	searchForFlashDrivers(void);
	bool			findDriverAble(CFlashDriver*&, VAddr, eMemoryLane, SFlashChipInformation&);

	NewtonErr	syncErasePhysicalBlock(PAddr inAddr);
	bool			internalCheckEraseCompletion(NewtonErr& outErr, eCheckEraseOption inOption);

	NewtonErr	copyUsingBuffer(ZAddr inFromAddr, ZAddr inToAddr, size_t inLength, void * inBuffer, size_t inBufLen);

	void			internalClobber(void);
	void			turnPowerOn(void);
	void			turnPowerOff(void);
	void			internalVppOn(void);
	void			internalVppOff(void);

	CMemoryAllocator *	fAllocator { nullptr };	//+10
	ArrayIndex		fNumOfRanges { 0 };		//+14
	size_t			fStoreSize { 0 };			//+18
	size_t			fBlockSize { 0 };			//+1C
	CFlashDriver *	fDriver[6] { nullptr };			//+20
	CFlashRange *	fRange[2] { nullptr };			//+38
	int				f40 { 0 };
	CFlashRange *	f44 { 0 };
	NewtonErr		fEraseErr { 0 };			//+48
	ArrayIndex		fNumOfDrivers { 0 };		//+4C
	CBankControlRegister * fBCR { nullptr };		//+50
	eInitHWOption	f54 { kHWNoMMU };					//+54
	bool				f58 { false };
	UShort *			fBlockMap { nullptr };			//+5C
	ArrayIndex		fNumOfBlocks { 0 };		//+60
	ULong				f64 { 0 };	// possibly UShort: it’s an entry in the fBlockMap
//	CULockingSemaphore *	fSemaphore;	//+68
//size+6C
};


#endif	/* __FLASH_H */
