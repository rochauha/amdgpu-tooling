#include "DyninstUtility.h"
#include <cassert>

bool DyninstUtility::cloneObj(const ELFIO::elfio &fromObj,
                              ELFIO::elfio &toObj) {
  if (fromObj.get_type() != ELFIO::ET_REL) {
    std::cerr << "Can only clone relocatable objects\n";
    return false;
  }

  cloneHeader(fromObj, toObj);
  cloneSections(fromObj, toObj);
  correctSectionLinks(fromObj, toObj);

  correctSectionInfoForRelocationSections(fromObj, toObj);
  correctSectionIndexForSymbols(fromObj, toObj);

  return true;
}

void DyninstUtility::cloneHeader(const ELFIO::elfio &ogObj,
                                 ELFIO::elfio &newObj) {
  newObj.create(ogObj.get_class(), ogObj.get_encoding());
  newObj.set_os_abi(ogObj.get_os_abi());
  newObj.set_abi_version(ogObj.get_abi_version());
  newObj.set_type(ogObj.get_type());
  newObj.set_machine(ogObj.get_machine());
  newObj.set_entry(ogObj.get_entry());
}

bool DyninstUtility::shouldClone(const ELFIO::section *section) {
  switch (section->get_type()) {
  case ELFIO::SHT_NULL:
    return false;

  case ELFIO::SHT_STRTAB:
    // Don't clone section header string table, ELFIO will create a new one
    if (section->get_name() == ".shstrtab")
      return false;
    return true;

  default:
    return true;
  }
}

void DyninstUtility::cloneSections(const ELFIO::elfio &ogObj,
                                   ELFIO::elfio &newObj) {
  auto ogSections = ogObj.sections;
  for (size_t i = 0; i < ogSections.size(); ++i) {
    ELFIO::section *ogSection = ogSections[i];

    if (!shouldClone(ogSection))
      continue;

    const std::string &name = ogSection->get_name();
    ELFIO::section *newSection = newObj.sections.add(name);
    newSection->set_type(ogSection->get_type());
    newSection->set_flags(ogSection->get_flags());
    newSection->set_info(ogSection->get_info());

    // NOTE: This can be incorrect link, and will be corrected later, after all
    // sections are cloned.
    newSection->set_link(ogSection->get_link());

    newSection->set_addr_align(ogSection->get_addr_align());
    newSection->set_entry_size(ogSection->get_entry_size());
    newSection->set_address(ogSection->get_address());
    newSection->set_size(ogSection->get_size());

    if (const char *contents = ogSection->get_data())
      newSection->set_data(contents, ogSection->get_size());

    ogToNewSectionMap[ogSection] = newSection;
    newToOgSectionMap[newSection] = ogSection;
  }
}

void DyninstUtility::correctSectionLinks(const ELFIO::elfio &ogObj,
                                         ELFIO::elfio &newObj) {
  auto ogSections = ogObj.sections;
  auto newSections = newObj.sections;

  // If ogSection's sh_link holds index in ogObj's section header table, we
  // must update newSection's sh_link hold corresponding index in newObj's
  // section header table.
  for (size_t i = 0; i < newSections.size(); ++i) {
    auto *currentNewSection = newSections[i];

    auto iter1 = newToOgSectionMap.find(currentNewSection);
    if (iter1 == newToOgSectionMap.end())
      continue;

    auto *ogSection = iter1->second;
    auto ogLinkSectionIdx = ogSection->get_link();
    auto *ogLinkSection = ogSections[ogLinkSectionIdx];

    auto iter2 = ogToNewSectionMap.find(ogLinkSection);
    if (iter2 == ogToNewSectionMap.end())
      continue;

    auto *newLinkSection = iter2->second;
    auto newLinkSectionIdx = newLinkSection->get_index();
    currentNewSection->set_link(newLinkSectionIdx);
  }
}

void DyninstUtility::replaceSectionContents(ELFIO::elfio &elfObj, const std::string &sectionName, const char *newContents, size_t newSize) {
  auto section = getSection(elfObj, sectionName);
  section->set_data(newContents, newSize);
}

