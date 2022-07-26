#include "KernelDescriptor.h"

#include <cassert>
#include <iomanip>

using namespace llvm;
using namespace llvm::amdhsa;
using namespace Dyninst::SymtabAPI;

void KernelDescriptor::readToKd(const uint8_t *rawBytes, size_t rawBytesLength,
                                size_t fromIndex, size_t numBytes,
                                uint8_t *data) {
  assert(fromIndex + numBytes <= rawBytesLength);

  for (size_t i = 0; i < numBytes; ++i) {
    size_t idx = fromIndex + i;
    data[i] = rawBytes[idx];
  }
}

KernelDescriptor::KernelDescriptor(const Region *region) {
  assert(region && "region must be non-null");

  const size_t kdSize = 64;

  assert(sizeof(kernel_descriptor_t) == kdSize);
  assert(region->getDiskSize() == kdSize);
  assert(region->getMemSize() == kdSize);

  const uint8_t *kdBytes = (const uint8_t *)region->getPtrToRawData();

  // We read from kdBytes to kdPtr as per the kernel descriptor format.
  uint8_t *kdPtr = (uint8_t *)&kdRepr;

  size_t idx = 0;
  while (idx != kdSize) {
    switch (idx) {
    case amdhsa::GROUP_SEGMENT_FIXED_SIZE_OFFSET:
      readToKd(kdBytes, kdSize, idx, sizeof(uint32_t), kdPtr + idx);
      idx += sizeof(uint32_t);
      break;

    case amdhsa::PRIVATE_SEGMENT_FIXED_SIZE_OFFSET:
      readToKd(kdBytes, kdSize, idx, sizeof(uint32_t), kdPtr + idx);
      idx += sizeof(uint32_t);
      break;

    case amdhsa::KERNARG_SIZE_OFFSET:
      readToKd(kdBytes, kdSize, idx, sizeof(uint32_t), kdPtr + idx);
      idx += sizeof(uint32_t);
      break;

    case amdhsa::RESERVED0_OFFSET:
      // 4 bytes from here are reserved, must be 0.
      for (int i = 0; i < 4; ++i) {
        int j = amdhsa::RESERVED0_OFFSET + i;
        assert(kdBytes[j] == 0 && "reserved bytes (reserved1) must be 0");
      }
      readToKd(kdBytes, kdSize, idx, 4 * sizeof(int8_t), kdPtr + idx);
      idx += 4 * sizeof(uint8_t);
      break;

    case amdhsa::KERNEL_CODE_ENTRY_BYTE_OFFSET_OFFSET:
      readToKd(kdBytes, kdSize, idx, sizeof(uint64_t), kdPtr + idx);
      idx += sizeof(uint64_t);
      break;

    case amdhsa::RESERVED1_OFFSET:
      // 20 bytes from here are reserved, must be 0.
      for (int i = 0; i < 20; ++i) {
        int j = amdhsa::RESERVED1_OFFSET + i;
        assert(kdBytes[j] == 0 && "reserved bytes (reserved1) must be 0");
      }
      readToKd(kdBytes, kdSize, idx, 20 * sizeof(uint8_t), kdPtr + idx);
      idx += 20 * sizeof(uint8_t);
      break;

    case amdhsa::COMPUTE_PGM_RSRC3_OFFSET:
      //  - Only set for GFX10, GFX6-9 have this to be 0.
      // TODO : see e_flags for subtarget
      idx += sizeof(uint32_t);
      break;

    case amdhsa::COMPUTE_PGM_RSRC1_OFFSET:
      readToKd(kdBytes, kdSize, idx, sizeof(uint32_t), kdPtr + idx);
      idx += sizeof(uint32_t);
      break;

    case amdhsa::COMPUTE_PGM_RSRC2_OFFSET:
      readToKd(kdBytes, kdSize, idx, sizeof(uint32_t), kdPtr + idx);
      idx += sizeof(uint32_t);
      break;

    case amdhsa::KERNEL_CODE_PROPERTIES_OFFSET:
      readToKd(kdBytes, kdSize, idx, sizeof(uint16_t), kdPtr + idx);
      idx += sizeof(uint16_t);
      break;

    case amdhsa::RESERVED2_OFFSET:
      // 6 bytes from here are reserved, must be 0.
      for (int i = 0; i < 6; ++i) {
        int j = amdhsa::RESERVED2_OFFSET + i;
        assert(kdBytes[j] == 0 && "reserved bytes (reserved2) must be 0");
      }
      readToKd(kdBytes, kdSize, idx, 6 * sizeof(uint8_t), kdPtr + idx);
      idx += 6 * sizeof(uint8_t);
      break;
    }
  }
}

uint32_t KernelDescriptor::getGroupSegmentFixedSize() const {
  return kdRepr.group_segment_fixed_size;
}

void KernelDescriptor::setGroupSegmentFixedSize(uint32_t value) {
  kdRepr.group_segment_fixed_size = value;
}

uint32_t KernelDescriptor::getPrivateSegmentFixedSize() const {
  return kdRepr.private_segment_fixed_size;
}

void KernelDescriptor::setPrivateSegmentFixedSize(uint32_t value) {
  kdRepr.private_segment_fixed_size = value;
}

uint32_t KernelDescriptor::getKernargSize() const {
  return kdRepr.kernarg_size;
}

void KernelDescriptor::setKernargSize(uint32_t value) {
  kdRepr.kernarg_size = value;
}

int64_t KernelDescriptor::getKernelCodeEntryByteOffset() const {
  return kdRepr.kernel_code_entry_byte_offset;
}

void KernelDescriptor::setKernelCodeEntryByteOffset(int64_t value) {
  kdRepr.kernel_code_entry_byte_offset = value;
}

#define GET_VALUE(MASK) ((fourByteBuffer & MASK) >> (MASK##_SHIFT))
#define SET_VALUE(MASK) (fourByteBuffer | ((value) << (MASK##_SHIFT)))
#define CLEAR_BITS(MASK) ((fourByteBuffer & !MASK))
#define CHECK_WIDTH(MASK) ((value) >> (MASK##_WIDTH) == 0)

// ----- COMPUTE_PGM_RSRC3 begin -----
//
//
uint32_t KernelDescriptor::getCOMPUTE_PGM_RSRC3() const {
  // TODO: THIS WHOLE REGISTER IS RESERVED FOR GFX6-9, MODIFY ALL THE CODE
  // ACCORDINGLY
  return kdRepr.compute_pgm_rsrc3;
}

