#include "Symtab.h"

#include "KdUtils.h"
#include "KernelDescriptor.h"

#include <cassert>
#include <iomanip>
#include <iostream>
#include <string>

using namespace Dyninst::SymtabAPI;

int main(int argc, char **argv) {
  assert(argc == 2);
  std::string filePath(argv[1]);

  Symtab *symtab = nullptr;
  if (!Symtab::openFile(symtab, filePath)) {
    std::cerr << "error loading " << filePath << '\n';
    exit(1);
  }

  std::vector<Symbol *> symbols;
  symtab->getAllSymbols(symbols);

  for (const Symbol *symbol : symbols) {
    if (isKernelDescriptor(symbol)) {
      KernelDescriptor kd(symbol->getRegion());
      kd.dump(std::cout);
      std::cout << '\n' << *symbol << '\n' << '\n';
    }
  }
}
