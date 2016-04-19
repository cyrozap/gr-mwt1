#!/usr/bin/env python2

import binascii
import math
import signal
import sys

from gnuradio import gr
import mwt1_packet_receiver

# rf object
rf = ''

def signal_handler(signal, frame):
    global rf
    print('You pressed Ctrl+C!')
    rf.stop()
    sys.exit(0)

if __name__ == "__main__":
    signal.signal(signal.SIGINT, signal_handler)

    try:
        rf = mwt1_packet_receiver.mwt1_packet_receiver()
        rf.start()

        while True:
            # Read a string
            msg = binascii.b2a_hex(rf.data.delete_head().to_string())
            print "[rcv] : \"%s\"" % msg

    except KeyboardInterrupt:
        print("W: interrupt received, proceeding")
