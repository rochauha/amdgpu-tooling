#include "hip/hip_runtime.h"

#include <dlfcn.h>
#include <iostream>

using namespace std;

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

__attribute__((constructor)) static void setup(void) {
  realLaunch = 0;
}
