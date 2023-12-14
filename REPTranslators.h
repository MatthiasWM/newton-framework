/*
	File:		REPTranslators.h

	Contains:	Read-Evaluate-Print I/O translators.

	Written by:	Newton Research Group.
*/

#if !defined(__REPTRANSLATORS_H)
#define __REPTRANSLATORS_H 1

#include "REP.h"

struct TranslatorInfo
{
	const char *	filename;
	size_t			bufSize;
};

struct StdioTranslatorInfo
{
	FILE *			fref;
	size_t			bufSize;
};


/*------------------------------------------------------------------------------
	P I n T r a n s l a t o r
	P-class interface.
	The protocol upon which all input translators must be based.
------------------------------------------------------------------------------*/

PROTOCOL PInTranslator : public CProtocol
{
public:
	static PInTranslator *	make(const char * inName);
  // -- inherited for CProtocol
	void			destroy(void);
  // -- added for PInTranslator
	virtual NewtonErr	init(void * inContext) = 0;
	virtual Timeout		idle(void) = 0;
	virtual bool			frameAvailable(void) = 0;
	virtual Ref			produceFrame(int inLevel) = 0;
};


/*------------------------------------------------------------------------------
	P H a m m e r I n T r a n s l a t o r
	Input from the debugger.
------------------------------------------------------------------------------*/

PROTOCOL PHammerInTranslator : public PInTranslator
{
public:
    // -- inherited from CProtocol
	PROTOCOL_IMPL_HEADER_MACRO(PHammerInTranslator)
	PHammerInTranslator *	make(void) override;
	void			destroy(void) override;
    // -- inherited PInTranslator
	NewtonErr	init(void * inContext) override;
	Timeout		idle(void) override;
	bool			frameAvailable(void) override;
	Ref			produceFrame(int inLevel) override;
    // -- end of protocol

private:
	FILE *	fileRef;
	char *	fBuf;
	size_t	fBufSize;
	long		f1C;
	long		f20;
};


/*------------------------------------------------------------------------------
	P N u l l I n T r a n s l a t o r
	Input from the bit bucket.
------------------------------------------------------------------------------*/

PROTOCOL PNullInTranslator : public PInTranslator
{
public:
    // -- inherited from CProtocol
	PROTOCOL_IMPL_HEADER_MACRO(PNullInTranslator)
	PNullInTranslator *	make(void) override;
	void			destroy(void) override;
    // -- inherited from PInTranslator
	NewtonErr	init(void * inContext) override;
	Timeout		idle(void) override;
	bool			frameAvailable(void) override;
	Ref			produceFrame(int inLevel) override;
};


/*------------------------------------------------------------------------------
	P S t d i o I n T r a n s l a t o r
	Input from stdio.
------------------------------------------------------------------------------*/

PROTOCOL PStdioInTranslator : public PInTranslator
{
public:
    // -- inherited from CProtocol
	PROTOCOL_IMPL_HEADER_MACRO(PStdioInTranslator)
	PStdioInTranslator *	make(void) override;
	void			destroy(void) override;
    // -- inherited from PInTranslator
	NewtonErr	init(void * inContext) override;
	Timeout		idle(void) override;
	bool			frameAvailable(void) override;
	Ref			produceFrame(int inLevel) override;
    // -- end of protocol

private:
	FILE *	fileRef;
	char *	fBuf;
	size_t	fBufSize;
};


/*------------------------------------------------------------------------------
	P O u t T r a n s l a t o r
	P-class interface.
	The protocol upon which all output translators must be based.
------------------------------------------------------------------------------*/

