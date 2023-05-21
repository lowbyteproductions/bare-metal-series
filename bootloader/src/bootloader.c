#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/memorymap.h>
#include <libopencm3/cm3/vector.h>
#include <libopencm3/cm3/scb.h>

#include <string.h>

#include "aes.h"
#include "core/firmware-info.h"
#include "core/uart.h"
#include "core/system.h"
#include "core/crc.h"
#include "core/simple-timer.h"
#include "comms.h"
#include "bl-flash.h"

#define UART_PORT     (GPIOA)
#define RX_PIN        (GPIO3)
#define TX_PIN        (GPIO2)

#define SYNC_SEQ_0 (0xc4)
#define SYNC_SEQ_1 (0x55)
#define SYNC_SEQ_2 (0x7e)
#define SYNC_SEQ_3 (0x10)

#define DEFAULT_TIMEOUT (5000)

typedef enum bl_state_t {
  BL_State_Sync,
  BL_State_WaitForUpdateReq,
  BL_State_DeviceIDReq,
  BL_State_DeviceIDRes,
  BL_State_FWLengthReq,
  BL_State_FWLengthRes,
  BL_State_EraseApplication,
  BL_State_ReceiveFirmware,
  BL_State_Done,
} bl_state_t;

static bl_state_t state = BL_State_Sync;
static uint32_t fw_length = 0;
static uint32_t bytes_written = 0;
static uint8_t sync_seq[4] = {0};
static simple_timer_t timer;
static comms_packet_t temp_packet;

static const uint8_t secret_key[AES_BLOCK_SIZE] = {
  0x00, 0x01, 0x02, 0x03,
  0x04, 0x05, 0x06, 0x07,
  0x08, 0x09, 0x0a, 0x0b,
  0x0c, 0x0d, 0x0e, 0x0f,
};

static void gpio_setup(void) {
  rcc_periph_clock_enable(RCC_GPIOA);
  gpio_mode_setup(UART_PORT, GPIO_MODE_AF, GPIO_PUPD_NONE, TX_PIN | RX_PIN);
  gpio_set_af(UART_PORT, GPIO_AF7, TX_PIN | RX_PIN);
}

static void gpio_teardown(void) {
  gpio_mode_setup(UART_PORT, GPIO_MODE_ANALOG, GPIO_PUPD_NONE, TX_PIN | RX_PIN);
  rcc_periph_clock_disable(RCC_GPIOA);
}

static void jump_to_main(void) {
  vector_table_t* main_vector_table = (vector_table_t*)MAIN_APP_START_ADDRESS;
  main_vector_table->reset();
}

static void aes_cbc_mac_step(AES_Block_t aes_state, AES_Block_t prev_state, const AES_Block_t *key_schedule) {
  // The CBC chaining operation
  for (uint8_t i = 0; i < AES_BLOCK_SIZE; i++) {
    ((uint8_t*)aes_state)[i] ^= ((uint8_t*)prev_state)[i];
  }

  AES_EncryptBlock(aes_state, key_schedule);
  memcpy(prev_state, aes_state, AES_BLOCK_SIZE);
}

