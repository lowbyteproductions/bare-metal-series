#include "core/firmware-info.h"

__attribute__ ((section (".firmware_info")))
firmware_info_t firmware_info = {
  .sentinel    = FWINFO_SENTINEL,
  .device_id   = DEVICE_ID,
  .version     = 0xffffffff,
  .length      = 0xffffffff,
  .reserved0   = 0xffffffff,
  .reserved1   = 0xffffffff,
  .reserved2   = 0xffffffff,
  .reserved3   = 0xffffffff,
  .reserved4   = 0xffffffff,
  .crc32       = 0xffffffff,
};
