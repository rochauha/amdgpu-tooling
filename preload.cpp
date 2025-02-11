#include "hip/hip_runtime.h"
#include <dlfcn.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "hip/hip_runtime_api.h"
#include <iostream>
#include <link.h>
#include <map>
#include <mutex>
#include <stdio.h>
#include <vector>
using namespace std;

typedef void **(*registerFatBinary_t)(const void *data);

registerFatBinary_t realRegisterFatBinary = 0;

struct __CudaFatBinaryWrapper {
  unsigned int magic;
  unsigned int version;
  void *binary;
  void *dummy1;
};

extern "C" void **__hipRegisterFatBinary(void *data) {
  if (realRegisterFatBinary == 0) {
    realRegisterFatBinary =
        (registerFatBinary_t)dlsym(RTLD_NEXT, "__hipRegisterFatBinary");
  }

  __CudaFatBinaryWrapper *fbwrapper =
      reinterpret_cast<__CudaFatBinaryWrapper *>(data);
 // fprintf(fdebug, "data %lx : %s %lx\n", (uint64_t)data, (char *)data,
        // (uint64_t)fbwrapper->binary);
  return realRegisterFatBinary(data);
}

typedef void (*registerFunc_t)(void **modules, const void *hostFunction,
                               char *deviceFunction, const char *deviceName,
                               unsigned int threadLimit, uint3 *tid, uint3 *bid,
                               dim3 *blockDim, dim3 *gridDim, int *wSize);

registerFunc_t realRegisterFunction;

// extern "C" void __hipRegisterFunction(void **modules, const void *hostFunction,
//                                       char *deviceFunction,
//                                       const char *deviceName,
//                                       unsigned int threadLimit, uint3 *tid,
//                                       uint3 *bid, dim3 *blockDim, dim3 *gridDim,
//                                       int *wSize) {
//   if (realRegisterFunction == 0) {
//     realRegisterFunction =
//         (registerFunc_t)dlsym(RTLD_NEXT, "__hipRegisterFunction");
//   }
//   realRegisterFunction(modules, hostFunction, deviceFunction, deviceName,
//                        threadLimit, tid, bid, blockDim, gridDim, wSize);
//   return;
// }

typedef uint32_t (*launch_t)(const void *hostFunction, dim3 gridDim,
                             dim3 blockDim, void **args, size_t sharedMemBytes,
                             hipStream_t stream);

launch_t realLaunch = 0;

// uint32_t * data, * data_h , data_size;
// uint32_t no_records;
uint32_t launch_id = 0;

extern "C" hipError_t hipLaunchKernel(const void *hostFunction, dim3 gridDim,
                                      dim3 blockDim, void **args,
                                      size_t sharedMemBytes,
                                      hipStream_t stream) {

  if (realLaunch == 0) {
    realLaunch = (launch_t)dlsym(RTLD_NEXT, "hipLaunchKernel");
  }

  assert(realLaunch != 0);

  unsigned instrumentationDataHost[1] = {0};
  unsigned *instrumentationDataDevice;

  hipError_t hip_ret =
      hipMalloc((void **)&instrumentationDataDevice, 1 * sizeof(unsigned));
  assert(hip_ret == hipSuccess);
  std::cerr << "allocated additional memory\n";

  hip_ret = hipMemcpy(instrumentationDataDevice, instrumentationDataHost,
                      /* size=*/1, hipMemcpyHostToDevice);

  assert(hip_ret == hipSuccess);


  int oldKernargNum = 5;
  int new_kernarg_vec_size = (oldKernargNum + 1) * 8;
  void **newArgs = (void **) malloc(new_kernarg_vec_size);
  std::cerr << "allocated newArgs\n";
  memcpy(newArgs, args, oldKernargNum * 8);

  std::cerr << "copied oldArgs to newArgs\n";

  *(newArgs + oldKernargNum) = (void *)(instrumentationDataDevice);
  std::cerr << "set additional arg\n";

  realLaunch(hostFunction, gridDim, blockDim, newArgs, sharedMemBytes, stream);

  std::cerr << "real launch done\n";

  hipStreamSynchronize(stream);

  hipMemcpy(instrumentationDataHost, instrumentationDataDevice, /* size = */ 4,
            hipMemcpyDeviceToHost);

  std::cout << instrumentationDataHost[0] << '\n';
  return hipSuccess;
}

// hip_memory.cpp

typedef hipError_t (*malloc_t)(void **ptr, size_t sizeBytes);
malloc_t realMalloc;

hipError_t hipMalloc(void **ptr, size_t sizeBytes) {
  if (realMalloc == 0) {
    realMalloc = (malloc_t)dlsym(RTLD_NEXT, "hipMalloc");
  }
  return realMalloc(ptr, sizeBytes);
}

typedef hipError_t (*memcpy_t)(void *dst, const void *src, size_t sizeBytes,
                               hipMemcpyKind kind);
memcpy_t realMemcpy;

__attribute__((constructor)) static void setup(void) {
  realLaunch = 0;
  // realRegisterFunction = 0;
  realMalloc = 0;
  realMemcpy = 0;
}
