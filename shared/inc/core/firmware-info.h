#ifndef INC_FIRMWARE_INFO_H
#define INC_FIRMWARE_INFO_H

#include <libopencm3/stm32/flash.h>
#include <libopencm3/cm3/vector.h>
#include "common-defines.h"

#define BOOTLOADER_SIZE                   (0x8000U)
#define MAIN_APP_START_ADDRESS            (FLASH_BASE + BOOTLOADER_SIZE)
#define MAX_FW_LENGTH                     ((1024U * 512U) - BOOTLOADER_SIZE)
#define DEVICE_ID                         (0x42)

#define FWINFO_SENTINEL                   (0xDEADC0DE)
#define FWINFO_ADDRESS                    (MAIN_APP_START_ADDRESS + sizeof(vector_table_t))
#define FWINFO_VALIDATE_FROM              (FWINFO_ADDRESS + sizeof(firmware_info_t))
#define FWINFO_VALIDATE_LENGTH(fw_length) (fw_length - (sizeof(vector_table_t) + sizeof(firmware_info_t)))

typedef struct firmware_info_t {
  uint32_t sentinel;
  uint32_t device_id;
  uint32_t version;
  uint32_t length;
  uint32_t reserved0;
  uint32_t reserved1;
  uint32_t reserved2;
  uint32_t reserved3;
  uint32_t reserved4;
  uint32_t crc32;
} firmware_info_t;

#endif // INC_FIRMWARE_INFO_H
