#include "core/ring-buffer.h"

void ring_buffer_setup(ring_buffer_t* rb, uint8_t* buffer, uint32_t size) {
  rb->buffer = buffer;
  rb->read_index = 0;
  rb->write_index = 0;
  rb->mask = size - 1;
}

bool ring_buffer_empty(ring_buffer_t* rb) {
  return rb->read_index == rb->write_index;
}

bool ring_buffer_read(ring_buffer_t* rb, uint8_t* byte) {
  uint32_t local_read_index = rb->read_index;
  uint32_t local_write_index = rb->write_index;

  if (local_read_index == local_write_index) {
    return false;
  }

  *byte = rb->buffer[local_read_index];
  local_read_index = (local_read_index + 1) & rb->mask;
  rb->read_index = local_read_index;

  return true;
}

bool ring_buffer_write(ring_buffer_t* rb, uint8_t byte) {
  uint32_t local_write_index = rb->write_index;
  uint32_t local_read_index = rb->read_index;

  uint32_t next_write_index = (local_write_index + 1) & rb->mask;

  if (next_write_index == local_read_index) {
    return false;
  }

  rb->buffer[local_write_index] = byte;
  rb->write_index = next_write_index;
  return true;
}

