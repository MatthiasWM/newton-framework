/*
	File:		AgentReporter.cc

	Contains:	TAgentReporter: the test reporter that talks to the test
				agent (errors, a test's start and end, free memory).

	ROM:		0x20684C (TAgentReporter::TAgentReporter) .. 0x206BF0 (after
				ReportMemoryInfo), MP2x00 US 2.1 (717006).
*/

#include "Testing/TestAgent.h"
#include "Dates.h"
#include "Utilities/Unicode.h"
#include "NewtonMemory.h"
#include "CLibrary/stdio.h"
#include "CLibrary/string.h"


TAgentReporter::TAgentReporter(ULong inArg1, ULong inPortId, ULong inArg3)
	: TTestReporter(inArg1, inPortId, inArg3)
{ }


TAgentReporter::~TAgentReporter()
{
	TTestReporter::~TTestReporter();	// and again after this: harmless (BUGS.md B6)
}


void
TAgentReporter::AgentReportError(char * inErrMsg, char * inExplanation, long inErr)
{
	char buf[256];
	fNumOfErrorsReported++;
	sprintf(buf, "TestAgent ERR\t%d\t%s\t%s\n", inErr, inExplanation, inErrMsg);
	SendToTestAgent(2, buf, 0);
}


void
TAgentReporter::AgentReportStatus(long inSelector, char * inTestName)
{
	char		buf[256];
	char		counts[128];
	UniChar		uDateStr[32];
	UniChar		uTimeStr[32];
	char		dateStr[32];
	char		timeStr[32];
	TDate		now;

	now.SetCurrentTime();
	now.ShortDateString(kIncludeAllElements, uDateStr, sizeof(uDateStr));
	now.TimeString(kIncludeAllElements, uTimeStr, sizeof(uTimeStr));
	ConvertFromUnicode(uDateStr, dateStr, kMacRomanEncoding, sizeof(dateStr));
	ConvertFromUnicode(uTimeStr, timeStr, kMacRomanEncoding, sizeof(timeStr));

	char * name = inTestName;
	if (name == NULL)
		name = "";

	switch (inSelector)
	{
	case 2:
	case 5:
	case 8:
		{
		char * where = (inSelector == 5) ? "ttsk" : "newt";
		fNumOfErrorsReported = 0;
		sprintf(buf, "....................\nTestAgent MSG: test %s started in %s at %s, %s\n", name, where, timeStr, dateStr);
		}
		break;

	case 4:
	case 6:
	case 9:
		ReportMemoryInfo();
		sprintf(buf, "TestAgent MSG: test %s finished at %s, %s\n", name, timeStr, dateStr);
		sprintf(counts, ".....%d errors reported, %d errors logged\n", fNumOfErrorsReported, (fNumOfErrorsReported <= fNumOfErrorsLogged) ? fNumOfErrorsReported : fNumOfErrorsLogged);
		strcat(buf, counts);
		break;

	default:
		BlockMove(name, buf, kTestAgentMessageLen);
		break;
	}
	SendToTestAgent(5, buf, inSelector);
	if (inSelector == 2 || inSelector == 5)
		ReportMemoryInfo();
}


void
TAgentReporter::ReportMemoryInfo(void)
{
	char buf[256];
	sprintf(buf, "TestAgent MSG\tTotalSystemFree=%d\n", TotalSystemFree());
	SendToTestAgent(1, buf, 0);
}
