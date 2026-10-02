/*
	File:		FaxOptions.h

	Contains:	The fax tool's options (reconstructed: not in the published
				headers). Labels and layouts from the ROM's constructors and
				uses, with newton-re's findings; names ours.
*/

#ifndef __FAXOPTIONS_H
#define __FAXOPTIONS_H

#ifndef __OPTIONARRAY_H
#include "CommAPI/OptionArray.h"
#endif


#define kCMOFaxPageSetUp		'fpsu'
#define kCMOFaxRemoteId			'frid'
#define kCMOFaxLocalId			'flid'


/* 'fpsu': the page's set-up */
class TCMOFaxPageSetUp : public TOption
{
public:
					TCMOFaxPageSetUp();

	ULong			fLength;			// +0C  T.30's page length
	ULong			fWidth;				// +10  T.30's page width
	ULong			fResolution;		// +14  1 normal, 2 fine
};


/* 'frid': the other machine's identity (20 characters and a nought) */
class TCMOFaxRemoteId : public TOption
{
public:
					TCMOFaxRemoteId();

	char			fId[24];			// +0C
};


/* 'flid': ours */
class TCMOFaxLocalId : public TOption
{
public:
					TCMOFaxLocalId();

	char			fId[24];			// +0C
};

#endif	/* __FAXOPTIONS_H */
