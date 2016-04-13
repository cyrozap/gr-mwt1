/* -*- c++ -*- */
/*
 * Copyright 2014 <+YOU OR YOUR COMPANY+>.
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this software; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "mwt1_packet_decoder_impl.h"
#include <gnuradio/io_signature.h>

namespace gr {
namespace mwt1 {

mwt1_packet_decoder::sptr
mwt1_packet_decoder::make(msg_queue::sptr target_queue, bool do_crc8_check,
                          bool verbose, bool drop_crc) {
  return gnuradio::get_initial_sptr(new mwt1_packet_decoder_impl(
      target_queue, do_crc8_check, verbose, drop_crc));
}

/*
 * The private constructor
 */
mwt1_packet_decoder_impl::mwt1_packet_decoder_impl(
    msg_queue::sptr arg_target_queue, bool arg_do_crc8_check, bool arg_verbose,
    bool arg_drop_crc)
    : gr::block("mwt1_packet_decoder",
                gr::io_signature::make(1, 1, sizeof(unsigned char)),
                gr::io_signature::make(1, 1, sizeof(unsigned char))),

      target_queue(arg_target_queue), do_crc8_check(arg_do_crc8_check),
      verbose(arg_verbose), drop_crc(arg_drop_crc),

      is_msg(false), buffer_expected_len(0), bit_index(0), buffer_i(0) {
  // reset buffer
  buffer_reset();
}

/*
 * Our virtual destructor.
 */
mwt1_packet_decoder_impl::~mwt1_packet_decoder_impl() {}

void mwt1_packet_decoder_impl::forecast(int noutput_items,
                                        gr_vector_int &ninput_items_required) {
  /* <+forecast+> e.g. ninput_items_required[0] = noutput_items */
  ninput_items_required[0] = noutput_items;
}

int mwt1_packet_decoder_impl::general_work(
    int noutput_items, gr_vector_int &ninput_items,
    gr_vector_const_void_star &input_items, gr_vector_void_star &output_items) {

  const unsigned char *in = (const unsigned char *)input_items[0];
  unsigned char *out = (unsigned char *)output_items[0];
  unsigned int produced = 0;

  // Do <+signal processing+>
  for (int i = 0; i < noutput_items; i++) {

    // we are currently processing a message
    if (is_msg) {
      // add new bit
      buffer_append(in[i]);
      // end of message ?
      if ((buffer_i != 0 && bit_index == 8 &&
           buffer_i == buffer_expected_len - 1) ||
          buffer_i == (BUF_MAX_SIZE)-1) {
        is_msg = false; // stop
        buffer_flush(out);
        produced = buffer_i + 1;
      }
    } else {
      // did we find the beginning of a new message ?
      if (in[i] & 0x02) {
        // reset buffer
        buffer_reset();
        // add the first bit
        buffer_append(in[i]);
        // swith processing mode
        is_msg = true;
      }
    }
  }

  // Tell runtime system how many input items we consumed on
  // each input stream.
  consume_each(noutput_items);

  // Tell runtime system how many output items we produced.
  return produced;
}

/****************************************************************
BUFFER FLUSH
****************************************************************/
// print/send out current buffer
int mwt1_packet_decoder_impl::buffer_flush(unsigned char *out) {

  uint8_t checksum;
  uint8_t checksum_found;

  if (verbose) {
    fprintf(stdout, "[verbose] Pkt: ");
    for (int j = 0; j < buffer_i + 1; j++) {
      fprintf(stdout, "%02x ", (unsigned char)buffer[j]);
    }
  }

  // CHECKSUM ?
  if (do_crc8_check) {

    // get CRC from frame (last byte)
    checksum_found = buffer[buffer_i];
    // compute real CRC
    for (int i = 0; i < (buffer_i + 1) - 2; i++)
      checksum = culCalcCRC((unsigned char)buffer[i], checksum);
    // drop frame if wrong CRC
    if (checksum != checksum_found) {
      if (verbose) {
        fprintf(stdout, "[crc error]\n");
      }

      buffer_i = -1; // on return, we do 'produced = buffer_i+1;'
      return 0;
    } else {
      // if CRC is correct, remove it from frame
      buffer_i -= 1;
    }
  }

  if (verbose) {
    fprintf(stdout, "\n");
  }

  // Drop preamble and sync word
  if (true) {
    // copy buffer to out port
    memcpy(out, buffer + 1, buffer_i);
    // .. and send a message to the queue (remove header (1byte))
    message::sptr msg = message::make(0, 0, 0, buffer_i);
    memcpy(msg->msg(), buffer + 1, buffer_i); // copy
    target_queue->insert_tail(msg);           // send it
    msg.reset();                              // free it up
  } else {
    // copy buffer to out port
    memcpy(out, buffer, buffer_i + 1);
    // .. and send a message to the queue
    message::sptr msg = message::make(0, 0, 0, buffer_i + 1);
    memcpy(msg->msg(), buffer, buffer_i + 1); // copy
    target_queue->insert_tail(msg);           // send it
    msg.reset();                              // free it up
  }

  return 1;
}