ELFIO::section *
DyninstUtility::getSection(const ELFIO::elfio &elfObj, const std::string& name) const {
  for (int i = 0; i < elfObj.sections.size(); ++i) {
    if (elfObj.sections[i]->get_name() == name)
      return elfObj.sections[i];
  }
  return nullptr;
}

ELFIO::section *
DyninstUtility::getSymtabSection(const ELFIO::elfio &elfObj) const {
  for (int i = 0; i < elfObj.sections.size(); ++i) {
    auto section = elfObj.sections[i];
    if (section->get_type() == ELFIO::SHT_SYMTAB)
      return section;
  }
  return nullptr;
}

ELFIO::section *
DyninstUtility::getStrtabSection(const ELFIO::elfio &elfObj) const {
  for (int i = 0; i < elfObj.sections.size(); ++i) {
    auto section = elfObj.sections[i];
    if (section->get_type() == ELFIO::SHT_STRTAB)
      return section;
  }
  return nullptr;
}

void DyninstUtility::correctSectionInfoForRelocationSections(
    const ELFIO::elfio &ogObj, ELFIO::elfio &newObj) {
  auto ogSections = ogObj.sections;
  auto newSections = newObj.sections;

  // If a relocation section ogSection's sh_info holds index in ogObj's section
  // header table, we must update newSection's sh_info hold corresponding index
  // in newObj's section header table.
  for (size_t i = 0; i < newSections.size(); ++i) {
    auto *currentNewSection = newSections[i];
    auto sectionType = currentNewSection->get_type();

    if (sectionType != ELFIO::SHT_REL && sectionType != ELFIO::SHT_RELA)
      continue;

    auto iter1 = newToOgSectionMap.find(currentNewSection);
    if (iter1 == newToOgSectionMap.end())
      continue;

    auto *ogSection = iter1->second;
    auto ogInfoSectionIdx = ogSection->get_info();
    auto *ogInfoSection = ogSections[ogInfoSectionIdx];

    auto iter2 = ogToNewSectionMap.find(ogInfoSection);
    if (iter2 == ogToNewSectionMap.end())
      continue;

    auto *newInfoSection = iter2->second;
    auto newInfoSectionIdx = newInfoSection->get_index();
    currentNewSection->set_info(newInfoSectionIdx);
  }
}

// Since we are cloning relocatables, symbol values are offsets from a
// some section. ELFIO creates a .shstrtab at index 1 which shifts indices
// for other sections. We want to edit the section index for symbols in newObj
// but ELFIO doesn't support editing in-place. So we edit contents of the buffer
// directly.

#include <elf.h>

void DyninstUtility::correctSectionIndexForSymbols(const ELFIO::elfio &ogObj,
                                                   ELFIO::elfio &newObj) {
  ELFIO::section *ogSymtab = getSymtabSection(ogObj);
  assert(ogSymtab && "original elf object must contain symbol table");
  ELFIO::symbol_section_accessor ogSymtabAccessor(ogObj, ogSymtab);

  ELFIO::section *newSymtab = getSymtabSection(newObj);
  char *newSymtabContents = (char *)newSymtab->get_data();
  Elf64_Sym *symbols = (Elf64_Sym *)(newSymtabContents);

  for (ELFIO::Elf_Xword idx = 0; idx < ogSymtabAccessor.get_symbols_num();
       ++idx) {
    Elf64_Half oldIdx = symbols[idx].st_shndx;
    ELFIO::section *oldSection = ogObj.sections[oldIdx];

    auto iter = ogToNewSectionMap.find(oldSection);
    if (iter == ogToNewSectionMap.end()) {
      continue;
    }
    symbols[idx].st_shndx = iter->second->get_index();
  }
}


bool DyninstUtility::getSymbol(const std::string &name,  Elf64_Sym& symbol) const {

ELFIO::Elf64_Addr& value;
ELFIO::Elf_Xwor& size;
unsigned char bind;
unsigned char type;
ELFIO::Elf_Half section_index;
unsigned char other;

}

void DyninstUtility::reset() {
  ogToNewSectionMap.clear();
  newToOgSectionMap.clear();
}
