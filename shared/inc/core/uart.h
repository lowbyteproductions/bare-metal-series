#ifndef INC_UART_H
#define INC_UART_H

#include "common-defines.h"

void uart_setup(void);
void uart_teardown(void);
void uart_write(uint8_t* data, const uint32_t length);
void uart_write_byte(uint8_t data);
uint32_t uart_read(uint8_t* data, const uint32_t length);
uint8_t uart_read_byte(void);
bool uart_data_available(void);

#endif // INC_UART_H
