#include "hip/hip_runtime.h"

#include <dlfcn.h>
#include <iostream>
#include <fstream>
#include <cassert>
#include <string>
#include <cstring>
#include <vector>
#include <unordered_map>

// Read words from a string
void getWords(const std::string &str, std::vector<std::string> &words) {
  std::stringstream ss(str);
  std::string word;
  while (ss >> word) {
    words.push_back(word);
  }
}

// ===== Instrumentation Variable Table begin =====
// The code here is to retrieve the map :
//  offset -> instrumentation variable name
//
// This will be used to print the names and values of the instrumentation variables after the kernel launch is done and the instrumentation variables are copied back.
struct InstrumentationVarTableEntry {
  int offset;
  std::string name;

  InstrumentationVarTableEntry(std::vector<std::string> words) {
    assert(words.size() == 2);
    offset = std::stoi(words[0]);
    name = words[1];
  }
};

// The table is sorted by offset
void readInstrumentedVarTable(const std::string& filePath, std::vector<InstrumentationVarTableEntry> &tableEntries) {
  std::ifstream tableFile(filePath);
  std::string line;

  assert(tableFile.is_open());

  std::vector<std::string> words;

  while (std::getline(tableFile, line)) {
    getWords(line, words);
    InstrumentationVarTableEntry tableEntry(words);
    tableEntries.push_back(tableEntry);
    words.clear();
  }

  tableFile.close();
}

// Environment variable for the instrumentation variable table path:
const char *instrumentationVariableTableEnv = "DYNINST_AMDGPU_INSTRUMENTATON_VAR_TABLE";

static std::vector<InstrumentationVarTableEntry> instrumentationVarTableEntries;
// ==== Instrumentation Variable Table end ====


// ==== Kernarg Size Map begin ====
//
// This is used to retrieve the map :
//   kernelName -> kernargBufferSize
//
// We extend the kernel signature to take an additional argument, which is the memory holding instrumentation variables.
// The map will be used to update the kernarg signature with a bigger kernarg buffer size, to accomodate for the additional argument.
void readKernargSizeMap(const std::string &filePath,
    std::unordered_map<std::string, int> &theMap) {

  std::ifstream mapFile(filePath);
  std::string line;

  assert(mapFile.is_open());

  std::vector<std::string> words;


  std::cerr << "theMap size : " << theMap.size() << '\n';
  for(auto it : theMap) {
    std::cerr << it.first << ' ' << it.second << '\n';
  }

  while (std::getline(mapFile, line)) {
    getWords(line, words);
    std::cerr << words.size() << '\n';
    assert(words.size() == 2); // (<kernel name> <kernarg size>)

    std::cerr << words[0] << ' ' << words[1] << '\n';
    std::string kernelName = words[0];
    // theMap["test"] = 0;
    int kernargSize = std::stoi(words[1]);

    // theMap[kernelName] = 0;
    words.clear();
  }
  mapFile.close();
}

// Environment variable for the instrumented kernel names path:
const char *instrumentedKernelNamesEnv = "DYNINST_AMDGPU_INSTRUMENTED_KERNEL_NAMES";

std::unordered_map<std::string, int> kernargSizeMap;
// ==== Kernarg Size Map end

typedef void (*registerFunc_t ) (
    void** modules,
    const void*  hostFunction,
    char*        deviceFunction,
    const char*  deviceName,
    unsigned int threadLimit,
    uint3*       tid,
    uint3*       bid,
    dim3*        blockDim,
    dim3*        gridDim,
    int*         wSize);

static registerFunc_t realRegisterFunction;

static std::unordered_map<const void *, std::string> addressToKernelName;

extern "C" void __hipRegisterFunction(
    void** modules,
    const void*  hostFunction,
    char*        deviceFunction,
    const char*  deviceName,
    unsigned int threadLimit,
    uint3*       tid,
    uint3*       bid,
    dim3*        blockDim,
    dim3*        gridDim,
    int*         wSize){
  // fprintf(fdebug,"modules = %p, hostFunciton = %p, devceFunciton = %s, deviceName = %s\n",modules,hostFunction, deviceFunction, deviceName);

  if(realRegisterFunction == 0){
    realRegisterFunction = (registerFunc_t) dlsym(RTLD_NEXT,"__hipRegisterFunction");
    // Map address to kernel name
    addressToKernelName[hostFunction] = std::string(deviceFunction);
  }
  realRegisterFunction(modules,hostFunction,deviceFunction,deviceName,threadLimit,tid,bid,blockDim,gridDim,wSize);
  return;
}

