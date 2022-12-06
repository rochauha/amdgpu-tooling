#include "DyninstUtility.h"

// Final goal -- clone and replace original code with original code + in-place
// instrumentation

int main(int argc, char **argv) {
  if (argc != 3) {
    std::cout << "exactly 2 arguments to " << argv[0] << " expected\n";
    exit(1);
  }

  const char *ogObjPath = argv[1];
  const char *newObjPath = argv[2];

  ELFIO::elfio ogObj;
  ELFIO::elfio newObj;
  DyninstUtility dyUtil;

  if (!ogObj.load(ogObjPath)) {
    std::cout << "can't find or process ELF file " << ogObjPath << '\n';
    exit(1);
  }

  dyUtil.cloneObj(ogObj, newObj);
  newObj.save(newObjPath);
}
