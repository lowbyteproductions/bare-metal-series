#include "core/crc.h"

uint8_t crc8(uint8_t* data, uint32_t length) {
  uint8_t crc = 0;

  for (uint32_t i = 0; i < length; i++) {
    crc ^= data[i];
    for (uint8_t j = 0; j < 8; j++) {
      if (crc & 0x80) {
        crc = (crc << 1) ^ 0x07;
      } else {
        crc <<= 1;
      }
    }
  }

  return crc;
}

uint32_t crc32(const uint8_t* data, const uint32_t length) {
   uint8_t byte;
   uint32_t crc = 0xffffffff;
   uint32_t mask;

   for (uint32_t i = 0; i < length; i++) {
      byte = data[i];
      crc = crc ^ byte;

      for (uint8_t j = 0; j < 8; j++) {
         mask = -(crc & 1);
         crc = (crc >> 1) ^ (0xedb88320 & mask);
      }
   }

   return ~crc;
}