// GFX90A, GFX940 begin
// TODO : ADD ASSERTS FOR GFX90A and GFX940
uint32_t KernelDescriptor::getCOMPUTE_PGM_RSRC3_AccumOffset() const {
  uint32_t fourByteBuffer = kdRepr.compute_pgm_rsrc3;
  return GET_VALUE(COMPUTE_PGM_RSRC3_GFX90A_ACCUM_OFFSET);
}

void KernelDescriptor::setCOMPUTE_PGM_RSRC3_AccumOffset(uint32_t value) {
  uint32_t fourByteBuffer = kdRepr.compute_pgm_rsrc3;
  fourByteBuffer = CLEAR_BITS(COMPUTE_PGM_RSRC3_GFX90A_ACCUM_OFFSET);
  assert(CHECK_WIDTH(COMPUTE_PGM_RSRC3_GFX90A_ACCUM_OFFSET) &&
         "value contains more bits than specificied");
  kdRepr.compute_pgm_rsrc3 = SET_VALUE(COMPUTE_PGM_RSRC3_GFX90A_ACCUM_OFFSET);
}

bool KernelDescriptor::getCOMPUTE_PGM_RSRC3_TgSplit() const {
  uint32_t fourByteBuffer = kdRepr.compute_pgm_rsrc3;
  return GET_VALUE(COMPUTE_PGM_RSRC3_GFX90A_TG_SPLIT);
}

void KernelDescriptor::setCOMPUTE_PGM_RSRC3_TgSplit(bool value) {
  uint32_t fourByteBuffer = kdRepr.compute_pgm_rsrc3;
  fourByteBuffer = CLEAR_BITS(COMPUTE_PGM_RSRC3_GFX90A_TG_SPLIT);
  kdRepr.compute_pgm_rsrc3 = SET_VALUE(COMPUTE_PGM_RSRC3_GFX90A_TG_SPLIT);
}
//
// GFX90A, GFX940 end

// GFX10, GFX11 begin
// TODO : ADD ASSERTS FOR GFX10 and GFX11
uint32_t KernelDescriptor::getCOMPUTE_PGM_RSRC3_SharedVgprCount() const {
  uint32_t fourByteBuffer = kdRepr.compute_pgm_rsrc3;
  return GET_VALUE(COMPUTE_PGM_RSRC3_GFX10_PLUS_SHARED_VGPR_COUNT);
}

void KernelDescriptor::setCOMPUTE_PGM_RSRC3_SharedVgprCount(uint32_t value) {
  uint32_t fourByteBuffer = kdRepr.compute_pgm_rsrc3;
  fourByteBuffer = CLEAR_BITS(COMPUTE_PGM_RSRC3_GFX10_PLUS_SHARED_VGPR_COUNT);
  assert(CHECK_WIDTH(COMPUTE_PGM_RSRC3_GFX10_PLUS_SHARED_VGPR_COUNT) &&
         "value contains more bits than specificied");
  kdRepr.compute_pgm_rsrc3 =
      SET_VALUE(COMPUTE_PGM_RSRC3_GFX10_PLUS_SHARED_VGPR_COUNT);
}

// TODO: ADD ASSERT FOR GFX11, THIS IS RESERVED IN GFX10
uint32_t KernelDescriptor::getCOMPUTE_PGM_RSRC3_InstPrefSize() const {
  uint32_t fourByteBuffer = kdRepr.compute_pgm_rsrc3;
  return GET_VALUE(COMPUTE_PGM_RSRC3_GFX10_PLUS_INST_PREF_SIZE);
}

void KernelDescriptor::setCOMPUTE_PGM_RSRC3_InstPrefSize(uint32_t value) {
  uint32_t fourByteBuffer = kdRepr.compute_pgm_rsrc3;
  fourByteBuffer = CLEAR_BITS(COMPUTE_PGM_RSRC3_GFX10_PLUS_INST_PREF_SIZE);
  assert(CHECK_WIDTH(COMPUTE_PGM_RSRC3_GFX10_PLUS_INST_PREF_SIZE) &&
         "value contains more bits than specificied");
  kdRepr.compute_pgm_rsrc3 =
      SET_VALUE(COMPUTE_PGM_RSRC3_GFX10_PLUS_INST_PREF_SIZE);
}

// TODO: ADD ASSERT FOR GFX11, THIS IS RESERVED IN GFX10
bool KernelDescriptor::getCOMPUTE_PGM_RSRC3_TrapOnStart() const {
  uint32_t fourByteBuffer = kdRepr.compute_pgm_rsrc3;
  return GET_VALUE(COMPUTE_PGM_RSRC3_GFX10_PLUS_TRAP_ON_START);
}

void KernelDescriptor::setCOMPUTE_PGM_RSRC3_TrapOnStart(bool value) {
  uint32_t fourByteBuffer = kdRepr.compute_pgm_rsrc3;
  fourByteBuffer = CLEAR_BITS(COMPUTE_PGM_RSRC3_GFX10_PLUS_TRAP_ON_START);
  kdRepr.compute_pgm_rsrc3 =
      SET_VALUE(COMPUTE_PGM_RSRC3_GFX10_PLUS_TRAP_ON_START);
}

// TODO: ADD ASSERT FOR GFX11, THIS IS RESERVED IN GFX10
bool KernelDescriptor::getCOMPUTE_PGM_RSRC3_TrapOnEnd() const {
  uint32_t fourByteBuffer = kdRepr.compute_pgm_rsrc3;
  return GET_VALUE(COMPUTE_PGM_RSRC3_GFX10_PLUS_TRAP_ON_END);
}

void KernelDescriptor::setCOMPUTE_PGM_RSRC3_TrapOnEnd(bool value) {
  uint32_t fourByteBuffer = kdRepr.compute_pgm_rsrc3;
  fourByteBuffer = CLEAR_BITS(COMPUTE_PGM_RSRC3_GFX10_PLUS_TRAP_ON_END);
  kdRepr.compute_pgm_rsrc3 =
      SET_VALUE(COMPUTE_PGM_RSRC3_GFX10_PLUS_TRAP_ON_END);
}

