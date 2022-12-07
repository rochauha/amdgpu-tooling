#include "DyninstUtility.h"
#include "third-party/RawElf.h"

// Final goal -- clone and replace original code with original code + in-place
// instrumentation. This tool exists to develop and test DyninstUtility.

int main(int argc, char **argv) {
  if (argc != 4) {
    std::cout << "exactly 3 arguments to " << argv[0] << " expected\n";
    exit(1);
  }

  const char *ogObjPath = argv[1];
  const char *instrObjPath = argv[2];
  const char *newObjPath = argv[3];

  ELFIO::elfio ogObj;
  ELFIO::elfio instrObj;
  ELFIO::elfio newObj;
  DyninstUtility dyUtil;

  if (!ogObj.load(ogObjPath)) {
    std::cout << "can't find or process ELF file " << ogObjPath << '\n';
    exit(1);
  }

  if (!instrObj.load(instrObjPath)) {
    std::cout << "can't find or process ELF file " << ogObjPath << '\n';
    exit(1);
  }

  dyUtil.cloneObj(ogObj, newObj);
  ELFIO::section *instrObjTextSection = dyUtil.getSection(instrObj, ".text");
  dyUtil.replaceSectionContents(newObj, ".text",
                                instrObjTextSection->get_data(),
                                instrObjTextSection->get_size());
  RawElf::Elf64_Sym symbol;
  if (!dyUtil.getSymbol(newObj, "main", symbol)) {
    std::cout << "can't find "
              << "name\n";
  }

  newObj.save(newObjPath);
}
