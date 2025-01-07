#ifndef KD_UTILS_H
#define KD_UTILS_H

#include "Symbol.h"

#include "Elf_X.h"

#include <string>

#include <memory>

#include "KernelDescriptor.h"

using KDPtr = std::shared_ptr<KernelDescriptor>;

// std::ends_with was introduced in c++20. However, for compiler compatibility,
// use the one below for the KD check
bool endsWith(const std::string &suffix, const std::string &str);

bool isKernelDescriptor(const Dyninst::SymtabAPI::Symbol *symbol);

void parseNoteMetadata(Dyninst::Elf_X_Shdr &sectionHeader);

unsigned getKernargPtrRegister(KDPtr kd);

int getKernargBufferSizeInternal(KDPtr kd, const char *sectionContents,
                                 size_t length);

unsigned getKernargBufferSize(KDPtr kd, Dyninst::Elf_X_Shdr &sectionHeader);

#endif