// TODO: ADD ASSERT FOR GFX11, THIS IS RESERVED IN GFX10
bool KernelDescriptor::getCOMPUTE_PGM_RSRC3_ImageOp() const {
  uint32_t fourByteBuffer = kdRepr.compute_pgm_rsrc3;
  return GET_VALUE(COMPUTE_PGM_RSRC3_GFX10_PLUS_IMAGE_OP);
}

void KernelDescriptor::setCOMPUTE_PGM_RSRC3_ImageOp(bool value) {
  uint32_t fourByteBuffer = kdRepr.compute_pgm_rsrc3;
  fourByteBuffer = CLEAR_BITS(COMPUTE_PGM_RSRC3_GFX10_PLUS_IMAGE_OP);
  kdRepr.compute_pgm_rsrc3 = SET_VALUE(COMPUTE_PGM_RSRC3_GFX10_PLUS_IMAGE_OP);
}
//
// GFX10, GFX11 end
// ----- COMPUTE_PGM_RSRC3 end -----

// ----- COMPUTE_PGM_RSRC1 begin -----
//
//
uint32_t KernelDescriptor::getCOMPUTE_PGM_RSRC1() const {
  return kdRepr.compute_pgm_rsrc1;
}

uint32_t
KernelDescriptor::getCOMPUTE_PGM_RSRC1_GranulatedWorkitemVgprCount() const {
  uint32_t fourByteBuffer = kdRepr.compute_pgm_rsrc1;
  return GET_VALUE(COMPUTE_PGM_RSRC1_GRANULATED_WORKITEM_VGPR_COUNT);
}

void KernelDescriptor::setCOMPUTE_PGM_RSRC1_GranulatedWorkitemVgprCount(
    uint32_t value) {
  uint32_t fourByteBuffer = kdRepr.compute_pgm_rsrc1;
  fourByteBuffer = CLEAR_BITS(COMPUTE_PGM_RSRC1_GRANULATED_WORKITEM_VGPR_COUNT);
  assert(CHECK_WIDTH(COMPUTE_PGM_RSRC1_GRANULATED_WORKITEM_VGPR_COUNT) &&
         "value contains more bits than specificied");
  kdRepr.compute_pgm_rsrc1 =
      SET_VALUE(COMPUTE_PGM_RSRC1_GRANULATED_WORKITEM_VGPR_COUNT);
}

uint32_t
KernelDescriptor::getCOMPUTE_PGM_RSRC1_GranulatedWavefrontSgprCount() const {
  uint32_t fourByteBuffer = kdRepr.compute_pgm_rsrc1;
  return GET_VALUE(COMPUTE_PGM_RSRC1_GRANULATED_WAVEFRONT_SGPR_COUNT);
}

void KernelDescriptor::setCOMPUTE_PGM_RSRC1_GranulatedWavefrontSgprCount(
    uint32_t value) {
  uint32_t fourByteBuffer = kdRepr.compute_pgm_rsrc1;
  fourByteBuffer =
      CLEAR_BITS(COMPUTE_PGM_RSRC1_GRANULATED_WAVEFRONT_SGPR_COUNT);
  assert(CHECK_WIDTH(COMPUTE_PGM_RSRC1_GRANULATED_WAVEFRONT_SGPR_COUNT) &&
         "value contains more bits than specificied");
  kdRepr.compute_pgm_rsrc1 =
      SET_VALUE(COMPUTE_PGM_RSRC1_GRANULATED_WAVEFRONT_SGPR_COUNT);
}

uint32_t KernelDescriptor::getCOMPUTE_PGM_RSRC1_Priority() const {
  uint32_t fourByteBuffer = kdRepr.compute_pgm_rsrc1;
  return GET_VALUE(COMPUTE_PGM_RSRC1_PRIORITY);
}

uint32_t KernelDescriptor::getCOMPUTE_PGM_RSRC1_FloatRoundMode32() const {
  uint32_t fourByteBuffer = kdRepr.compute_pgm_rsrc1;
  return GET_VALUE(COMPUTE_PGM_RSRC1_FLOAT_ROUND_MODE_32);
}

void KernelDescriptor::setCOMPUTE_PGM_RSRC1_FloatRoundMode32(uint32_t value) {
  uint32_t fourByteBuffer = kdRepr.compute_pgm_rsrc1;
  fourByteBuffer = CLEAR_BITS(COMPUTE_PGM_RSRC1_FLOAT_ROUND_MODE_32);
  assert(CHECK_WIDTH(COMPUTE_PGM_RSRC1_FLOAT_ROUND_MODE_32) &&
         "value contains more bits than specificied");
  kdRepr.compute_pgm_rsrc1 = SET_VALUE(COMPUTE_PGM_RSRC1_FLOAT_ROUND_MODE_32);
}

uint32_t KernelDescriptor::getCOMPUTE_PGM_RSRC1_FloatRoundMode1664() const {
  uint32_t fourByteBuffer = kdRepr.compute_pgm_rsrc1;
  return GET_VALUE(COMPUTE_PGM_RSRC1_FLOAT_ROUND_MODE_16_64);
}

void KernelDescriptor::setCOMPUTE_PGM_RSRC1_FloatRoundMode1664(uint32_t value) {
  uint32_t fourByteBuffer = kdRepr.compute_pgm_rsrc1;
  fourByteBuffer = CLEAR_BITS(COMPUTE_PGM_RSRC1_FLOAT_ROUND_MODE_16_64);
  assert(CHECK_WIDTH(COMPUTE_PGM_RSRC1_FLOAT_ROUND_MODE_16_64) &&
         "value contains more bits than specificied");
  kdRepr.compute_pgm_rsrc1 =
      SET_VALUE(COMPUTE_PGM_RSRC1_FLOAT_ROUND_MODE_16_64);
}

uint32_t KernelDescriptor::getCOMPUTE_PGM_RSRC1_FloatDenormMode32() const {
  uint32_t fourByteBuffer = kdRepr.compute_pgm_rsrc1;
  return GET_VALUE(COMPUTE_PGM_RSRC1_FLOAT_DENORM_MODE_32);
}

void KernelDescriptor::setCOMPUTE_PGM_RSRC1_FloatDenormMode32(uint32_t value) {
  uint32_t fourByteBuffer = kdRepr.compute_pgm_rsrc1;
  fourByteBuffer = CLEAR_BITS(COMPUTE_PGM_RSRC1_FLOAT_DENORM_MODE_32);
  assert(CHECK_WIDTH(COMPUTE_PGM_RSRC1_FLOAT_DENORM_MODE_32) &&
         "value contains more bits than specificied");
  kdRepr.compute_pgm_rsrc1 = SET_VALUE(COMPUTE_PGM_RSRC1_FLOAT_DENORM_MODE_32);
}

