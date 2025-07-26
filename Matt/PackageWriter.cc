/*
	File:		NewtonPackage.cc

	Abstract:	An object that loads a Newton package file containing 32-bit big-endian Refs
					and presents part entries as 64-bit platform-endian Refs.

	Written by:	Newton Research Group, 2015.
*/

#include "Newton.h"

#include "Objects.h"
#include "NewtonPackage.h"
#include "Matt/PackageWriter.h"
#include "PackageTypes.h"
#include "ObjHeader.h"
#include "Ref32.h"
#include "ROMResources.h"
#include "Symbols.h"

#include <ios>
#include <fstream>
#include <iostream>
#include <sstream>
#include <cassert>
#include <arpa/inet.h>


class BinaryOutStream: public std::ostringstream {
public:
  BinaryOutStream& operator<<(const uint32_t& v) {
    uint32_t vn = htonl(v);
    write((const char*)(&vn), 4);
    return *this;
  }
  BinaryOutStream& operator<<(const uint16_t& v) {
    uint16_t vn = htons(v);
    write((const char*)(&vn), 2);
    return *this;
  }
  BinaryOutStream& operator<<(const uint8_t& v) {
    write((const char*)(&v), 1);
    return *this;
  }
  void writeFourCC(RefArg id, const char *fallback=nullptr) {
    if (IsString(id)) {
      RefVar ascii = ASCIIString(id);
      write(BinaryData(ascii), Length(ascii)-1);
    } else if (IsBinary(id)) {
      write(BinaryData(id), Length(id));
    } else if (fallback) {
      write(fallback, strlen(fallback));
    } else {
      uint32_t nul = 0;
      write((const char*)&nul, 4);
    }
  }
  void writeUniString(RefArg id) {
    if (IsString(id)) {
      uint16_t *s = (uint16_t*)BinaryData(id);
      int n = Length(id)/2;
      for (int i=0; i<n; i++) {
        *this << s[i];
      }
    }
  }
  void alignTo4() {
    assert((size_t)tellp() == str().size());
    size_t len = str().size();
    size_t aligned_len = (len + 3) & ~0x00000003;
    size_t filler = aligned_len - len;
    if (filler) {
      uint32_t bf = 0xbfbfbfbf;
      write((const char*)&bf, aligned_len-len);
    }
  }
  void alignTo8() {
    assert((size_t)tellp() == str().size());
    size_t len = str().size();
    size_t aligned_len = (len + 7) & ~0x00000007;
    size_t filler = aligned_len - len;
    if (filler) {
      uint32_t bf[2] = { 0xbfbfbfbf, 0xbfbfbfbf };
      write((const char*)bf, aligned_len-len);
    }
  }
};


NewtonPackageWriter::NewtonPackageWriter()
{
}

/**
  \brief Create a .pkg file based on the provided pkg object.

  The pkg header must be defined like this:
  ```
  {
    signature: 'package0, // or 'package1
    id: "xxxx",           // optional FourCC string or binary, defaults ot "xxxx"
    flags: {              // flags are optional and set to true or nil, nil by default
      autoRemove: nil,
      copyProtect: nil,
      invisible: nil,
      noCompression: nil,
      relocation: nil,    // relocation is currently not supported (only in package1)
      useFasterCompression: nil
    },
    version: 1,           // optional, defaults to 1
    copyright: "(c)2025", // optional
    name: "mpg:SIG",
    creationDate: 12345,  // optional
    modifyDate: 12346,    // optional
    info: "Created by Matt", // optional (C-String)
    part: [
      // see package parts
    ]
  }
  ```
  The part header is defined as:
  ```
  {
    type: "form", // FourCC
    flags: {
      type: 'nos, // 'protocol, 'raw', and 'package are not supported
      autoLoad: nil,
      autoRemove: nil,
      compressed: nil,
      notify: nil,
      autoCopy: nil,
      nos2: nil
    },
    info: "A Newton Toolkit Application.", // ASCII String
    data: {  ... the NewtonOS FORM ... }
  }
 ```
 */
