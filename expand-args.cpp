#include <algorithm>
#include <fstream>
#include <iostream>
#include <msgpack.hpp>
#include <sstream>
#include <string>
#include <vector>

static bool startsWith(const std::string &prefix, const std::string &str) {
  if (prefix.length() > str.length())
    return false;

  return str.substr(0, prefix.length()) == prefix;
}

struct KernelInfo {
  std::string name;
  unsigned newKernargBufferSize;
  unsigned firstHiddenArgIndex;
};

// This number comes from LLVM AMDGPUUsage - https://llvm.org/docs/AMDGPUUsage.html
static const uint32_t GFX908_MAX_SGPR_COUNT = 112;

// Pointers need to be 8-byte aligned
static const uint32_t PTR_ALIGNMENT = 8;

// The extra argument for Dyninst's memory is a pointer of 8 bytes
static const uint32_t DYNINST_ARG_SIZE = 8;

// Create a new argument, which is pointer to the Dyninst's memory buffer for
// variables
void createNewArgument(std::map<std::string, msgpack::object> &newArgument, int offset,
                       msgpack::zone &z) {
  newArgument[".name"] = msgpack::object(std::string("dyninst_mem"), z);
  newArgument[".address_space"] = msgpack::object(std::string("global"), z);
  newArgument[".offset"] = msgpack::object(offset, z);
  newArgument[".size"] = msgpack::object(DYNINST_ARG_SIZE, z);
  newArgument[".value_kind"] = msgpack::object(std::string("global_buffer"), z);
  newArgument[".access"] = msgpack::object(std::string("read_write"), z);

  std::cerr << "created new argument with offset = " << offset << '\n';
}

// The argument list we create is just the signature in the metadata, which is the runtime uses
// to setup the actual kernel arguments.
// The runtime expects all regular arguments first in the signature, even if the argument comes
// after the hidden arguments in the kernarg.
static void createNewArgumentList(std::vector<msgpack::object> &ogArgumentListMap,
                           std::vector<msgpack::object> &newArgumentListMap,
                           unsigned newKernargBufferSize, msgpack::zone &z, KernelInfo &kernelInfo) {
  std::map<std::string, msgpack::object> arg;
  std::string valueKind;
  int i = 0;
  for (; i < ogArgumentListMap.size(); ++i) {
    ogArgumentListMap[i].convert(arg);
    msgpack::object valueKindObject = arg[".value_kind"];
    valueKindObject.convert(valueKind);
    if (startsWith("hidden", valueKind)) {
      break;
    }
    newArgumentListMap.push_back(ogArgumentListMap[i]);
  }

  // Now we are at the first hidden arg.
  assert(i < ogArgumentListMap.size() && startsWith("hidden", valueKind));
  kernelInfo.firstHiddenArgIndex = i;

  std::map<std::string, msgpack::object> newArg;
  createNewArgument(newArg, newKernargBufferSize, z);
  newArgumentListMap.push_back(msgpack::object(newArg, z));
  std::cerr << "added newArg to new list\n";

  // Push other arguments
  for (; i < ogArgumentListMap.size(); ++i) {
    newArgumentListMap.push_back(ogArgumentListMap[i]);
  }
}

static std::string readNoteFile(const std::string &fileName) {
  std::ifstream file(fileName);
  std::stringstream buffer;

  buffer << file.rdbuf();
  buffer.seekg(0);
  return std::string(buffer.str());
}

