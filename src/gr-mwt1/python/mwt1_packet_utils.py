# -*- coding: utf-8 -*-
#
# Copyright 2014 funoverip.net.
#
# This is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3, or (at your option)
# any later version.
#
# This software is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this software; see the file COPYING.  If not, write to
# the Free Software Foundation, Inc., 51 Franklin Street,
# Boston, MA 02110-1301, USA.
#


import math
import struct
from gnuradio import gru
from gnuradio.digital import packet_utils


ACCESS_CODE = '\xFF\x00' * 2
PREAMBLE    = '\xAA' * 16
PADDING     = '\x00' * 10

ENCODER_TABLE = {
	0x0: 0x15,
	0x1: 0x31,
	0x2: 0x32,
	0x3: 0x23,
	0x4: 0x34,
	0x5: 0x25,
	0x6: 0x26,
	0x7: 0x16,
	0x8: 0x1a,
	0x9: 0x19,
	0xa: 0x2a,
	0xb: 0x0b,
	0xc: 0x2c,
	0xd: 0x0d,
	0xe: 0x0e,
	0xf: 0x1c,
}

# CRC8 lookup table and function adapted from here:
# https://github.com/bewest/comlink2-uart/blob/master/lib.js

CRC8_TABLE = [
	0x00, 0x9b, 0xad, 0x36, 0xc1, 0x5a, 0x6c, 0xf7,
	0x19, 0x82, 0xb4, 0x2f, 0xd8, 0x43, 0x75, 0xee,
	0x32, 0xa9, 0x9f, 0x04, 0xf3, 0x68, 0x5e, 0xc5,
	0x2b, 0xb0, 0x86, 0x1d, 0xea, 0x71, 0x47, 0xdc,
	0x64, 0xff, 0xc9, 0x52, 0xa5, 0x3e, 0x08, 0x93,
	0x7d, 0xe6, 0xd0, 0x4b, 0xbc, 0x27, 0x11, 0x8a,
	0x56, 0xcd, 0xfb, 0x60, 0x97, 0x0c, 0x3a, 0xa1,
	0x4f, 0xd4, 0xe2, 0x79, 0x8e, 0x15, 0x23, 0xb8,
	0xc8, 0x53, 0x65, 0xfe, 0x09, 0x92, 0xa4, 0x3f,
	0xd1, 0x4a, 0x7c, 0xe7, 0x10, 0x8b, 0xbd, 0x26,
	0xfa, 0x61, 0x57, 0xcc, 0x3b, 0xa0, 0x96, 0x0d,
	0xe3, 0x78, 0x4e, 0xd5, 0x22, 0xb9, 0x8f, 0x14,
	0xac, 0x37, 0x01, 0x9a, 0x6d, 0xf6, 0xc0, 0x5b,
	0xb5, 0x2e, 0x18, 0x83, 0x74, 0xef, 0xd9, 0x42,
	0x9e, 0x05, 0x33, 0xa8, 0x5f, 0xc4, 0xf2, 0x69,
	0x87, 0x1c, 0x2a, 0xb1, 0x46, 0xdd, 0xeb, 0x70,
	0x0b, 0x90, 0xa6, 0x3d, 0xca, 0x51, 0x67, 0xfc,
	0x12, 0x89, 0xbf, 0x24, 0xd3, 0x48, 0x7e, 0xe5,
	0x39, 0xa2, 0x94, 0x0f, 0xf8, 0x63, 0x55, 0xce,
	0x20, 0xbb, 0x8d, 0x16, 0xe1, 0x7a, 0x4c, 0xd7,
	0x6f, 0xf4, 0xc2, 0x59, 0xae, 0x35, 0x03, 0x98,
	0x76, 0xed, 0xdb, 0x40, 0xb7, 0x2c, 0x1a, 0x81,
	0x5d, 0xc6, 0xf0, 0x6b, 0x9c, 0x07, 0x31, 0xaa,
	0x44, 0xdf, 0xe9, 0x72, 0x85, 0x1e, 0x28, 0xb3,
	0xc3, 0x58, 0x6e, 0xf5, 0x02, 0x99, 0xaf, 0x34,
	0xda, 0x41, 0x77, 0xec, 0x1b, 0x80, 0xb6, 0x2d,
	0xf1, 0x6a, 0x5c, 0xc7, 0x30, 0xab, 0x9d, 0x06,
	0xe8, 0x73, 0x45, 0xde, 0x29, 0xb2, 0x84, 0x1f,
	0xa7, 0x3c, 0x0a, 0x91, 0x66, 0xfd, 0xcb, 0x50,
	0xbe, 0x25, 0x13, 0x88, 0x7f, 0xe4, 0xd2, 0x49,
	0x95, 0x0e, 0x38, 0xa3, 0x54, 0xcf, 0xf9, 0x62,
	0x8c, 0x17, 0x21, 0xba, 0x4d, 0xd6, 0xe0, 0x7b
]

def crc8(data):
	result = 0
	for i in range(0, len(data)):
		result = CRC8_TABLE[(result ^ ord(data[i]))]
	return str(bytearray([result]))

def bits_to_bytes(bits):
	encoded_bytes = bytearray(int(math.ceil(len(bits)/8.0)))
	for i in range(0, len(bits)):
		encoded_bytes[i / 8] ^= bits[i] << (7 - (i % 8))
	return encoded_bytes

def make_packet(payload, samples_per_symbol, bits_per_symbol, pad_for_usrp=True):
	"""
	Build a packet

	Args:
		payload: packet payload, len [0, 4096]
		samples_per_symbol: samples per symbol (needed for padding calculation) (int)
		bits_per_symbol: (needed for padding calculation) (int)
		pad_for_usrp:

	Packet will have the preamble and access code at the beginning, followed by
	the encoded payload and an 8-bit CRC.
	"""

	# CRC
	crc = crc8(payload)
	raw_message = ''.join((payload, crc))

	# 4b/6b encoding
	encoded_nibbles = []
	for element in raw_message:
		for shift in [4, 0]:
			encoded_nibble = ENCODER_TABLE[(ord(element) >> shift) & 0xf]
			encoded_nibbles.append(encoded_nibble)

	# Nibble to bit conversion
	bits = []
	for element in encoded_nibbles:
		for bit_index in range(0, 6)[::-1]:
			bit = (element >> bit_index) & 0x01
			bits.append(bit)

	# Bit padding
	if len(bits) % 8:
		padding = [0, 1] * ((len(bits) % 8) / 2)
		bits.extend(padding)

	# Convert the bits to bytes
	encoded_message = str(bits_to_bytes(bits))

	# Prepend the preamble and sync words/access code to the message
	packet = ''.join((PREAMBLE, ACCESS_CODE, encoded_message, PADDING))

	# Padding (optional)
	if pad_for_usrp:
		usrp_packing = packet_utils._npadding_bytes(len(packet), samples_per_symbol, bits_per_symbol) * '\x00'
		packet = packet + usrp_packing

	return packet
