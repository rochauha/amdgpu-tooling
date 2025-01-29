#include "KernelDescriptor.h"
#include <cassert>
#include <cstring>
#include <elf.h>
#include <fstream>
#include <iostream>
#include <vector>
#include <algorithm>

// NOTE : This currently only considers GFX908

static void modifyKDs(const char *filename,
                      const std::vector<std::string> &kernelNames) {
  std::ifstream file(filename, std::ios::in | std::ios::binary);
  if (!file.is_open()) {
    std::cerr << "ERROR opening the file\n";
    return;
  }

  file.seekg(0, std::ios::end);
  auto length = static_cast<std::streamoff>(file.tellg());
  file.seekg(0, std::ios::beg);
  char buffer[length];
  file.read(buffer, length);

  file.seekg(0, std::ios::beg);

  // Read ELF header
  Elf64_Ehdr elfHeader;
  file.read(reinterpret_cast<char *>(&elfHeader), sizeof(elfHeader));

  // Check for ELF magic number
  if (std::memcmp(elfHeader.e_ident, ELFMAG, SELFMAG) != 0) {
    std::cerr << "Not a valid ELF file\n";
    return;
  }

  // Locate the section headers
  file.seekg(elfHeader.e_shoff, std::ios::beg);
  std::vector<Elf64_Shdr> sectionHeaders(elfHeader.e_shnum);
  file.read(reinterpret_cast<char *>(sectionHeaders.data()),
            elfHeader.e_shnum * sizeof(Elf64_Shdr));

  // Locate symbol table and string table
  Elf64_Shdr *symTab = nullptr;
  Elf64_Shdr *strTab = nullptr;
  Elf64_Shdr *rodataSection = nullptr;

  auto shStrTabIndex = elfHeader.e_shstrndx;
  for (auto &sh : sectionHeaders) {
    auto namePtr = buffer + sectionHeaders[shStrTabIndex].sh_offset + sh.sh_name;
    if (sh.sh_type == SHT_SYMTAB && strcmp(namePtr, ".symtab") == 0) {
      symTab = &sh;
    } else if (sh.sh_type == SHT_STRTAB && strcmp(namePtr, ".strtab") == 0) {
      strTab = &sh;
    } else if (strcmp(namePtr, ".rodata") == 0) {
      rodataSection = &sh;
    }
  }

  if (!symTab) {
    std::cerr << ".symtab section not found\n";
    return;
  }

  if (!strTab) {
    std::cerr << ".strtab section not found\n";
    return;
  }

  if (!rodataSection) {
    std::cerr << ".rodata section not found\n";
    return;
  }

  std::vector<Elf64_Sym> symbols(symTab->sh_size / sizeof(Elf64_Sym));
  file.seekg(symTab->sh_offset, std::ios::beg);
  file.read(reinterpret_cast<char *>(symbols.data()), symTab->sh_size);

  // Read string table
  std::vector<char> stringTable(strTab->sh_size);
  file.seekg(strTab->sh_offset, std::ios::beg);
  file.read(stringTable.data(), strTab->sh_size);
  file.close();

  // Find the KD symbols and modify them
  for (auto &kernelName : kernelNames) {
    const std::string &kdName = kernelName + ".kd";

    for (const auto &symbol : symbols) {
      const char *symbolName = &stringTable[symbol.st_name];
      if (std::strcmp(symbolName, kdName.c_str()) == 0) {
        std::cout << "Modifying symbol: " << symbolName << "\n";

        size_t byteOffset = symbol.st_value; //rodataSection->sh_offset + symbol.st_value;
        KernelDescriptor kd((uint8_t *)(buffer + byteOffset), 64);

        uint32_t newValue = 2 * ((112 / 16) - 1); // Set SGPRs to 112 (max value)
        kd.setCOMPUTE_PGM_RSRC1_GranulatedWavefrontSgprCount(newValue);

        uint32_t kernargSize = kd.getKernargSize();
        kd.setKernargSize(kernargSize + 8);

        kd.writeToMemory((uint8_t *)(buffer) + byteOffset);
      }
    }
  }

  std::ofstream outputFile(std::string(filename) + ".updated",
                           std::ios::out | std::ios::binary);
  // Write back modified buffer to file
  outputFile.write(buffer, length);
  outputFile.close();
}

static void
readInstrumentedKernelNames(const std::string &filePath,
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
  if (argc != 3) {
    std::cerr << "Usage: " << argv[0] << " <namesFile> <ELF file>"
              << std::endl;
    return 1;
  }

  std::vector<std::string> instrumentedKernelNames;
  readInstrumentedKernelNames(argv[1], instrumentedKernelNames);

  modifyKDs(argv[2], instrumentedKernelNames);

  return 0;
}
