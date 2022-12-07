#ifndef DYNINST_UTILTY_H
#define DYNINST_UTILTY_H

#include "elfio/elfio.hpp"

#include <unordered_map>

class DyninstUtility {
public:
  DyninstUtility() = default;
  ~DyninstUtility() = default;

  DyninstUtility(const DyninstUtility &other) = delete;

  void reset();

  bool cloneObj(const ELFIO::elfio &fromObj, ELFIO::elfio &toObj);

  // Replace contents of the section sectionName with newContents
  void replaceSectionContents(ELFIO::elfio &elfObj, const std::string &sectionName, const char *newContents, size_t newSize);

  // When contents are replaced with og code + instrumentation, symtab needs to be updated. Use this method to update the offset of the symbol.
  void updateSymbolOffset(ELFIO::elfio &elfObj, const std::string &symName, size_t newOffset);

private:
  bool shouldClone(const ELFIO::section *section);
  void cloneHeader(const ELFIO::elfio &ogObj, ELFIO::elfio &newObj);
  void cloneSections(const ELFIO::elfio &ogObj, ELFIO::elfio &newObj);
  void correctSectionLinks(const ELFIO::elfio &ogObj, ELFIO::elfio &nebwObj);

  void correctSectionInfoForRelocationSections(const ELFIO::elfio &ogObj,
                                               ELFIO::elfio &newObj);
  void correctSectionIndexForSymbols(const ELFIO::elfio &ogObj,
                                     ELFIO::elfio &newObj);

  ELFIO::section *getSymtabSection(const ELFIO::elfio &elfObj) const;
  ELFIO::section *getStrtabSection(const ELFIO::elfio &elfObj) const;

  std::unordered_map<ELFIO::section *, ELFIO::section *> ogToNewSectionMap;
  std::unordered_map<ELFIO::section *, ELFIO::section *> newToOgSectionMap;
};

#endif
