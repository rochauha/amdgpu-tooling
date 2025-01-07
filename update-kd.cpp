#include "Elf_X.h"
#include "Symtab.h"

#include "KdUtils.h"
#include "KernelDescriptor.h"

#include <cassert>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>

#include "msgpack.hpp"

using namespace Dyninst;
using namespace SymtabAPI;

void readInstrumentedKernelNames(
    const std::string &filePath,
    std::vector<std::string> &instrumentedKernelNames) {
  std::ifstream file(filePath);
  std::string word;

  assert(file.is_open());
  while (file >> word) {
    instrumentedKernelNames.push_back(word);
  }
  file.close();
}

int main(int argc, char **argv) {
  assert(argc == 3);

  std::vector<std::string> instrumentedKernelNames;
  readInstrumentedKernelNames(argv[1], instrumentedKernelNames);

  std::string filePath(argv[2]);
  std::ifstream inputFile(filePath);

  inputFile.seekg(0, std::ios::end);
  size_t length = inputFile.tellg();
  inputFile.seekg(0, std::ios::beg);

  char *buffer = new char[length];
  inputFile.read(buffer, length);

  Elf_X *elfHeader = Elf_X::newElf_X(buffer, length);

  std::string name;
  bool error;
  Symtab symtab(reinterpret_cast<unsigned char *>(buffer), length, name,
                /*defensive_binary=*/false, error);

  std::vector<Symbol *> symbols;
  symtab.getAllSymbols(symbols);

  std::vector<KDPtr> kds;
  for (const Symbol *symbol : symbols) {
    for (auto &kernelName : instrumentedKernelNames)  {
      std::string mangledName = symbol->getMangledName();
      bool condition = isKernelDescriptor(symbol)
                       && startsWith(kernelName, mangledName)
                       && (mangledName.length() == kernelName.length() + 3);

      if (!condition)
        continue;

      KDPtr kd = std::make_shared<KernelDescriptor>(symbol, elfHeader);

      // WE ASSUME THAT WE ARE ON GFX908, OTHER TARGETS ARE CURRENTLY NOT SUPPORTED.
      uint32_t oldValue = kd->getCOMPUTE_PGM_RSRC1_GranulatedWavefrontSgprCount();
      uint32_t newValue = 2 * ((102/16) + 1);
      if (newValue > oldValue)
        kd->setCOMPUTE_PGM_RSRC1_GranulatedWavefrontSgprCount(newValue);

      symbol->getRegion()->patchData(symbol->getPtrOffset(), kd->getRawPtr(), /* size =*/64);
    }
  }

  std::string outputFileName = std::string(argv[2]) + ".updated";
  symtab.emit(outputFileName);
}