void NewtonPackageWriter::save(RefArg pkg, const std::string &filename)
{
  // Create all required streams.
  openStreams(pkg);

  // Write to all streams.
  writePackage(pkg);

  // Assemble and save the final package file.
  std::ofstream out(filename, std::ios_base::out | std::ios_base::binary);
  if (out.bad()) ThrowMsg("Failed to open file for writing");

  out << header_->str();
  for (const auto &dir : dir_) {
    out << dir->str();
  }
  out << varData_->str();
  for (const auto &part : part_) {
    out << part->str();
  }
}

/**
  \brief Open the streams for writing the package.

  This will check if the pkg is a valid package and prepare the streams.
  Throws an exception if the pkg is not a valid package.
 */
void NewtonPackageWriter::openStreams(RefArg package)
{
  if (!IsFrame(package)) ThrowMsg("Not a package: expected a frame");

  RefVar signature = GetFrameSlot(package, MakeSymbol("signature"));
  if (!IsSymbol(signature)) ThrowMsg("Not a package: needs signature");
  if (SymbolCompare(signature, MakeSymbol("package0"))==0) {
  } else if (SymbolCompare(signature, MakeSymbol("package1"))==0) {
  } else {
    ThrowMsg("Not a package: unknown signature");
  }

  RefVar part = GetFrameSlot(package, MakeSymbol("part"));
  if (!IsArray(part)) ThrowMsg("Not a package: no part array found");

  numParts_ = Length(part);
  if (numParts_ < 1) ThrowMsg("Not a package: no parts found");

  header_ = std::make_unique<BinaryOutStream>();
  varData_ = std::make_unique<BinaryOutStream>();
  dir_.resize(numParts_);
  part_.resize(numParts_);
  for (int i = 0; i < numParts_; ++i) {
    dir_[i] = std::make_unique<BinaryOutStream>();
    part_[i] = std::make_unique<BinaryOutStream>();
  }
}

/**
 \brief Calculate the size of the VarData section.

 This is needed for many offsets and sizes in the following code.
 */
uint32_t NewtonPackageWriter::calculateVarDataSize(RefArg package)
{
  uint32_t size = 0;

  // Header Copyright (UTF-16 String)
  RefVar copyright = GetFrameSlot(package, MakeSymbol("copyright"));
  if (IsString(copyright)) size += Length(copyright);

  // Header Name (UTF-16 String)
  RefVar name = GetFrameSlot(package, MakeSymbol("name"));
  if (IsString(name)) size += Length(name);

  // For all parts (ASCII + NUL)
  RefVar partArray = GetFrameSlot(package, MakeSymbol("part"));
  for (int i=0; i<numParts_; i++) {
    // Part Info
    RefVar part = GetArraySlot(partArray, i);
    RefVar info = GetFrameSlot(part, MakeSymbol("info"));
    if (IsString(info)) {
      Ref ascii = ASCIIString(info);
      const char *txt = BinaryData(ascii);
      size += (uint32_t)strlen(txt); // No(?) Terminating NUL character
    }
  }

  // Header Info (ASCII + NUL)
  RefVar info = GetFrameSlot(package, MakeSymbol("info"));
  if (IsString(info)) {
    Ref ascii = ASCIIString(info);
    const char *txt = BinaryData(ascii);
    size += (uint32_t)strlen(txt); // *No* terminating NUL character
  }

  // align to 4:
  size = (size + 3) & 0xfffffffc;

  return size;
}

