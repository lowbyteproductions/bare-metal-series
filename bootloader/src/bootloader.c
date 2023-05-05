#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/memorymap.h>
#include <libopencm3/cm3/vector.h>

#include "core/uart.h"
#include "core/system.h"
#include "core/simple-timer.h"
#include "comms.h"
#include "bl-flash.h"

#define UART_PORT     (GPIOA)
#define RX_PIN        (GPIO3)
#define TX_PIN        (GPIO2)

#define BOOTLOADER_SIZE        (0x8000U)
#define MAIN_APP_START_ADDRESS (FLASH_BASE + BOOTLOADER_SIZE)
#define MAX_FW_LENGTH          ((1024U * 512U) - BOOTLOADER_SIZE)

#define DEVICE_ID (0x42)

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

  jump_to_main();

  // Never return
  return 0;
}