uint32_t KernelDescriptor::getCOMPUTE_PGM_RSRC1_FloatDenormMode1664() const {
  uint32_t fourByteBuffer = kdRepr.compute_pgm_rsrc1;
  return GET_VALUE(COMPUTE_PGM_RSRC1_FLOAT_DENORM_MODE_16_64);
}

void KernelDescriptor::setCOMPUTE_PGM_RSRC1_FloatDenormMode1664(
    uint32_t value) {
  uint32_t fourByteBuffer = kdRepr.compute_pgm_rsrc1;
  fourByteBuffer = CLEAR_BITS(COMPUTE_PGM_RSRC1_FLOAT_ROUND_MODE_16_64);
  assert(CHECK_WIDTH(COMPUTE_PGM_RSRC1_FLOAT_DENORM_MODE_16_64) &&
         "value contains more bits than specificied");
  kdRepr.compute_pgm_rsrc1 =
      SET_VALUE(COMPUTE_PGM_RSRC1_FLOAT_DENORM_MODE_16_64);
}

bool KernelDescriptor::getCOMPUTE_PGM_RSRC1_Priv() const {
  uint32_t fourByteBuffer = kdRepr.compute_pgm_rsrc1;
  return GET_VALUE(COMPUTE_PGM_RSRC1_PRIORITY);
}

bool KernelDescriptor::getCOMPUTE_PGM_RSRC1_EnableDx10Clamp() const {
  uint32_t fourByteBuffer = kdRepr.compute_pgm_rsrc1;
  return GET_VALUE(COMPUTE_PGM_RSRC1_ENABLE_DX10_CLAMP);
}

void KernelDescriptor::setCOMPUTE_PGM_RSRC1_EnableDx10Clamp(bool value) {
  uint32_t fourByteBuffer = kdRepr.compute_pgm_rsrc1;
  fourByteBuffer = CLEAR_BITS(COMPUTE_PGM_RSRC1_ENABLE_DX10_CLAMP);
  kdRepr.compute_pgm_rsrc1 = SET_VALUE(COMPUTE_PGM_RSRC1_ENABLE_DX10_CLAMP);
}

bool KernelDescriptor::getCOMPUTE_PGM_RSRC1_DebugMode() const {
  uint32_t fourByteBuffer = kdRepr.compute_pgm_rsrc1;
  return GET_VALUE(COMPUTE_PGM_RSRC1_DEBUG_MODE);
}

bool KernelDescriptor::getCOMPUTE_PGM_RSRC1_EnableIeeeMode() const {
  uint32_t fourByteBuffer = kdRepr.compute_pgm_rsrc1;
  return GET_VALUE(COMPUTE_PGM_RSRC1_ENABLE_IEEE_MODE);
}

void KernelDescriptor::setCOMPUTE_PGM_RSRC1_EnableIeeeMode(bool value) {
  uint32_t fourByteBuffer = kdRepr.compute_pgm_rsrc1;
  fourByteBuffer = CLEAR_BITS(COMPUTE_PGM_RSRC1_ENABLE_IEEE_MODE);
  kdRepr.compute_pgm_rsrc1 = SET_VALUE(COMPUTE_PGM_RSRC1_ENABLE_IEEE_MODE);
}

bool KernelDescriptor::getCOMPUTE_PGM_RSRC1_Bulky() const {
  uint32_t fourByteBuffer = kdRepr.compute_pgm_rsrc1;
  return GET_VALUE(COMPUTE_PGM_RSRC1_BULKY);
}

bool KernelDescriptor::getCOMPUTE_PGM_RSRC1_CdbgUser() const {
  uint32_t fourByteBuffer = kdRepr.compute_pgm_rsrc1;
  return GET_VALUE(COMPUTE_PGM_RSRC1_CDBG_USER);
}

// TODO: RESERVED FOR GFX6-8, ADD CHECKS
bool KernelDescriptor::getCOMPUTE_PGM_RSRC1_Fp16Ovfl() const {
  uint32_t fourByteBuffer = kdRepr.compute_pgm_rsrc1;
  return GET_VALUE(COMPUTE_PGM_RSRC1_FP16_OVFL);
}

void KernelDescriptor::setCOMPUTE_PGM_RSRC1_Fp16Ovfl(bool value) {
  uint32_t fourByteBuffer = kdRepr.compute_pgm_rsrc1;
  fourByteBuffer = CLEAR_BITS(COMPUTE_PGM_RSRC1_FP16_OVFL);
  kdRepr.compute_pgm_rsrc1 = SET_VALUE(COMPUTE_PGM_RSRC1_FP16_OVFL);
}

// TODO: RESERVED FOR GFX6-9, ADD CHECKS
bool KernelDescriptor::getCOMPUTE_PGM_RSRC1_WgpMode() const {
  uint32_t fourByteBuffer = kdRepr.compute_pgm_rsrc1;
  return GET_VALUE(COMPUTE_PGM_RSRC1_WGP_MODE);
}

void KernelDescriptor::setCOMPUTE_PGM_RSRC1_WgpMode(bool value) {
  uint32_t fourByteBuffer = kdRepr.compute_pgm_rsrc1;
  fourByteBuffer = CLEAR_BITS(COMPUTE_PGM_RSRC1_WGP_MODE);
  kdRepr.compute_pgm_rsrc1 = SET_VALUE(COMPUTE_PGM_RSRC1_WGP_MODE);
}

// TODO: RESERVED FOR GFX6-9, ADD CHECKS
bool KernelDescriptor::getCOMPUTE_PGM_RSRC1_MemOrdered() const {
  uint32_t fourByteBuffer = kdRepr.compute_pgm_rsrc1;
  return GET_VALUE(COMPUTE_PGM_RSRC1_MEM_ORDERED);
}

void KernelDescriptor::setCOMPUTE_PGM_RSRC1_MemOrdered(bool value) {
  uint32_t fourByteBuffer = kdRepr.compute_pgm_rsrc1;
  fourByteBuffer = CLEAR_BITS(COMPUTE_PGM_RSRC1_MEM_ORDERED);
  kdRepr.compute_pgm_rsrc1 = SET_VALUE(COMPUTE_PGM_RSRC1_MEM_ORDERED);
}

