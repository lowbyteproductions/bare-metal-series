#include <libopencm3/cm3/nvic.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/usart.h>

#include "core/uart.h"

#define BAUD_RATE (115200)

static uint8_t data_buffer = 0U;
static bool data_available = false;

void usart2_isr(void) {
  const bool overrun_occurred = usart_get_flag(USART2, USART_FLAG_ORE) == 1;
  const bool received_data = usart_get_flag(USART2, USART_FLAG_RXNE) == 1;

  if (received_data || overrun_occurred) {
    data_buffer = (uint8_t)usart_recv(USART2);
    data_available = true;
  }
}

void uart_setup(void) {
  rcc_periph_clock_enable(RCC_USART2);

  usart_set_mode(USART2, USART_MODE_TX_RX);
  usart_set_flow_control(USART2, USART_FLOWCONTROL_NONE);
  usart_set_databits(USART2, 8);
  usart_set_baudrate(USART2, BAUD_RATE);
  usart_set_parity(USART2, 0);
  usart_set_stopbits(USART2, 1);

  usart_enable_rx_interrupt(USART2);
  nvic_enable_irq(NVIC_USART2_IRQ);

  usart_enable(USART2);
}

void uart_write(uint8_t* data, const uint32_t length) {
  for (uint32_t i = 0; i < length; i++) {
    uart_write_byte(data[i]);
  }
}

void uart_write_byte(uint8_t data) {
  usart_send_blocking(USART2, (uint16_t)data);
}

uint32_t uart_read(uint8_t* data, const uint32_t length) {
  if (length > 0 && data_available) {
    *data = data_buffer;
    data_available = false;
    return 1;
  }
  return 0;
}

uint8_t uart_read_byte(void) {
  data_available = false;
  return data_buffer;
}

bool uart_data_available(void) {
  return data_available;
}
