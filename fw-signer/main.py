#!/usr/bin/env python3

import sys
import os
import subprocess
import struct

AES_BLOCK_SIZE = 16
BOOTLOADER_SIZE = 0x8000
FWINFO_OFFSET = 0x01B0
SIGNATURE_OFFSET = FWINFO_OFFSET + AES_BLOCK_SIZE

FWINFO_VERSION_OFFSET = 8
FWINFO_LENGTH_OFFSET = 12

signing_key = "000102030405060708090a0b0c0d0e0f"
zeroed_iv = "00000000000000000000000000000000"

signing_image_filename = "image_to_be_signed.bin"
encrypted_filename = "encryped_image.bin"
signed_filename = "signed.bin"

if len(sys.argv) < 3:
    print("usage: fw-signer.py <input file> <version number hex>")
    exit(1)

with open(sys.argv[1], "rb") as f:
    f.seek(BOOTLOADER_SIZE)
    fw_image = bytearray(f.read())
    f.close()

version_hex = sys.argv[2]
version_value = int(version_hex, base=16)
struct.pack_into("<I", fw_image, FWINFO_OFFSET + FWINFO_LENGTH_OFFSET, len(fw_image))
struct.pack_into("<I", fw_image, FWINFO_OFFSET + FWINFO_VERSION_OFFSET, version_value)

signing_image = fw_image[FWINFO_OFFSET:FWINFO_OFFSET + AES_BLOCK_SIZE]
signing_image += fw_image[:FWINFO_OFFSET]
signing_image += fw_image[FWINFO_OFFSET + AES_BLOCK_SIZE * 2:]

with open(signing_image_filename, "wb") as f:
    f.write(signing_image)
    f.close()

openssl_command = f"openssl enc -aes-128-cbc -nosalt -K {signing_key} -iv {zeroed_iv} -in {signing_image_filename} -out {encrypted_filename}"
subprocess.call(openssl_command.split(" "))

with open(encrypted_filename, "rb") as f:
    f.seek(-AES_BLOCK_SIZE, os.SEEK_END)
    signature = f.read()
    f.close()

signature_text = ""
for byte in signature:
    signature_text += f"{byte:02x}"

print(f"Signed firmeware version {version_hex}")
print(f"key       = {signing_key}")
print(f"signature = {signature_text}")

os.remove(signing_image_filename)
os.remove(encrypted_filename)

fw_image[SIGNATURE_OFFSET:SIGNATURE_OFFSET + AES_BLOCK_SIZE] = signature

with open(signed_filename, "wb") as f:
    f.write(fw_image)
    f.close()