// TODO: RESERVED FOR GFX6-9, ADD CHECKS
bool KernelDescriptor::getCOMPUTE_PGM_RSRC1_FwdProgress() const {
  uint32_t fourByteBuffer = kdRepr.compute_pgm_rsrc1;
  return GET_VALUE(COMPUTE_PGM_RSRC1_FWD_PROGRESS);
}

void KernelDescriptor::setCOMPUTE_PGM_RSRC1_FwdProgress(bool value) {
  uint32_t fourByteBuffer = kdRepr.compute_pgm_rsrc1;
  fourByteBuffer = CLEAR_BITS(COMPUTE_PGM_RSRC1_FWD_PROGRESS);
  kdRepr.compute_pgm_rsrc1 = SET_VALUE(COMPUTE_PGM_RSRC1_FWD_PROGRESS);
}
//
// ----- COMPUTE_PGM_RSRC1 end -----

// ----- COMPUTE_PGM_RSRC2 begin -----
//
uint32_t KernelDescriptor::getCOMPUTE_PGM_RSRC2() const {
  return kdRepr.compute_pgm_rsrc2;
}

bool KernelDescriptor::getCOMPUTE_PGM_RSRC2_EnablePrivateSegment() const {
  uint32_t fourByteBuffer = kdRepr.compute_pgm_rsrc2;
  return GET_VALUE(COMPUTE_PGM_RSRC2_ENABLE_PRIVATE_SEGMENT);
}

void KernelDescriptor::setCOMPUTE_PGM_RSRC2_EnablePrivateSegment(bool value) {
  uint32_t fourByteBuffer = kdRepr.compute_pgm_rsrc2;
  fourByteBuffer = CLEAR_BITS(COMPUTE_PGM_RSRC2_ENABLE_PRIVATE_SEGMENT);
  kdRepr.compute_pgm_rsrc1 =
      SET_VALUE(COMPUTE_PGM_RSRC2_ENABLE_PRIVATE_SEGMENT);
}

uint32_t KernelDescriptor::getCOMPUTE_PGM_RSRC2_UserSgprCount() const {
  uint32_t fourByteBuffer = kdRepr.compute_pgm_rsrc2;
  return GET_VALUE(COMPUTE_PGM_RSRC2_USER_SGPR_COUNT);
}

void KernelDescriptor::setCOMPUTE_PGM_RSRC2_UserSgprCount(uint32_t value) {
  uint32_t fourByteBuffer = kdRepr.compute_pgm_rsrc2;
  fourByteBuffer = CLEAR_BITS(COMPUTE_PGM_RSRC2_USER_SGPR_COUNT);
  assert(CHECK_WIDTH(COMPUTE_PGM_RSRC2_USER_SGPR_COUNT) &&
         "value contains more bits than specificied");
  kdRepr.compute_pgm_rsrc1 = SET_VALUE(COMPUTE_PGM_RSRC2_USER_SGPR_COUNT);
}

bool KernelDescriptor::getCOMPUTE_PGM_RSRC2_EnableTrapHandler() const {
  uint32_t fourByteBuffer = kdRepr.compute_pgm_rsrc2;
  return GET_VALUE(COMPUTE_PGM_RSRC2_ENABLE_TRAP_HANDLER);
}

bool KernelDescriptor::getCOMPUTE_PGM_RSRC2_EnableSgprWorkgroupIdX() const {
  uint32_t fourByteBuffer = kdRepr.compute_pgm_rsrc2;
  return GET_VALUE(COMPUTE_PGM_RSRC2_ENABLE_SGPR_WORKGROUP_ID_X);
}

void KernelDescriptor::setCOMPUTE_PGM_RSRC2_EnableSgprWorkgroupIdX(bool value) {
  uint32_t fourByteBuffer = kdRepr.compute_pgm_rsrc2;
  fourByteBuffer = CLEAR_BITS(COMPUTE_PGM_RSRC2_ENABLE_SGPR_WORKGROUP_ID_X);
  kdRepr.compute_pgm_rsrc1 =
      SET_VALUE(COMPUTE_PGM_RSRC2_ENABLE_SGPR_WORKGROUP_ID_X);
}

bool KernelDescriptor::getCOMPUTE_PGM_RSRC2_EnableSgprWorkgroupIdY() const {
  uint32_t fourByteBuffer = kdRepr.compute_pgm_rsrc2;
  return GET_VALUE(COMPUTE_PGM_RSRC2_ENABLE_SGPR_WORKGROUP_ID_Y);
}

void KernelDescriptor::setCOMPUTE_PGM_RSRC2_EnableSgprWorkgroupIdY(bool value) {
  uint32_t fourByteBuffer = kdRepr.compute_pgm_rsrc2;
  fourByteBuffer = CLEAR_BITS(COMPUTE_PGM_RSRC2_ENABLE_SGPR_WORKGROUP_ID_Y);
  kdRepr.compute_pgm_rsrc1 =
      SET_VALUE(COMPUTE_PGM_RSRC2_ENABLE_SGPR_WORKGROUP_ID_Y);
}

bool KernelDescriptor::getCOMPUTE_PGM_RSRC2_EnableSgprWorkgroupIdZ() const {
  uint32_t fourByteBuffer = kdRepr.compute_pgm_rsrc2;
  return GET_VALUE(COMPUTE_PGM_RSRC2_ENABLE_SGPR_WORKGROUP_ID_Z);
}

void KernelDescriptor::setCOMPUTE_PGM_RSRC2_EnableSgprWorkgroupIdZ(bool value) {
  uint32_t fourByteBuffer = kdRepr.compute_pgm_rsrc2;
  fourByteBuffer = CLEAR_BITS(COMPUTE_PGM_RSRC2_ENABLE_SGPR_WORKGROUP_ID_Z);
  kdRepr.compute_pgm_rsrc1 =
      SET_VALUE(COMPUTE_PGM_RSRC2_ENABLE_SGPR_WORKGROUP_ID_Z);
}

bool KernelDescriptor::getCOMPUTE_PGM_RSRC2_EnableSgprWorkgroupInfo() const {
  uint32_t fourByteBuffer = kdRepr.compute_pgm_rsrc2;
  return GET_VALUE(COMPUTE_PGM_RSRC2_ENABLE_SGPR_WORKGROUP_INFO);
}

