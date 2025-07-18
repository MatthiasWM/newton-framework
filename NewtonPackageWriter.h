/*
	File:		NewtonPackageWriter.h

	Abstract:	Takes a Newton Ref describing the package and writes it in
            package0 or package1 format.

	Written by:	Newton Research Group, 2015.
*/

#if !defined(__NEWTONPACKAGEWRITER_H)
#define __NEWTONPACKAGEWRITER_H 1

#include <stdio.h>
#include "PackageParts.h"
#include "Frames/ObjHeader.h"
#include <string>
#include <memory>
#include <vector>
#include <map>

class BinaryOutStream;

class NewtonPackageWriter
{
private:
	std::unique_ptr<BinaryOutStream> header_;
	std::unique_ptr<BinaryOutStream> varData_;
	std::vector<std::unique_ptr<BinaryOutStream>> dir_;
	std::vector<std::unique_ptr<BinaryOutStream>> part_;
	int numParts_ = 0;
	uint32_t partOffset_ = 1;
	uint32_t varDataSize_ = 0;
	bool nos2_ = false;

	std::map<Ref, uint32_t> precedent_;
	std::map<uint32_t, Ref> fixup_;

	void openStreams(RefArg pkg);
	uint32_t calculateVarDataSize(RefArg pkg);
	void writePackage(RefArg pkg);
	void writePartDir(int index, RefArg pkg);
	void writePart(int index, RefArg data);
	uint32_t writeRef(BinaryOutStream *out, Ref ref);
	uint32_t writeObject(BinaryOutStream *out, Ref ref);
	void writeSlottedObject(BinaryOutStream *out, Ref ref);
	void writeString(BinaryOutStream *out, Ref ref);
	void writeSymbol(BinaryOutStream *out, Ref ref);
	void writeBinary(BinaryOutStream *out, Ref ref);
	void align(BinaryOutStream *out);
public:
	NewtonPackageWriter();
	~NewtonPackageWriter() = default;
	void save(RefArg pkg, const std::string &filename);
};

void writePackageToFile(RefArg pkg, const std::string &filename);

#endif	/* __NEWTONPACKAGEWRITER_H */