void expand_args(const std::string &fileName, std::vector<KernelInfo> &instrumentedKernelInfos) {
  std::string newFileName = fileName + ".expanded";

  // Step 1 - read .note file into buffer
  std::string noteBuffer = readNoteFile(fileName);

  std::map<std::string, msgpack::object> kvmap;
  std::vector<msgpack::object> kernargList;
  std::map<std::string, msgpack::object> kernargListMap;
  std::vector<msgpack::object> argumentListMap;
  msgpack::zone z;

  uint32_t offset = 0;

  // Step 2 - parse some non-msgpack header data
  // First 4 bytes  : Size of the Name str (should be AMDGPU\0)
  // Second 4 bytes : Size of the note in msgpack format
  // Third 4 bytes  : Type of the note (should be 32)
  // Followed by the Name str
  // Followed by padding until 4 byte aligned
  // Followed by msgpack data
  std::string name_szstr = noteBuffer.substr(0, 4);
  uint32_t name_sz = *((uint32_t *)name_szstr.c_str());
  std::string noteType = noteBuffer.substr(8, 4);
  std::string name = noteBuffer.substr(12, name_sz);
  offset = 12 + name_sz;
  while (offset % 4)
    offset++;

  // Now we are ready to process the msgpack data
  // We unpack from offset calculated above
  // Unpack until we get to .group_segment_fixed_size
  msgpack::object_handle objHandle;
  objHandle = msgpack::unpack(noteBuffer.data() + offset, noteBuffer.size() - offset);
  msgpack::object map_root = objHandle.get();
  map_root.convert(kvmap);
  kvmap["amdhsa.kernels"].convert(kernargList);

  // Go over each kernel entry and modify the argument list in the metadata for the
  // instrumented ones
  for (uint32_t i = 0; i < kernargList.size(); i++) {
    kernargList[i].convert(kernargListMap);

    std::string kernelName = "";
    kernargListMap[".name"].convert(kernelName);

    auto iter = std::find_if(instrumentedKernelInfos.begin(), instrumentedKernelInfos.end(),
                             [&kernelName](const KernelInfo &KI) { return KI.name == kernelName; });

    if (iter == instrumentedKernelInfos.end()) {
      continue;
    }

    kernargListMap[".args"].convert(argumentListMap);
    std::vector<msgpack::object> newArgumentListMap;
    uint32_t oldKernargSize = 0;
    kernargListMap[".kernarg_segment_size"].convert(oldKernargSize);

    // Rounding up to alignment requirement
    uint32_t newArgOffset= ((oldKernargSize + PTR_ALIGNMENT - 1) / PTR_ALIGNMENT) * PTR_ALIGNMENT;

    createNewArgumentList(argumentListMap, newArgumentListMap, newArgOffset, z, *iter);
    kernargListMap[".args"] = msgpack::object(newArgumentListMap, z);

    // Dyninst already updated the kernarg size in the kernel descriptor.
    // We picked that up when parsing kernelInfos
    uint32_t newKernargSize = iter->newKernargBufferSize;

    assert(newArgOffset + DYNINST_ARG_SIZE == newKernargSize);

    kernargListMap[".kernarg_segment_size"] = msgpack::object(newKernargSize, z);

    // We also set sgpr_count to 112 (max count for gfx908)
    kernargListMap[".sgpr_count"] = msgpack::object(GFX908_MAX_SGPR_COUNT, z);

    kernargList[i] = msgpack::object(kernargListMap, z);
  }

  kvmap["amdhsa.kernels"] = msgpack::object(kernargList, z);
  msgpack::sbuffer outBuffer;
  msgpack::pack(outBuffer, kvmap);
  std::string outString = std::string(outBuffer.data(), outBuffer.size());

  uint32_t outOffset;
  uint32_t desc_sz = outBuffer.size();
  outOffset = name_sz;

  std::ofstream outFile;
  outFile.open(newFileName, std::ios::binary);
  // Write the headers
  outFile.write(reinterpret_cast<char *>(&name_sz), sizeof(name_sz));
  outFile.write(reinterpret_cast<char *>(&desc_sz), sizeof(desc_sz));
  outFile.write(noteType.c_str(), noteType.size());
  outFile.write(name.c_str(), name.size());

  // Padding
  while (outOffset % 4 != 0) {
    outFile.put('\0');
    outOffset += 1;
  }

  // Write the msgpack data
  outFile << outString;
  outOffset += outString.size();

  // Padding
  while (outOffset % 4 != 0) {
    outFile.put('\0');
    outOffset += 1;
  }
  outFile.close();
}

void readInstrumentedKernelInfos(const std::string &filePath,
                                 std::vector<KernelInfo> &instrumentedKernelInfos) {
  std::ifstream file(filePath);
  std::string word;

  assert(file.is_open());

  KernelInfo kernelInfo;
  while (file >> kernelInfo.name >> kernelInfo.newKernargBufferSize) {
    instrumentedKernelInfos.push_back(kernelInfo);
  }

  file.close();
}

void writeUpdatedKernelInfos(const std::string &filePath,
                             std::vector<KernelInfo> &instrumentedKernelInfos) {
  std::ofstream file(filePath);
  assert(file.is_open());

  for (auto const kernelInfo : instrumentedKernelInfos) {
    file << kernelInfo.name << ' ' << kernelInfo.newKernargBufferSize << ' '
         << kernelInfo.firstHiddenArgIndex << '\n';
  }
  file.close();
}

int main(int argc, char *argv[]) {
  if (argc != 3) {
    printf("usage expand_args <.names file> <.note file>\n");
    return -1;
  }

  std::string namesFile(argv[1]);
  std::string noteFile(argv[2]);

  std::vector<KernelInfo> instrumentedKernelInfos;
  readInstrumentedKernelInfos(namesFile, instrumentedKernelInfos);

  expand_args(noteFile, instrumentedKernelInfos);

  std::string outputFileName = namesFile + ".preload";
  writeUpdatedKernelInfos(outputFileName, instrumentedKernelInfos);

  return 0;
}