void KernelDescriptor::setCOMPUTE_PGM_RSRC2_EnableSgprWorkgroupInfo(
    bool value) {
  uint32_t fourByteBuffer = kdRepr.compute_pgm_rsrc2;
  fourByteBuffer = CLEAR_BITS(COMPUTE_PGM_RSRC2_ENABLE_SGPR_WORKGROUP_INFO);
  kdRepr.compute_pgm_rsrc1 =
      SET_VALUE(COMPUTE_PGM_RSRC2_ENABLE_SGPR_WORKGROUP_INFO);
}

uint32_t KernelDescriptor::getCOMPUTE_PGM_RSRC2_EnableVgprWorkitemId() const {
  uint32_t fourByteBuffer = kdRepr.compute_pgm_rsrc2;
  return GET_VALUE(COMPUTE_PGM_RSRC2_ENABLE_VGPR_WORKITEM_ID);
}

void KernelDescriptor::setCOMPUTE_PGM_RSRC2_EnableVgprWorkitemId(
    uint32_t value) {
  uint32_t fourByteBuffer = kdRepr.compute_pgm_rsrc2;
  fourByteBuffer = CLEAR_BITS(COMPUTE_PGM_RSRC2_ENABLE_VGPR_WORKITEM_ID);
  assert(CHECK_WIDTH(COMPUTE_PGM_RSRC2_ENABLE_VGPR_WORKITEM_ID) &&
         "value contains more bits than specificied");
  kdRepr.compute_pgm_rsrc1 =
      SET_VALUE(COMPUTE_PGM_RSRC2_ENABLE_VGPR_WORKITEM_ID);
}

bool KernelDescriptor::getCOMPUTE_PGM_RSRC2_EnableExceptionAddressWatch()
    const {
  uint32_t fourByteBuffer = kdRepr.compute_pgm_rsrc2;
  return GET_VALUE(COMPUTE_PGM_RSRC2_ENABLE_EXCEPTION_ADDRESS_WATCH);
}

bool KernelDescriptor::getCOMPUTE_PGM_RSRC2_EnableExceptionMemory() const {
  uint32_t fourByteBuffer = kdRepr.compute_pgm_rsrc2;
  return GET_VALUE(COMPUTE_PGM_RSRC2_ENABLE_EXCEPTION_MEMORY);
}

uint32_t KernelDescriptor::getCOMPUTE_PGM_RSRC2_GranulatedLdsSize() const {
  uint32_t fourByteBuffer = kdRepr.compute_pgm_rsrc2;
  return GET_VALUE(COMPUTE_PGM_RSRC2_GRANULATED_LDS_SIZE);
}

bool KernelDescriptor::
    getCOMPUTE_PGM_RSRC2_EnableExceptionIeee754FpInvalidOperation() const {
  uint32_t fourByteBuffer = kdRepr.compute_pgm_rsrc2;
  return GET_VALUE(
      COMPUTE_PGM_RSRC2_ENABLE_EXCEPTION_IEEE_754_FP_INVALID_OPERATION);
}

void KernelDescriptor::
    getCOMPUTE_PGM_RSRC2_EnableExceptionIeee754FpInvalidOperation(bool value) {
  uint32_t fourByteBuffer = kdRepr.compute_pgm_rsrc2;
  fourByteBuffer = CLEAR_BITS(
      COMPUTE_PGM_RSRC2_ENABLE_EXCEPTION_IEEE_754_FP_INVALID_OPERATION);
  kdRepr.compute_pgm_rsrc1 = SET_VALUE(
      COMPUTE_PGM_RSRC2_ENABLE_EXCEPTION_IEEE_754_FP_INVALID_OPERATION);
}

bool KernelDescriptor::getCOMPUTE_PGM_RSRC2_EnableExceptionFpDenormalSource()
    const {
  uint32_t fourByteBuffer = kdRepr.compute_pgm_rsrc2;
  return GET_VALUE(COMPUTE_PGM_RSRC2_ENABLE_EXCEPTION_FP_DENORMAL_SOURCE);
}

void KernelDescriptor::setCOMPUTE_PGM_RSRC2_EnableExceptionFpDenormalSource(
    bool value) {
  uint32_t fourByteBuffer = kdRepr.compute_pgm_rsrc2;
  fourByteBuffer =
      CLEAR_BITS(COMPUTE_PGM_RSRC2_ENABLE_EXCEPTION_FP_DENORMAL_SOURCE);
  kdRepr.compute_pgm_rsrc1 =
      SET_VALUE(COMPUTE_PGM_RSRC2_ENABLE_EXCEPTION_FP_DENORMAL_SOURCE);
}

bool KernelDescriptor::
    getCOMPUTE_PGM_RSRC2_EnableExceptionIeee754FpDivisionByZero() const {
  uint32_t fourByteBuffer = kdRepr.compute_pgm_rsrc2;
  return GET_VALUE(
      COMPUTE_PGM_RSRC2_ENABLE_EXCEPTION_IEEE_754_FP_DIVISION_BY_ZERO);
}

void KernelDescriptor::
    setCOMPUTE_PGM_RSRC2_EnableExceptionIeee754FpDivisionByZero(bool value) {
  uint32_t fourByteBuffer = kdRepr.compute_pgm_rsrc2;
  fourByteBuffer = CLEAR_BITS(
      COMPUTE_PGM_RSRC2_ENABLE_EXCEPTION_IEEE_754_FP_DIVISION_BY_ZERO);
  kdRepr.compute_pgm_rsrc1 = SET_VALUE(
      COMPUTE_PGM_RSRC2_ENABLE_EXCEPTION_IEEE_754_FP_DIVISION_BY_ZERO);
}

bool KernelDescriptor::getCOMPUTE_PGM_RSRC2_EnableExceptionIeee754FpOverflow()
    const {
  uint32_t fourByteBuffer = kdRepr.compute_pgm_rsrc2;
  return GET_VALUE(COMPUTE_PGM_RSRC2_ENABLE_EXCEPTION_IEEE_754_FP_OVERFLOW);
}

void KernelDescriptor::setCOMPUTE_PGM_RSRC2_EnableExceptionIeee754FpOverflow(
    bool value) {
  uint32_t fourByteBuffer = kdRepr.compute_pgm_rsrc2;
  fourByteBuffer =
      CLEAR_BITS(COMPUTE_PGM_RSRC2_ENABLE_EXCEPTION_IEEE_754_FP_OVERFLOW);
  kdRepr.compute_pgm_rsrc1 =
      SET_VALUE(COMPUTE_PGM_RSRC2_ENABLE_EXCEPTION_IEEE_754_FP_OVERFLOW);
}