void NewtonPackageWriter::writePackage(RefArg package)
{
  varDataSize_ = calculateVarDataSize(package);

  //   Byte signature[8];
  RefVar signature = GetFrameSlot(package, MakeSymbol("signature"));
  if (!IsSymbol(signature)) ThrowMsg("Not a package: needs signature");
  if (SymbolCompare(signature, MakeSymbol("package0"))==0) {
    header_->write("package0", 8);
  } else if (SymbolCompare(signature, MakeSymbol("package1"))==0) {
    header_->write("package1", 8);
  } else {
    ThrowMsg("Not a package: unknown signature");
  }

  //   ULong reserved1;
  header_->writeFourCC(GetFrameSlot(package, MakeSymbol("id")), "xxxx");

  //   ULong flags;
  uint32_t flags = 0;
  if (RefVar flagsRef = GetFrameSlot(package, MakeSymbol("flags"))) {
    if (GetFrameSlot(flagsRef, MakeSymbol("autoRemove")) == TRUEREF)
      flags |= kAutoRemoveFlag;
    if (GetFrameSlot(flagsRef, MakeSymbol("copyProtect")) == TRUEREF)
      flags |= kCopyProtectFlag;
    if (GetFrameSlot(flagsRef, MakeSymbol("invisible")) == TRUEREF)
      flags |= kInvisibleFlag;
    if (GetFrameSlot(flagsRef, MakeSymbol("noCompression")) == TRUEREF)
      flags |= kNoCompressionFlag;
    if (GetFrameSlot(flagsRef, MakeSymbol("relocation")) == TRUEREF)
      flags |= kRelocationFlag;
    if (GetFrameSlot(flagsRef, MakeSymbol("useFasterCompression")) == TRUEREF)
      flags |= kUseFasterCompressionFlag;
  }
  *header_ << flags;

  //   ULong version;
  RefVar version = GetFrameSlot(package, MakeSymbol("version"));
  if (!ISNIL(version)) {
    if (!IsInt(version)) ThrowMsg("Not a package: version must be an integer");
    *header_ << (uint32_t)RefToInt(version);
  } else {
    *header_ << (uint32_t)1;
  }

  //   InfoRef copyright
  int varDataSize = (int)varData_->str().size();
  RefVar copyright = GetFrameSlot(package, MakeSymbol("copyright"));
  if (IsString(copyright)) {
    *header_ << (uint16_t)varDataSize;
    *header_ << (uint16_t)Length(copyright);
    varData_->writeUniString(copyright);
  } else {
    *header_ << (uint16_t)varDataSize;
    *header_ << (uint16_t)0;
  }

  //   InfoRef name;
  varDataSize = (int)varData_->str().size();
  RefVar name = GetFrameSlot(package, MakeSymbol("name"));
  if (IsString(name)) {
    *header_ << (uint16_t)varDataSize;
    *header_ << (uint16_t)Length(name);
    varData_->writeUniString(name);
  } else {
    ThrowMsg("Not a package: no package name");
  }

  // To know the size of the file, we must generate all variable data
  // and all parts first.
  for (int i=0; i<numParts_; i++) {
    writePartDir(i, package);
  }

  // Now write the unofficial info chunk at the end of varData
  RefVar info = GetFrameSlot(package, MakeSymbol("info"));
  if (IsString(info)) {
    Ref ascii = ASCIIString(info);
    const char *txt = BinaryData(ascii);
    int length = (int)strlen(txt);
    varData_->write(txt, length);
  }
  // If you change this here, change this in calculateVarDataSize as well
  // align(varData_.get());
  varData_->alignTo4();

  //   ULong size;
  assert(varDataSize_ == (uint32_t)varData_->str().size());
  uint32_t dir_size = 52 + numParts_*32 + varDataSize_;
  uint32_t file_size = dir_size;
  for (int i=0; i<numParts_; i++) {
    file_size += part_[i]->str().size();
  }
  *header_ << file_size;

  //   Date creationDate;
  // Unsigned 32-bit integer representing a date and time as the
  // number of seconds since midnight, January 4, 1904
  RefVar creationDate = GetFrameSlot(package, MakeSymbol("creationDate"));
  if (IsInt(creationDate)) {
    *header_ << (uint32_t)RefToInt(creationDate);
  } else {
    *header_ << (uint32_t)0;
  }

  //   ULong reserved2;
  RefVar modifyDate = GetFrameSlot(package, MakeSymbol("modifyDate"));
  if (IsInt(modifyDate)) {
    *header_ << (uint32_t)RefToInt(modifyDate);
  } else {
    *header_ << (uint32_t)0;
  }

  //   ULong reserved3;
  *header_ << (uint32_t)0;

  //   ULong directorySize;
  *header_ << (uint32_t)dir_size;

  //   ULong numParts;
  *header_ << (uint32_t)numParts_;
}