static bool validate_firmware_image(void) {
  firmware_info_t* firmware_info = (firmware_info_t*)FWINFO_ADDRESS;
  const uint8_t* signature = (const uint8_t*)SIGNATURE_ADDRESS;

  if (firmware_info->sentinel != FWINFO_SENTINEL) {
    return false;
  }

  if (firmware_info->device_id != DEVICE_ID) {
    return false;
  }

  AES_Block_t round_keys[NUM_ROUND_KEYS_128];
  AES_KeySchedule128(secret_key, round_keys);

  AES_Block_t aes_state = {0};
  AES_Block_t prev_state = {0};

  uint8_t bytes_to_pad = 16 - (firmware_info->length % 16);
  if (bytes_to_pad == 0) {
    bytes_to_pad = 16;
  }

  memcpy(aes_state, firmware_info, AES_BLOCK_SIZE);
  aes_cbc_mac_step(aes_state, prev_state, round_keys);

  uint32_t offset = 0;
  while (offset < firmware_info->length) {
    // Are we are the point where we need to skip the info and signature sections?
    if (offset == (FWINFO_ADDRESS - MAIN_APP_START_ADDRESS)) {
      offset += AES_BLOCK_SIZE * 2;
      continue;
    }

    if (firmware_info->length - offset > AES_BLOCK_SIZE) {
      // The regular case
      memcpy(aes_state, (void*)(MAIN_APP_START_ADDRESS + offset), AES_BLOCK_SIZE);
      aes_cbc_mac_step(aes_state, prev_state, round_keys);
    } else {
      // The case of padding
      if (bytes_to_pad == 16) {
        // Add a whole extra block of padding
        memcpy(aes_state, (void*)(MAIN_APP_START_ADDRESS + offset), AES_BLOCK_SIZE);
        aes_cbc_mac_step(aes_state, prev_state, round_keys);

        memset(aes_state, AES_BLOCK_SIZE, AES_BLOCK_SIZE);
        aes_cbc_mac_step(aes_state, prev_state, round_keys);
      } else {
        memcpy(aes_state, (void*)(MAIN_APP_START_ADDRESS + offset), AES_BLOCK_SIZE - bytes_to_pad);
        memset((void*)(aes_state) + (AES_BLOCK_SIZE - bytes_to_pad), bytes_to_pad, bytes_to_pad);
        aes_cbc_mac_step(aes_state, prev_state, round_keys);
      }
    }

    offset += AES_BLOCK_SIZE;
  }

  return memcmp(signature, aes_state, AES_BLOCK_SIZE) == 0;
}

static void bootloading_fail(void) {
  comms_create_single_byte_packet(&temp_packet, BL_PACKET_NACK_DATA0);
  comms_write(&temp_packet);
  state = BL_State_Done;
}

static void check_for_timeout(void) {
  if (simple_timer_has_elapsed(&timer)) {
    bootloading_fail();
  }
}

static bool is_device_id_packet(const comms_packet_t* packet) {
  if (packet->length != 2) {
    return false;
  }

  if (packet->data[0] != BL_PACKET_DEVICE_ID_RES_DATA0) {
    return false;
  }

  for (uint8_t i = 2; i < PACKET_DATA_LENGTH; i++) {
    if (packet->data[i] != 0xff) {
      return false;
    }
  }

  return true;
}

static bool is_fw_length_packet(const comms_packet_t* packet) {
  if (packet->length != 5) {
    return false;
  }

  if (packet->data[0] != BL_PACKET_FW_LENGTH_RES_DATA0) {
    return false;
  }

  for (uint8_t i = 5; i < PACKET_DATA_LENGTH; i++) {
    if (packet->data[i] != 0xff) {
      return false;
    }
  }

  return true;
}