bool KernelDescriptor::getCOMPUTE_PGM_RSRC2_EnableExceptionIeee754FpUnderflow()
    const {
  uint32_t fourByteBuffer = kdRepr.compute_pgm_rsrc2;
  return GET_VALUE(COMPUTE_PGM_RSRC2_ENABLE_EXCEPTION_IEEE_754_FP_UNDERFLOW);
}

void KernelDescriptor::setCOMPUTE_PGM_RSRC2_EnableExceptionIeee754FpUnderflow(
    bool value) {
  uint32_t fourByteBuffer = kdRepr.compute_pgm_rsrc2;
  fourByteBuffer =
      CLEAR_BITS(COMPUTE_PGM_RSRC2_ENABLE_EXCEPTION_IEEE_754_FP_UNDERFLOW);
  kdRepr.compute_pgm_rsrc1 =
      SET_VALUE(COMPUTE_PGM_RSRC2_ENABLE_EXCEPTION_IEEE_754_FP_UNDERFLOW);
}

bool KernelDescriptor::getCOMPUTE_PGM_RSRC2_EnableExceptionIeee754FpInexact()
    const {
  uint32_t fourByteBuffer = kdRepr.compute_pgm_rsrc2;
  return GET_VALUE(COMPUTE_PGM_RSRC2_ENABLE_EXCEPTION_IEEE_754_FP_INEXACT);
}

void KernelDescriptor::setCOMPUTE_PGM_RSRC2_EnableExceptionIeee754FpInexact(
    bool value) {
  uint32_t fourByteBuffer = kdRepr.compute_pgm_rsrc2;
  fourByteBuffer =
      CLEAR_BITS(COMPUTE_PGM_RSRC2_ENABLE_EXCEPTION_IEEE_754_FP_INEXACT);
  kdRepr.compute_pgm_rsrc1 =
      SET_VALUE(COMPUTE_PGM_RSRC2_ENABLE_EXCEPTION_IEEE_754_FP_INEXACT);
}

bool KernelDescriptor::getCOMPUTE_PGM_RSRC2_EnableExceptionIntDivideByZero()
    const {
  uint32_t fourByteBuffer = kdRepr.compute_pgm_rsrc2;
  return GET_VALUE(COMPUTE_PGM_RSRC2_ENABLE_EXCEPTION_INT_DIVIDE_BY_ZERO);
}

void KernelDescriptor::setCOMPUTE_PGM_RSRC2_EnableExceptionIntDivideByZero(
    bool value) {
  uint32_t fourByteBuffer = kdRepr.compute_pgm_rsrc2;
  fourByteBuffer =
      CLEAR_BITS(COMPUTE_PGM_RSRC2_ENABLE_EXCEPTION_INT_DIVIDE_BY_ZERO);
  kdRepr.compute_pgm_rsrc1 =
      SET_VALUE(COMPUTE_PGM_RSRC2_ENABLE_EXCEPTION_INT_DIVIDE_BY_ZERO);
}
//
// ----- COMPUTE_PGM_RSRC2 end -----
//

#undef GET_VALUE
#undef SET_VALUE
#undef CLEAR_BITS
#undef CHECK_WIDTH

#define GET_VALUE(MASK) ((twoByteBuffer & MASK) >> (MASK##_SHIFT))
#define SET_VALUE(MASK) (twoByteBuffer | ((value) << (MASK##_SHIFT)))
#define CLEAR_BITS(MASK) ((twoByteBuffer & !MASK))
#define CHECK_WIDTH(MASK) ((value) >> (MASK##_WIDTH) == 0)

// ----- KERNEL_CODE_PROPERTIES begin -----
//
bool KernelDescriptor::getKernelCodeProperty_EnableSgprPrivateSegmentBuffer()
    const {
  uint16_t twoByteBuffer = kdRepr.kernel_code_properties;
  return GET_VALUE(KERNEL_CODE_PROPERTY_ENABLE_SGPR_PRIVATE_SEGMENT_BUFFER);
}

void KernelDescriptor::setKernelCodeProperty_EnableSgprPrivateSegmentBuffer(
    bool value) {
  uint16_t twoByteBuffer = kdRepr.kernel_code_properties;
  twoByteBuffer =
      CLEAR_BITS(KERNEL_CODE_PROPERTY_ENABLE_SGPR_PRIVATE_SEGMENT_BUFFER);
  kdRepr.kernel_code_properties =
      SET_VALUE(KERNEL_CODE_PROPERTY_ENABLE_SGPR_PRIVATE_SEGMENT_BUFFER);
}

bool KernelDescriptor::getKernelCodeProperty_EnableSgprDispatchPtr() const {
  uint16_t twoByteBuffer = kdRepr.kernel_code_properties;
  return GET_VALUE(KERNEL_CODE_PROPERTY_ENABLE_SGPR_DISPATCH_PTR);
}

void KernelDescriptor::setKernelCodeProperty_EnableSgprDispatchPtr(bool value) {
  uint16_t twoByteBuffer = kdRepr.kernel_code_properties;
  twoByteBuffer = CLEAR_BITS(KERNEL_CODE_PROPERTY_ENABLE_SGPR_DISPATCH_PTR);
  kdRepr.kernel_code_properties =
      SET_VALUE(KERNEL_CODE_PROPERTY_ENABLE_SGPR_DISPATCH_PTR);
}

bool KernelDescriptor::getKernelCodeProperty_EnableSgprQueuePtr() const {
  uint16_t twoByteBuffer = kdRepr.kernel_code_properties;
  return GET_VALUE(KERNEL_CODE_PROPERTY_ENABLE_SGPR_QUEUE_PTR);
}

void KernelDescriptor::setKernelCodeProperty_EnableSgprQueuePtr(bool value) {
  uint16_t twoByteBuffer = kdRepr.kernel_code_properties;
  twoByteBuffer = CLEAR_BITS(KERNEL_CODE_PROPERTY_ENABLE_SGPR_QUEUE_PTR);
  kdRepr.kernel_code_properties =
      SET_VALUE(KERNEL_CODE_PROPERTY_ENABLE_SGPR_QUEUE_PTR);
}

