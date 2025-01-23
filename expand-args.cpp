#include <algorithm>
#include <fstream>
#include <iostream>
#include <msgpack.hpp>
#include <sstream>
#include <string>

void expand_args(const std::string &fileName,
                 std::vector<std::string> &instrumentedKernelNames) {
  std::string newFileName = fileName + ".expanded";

  /*
   * Step 1, read .note file into buffer
   */
  std::ifstream t(fileName);
  std::stringstream buffer;
  buffer << t.rdbuf();
  buffer.seekg(0);
  std::string str(buffer.str());

  std::map<std::string, msgpack::object> kvmap;
  std::vector<msgpack::object> kernargList;
  std::map<std::string, msgpack::object> kernargListMap;
  std::vector<msgpack::object> argumentListMap;
  msgpack::zone z;

  uint32_t offset = 0;

  /**
   * Step 2, parse some non-msgpack header data
   * First 4 bytes : Size of the Name str (should be AMDGPU\0)
   * Second 4 bytes: Size of the note in msgpack format
   * Third 4 bytes : Type of the note (should be 32)
   * Followed by the Name str
   * Padding until 4 byte aligned
   * Followed by msgpack data
   */
  msgpack::object_handle oh;
  std::string name_szstr = str.substr(0, 4);
  uint32_t name_sz = *((uint32_t *)name_szstr.c_str());
  std::string noteType = str.substr(8, 4);
  std::string name = str.substr(12, name_sz);
  offset = 12 + name_sz;
  while (offset % 4)
    offset++;

  /*
   * Now we are ready to process the msgpack data
   * We unpack from offset calculated above
   * Unpack until we get to .group_segment_fixed_size
   *
   */
  oh = msgpack::unpack(str.data() + offset, str.size() - offset);
  msgpack::object map_root = oh.get();
  map_root.convert(kvmap);
  kvmap["amdhsa.kernels"].convert(kernargList);

  for (uint32_t k_list_i = 0; k_list_i < kernargList.size(); k_list_i++) {
    kernargList[k_list_i].convert(kernargListMap);

    std::string kernelName = "";
    kernargListMap[".name"].convert(kernelName);

    auto iter = std::find(instrumentedKernelNames.begin(),
                          instrumentedKernelNames.end(), kernelName);
    if (iter == instrumentedKernelNames.end())
      continue;

    kernargListMap[".args"].convert(argumentListMap);

    uint32_t oldKernargSize = 0;
    kernargListMap[".kernarg_segment_size"].convert(oldKernargSize);

    // Create a new argument, which is pointer to the dyninst's memory buffer for variables
    std::map<std::string, msgpack::object> newArgument;
    newArgument[".name"] = msgpack::object(std::string("dyninst_mem"), z);
    newArgument[".address_space"] = msgpack::object(std::string("global"), z);
    newArgument[".offset"] = msgpack::object(oldKernargSize, z);
    newArgument[".size"] = msgpack::object(8, z);
    newArgument[".value_kind"] = msgpack::object(std::string("global_buffer"), z);
    newArgument[".actual_access"] = msgpack::object(std::string("read_write"), z);
    newArgument[".is_const"] = msgpack::object(true, z); // the pointer can't change address
    newArgument[".is_restrict"] = msgpack::object(false, z); // keep it false to be safe
    newArgument[".is_volatile"] = msgpack::object(false, z); // keep it false to be safe

    argumentListMap.push_back(msgpack::object(newArgument, z));
    kernargListMap[".args"] = msgpack::object(argumentListMap, z);

    uint32_t newKernargSize = oldKernargSize + 8;
    kernargListMap[".kernarg_segment_size"] = msgpack::object(newKernargSize, z);

    // We also set spr_count to 102 (max count for gfx908)
    kernargListMap[".sgpr_count"] = msgpack::object(102, z);

    kernargList[k_list_i] = msgpack::object(kernargListMap, z);
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
  while (outOffset % 4) {
    outFile.put('\0');
    outOffset += 1;
  }
  // Write the msgpack data
  outFile << outString;
  outOffset += outString.size();

  // Padding
  while (outOffset % 4) {
    outFile.put('\0');
    outOffset += 1;
  }
  outFile.close();
}

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

int main(int argc, char *argv[]) {
  if (argc != 3) {
    printf("usage expand_args <.names file> <.note file>\n");
    return -1;
  }
  std::vector<std::string> instrumentedKernelNames;
  readInstrumentedKernelNames(argv[1], instrumentedKernelNames);
  expand_args(argv[2], instrumentedKernelNames);
  return 0;
}