typedef uint32_t (*launch_t)(const void *hostFunction, dim3 gridDim,
                             dim3 blockDim, void **args, size_t sharedMemBytes,
                             hipStream_t stream);
launch_t realLaunch = 0;

extern "C" hipError_t hipLaunchKernel(const void *hostFunction, dim3 gridDim,
                                      dim3 blockDim, void **args,
                                      size_t sharedMemBytes,
                                      hipStream_t stream) {

  unsigned numCounters = 3;
  size_t allocSize = sizeof(unsigned) * numCounters;
  if (realLaunch == 0) {
    realLaunch = (launch_t)dlsym(RTLD_NEXT, "hipLaunchKernel");
  }

  assert(realLaunch != 0);

  unsigned *instrumentationDataHost = (unsigned *)calloc(1, allocSize);

  // std::cerr << "allocated counter on host at " << instrumentationDataHost << '\n';
  std::cerr << '\n';
  std::cerr << "counters on host : " << '\n';

  for (unsigned i = 0; i < numCounters; ++i) {
    std::cerr << "counter_" << i << " = " << instrumentationDataHost[i] << '\n';
  }
  std::cerr << '\n';

  unsigned *instrumentationDataDevice;

  hipError_t hip_ret =
      hipMalloc((void **)&instrumentationDataDevice, allocSize);
  assert(hip_ret == hipSuccess);
  // std::cerr << "allocated additional memory\n";

  hip_ret = hipMemset(instrumentationDataDevice, 0, allocSize);

  assert(hip_ret == hipSuccess);

  // int oldKernargNum = 5;
  int new_kernarg_vec_size = 288 + 8;
  void **newArgs = (void **)malloc(new_kernarg_vec_size);
  // std::cerr << "allocated newArgs\n";
  memcpy(newArgs, args, 40);


  // std::cerr << "original args\n";
  // for (int i = 0; i < 40/8; ++i) {
  //   std::cerr << args[i] << ' ' << std::hex << *((uint64_t *)args[i]) << '\n';
  // }


  // std::cerr << std::dec << "copied oldArgs to newArgs\n";

  newArgs[40/8] = (void *)(&instrumentationDataDevice);
  // std::cerr << "set additional arg\n";

  //memcpy((char *)newArgs + 48, (char *)args + 40, 288-40);
  // std::cerr << "copied hidden args\n";

  // std::cerr << "new args\n";
  // for (int i = 0; i < 48/8; ++i) {
  //   std::cerr << newArgs[i] << ' ' << std::hex << *((uint64_t *)newArgs[i]) << '\n';
  // }
  // std::cerr << std::dec;
  realLaunch(hostFunction, gridDim, blockDim, newArgs, sharedMemBytes, stream);
  hipDeviceSynchronize();

  std::cerr << "real launch done\n";

  hipStreamSynchronize(stream);

  hipMemcpy(instrumentationDataHost, instrumentationDataDevice, /* size = */ allocSize,
            hipMemcpyDeviceToHost);

  std::cerr << "copied data back\n";

  std::cerr << '\n';
  std::cerr << "counters on host : \n";

  for (unsigned i = 0; i < numCounters; ++i) {
    std::cerr << "counter_" << i << " = " << instrumentationDataHost[i] << '\n';
  }
  std::cerr << '\n';
  return hipSuccess;
}

__attribute__((constructor)) void setup(void) {
  realLaunch = 0;

  const char *kernargSizeMapPath = getenv(instrumentedKernelNamesEnv);
  if (!kernargSizeMapPath) {
    std::cerr << "LD_PRELOAD setup: " << instrumentedKernelNamesEnv << " not defined\n";
    exit(1);
  }
  readKernargSizeMap(kernargSizeMapPath, kernargSizeMap);

  const char *tableFilePath = getenv(instrumentationVariableTableEnv);
  if (!tableFilePath) {
    std::cerr << "LD_PRELOAD setup: " << instrumentationVariableTableEnv << " not defined\n";
    exit(1);
  }
  // readInstrumentedVarTable(tableFilePath, instrumentationVarTableEntries);
}

