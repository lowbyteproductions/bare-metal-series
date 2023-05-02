import {SerialPort} from 'serialport';

// Constants for the packet protocol
const PACKET_LENGTH_BYTES   = 1;
const PACKET_DATA_BYTES     = 16;
const PACKET_CRC_BYTES      = 1;
const PACKET_CRC_INDEX      = PACKET_LENGTH_BYTES + PACKET_DATA_BYTES;
const PACKET_LENGTH         = PACKET_LENGTH_BYTES + PACKET_DATA_BYTES + PACKET_CRC_BYTES;

const PACKET_ACK_DATA0      = 0x15;
const PACKET_RETX_DATA0     = 0x19;

// Details about the serial port connection
const serialPath            = "/dev/ttyUSB0";
const baudRate              = 115200;

// CRC8 implementation
const crc8 = (data: Buffer | Array<number>) => {
  let crc = 0;

  for (const byte of data) {
    crc = (crc ^ byte) & 0xff;
    for (let i = 0; i < 8; i++) {
      if (crc & 0x80) {
        crc = ((crc << 1) ^ 0x07) & 0xff;
      } else {
        crc = (crc << 1) & 0xff;
      }
    }
  }

  return crc;
};

// Async delay function, which gives the event loop time to process outside input
const delay = (ms: number) => new Promise(r => setTimeout(r, ms));

// Class for serialising and deserialising packets
class Packet {
  length: number;
  data: Buffer;
  crc: number;

  static retx = new Packet(1, Buffer.from([PACKET_RETX_DATA0])).toBuffer();
  static ack = new Packet(1, Buffer.from([PACKET_ACK_DATA0])).toBuffer();

  constructor(length: number, data: Buffer, crc?: number) {
    this.length = length;
    this.data = data;

    const bytesToPad = PACKET_DATA_BYTES - this.data.length;
    const padding = Buffer.alloc(bytesToPad).fill(0xff);
    this.data = Buffer.concat([this.data, padding]);

    if (typeof crc === 'undefined') {
      this.crc = this.computeCrc();
    } else {
      this.crc = crc;
    }
  }

  computeCrc() {
    const allData = [this.length, ...this.data];
    return crc8(allData);
  }

  toBuffer() {
    return Buffer.concat([ Buffer.from([this.length]), this.data, Buffer.from([this.crc]) ]);
  }

  isSingleBytePacket(byte: number) {
    if (this.length !== 1) return false;
    if (this.data[0] !== byte) return false;
    for (let i = 1; i < PACKET_DATA_BYTES; i++) {
      if (this.data[i] !== 0xff) return false;
    }
    return true;
  }

  isAck() {
    return this.isSingleBytePacket(PACKET_ACK_DATA0);
  }

  isRetx() {
    return this.isSingleBytePacket(PACKET_RETX_DATA0);
  }
}

// Serial port instance
const uart = new SerialPort({ path: serialPath, baudRate });

// Packet buffer
let packets: Packet[] = [];

let lastPacket: Buffer = Packet.ack;
const writePacket = (packet: Buffer) => {
  uart.write(packet);
  lastPacket = packet;
};

// Serial data buffer, with a splice-like function for consuming data
let rxBuffer = Buffer.from([]);
const consumeFromBuffer = (n: number) => {
  const consumed = rxBuffer.slice(0, n);
  rxBuffer = rxBuffer.slice(n);
  return consumed;
}

// This function fires whenever data is received over the serial port. The whole
// packet state machine runs here.
uart.on('data', data => {
  console.log(`Received ${data.length} bytes through uart`);
  // Add the data to the packet
  rxBuffer = Buffer.concat([rxBuffer, data]);

  // Can we build a packet?
  if (rxBuffer.length >= PACKET_LENGTH) {
    console.log(`Building a packet`);
    const raw = consumeFromBuffer(PACKET_LENGTH);
    const packet = new Packet(raw[0], raw.slice(1, 1+PACKET_DATA_BYTES), raw[PACKET_CRC_INDEX]);
    const computedCrc = packet.computeCrc();

    // Need retransmission?
    if (packet.crc !== computedCrc) {
      console.log(`CRC failed, computed 0x${computedCrc.toString(16)}, got 0x${packet.crc.toString(16)}`);
      writePacket(Packet.retx);
      return;
    }

    // Are we being asked to retransmit?
    if (packet.isRetx()) {
    console.log(`Retransmitting last packet`);
    writePacket(lastPacket);
      return;
    }

    // If this is an ack, move on
    if (packet.isAck()) {
      console.log(`It was an ack, nothing to do`);
      return;
    }

    // Otherwise write the packet in to the buffer, and send an ack
    console.log(`Storing packet and ack'ing`);
    packets.push(packet);
    writePacket(Packet.ack);
  }
});

// Function to allow us to await a packet
const waitForPacket = async () => {
  while (packets.length < 1) {
    await delay(1);
  }
  const packet = packets[0];
  packets = packets.slice(1);
  return packet;
}

console.log(Packet.ack)

// Do everything in an async function so we can have loops, awaits etc
const main = async () => {
  console.log('Waiting for packet...');
  const packet = await waitForPacket();
  console.log(packet);

  const packetToSend = new Packet(4, Buffer.from([5, 6, 7, 8]));
  packetToSend.crc++;
  uart.write(packetToSend.toBuffer());
}

main();