void NewtonPackageWriter::writePartDir(int index, RefArg package) {
  RefVar partArray = GetFrameSlot(package, MakeSymbol("part"));
  if (!IsArray(partArray)) ThrowMsg("Not a package: no part array");

  RefVar part = GetArraySlot(partArray, index);
  if (!IsFrame(part)) ThrowMsg("Not a package: part description is not a frame");

  nos2_ = false;
  uint32_t flags = 0;
  RefVar flagsRef = GetFrameSlot(part, MakeSymbol("flags"));
  if (IsFrame(flagsRef)) {
    RefVar type = GetFrameSlot(flagsRef, MakeSymbol("type"));
    if (!IsSymbol(type)) ThrowMsg("Not a valid package: no part type set");
    if (SymbolCompare(type, MakeSymbol("protocol"))==0) {
      flags = 0; ThrowMsg("Unsupported package: part type 'protocol' not supported");
    } else if (SymbolCompare(type, MakeSymbol("nos"))==0) {
      flags = 1; // Yes, we support that
    } else if (SymbolCompare(type, MakeSymbol("raw"))==0) {
      flags = 2; ThrowMsg("Unsupported package: part type 'raw' not supported");
    } else if (SymbolCompare(type, MakeSymbol("package"))==0) {
      flags = 3; ThrowMsg("Unsupported package: part type 'package' not supported");
    }
    if (GetFrameSlot(flagsRef, MakeSymbol("autoLoad")) == TRUEREF)
      flags |= kAutoLoadPartFlag;
    if (GetFrameSlot(flagsRef, MakeSymbol("autoRemove")) == TRUEREF)
      flags |= kAutoRemovePartFlag;
    if (GetFrameSlot(flagsRef, MakeSymbol("compressed")) == TRUEREF)
      flags |= kCompressedFlag;
    if (GetFrameSlot(flagsRef, MakeSymbol("notify")) == TRUEREF)
      flags |= kNotifyFlag;
    if (GetFrameSlot(flagsRef, MakeSymbol("autoCopy")) == TRUEREF)
      flags |= kAutoCopyFlag;
    if (GetFrameSlot(flagsRef, MakeSymbol("nos2")) == TRUEREF)
      nos2_ = true;
  }

  // Write the directory to dir.
  BinaryOutStream *dir = dir_[index].get();

  //   ULong offset;
  uint32_t start = 0;
  for (int i=0; i<index; i++) {
    start += (uint32_t)part_[i]->str().size();
  }
  uint32_t dir_size = 52 + numParts_*32 + varDataSize_;
  partOffset_ = dir_size + start;
  *dir << start;

  // To get the size, we must generate the part data block first
  RefVar data = GetFrameSlot(part, MakeSymbol("data"));
  if (!IsFrame(data)) ThrowMsg("Unsupported package: part type 'nos' has no data frame");
  writePart(index, data);

  //   ULong size;
  uint32_t size = (uint32_t)part_[index]->str().size();
  *dir << size;

  //   ULong size2;
  *dir << size;

  //   ULong type;
  dir->writeFourCC(GetFrameSlot(part, MakeSymbol("type")), "xxxx");

  //   ULong reserved1;
  *dir << (uint32_t)0;

  //   ULong flags;
  *dir << flags;

  //   InfoRef info;
  int varDataSize = (int)varData_->str().size();
  RefVar info = GetFrameSlot(part, MakeSymbol("info"));
  if (IsString(info)) {
    // Documentation says write trailing NUL, but sample files do not.
    Ref ascii = ASCIIString(info);
    const char *txt = BinaryData(ascii);
    int length = (int)strlen(txt);
    varData_->write(txt, length);
    *dir << (uint16_t)varDataSize;
    *dir << (uint16_t)length;
  } else {
    *dir << (uint16_t)varDataSize;
    *dir << (uint16_t)0;
  }

  //   ULong reserved2;
  *dir << (uint32_t)0;
}