/****************************************************************
BUFFER APPEND
****************************************************************/
// we receive a byte and add the first bit to the end of the buffer
int mwt1_packet_decoder_impl::buffer_append(unsigned char byte) {

  // need a new byte in buffer[] ?
  if (bit_index == 8) {
    bit_index = 0;
    buffer_i++;
  }

  // is the first byte complete ? If yes, it contains the length of the message
  // header len and CRC must be added
  if (buffer_i == 1 && bit_index == 0) {
    // payload len
    buffer_expected_len = (int)(buffer[0]);
    // add crc len ?
    if (do_crc8_check) {
      buffer_expected_len += 1; // CRC8 is of course 1 byte length
    }
    // add header len
    buffer_expected_len += 1; // header is 1 byte (len)
  }

  // add new bit to the end of the buffer
  if (bit_index == 0) {
    buffer[buffer_i] = (byte & 0x1);
  } else {
    buffer[buffer_i] = (buffer[buffer_i] << 1) | (byte & 0x1);
  }
  // inc bit index
  bit_index++;

  return 1;
}

/****************************************************************
BUFFER RESET
****************************************************************/
int mwt1_packet_decoder_impl::buffer_reset() {
  memset(buffer, 0, BUF_MAX_SIZE);
  bit_index = 0;
  buffer_i = 0;
  return 1;
}

/****************************************************************
CRC8
****************************************************************/
uint8_t mwt1_packet_decoder_impl::culCalcCRC(unsigned char crcData,
                                             uint8_t crcReg) {
  const unsigned char crc8_table[] = {
      0x00, 0x9b, 0xad, 0x36, 0xc1, 0x5a, 0x6c, 0xf7, 0x19, 0x82, 0xb4, 0x2f,
      0xd8, 0x43, 0x75, 0xee, 0x32, 0xa9, 0x9f, 0x04, 0xf3, 0x68, 0x5e, 0xc5,
      0x2b, 0xb0, 0x86, 0x1d, 0xea, 0x71, 0x47, 0xdc, 0x64, 0xff, 0xc9, 0x52,
      0xa5, 0x3e, 0x08, 0x93, 0x7d, 0xe6, 0xd0, 0x4b, 0xbc, 0x27, 0x11, 0x8a,
      0x56, 0xcd, 0xfb, 0x60, 0x97, 0x0c, 0x3a, 0xa1, 0x4f, 0xd4, 0xe2, 0x79,
      0x8e, 0x15, 0x23, 0xb8, 0xc8, 0x53, 0x65, 0xfe, 0x09, 0x92, 0xa4, 0x3f,
      0xd1, 0x4a, 0x7c, 0xe7, 0x10, 0x8b, 0xbd, 0x26, 0xfa, 0x61, 0x57, 0xcc,
      0x3b, 0xa0, 0x96, 0x0d, 0xe3, 0x78, 0x4e, 0xd5, 0x22, 0xb9, 0x8f, 0x14,
      0xac, 0x37, 0x01, 0x9a, 0x6d, 0xf6, 0xc0, 0x5b, 0xb5, 0x2e, 0x18, 0x83,
      0x74, 0xef, 0xd9, 0x42, 0x9e, 0x05, 0x33, 0xa8, 0x5f, 0xc4, 0xf2, 0x69,
      0x87, 0x1c, 0x2a, 0xb1, 0x46, 0xdd, 0xeb, 0x70, 0x0b, 0x90, 0xa6, 0x3d,
      0xca, 0x51, 0x67, 0xfc, 0x12, 0x89, 0xbf, 0x24, 0xd3, 0x48, 0x7e, 0xe5,
      0x39, 0xa2, 0x94, 0x0f, 0xf8, 0x63, 0x55, 0xce, 0x20, 0xbb, 0x8d, 0x16,
      0xe1, 0x7a, 0x4c, 0xd7, 0x6f, 0xf4, 0xc2, 0x59, 0xae, 0x35, 0x03, 0x98,
      0x76, 0xed, 0xdb, 0x40, 0xb7, 0x2c, 0x1a, 0x81, 0x5d, 0xc6, 0xf0, 0x6b,
      0x9c, 0x07, 0x31, 0xaa, 0x44, 0xdf, 0xe9, 0x72, 0x85, 0x1e, 0x28, 0xb3,
      0xc3, 0x58, 0x6e, 0xf5, 0x02, 0x99, 0xaf, 0x34, 0xda, 0x41, 0x77, 0xec,
      0x1b, 0x80, 0xb6, 0x2d, 0xf1, 0x6a, 0x5c, 0xc7, 0x30, 0xab, 0x9d, 0x06,
      0xe8, 0x73, 0x45, 0xde, 0x29, 0xb2, 0x84, 0x1f, 0xa7, 0x3c, 0x0a, 0x91,
      0x66, 0xfd, 0xcb, 0x50, 0xbe, 0x25, 0x13, 0x88, 0x7f, 0xe4, 0xd2, 0x49,
      0x95, 0x0e, 0x38, 0xa3, 0x54, 0xcf, 0xf9, 0x62, 0x8c, 0x17, 0x21, 0xba,
      0x4d, 0xd6, 0xe0, 0x7b};
  crcReg = crc8_table[crcReg ^ crcData];
  return crcReg;
}

uint8_t mwt1_packet_decoder_impl::decode_4b6b(unsigned char byte) {
  const unsigned char decode_table[] = {
      0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x0b,
      0xff, 0x0d, 0x0e, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x07, 0xff,
      0xff, 0x09, 0x08, 0xff, 0x0f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x03,
      0xff, 0x05, 0x06, 0xff, 0xff, 0xff, 0x0a, 0xff, 0x0c, 0xff, 0xff, 0xff,
      0xff, 0x01, 0x02, 0xff, 0x04, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
      0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
      0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
      0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
      0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
      0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
      0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
      0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
      0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
      0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
      0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
      0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
      0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
      0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
      0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
      0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
      0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
      0xff, 0xff, 0xff, 0xff};

  return decode_table[byte];
}

} /* namespace mwt1 */
} /* namespace gr */
