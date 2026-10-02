/*
	File:		Msg.h

	Contains:	TMsg, the recognizers' debug messages (reconstructed: not in
				the published headers).
*/

#ifndef __MSG_H
#define __MSG_H

#ifndef __NEWTON_H
#include "Newton.h"
#endif

class TMsg
{
public:
	static TMsg *	Make(void);
	static TMsg *	Msg(char * inStr);

	void		Dispose(void);
	void		MsgLF(void);
	void		MsgPrintf(void);
	void		MsgStr(char * inStr);
	void		MsgChar(char inChar);
	void		MsgType(ULong inType);
	void		MsgNum(ULong inNum, long inWidth);
	void		MsgHex(ULong inNum, long inWidth);
};

/* Whether dumps say more */
extern long		verbose;

#endif	/* __MSG_H */