bool KernelDescriptor::getKernelCodeProperty_EnableSgprKernargSegmentPtr()
    const {
  uint16_t twoByteBuffer = kdRepr.kernel_code_properties;
  return GET_VALUE(KERNEL_CODE_PROPERTY_ENABLE_SGPR_KERNARG_SEGMENT_PTR);
}

void KernelDescriptor ::setKernelCodeProperty_EnableSgprKernargSegmentPtr(
    bool value) {
  uint16_t twoByteBuffer = kdRepr.kernel_code_properties;
  twoByteBuffer =
      CLEAR_BITS(KERNEL_CODE_PROPERTY_ENABLE_SGPR_KERNARG_SEGMENT_PTR);
  kdRepr.kernel_code_properties =
      SET_VALUE(KERNEL_CODE_PROPERTY_ENABLE_SGPR_KERNARG_SEGMENT_PTR);
}

bool KernelDescriptor::getKernelCodeProperty_EnableSgprDispatchId() const {
  uint16_t twoByteBuffer = kdRepr.kernel_code_properties;
  return GET_VALUE(KERNEL_CODE_PROPERTY_ENABLE_SGPR_DISPATCH_ID);
}

void KernelDescriptor::setKernelCodeProperty_EnableSgprDispatchId(bool value) {
  uint16_t twoByteBuffer = kdRepr.kernel_code_properties;
  twoByteBuffer = CLEAR_BITS(KERNEL_CODE_PROPERTY_ENABLE_SGPR_DISPATCH_ID);
  kdRepr.kernel_code_properties =
      SET_VALUE(KERNEL_CODE_PROPERTY_ENABLE_SGPR_DISPATCH_ID);
}

bool KernelDescriptor::getKernelCodeProperty_EnableSgprFlatScratchInit() const {
  uint16_t twoByteBuffer = kdRepr.kernel_code_properties;
  return GET_VALUE(KERNEL_CODE_PROPERTY_ENABLE_SGPR_FLAT_SCRATCH_INIT);
}

void KernelDescriptor::setKernelCodeProperty_EnableSgprFlatScratchInit(
    bool value) {
  uint16_t twoByteBuffer = kdRepr.kernel_code_properties;
  twoByteBuffer =
      CLEAR_BITS(KERNEL_CODE_PROPERTY_ENABLE_SGPR_FLAT_SCRATCH_INIT);
  kdRepr.kernel_code_properties =
      SET_VALUE(KERNEL_CODE_PROPERTY_ENABLE_SGPR_FLAT_SCRATCH_INIT);
}

bool KernelDescriptor::getKernelCodeProperty_EnablePrivateSegmentSize() const {
  uint16_t twoByteBuffer = kdRepr.kernel_code_properties;
  return GET_VALUE(KERNEL_CODE_PROPERTY_ENABLE_SGPR_PRIVATE_SEGMENT_SIZE);
}

void KernelDescriptor::setKernelCodeProperty_EnablePrivateSegmentSize(
    bool value) {
  uint16_t twoByteBuffer = kdRepr.kernel_code_properties;
  twoByteBuffer =
      CLEAR_BITS(KERNEL_CODE_PROPERTY_ENABLE_SGPR_PRIVATE_SEGMENT_SIZE);
  kdRepr.kernel_code_properties =
      SET_VALUE(KERNEL_CODE_PROPERTY_ENABLE_SGPR_PRIVATE_SEGMENT_SIZE);
}

// TODO: RESERVED FOR GFX9, ADD CHECKS ACCORDINGLY
bool KernelDescriptor::getKernelCodeProperty_EnableWavefrontSize32() const {
  uint16_t twoByteBuffer = kdRepr.kernel_code_properties;
  return GET_VALUE(KERNEL_CODE_PROPERTY_ENABLE_WAVEFRONT_SIZE32);
}

void KernelDescriptor::setKernelCodeProperty_EnableWavefrontSize32(bool value) {
  uint16_t twoByteBuffer = kdRepr.kernel_code_properties;
  twoByteBuffer = CLEAR_BITS(KERNEL_CODE_PROPERTY_ENABLE_WAVEFRONT_SIZE32);
  kdRepr.kernel_code_properties =
      SET_VALUE(KERNEL_CODE_PROPERTY_ENABLE_WAVEFRONT_SIZE32);
}

// TODO: fix this in llvm KD first
bool KernelDescriptor::getKernelCodeProperty_UsesDynamicStack() const {
  // uint16_t twoByteBuffer = kdRepr.kernel_code_properties;
  // return GET_VALUE();
  return 0;
}

void KernelDescriptor::setKernelCodeProperty_UsesDynamicStack(bool value) {
  // uint16_t twoByteBuffer = kdRepr.kernel_code_properties;
  // twoByteBuffer =
  //     CLEAR_BITS();
  // kdRepr.kernel_code_properties =
  //     SET_VALUE();
}
//
// ----- KERNEL_CODE_PROPERTIES end -----
//

#undef GET_VALUE
#undef SET_VALUE
#undef CLEAR_BITS
#undef CHECK_WIDTH

void KernelDescriptor::dump(std::ostream &os) const {
  os << std::hex;
  os << "GROUP_SEGMENT_FIXED_SIZE : "
     << "0x" << kdRepr.group_segment_fixed_size << '\n';
  os << "PRIVATE_SEGMENT_FIXED_SIZE : "
     << "0x" << kdRepr.private_segment_fixed_size << '\n';
  os << "KERNARG_SIZE : "
     << "0x" << kdRepr.kernarg_size << '\n';
  os << "KERNEL_CODE_ENTRY_BYTE_OFFSET : "
     << "0x" << kdRepr.kernel_code_entry_byte_offset << '\n';
  os << "COMPUTE_PGM_RSRC3 : "
     << "0x" << kdRepr.compute_pgm_rsrc3 << '\n';
  os << "COMPUTE_PGM_RSRC1 : "
     << "0x" << kdRepr.compute_pgm_rsrc1 << '\n';
  os << "COMPUTE_PGM_RSRC2 : "
     << "0x" << kdRepr.compute_pgm_rsrc2 << '\n';
  os << "KERNEL_CODE_PROPERTIES : " << kdRepr.kernel_code_properties << '\n';
  os << std::dec;
}

void KernelDescriptor::dumpDetailed(std::ostream &os) const { os << "TODO\n"; }