void NewtonPackageWriter::writePart(int index, RefArg data)
{
  precedent_.clear();
  fixup_.clear();

  // In OS 2.0, the low-order bit of the second long of this array—normally set to zero in all
  // objects—is used as an alignment flag. If the bit is set, the objects in the part are padded to
  // four-byte boundaries. Otherwise, the objects are padded to eight-byte boundaries.
  BinaryOutStream *out = part_[index].get();
  *out << (uint32_t)0x00001041;    // single element array
  if (nos2_)
    *out << (uint32_t)0x00000001;  // special flag for nos2 4 byte alignment
  else
    *out << (uint32_t)0x00000000;  // nos1 8 byte alignment
  *out << (uint32_t)0x00000002;    // array type is NIL
  // Remember to fix this reference later.
  auto pos = out->str().size();
  fixup_.insert(std::make_pair(pos, data));
  *out << (uint32_t)0x00000000;

  // Write the root object
  writeObject(out, data);

  // Write the fixup for all references that were not written yet
  for (const auto &f : fixup_) {
    out->seekp(f.first);
    auto p = precedent_.find(f.second);
    if (p == precedent_.end()) ThrowMsg("A fixup address has no precedent");
    *out << (uint32_t)(p->second + partOffset_ + 1);
  }
  // set the cursor to the end, just in case
  out->seekp(0, std::ios_base::end);
}

void NewtonPackageWriter::align(BinaryOutStream *out)
{
  if (nos2_) {
    out->alignTo4();
  } else {
    out->alignTo8();
  }
}

/**
  \brief Write a slotted object to the output stream.

  This will write the object header followed by all slots
 */
void NewtonPackageWriter::writeSlottedObject(BinaryOutStream *out, Ref frame)
{
  // Get information about the frame
  SlottedObject *f = (SlottedObject*)ObjectPtr(frame);
  assert(ISSLOTTED(f));
  uint32_t numSlots = (f->size - sizeof(ObjHeader)) / sizeof(Ref);

  // Now write the frame itself in 32 bit format
  *out << (uint32_t)(((8 + numSlots*4) << 8)|kObjReadOnly|f->flags);
  *out << (uint32_t)0;
  for (uint32_t i = 0; i < numSlots; ++i) {
    writeRef(out, f->slot[i]);
  }
  align(out);
  for (uint32_t i = 0; i < numSlots; ++i) {
    Ref slot = f->slot[i];
    if (ISREALPTR(slot))
      writeObject(out, slot);
  }
}

void NewtonPackageWriter::writeString(BinaryOutStream *out, Ref ref)
{
  assert(IsString(ref));
  BinaryObject *bin = (BinaryObject*)ObjectPtr(ref);
  uint32_t dataSize = bin->size - sizeof(ObjHeader) - sizeof(Ref);
  *out << (uint32_t)(((12 + dataSize) << 8)|kObjReadOnly|bin->flags);
  *out << (uint32_t)0;
  writeRef(out, bin->objClass);
  int n = dataSize / 2;
  uint16_t *str = (uint16_t*)bin->data;
  for (int i=0; i<n; i++) {
    *out << str[i]; // Convert to MSB byte order.
  }
  align(out);
  if (ISREALPTR(bin->objClass))
    writeObject(out, bin->objClass);
}

void NewtonPackageWriter::writeSymbol(BinaryOutStream *out, Ref ref)
{
  assert(IsSymbol(ref));
  SymbolObject *sym = (SymbolObject*)ObjectPtr(ref);
  uint32_t dataSize = sym->size - sizeof(ObjHeader) - sizeof(Ref);
  *out << (uint32_t)(((12 + dataSize) << 8)|kObjReadOnly|sym->flags);
  *out << (uint32_t)0;
  writeRef(out, sym->objClass);
  *out << sym->hash;
  out->write(sym->name, dataSize-sizeof(ULong));
  align(out);
  if (ISREALPTR(sym->objClass))
    writeObject(out, sym->objClass);
}