PROTOCOL POutTranslator : public CProtocol
{
public:
	static POutTranslator *	make(const char * inName);
	void			destroy(void) override;

	virtual NewtonErr	init(void * inContext) = 0;
	virtual Timeout		idle(void) = 0;
	virtual void consumeFrame(RefArg inObj, int inDepth, int indent) = 0;
	virtual void prompt(int inLevel) = 0;
  virtual int print(const char * inFormat, ...) = 0;
	virtual int	vprint(const char * inFormat, va_list args) = 0;	// not in original, but required for REPprintf
	virtual int			putc(int inCh) = 0;
	virtual void flush(void) = 0;

	virtual void			enterBreakLoop(int) = 0;
	virtual void			exitBreakLoop(void) = 0;

	virtual void			stackTrace(void * interpreter) = 0;
	virtual void			exceptionNotify(Exception * inException) = 0;
};


/*------------------------------------------------------------------------------
	P H a m m e r O u t T r a n s l a t o r
	Output to the debugger.
------------------------------------------------------------------------------*/

PROTOCOL PHammerOutTranslator : public POutTranslator
{
public:
    // -- inherited from CProtocol
	PROTOCOL_IMPL_HEADER_MACRO(PHammerOutTranslator)
	PHammerOutTranslator *	make(void) override;
	void			destroy(void) override;
    // -- inherited from POutTranslator
	NewtonErr	init(void * inContext) override;
	Timeout		idle(void) override;
	void			consumeFrame(RefArg inObj, int inDepth, int indent) override;
	void			prompt(int inLevel) override;
	int			print(const char * inFormat, ...) override;
	int			vprint(const char * inFormat, va_list args) override;
	int			putc(int inCh) override;
	void			flush(void) override;

	void			enterBreakLoop(int) override;
	void			exitBreakLoop(void) override;

	void			stackTrace(void * interpreter) override;
	void			exceptionNotify(Exception * inException) override;
    // -- end of protocol

private:
	FILE *	fileRef;
	char *	fBuf;
};


/*------------------------------------------------------------------------------
	P N u l l O u t T r a n s l a t o r
	Output to the bit bucket.
------------------------------------------------------------------------------*/

PROTOCOL PNullOutTranslator : public POutTranslator
{
public:
    // -- inhertied from CProtocol
	PROTOCOL_IMPL_HEADER_MACRO(PNullOutTranslator)
	PNullOutTranslator *	make(void) override;
	void			destroy(void) override;
    // -- inherited from POutTranslator
	NewtonErr	init(void * inContext) override;
	Timeout		idle(void) override;
	void			consumeFrame(RefArg inObj, int inDepth, int indent) override;
  void      prompt(int inLevel) override;
  int      print(const char * inFormat, ...) override;
  int      vprint(const char * inFormat, va_list args) override;
  int      putc(int inCh) override;

	void			flush(void) override;

	void			enterBreakLoop(int) override;
	void			exitBreakLoop(void) override;

	void			stackTrace(void * interpreter) override;
	void			exceptionNotify(Exception * inException) override;
    // -- end of protocol

private:

	bool		fIsInBreakLoop;
};


/*------------------------------------------------------------------------------
	P S t d i o O u t T r a n s l a t o r
	Output to stdio.
------------------------------------------------------------------------------*/

PROTOCOL PStdioOutTranslator : public POutTranslator
{
public:
    // -- override from CProtocol
	PROTOCOL_IMPL_HEADER_MACRO(PStdioOutTranslator)
	PStdioOutTranslator *	make(void) override;
	void			destroy(void) override;
    // -- override from POutTranslator
	NewtonErr	init(void * inContext) override;
	Timeout		idle(void) override;
	void			consumeFrame(RefArg inObj, int inDepth, int indent) override;
	void			prompt(int inLevel) override;
	int			print(const char * inFormat, ...) override;
	int			vprint(const char * inFormat, va_list args) override;
	int			putc(int inCh) override;
	void			flush(void) override;

	void			enterBreakLoop(int) override;
	void			exitBreakLoop(void) override;

	void			stackTrace(void * interpreter) override;
	void			exceptionNotify(Exception * inException) override;
    // -- end of protocol

private:
	FILE *	fileRef;
};


extern PInTranslator *	gREPin;
extern POutTranslator *	gREPout;

#endif	/* __REPTRANSLATORS_H */
