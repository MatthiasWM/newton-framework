/*
	File:		TestAgent.h

	Contains:	Reporting the results of tests to a test agent (reconstructed:
				not in the published headers; the port's CTestReporter and
				CAgentReporter, Apple's names).
*/

#ifndef __TESTAGENT_H
#define __TESTAGENT_H

#ifndef __NEWTON_H
#include "Newton.h"
#endif

#define kTestAgentMessageLen	224

class TTestReporter
{
public:
				TTestReporter(ULong inArg1, ULong inPortId, ULong inArg3);
				~TTestReporter();

	void		SendToTestAgent(ULong inType, char * inMsg, long inErr);

protected:
	UChar		fReserved[96];				// +000
	char		f60;						// +060
	UChar		f61[295];					// +061
	ULong		fPortId;					// +188
	ULong		f18C;						// +18C
	ULong		f190;						// +190
	ULong		f194;						// +194
	ULong		fNumOfErrorsLogged;			// +198
	ULong		fNumOfErrorsReported;		// +19C
};


class TAgentReporter : public TTestReporter
{
public:
				TAgentReporter(ULong inArg1, ULong inPortId, ULong inArg3);
				~TAgentReporter();

	void		AgentReportError(char * inErrMsg, char * inExplanation, long inErr);
	void		AgentReportStatus(long inSelector, char * inTestName);
	void		ReportMemoryInfo(void);
};

#endif	/* __TESTAGENT_H */
