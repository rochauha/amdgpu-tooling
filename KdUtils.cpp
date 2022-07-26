#include "Region.h"

#include "KdUtils.h"

using namespace Dyninst;
using namespace SymtabAPI;

bool endsWith(const std::string &suffix, const std::string &str) {
  if (suffix.length() > str.length())
    return false;

  const char *suffixPtr = suffix.c_str() + suffix.length() - 1;
  const char *strPtr = str.c_str() + str.length() - 1;

  for (size_t i = 0; i < suffix.length(); ++i) {
    if (*suffixPtr != *strPtr) {
      return false;
    }
    --suffixPtr;
    --strPtr;
  }

  return true;
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