void NewtonPackageWriter::writeReal(BinaryOutStream *out, Ref ref)
{
  assert(IsReal(ref));
  BinaryObject *bin = (BinaryObject*)ObjectPtr(ref);
  uint32_t dataSize = bin->size - sizeof(ObjHeader) - sizeof(Ref);
  *out << (uint32_t)(((12 + dataSize) << 8)|kObjReadOnly|bin->flags);
  *out << (uint32_t)0;
  writeRef(out, bin->objClass);
  if (dataSize == 8) { // to be expected
    uint8_t *d = (uint8_t*)bin->data;
    *out << d[7] << d[6] << d[5] << d[4] << d[3] << d[2] << d[1] << d[0];
  } else { // fallback
    out->write(bin->data, dataSize);
  }
  align(out);
  if (ISREALPTR(bin->objClass))
    writeObject(out, bin->objClass);
}

/**
  \brief Write a binary object to the output stream.

  Copy binary data verbatim to the file. This is the last resort if we don't
  know the type of the object. We may miss byte swapping or other special
  handling of the binary data here.
 */
void NewtonPackageWriter::writeBinary(BinaryOutStream *out, Ref ref)
{
  assert(IsBinary(ref));
  BinaryObject *bin = (BinaryObject*)ObjectPtr(ref);
  uint32_t dataSize = bin->size - sizeof(ObjHeader) - sizeof(Ref);
  *out << (uint32_t)(((12 + dataSize) << 8)|kObjReadOnly|bin->flags);
  *out << (uint32_t)0;
  writeRef(out, bin->objClass);
  out->write(bin->data, dataSize);
  align(out);
  if (ISREALPTR(bin->objClass))
    writeObject(out, bin->objClass);
}

/**
  \brief Write an object to the output stream.

  If the object was already written to the file (a precedent exists), the
  method returns the file offset of the precedent.

  This will write the object based on its type. If it is a slotted object,
  it will call writeSlottedObject, otherwise it will call writeBinary.
 */
uint32_t NewtonPackageWriter::writeObject(BinaryOutStream *out, Ref ref)
{
  // Check arguments
  assert(ISREALPTR(ref));

  // Check if we already have a precedent for this object
  // and if so, return the original offset
  auto p = precedent_.find(ref);
  if (p != precedent_.end())
    return p->second;

  // Call the correct writer for this object type
  uint32_t pos = (uint32_t)out->str().size();
  precedent_.insert(std::make_pair(ref, pos));
  ObjHeader *o = ObjectPtr(ref);
  if (ISSLOTTED(o)) {
    writeSlottedObject(out, ref);
  } else if (IsString(ref)) {
    writeString(out, ref);
  } else if (IsSymbol(ref)) {
    writeSymbol(out, ref);
  } else if (IsReal(ref)) {
    writeReal(out, ref);
  } else {
    writeBinary(out, ref);
  }
  return pos;
}


uint32_t NewtonPackageWriter::writeRef(BinaryOutStream *out, Ref ref)
{
  uint32_t pos = (uint32_t)out->str().size();
  if (ISREALPTR(ref)) {
    auto p = precedent_.find(ref);
    if (p != precedent_.end()) {
      // We have a precedent, so write the offset
      *out << (uint32_t)(p->second + partOffset_ + 1);
    } else {
      // No precedent, so mark this for fixup later
      *out << (uint32_t)0xbadcafe0;
      fixup_.insert(std::make_pair(pos, ref));
    }
  } else {
    // We are going from 64 to 32 bit, so we may crop information here
    *out << (uint32_t)ref;
  }
  return pos;
}


void writePackageToFile(RefArg pkg, const std::string &filename) {
  NewtonPackageWriter out;
  out.save(pkg, filename);
}
