#include "Region.h"



#include "KdUtils.h"

#include <iostream>
using namespace Dyninst;
using namespace SymtabAPI;

bool endsWith(const std::string &suffix, const std::string &str) {
  if (suffix.length() > str.length())
    return false;

  return str.substr(str.length() - suffix.length()) == suffix;
}

bool startsWith(const std::string &prefix, const std::string &str) {
  if (prefix.length() > str.length())
    return false;

  return str.substr(0, prefix.length()) == prefix;
}

bool isKernelDescriptor(const Dyninst::SymtabAPI::Symbol *symbol) {
  // symbol
  // - is in symtab, not dynsym
  // - type is object, size is 64
  // - is in rodata section
  // - name ends with ".kd"

  if (symbol->isInDynSymtab())
    return false;

  bool c1 = symbol->getType() == Symbol::ST_OBJECT && symbol->getSize() == 64;

  const Region *region = symbol->getRegion();
  bool c2 = region->isData() && region->getRegionPermissions() == Region::RP_R;

  if (c1 && c2 == false)
    return false;

  std::string suffix = ".kd";
  return endsWith(suffix, symbol->getMangledName());
}

void dumpMsgpackObjType(msgpack::object &object) {
  switch (object.type) {
  case msgpack::type::NIL:
    std::cout << "nil" << '\n';
    break;

  case msgpack::type::BOOLEAN:
    std::cout << "boolean" << '\n';
    break;

  case msgpack::type::POSITIVE_INTEGER:
    std::cout << "uint" << '\n';
    break;

  case msgpack::type::NEGATIVE_INTEGER:
    std::cout << "int" << '\n';
    break;

  case msgpack::type::FLOAT32:
    std::cout << "float 32" << '\n';
    break;

  case msgpack::type::FLOAT64:
    std::cout << "float 64" << '\n';
    break;

  case msgpack::type::STR:
    std::cout << "str" << '\n';
    break;

  case msgpack::type::BIN:
    std::cout << "bin" << '\n';
    break;

  case msgpack::type::EXT:
    std::cout << "ext" << '\n';
    break;

  case msgpack::type::ARRAY:
    std::cout << "arrray" << '\n';
    break;

  case msgpack::type::MAP:
    std::cout << "map" << '\n';
    break;
  }
}

unsigned getKernargPtrRegister(KDPtr kd) {
  unsigned kernargPtrReg = 0;
  if (kd->getKernelCodeProperty_EnableSgprPrivateSegmentBuffer()) {
    kernargPtrReg += 4;
  }

  if (kd->getKernelCodeProperty_EnableSgprDispatchPtr()) {
    kernargPtrReg += 2;
  }

  if (kd->getKernelCodeProperty_EnableSgprQueuePtr()) {
    kernargPtrReg += 2;
  }

  return kernargPtrReg;
}

int getNewArgOffset(KDPtr kd, const char *sectionContents, size_t length) {
  /*
   * Step 1, read entire .note section into buffer
   */
  std::stringstream ss;
  ss.write(sectionContents, length);

  std::map<std::string, msgpack::object> kvmap;
  std::vector<msgpack::object> kernargList;
  std::map<std::string, msgpack::object> kernargListMap;
  std::string str = ss.str();
  msgpack::zone z;

  uint32_t offset = 0;

  /**
   * Step 2, parse some non-msgpack header data
   * First 4 bytes : Size of the Name str (should be AMDGPU\0)
   * Second 4 bytes: Size of the note in msgpack format
   * Third 4 bytes : Type of the note (should be 32)
   * Followed by the Name str
   * Padding until 4 byte aligned
   * Followed by msgpack data
   */

  msgpack::object_handle oh;
  std::string name_szstr = str.substr(0, 4);
  uint32_t name_sz = *((uint32_t *)name_szstr.c_str());
  std::string noteType = str.substr(8, 4);
  std::string name = str.substr(12, name_sz);
  offset = 12 + name_sz;

  while (offset % 4) {
    offset++;
  }

  /*
   * Now we are ready to process the msgpack data
   * We unpack from offset calculated above
   * Unpack until we get to .group_segment_fixed_size
   *
   */
  // std::cout << str.size() - offset << '\n';
  oh = msgpack::unpack(str.data() + offset, str.size() - offset);
  msgpack::object mapRoot = oh.get();
  mapRoot.convert(kvmap);
  kvmap["amdhsa.kernels"].convert(kernargList);

  for (uint32_t k_list_i = 0; k_list_i < kernargList.size(); k_list_i++) {
    kernargList[k_list_i].convert(kernargListMap);

    std::string kernelSymbol = "";
    kernargListMap[".symbol"].convert(kernelSymbol);

    if (kernelSymbol == kd->getName()) {
      int kernargSegmentSize = 0;
      kernargListMap[".kernarg_segment_size"].convert(kernargSegmentSize);
      assert(kernargSegmentSize == kd->getKernargSize());

      std::vector<msgpack::object> argumentListMap;
      kernargListMap[".args"].convert(argumentListMap);
      return getFirstHiddenArgOffset(argumentListMap);
    }
  }

  return -1;
}

int getFirstHiddenArgOffset(std::vector<msgpack::object> &argumentListMap) {
  std::map<std::string, msgpack::object> arg;
  std::string valueKind = "";

  int i = 0;
  for (i; i < argumentListMap.size(); ++i) {
    argumentListMap[i].convert(arg);

    msgpack::object valueKindObject = arg[".value_kind"];
    valueKindObject.convert(valueKind);

    if (startsWith("hidden", valueKind)) {
      break;
    }
  }

  assert(i < argumentListMap.size() && startsWith("hidden", valueKind));

  int offset;
  arg[".offset"].convert(offset);
  assert(offset > 0);

  return offset;
}
