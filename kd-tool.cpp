#include "CodeObject.h"
#include "InstructionDecoder.h"

#include "KernelDescriptor.h"

#include <cassert>
#include <iomanip>
#include <iostream>
#include <string>

using namespace Dyninst;
using namespace SymtabAPI;

using namespace llvm;

// std::ends_with was introduced in c++20. However, for compiler compatibility,
// use the one below for the KD check
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

bool isKernelDescriptor(const Symbol *symbol) {
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

  // Right now we check for both mangled and unmangled names.
  std::string suffix = ".kd";
  return endsWith(suffix, symbol->getMangledName());
}

int main(int argc, char **argv) {
  assert(argc == 2);
  std::string filePath(argv[1]);

  Symtab *obj = nullptr;
  if (!Symtab::openFile(obj, filePath)) {
    std::cerr << "error loading " << filePath << '\n';
    exit(1);
  }

  std::vector<Symbol *> symbols;
  obj->getAllSymbols(symbols);

  for (const Symbol *symbol : symbols) {
    if (isKernelDescriptor(symbol)) {
      Region *region = symbol->getRegion();
      KernelDescriptor kd{region};
      kd.dump(std::cout);
      std::cout << '\n' << *symbol << '\n' << '\n';
    }
  }
}
