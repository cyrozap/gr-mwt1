# gr-mwt1

gr-mwt1 provides GNU Radio blocks to handle the Medtronic MWT1 packet format.
The TI CC11xx series of ICs have the ability to decode at least the framing of
each packet, so this code will be based off of Jerome Nokin's
[gr-cc1111](https://github.com/funoverip/gr-cc1111).

Once complete, this module will provide the following GNU Radio blocks:
- "Packet Encoder (MWT1) Source" : Read payloads from gr.msg_queue() and format them as MWT1 packets.
- "Packet Decoder (MWT1)" : Decode MWT1 packets from a GR flow graph and send payload to gr.msg_queue().

## Installation

```
cd src/gr-mwt1
mkdir build
cd build
cmake ../
make
sudo make install
```
