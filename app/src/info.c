#include "core/firmware-info.h"

__attribute__ ((section (".firmware_info")))
firmware_info_t firmware_info = {
  .sentinel    = FWINFO_SENTINEL,
  .device_id   = DEVICE_ID,
  .version     = 0xffffffff,
  .length      = 0xffffffff,
};

__attribute__ ((section (".firmware_signature")))
uint8_t firmware_signature[16] = {0};