int main(void) {
  system_setup();
  gpio_setup();
  uart_setup();
  comms_setup();

  simple_timer_setup(&timer, DEFAULT_TIMEOUT, false);

  while (state != BL_State_Done) {
    if (state == BL_State_Sync) {
      if (uart_data_available()) {
        sync_seq[0] = sync_seq[1];
        sync_seq[1] = sync_seq[2];
        sync_seq[2] = sync_seq[3];
        sync_seq[3] = uart_read_byte();

        bool is_match = sync_seq[0] == SYNC_SEQ_0;
        is_match = is_match && (sync_seq[1] == SYNC_SEQ_1);
        is_match = is_match && (sync_seq[2] == SYNC_SEQ_2);
        is_match = is_match && (sync_seq[3] == SYNC_SEQ_3);

        if (is_match) {
          comms_create_single_byte_packet(&temp_packet, BL_PACKET_SYNC_OBSERVED_DATA0);
          comms_write(&temp_packet);
          simple_timer_reset(&timer);
          state = BL_State_WaitForUpdateReq;
        } else {
          check_for_timeout();
        }
      } else {
        check_for_timeout();
      }

      continue;
    }

    comms_update();

    switch (state) {
      case BL_State_WaitForUpdateReq: {
        if (comms_packets_available()) {
          comms_read(&temp_packet);

          if (comms_is_single_byte_packet(&temp_packet, BL_PACKET_FW_UPDATE_REQ_DATA0)) {
            simple_timer_reset(&timer);
            comms_create_single_byte_packet(&temp_packet, BL_PACKET_FW_UPDATE_RES_DATA0);
            comms_write(&temp_packet);
            state = BL_State_DeviceIDReq;
          } else {
            bootloading_fail();
          }
        } else {
          check_for_timeout();
        }
      } break;

      case BL_State_DeviceIDReq: {
        simple_timer_reset(&timer);
        comms_create_single_byte_packet(&temp_packet, BL_PACKET_DEVICE_ID_REQ_DATA0);
        comms_write(&temp_packet);
        state = BL_State_DeviceIDRes;
      } break;

      case BL_State_DeviceIDRes: {
        if (comms_packets_available()) {
          comms_read(&temp_packet);

          if (is_device_id_packet(&temp_packet) && (temp_packet.data[1] == DEVICE_ID)) {
            simple_timer_reset(&timer);
            state = BL_State_FWLengthReq;
          } else {
            bootloading_fail();
          }
        } else {
          check_for_timeout();
        }
      } break;

      case BL_State_FWLengthReq: {
        simple_timer_reset(&timer);
        comms_create_single_byte_packet(&temp_packet, BL_PACKET_FW_LENGTH_REQ_DATA0);
        comms_write(&temp_packet);
        state = BL_State_FWLengthRes;
      } break;

      case BL_State_FWLengthRes: {
        if (comms_packets_available()) {
          comms_read(&temp_packet);

          fw_length = (
            (temp_packet.data[1])       |
            (temp_packet.data[2] << 8)  |
            (temp_packet.data[3] << 16) |
            (temp_packet.data[4] << 24)
          );

          if (is_fw_length_packet(&temp_packet) && (fw_length <= MAX_FW_LENGTH)) {
            state = BL_State_EraseApplication;
          } else {
            bootloading_fail();
          }
        } else {
          check_for_timeout();
        }
      } break;

      case BL_State_EraseApplication: {
        bl_flash_erase_main_application();
        comms_create_single_byte_packet(&temp_packet, BL_PACKET_READY_FOR_DATA_DATA0);
        comms_write(&temp_packet);
        simple_timer_reset(&timer);
        state = BL_State_ReceiveFirmware;
      } break;

      case BL_State_ReceiveFirmware: {
        if (comms_packets_available()) {
          comms_read(&temp_packet);

          const uint8_t packet_length = (temp_packet.length & 0x0f) + 1;
          bl_flash_write(MAIN_APP_START_ADDRESS + bytes_written, temp_packet.data, packet_length);
          bytes_written += packet_length;
          simple_timer_reset(&timer);

          if (bytes_written >= fw_length) {
            comms_create_single_byte_packet(&temp_packet, BL_PACKET_UPDATE_SUCCESSFUL_DATA0);
            comms_write(&temp_packet);
            state = BL_State_Done;
          } else {
            comms_create_single_byte_packet(&temp_packet, BL_PACKET_READY_FOR_DATA_DATA0);
            comms_write(&temp_packet);
          }
        } else {
          check_for_timeout();
        }
      } break;

      default: {
        state = BL_State_Sync;
      }
    }

  }

  system_delay(150);
  uart_teardown();
  gpio_teardown();
  system_teardown();

  if (validate_firmware_image()) {
    jump_to_main();
  } else {
    scb_reset_core();
  }

  // Never return
  return 0;
}
