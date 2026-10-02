/*
	File:		RecDomain.cc

	Contains:	TDomain, the base of the recognition domains.

	ROM:		0x20CD24 (TDomain::TDomain) .. 0x20D0F0 (after Group), MP2x00
				US 2.1 (717006).
*/

#include "Recognition/RecDomain.h"
#include "Recognition/RecGlue.h"
#include "Recognition/Msg.h"
#include "CLibrary/stdio.h"


TDomain::TDomain()
{ }


TDomain::~TDomain()
{ }


long
TDomain::PreGroup(TUnit * inUnit)
{
	return 0;
}


long
TDomain::PruneDictionary(TUnit * inUnit)
{
	return 0;
}


long
TDomain::PruneConstraints(TUnit * inUnit)
{
	return 0;
}


long
TDomain::CompleteUnit(void)
{
	return 0;
}


void
TDomain::Dump(TMsg * outMsg)
{
	char buf[100];
	DumpName(outMsg);
	if (verbose)
	{
		sprintf(buf, "\n\tFlags:  %ld \n", fFlags);
		outMsg->MsgStr(buf);
		sprintf(buf, "  %ld PieceTypes \n", fPieceTypes->fSize);
		outMsg->MsgStr(buf);
		for (ULong i = 0; i < fPieceTypes->fSize; i++)
		{
			outMsg->MsgStr("  ");
			outMsg->MsgType(fPieceTypes->GetType(i));
		}
	}
	outMsg->MsgLF();
}


void
TDomain::DumpName(TMsg * outMsg)
{
	char buf[100];
	sprintf(buf, "DOMAIN: %s ", fName);
	outMsg->MsgStr(buf);
	outMsg->MsgChar('(');
	outMsg->MsgType(fDomainType);
}


Boolean
TDomain::SetParameters(char ** inParams)
{
	if (fParams == inParams)
		return false;
	fParams = inParams;
	return true;
}


void
TDomain::InvalParameters(void)
{
	fParams = (char **) -1;
}


long
TDomain::DomainParameter(ULong inSelector, ULong ioParam, ULong inArg3)
{
	printf("DomainParameter(%ld, %ld, %ld)\n", inSelector, ioParam, inArg3);
	if (inSelector == 0)
		*(ULong *) ioParam = 0;
	return 0;
}


void
TDomain::ConfigureSubDomain(TRecArea * inArea)
{ }


TDomain *
TDomain::Make(TController * inController, ULong inType, char * inName)
{
	TDomain * domain = new TDomain;
	domain->IDomain(inController, inType, inName);
	return domain;
}


Boolean
TDomain::VUnitInClass(ULong inUnitType, ULong inClass)
{
	if (inClass == 'WORD' && (inUnitType == 'XRWR' || inUnitType == 'KANJ' || inUnitType == 'WREC'))
		return true;
	return false;
}


void
TDomain::IDomain(TController * inController, ULong inType, char * inName)
{
	fFlags = 0;
	f18 = 0;
	fController = inController;
	fDomainType = inType;
	fName = inName;
	f1C = 0;
	fPieceTypes = TTypeList::Make();
	fParams = NULL;
	NamePtr((char *) this, 'TDom');
}


void
TDomain::Dispose(void)
{
	fPieceTypes->Dispose();
	delete this;
}


ULong
TDomain::SizeInBytes(void)
{
	return TRecObject::SizeInBytes() + fPieceTypes->SizeInBytes();
}


void
TDomain::AddPieceType(ULong inType)
{
	fPieceTypes->AddUnique(inType);
	fPieceTypes->Compact();
}


void
TDomain::Classify(TUnit * inUnit)
{ }


void
TDomain::Reclassify(TUnit * inUnit)
{ }


long
TDomain::Group(TUnit * inUnit, dInfoRec * ioInfo)
{
	return 0;
}
