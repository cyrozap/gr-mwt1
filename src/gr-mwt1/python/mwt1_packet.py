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


from binascii import hexlify
from gnuradio import gr, digital
from gnuradio import blocks
from gnuradio.digital import packet_utils
import mwt1_packet_utils
import gnuradio.gr.gr_threading as _threading


##how many messages in a queue
DEFAULT_MSGQ_LIMIT = 2


##################################################
## MWT1 Packet Encoder
##################################################
class _mwt1_packet_encoder_thread(_threading.Thread):

	def __init__(self, msgq, send):
		self._msgq = msgq
		self._send = send
		_threading.Thread.__init__(self)
		self.setDaemon(1)
		self.keep_running = True
		self.start()

	def run(self):
		while self.keep_running:
			msg = self._msgq.delete_head() #blocking read of message queue
			sample = msg.to_string() #get the body of the msg as a string
			self._send(sample)

class mwt1_packet_encoder(gr.hier_block2):
	"""
	Hierarchical block for wrapping packet-based modulators.
	"""
	def __init__(self, samples_per_symbol, bits_per_symbol, pad_for_usrp=True):
		"""
		packet_mod constructor.

		Args:
			samples_per_symbol: number of samples per symbol
			bits_per_symbol: number of bits per symbol
			pad_for_usrp: If true, packets are padded such that they end up a multiple of 128 samples
		"""

		#setup parameters
		self._samples_per_symbol = samples_per_symbol
		self._bits_per_symbol = bits_per_symbol
		self._pad_for_usrp = pad_for_usrp

		#create blocks
		msg_source = blocks.message_source(gr.sizeof_char, DEFAULT_MSGQ_LIMIT)
		self._msgq_out = msg_source.msgq()
		#initialize hier2
		gr.hier_block2.__init__(
			self,
			"mwt1_packet_encoder",
			gr.io_signature(0, 0, 0), # Input signature
			gr.io_signature(1, 1, gr.sizeof_char) # Output signature
		)
		#connect
		self.connect(msg_source, self)

	def send_pkt(self, payload):
		"""
		Wrap the payload in a packet and push onto the message queue.

		Args:
			payload: string, data to send
		"""
		packet = mwt1_packet_utils.make_packet(
			payload,
			self._samples_per_symbol,
			self._bits_per_symbol,
			self._pad_for_usrp,
		)
		msg = gr.message_from_string(packet)
		self._msgq_out.insert_tail(msg)


class mwt1_packet_mod_base(gr.hier_block2):
	"""
	Hierarchical block for wrapping packet source block.
	"""

	def __init__(self, packet_source, source_queue):
		#initialize hier2
		gr.hier_block2.__init__(
			self,
			"mwt1_packet_mod_base",
			gr.io_signature(0, 0, 0), # Input signature
			gr.io_signature(1, 1, packet_source._hb.output_signature().sizeof_stream_item(0)) # Output signature
		)
		self.connect(packet_source, self)
		#start thread
		_mwt1_packet_encoder_thread(source_queue, packet_source.send_pkt)
