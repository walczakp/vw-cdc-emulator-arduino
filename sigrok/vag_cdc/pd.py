import sigrokdecode as srd
from .lists import commands
# import datetime

class SamplerateError(Exception):
    pass

class Decoder(srd.Decoder):
    api_version = 3
    id = 'vag_cdc'
    name = 'VAG CDC'
    longname = 'VAG CD Changer protocol decoder'
    desc = 'VAG CD Changer protocol decoder for radio -> changer communication.'
    license = 'gplv2+'
    inputs = ['logic']
    outputs = []
    tags = ['Automotive','Audio']
    channels = (
         {'id': 'data_out', 'name': 'DATA OUT', 'desc': 'DATA OUT line from CD Changer header'},
    )
    options = ()
    annotations = (
        ('bit', 'Bit'),
        ('cmd', 'Command'),
        ('cmd-desc', 'Description'),
    )
    annotation_rows = (
        ('bits', 'Bits', (0,)),
        ('cmds', 'Commands', (1,)),
        ('cmds-desc', 'Commands (description)', (2,)),
    )

    def __init__(self):
        self.reset()

    def reset(self):
        self.samplerate = None
        self.reset_decoder_state()

    def start(self):
        self.out_ann = self.register(srd.OUTPUT_ANN)
        self.next_edge = 'h'

    def metadata(self, key, value):
        if key == srd.SRD_CONF_SAMPLERATE:
            self.samplerate = value
            
            # based on https://martinsuniverse.de/projekte/cdc_protokoll/cdc_protokoll.html
            self.len_startbit_hi = int(self.samplerate * 0.009)     # 9ms high
            self.len_startbit_lo = int(self.samplerate * 0.0045)    # 4.5ms low
            self.len_bit1_hi = int(self.samplerate * 0.00055)       # 0.55ms high
            self.len_bit1_lo = int(self.samplerate * 0.0017)        # 1.7ms low
            self.len_bit0_hi = self.len_bit1_hi                     # 0.55ms high
            self.len_bit0_lo = self.len_bit1_hi                     # 0.55ms low
            self.len_margin = int(self.len_bit1_hi / 2.0)           # margin of error - half of the lowest interval

    def distance_in_range(self, actual, expected):
        return actual in range(expected - self.len_margin, expected + self.len_margin + 1)

    def reset_decoder_state(self):
        self.edges, self.bits = [], []
        self.lo_distance = 0
        self.hi_distance = 0
        self.state = 'IDLE'

    def decode(self):
        if not self.samplerate:
            raise SamplerateError('Cannot decode without samplerate.')
        # log = open("vag_cdc.log",'w')
        # log.write("{} self.len_startbit_hi = {}\n".format(datetime.datetime.now().isoformat(), self.len_startbit_hi))
        # log.write("{} self.len_startbit_lo = {}\n".format(datetime.datetime.now().isoformat(), self.len_startbit_lo))
        # log.write("{} self.len_bit1_hi = {}\n".format(datetime.datetime.now().isoformat(), self.len_bit1_hi))
        # log.write("{} self.len_bit1_lo = {}\n".format(datetime.datetime.now().isoformat(), self.len_bit1_lo))
        # log.write("{} self.len_bit0_hi = {}\n".format(datetime.datetime.now().isoformat(), self.len_bit0_hi))
        # log.write("{} self.len_bit0_lo = {}\n".format(datetime.datetime.now().isoformat(), self.len_bit0_lo))
        # log.write("{} self.len_margin = {}\n".format(datetime.datetime.now().isoformat(), self.len_margin))
        while True:
            (self.pin_state,) = self.wait({0: self.next_edge})

            if self.state == 'IDLE':
                # log.write("{} Starting {}\n".format(datetime.datetime.now().isoformat(), self.samplenum))
                self.state = 'START'
                self.next_edge = 'l' if self.pin_state else 'h'
                self.edges.append(self.samplenum)
                continue

            if self.pin_state and self.state != 'START':
                # changed low -> high
                self.lo_distance = self.samplenum - self.edges[-1]
                self.state = 'GOT_LOW'
            else:
                # changed high -> low
                self.hi_distance = self.samplenum - self.edges[-1]
                self.lo_distance = 0
                self.state = 'GOT_HIGH'
            
            if self.state == 'GOT_LOW':
                bit_start = self.edges[-2]
                bit_end = self.samplenum
                if self.distance_in_range(self.hi_distance, self.len_startbit_hi) and self.distance_in_range(self.lo_distance, self.len_startbit_lo):
                    # got startbit
                    self.put(bit_start, bit_end, self.out_ann, [0, ['Start', 'St', 'S']])
                elif self.distance_in_range(self.hi_distance, self.len_bit1_hi) and self.distance_in_range(self.lo_distance, self.len_bit1_lo):
                    # got bit1
                    self.bits.append([bit_start, bit_end, 1])
                    self.put(bit_start, bit_end, self.out_ann, [0, ['1']])
                elif self.distance_in_range(self.hi_distance, self.len_bit0_hi) and self.distance_in_range(self.lo_distance, self.len_bit0_lo):
                    # got bit0
                    self.bits.append([bit_start, bit_end, 0])
                    self.put(bit_start, bit_end, self.out_ann, [0, ['0']])

            self.edges.append(self.samplenum)
            if len(self.bits) == 32:
                command = []
                for i in range(4):
                    bit_start = self.bits[i*8][0]
                    bit_end = self.bits[i*8+7][1]
                    bits = [self.bits[i*8+bit][2] for bit in range(8)]
                    bytes = self.bits_to_byte(bits)
                    command.append(bytes)
                    self.put(bit_start, bit_end, self.out_ann, [1, ['0x{0:02X}'.format(bytes)]])
                
                if command[0] == 0xCA and command[1] == 0x34 and command[2] == (command[3] ^ 0xFF): # xor with FF - unsigned not of a byte
                    if command[2] in commands:
                        self.put(self.bits[0][0], self.bits[31][1], self.out_ann, [2, [ commands[command[2]] ]])
                    else:
                        self.put(self.bits[0][0], self.bits[31][1], self.out_ann, [2, ['unknown']])
                else:
                    self.put(self.bits[0][0], self.bits[31][1], self.out_ann, [2, ['invalid']])

                self.reset_decoder_state()

            self.next_edge = 'l' if self.pin_state else 'h'
    
    def bits_to_byte(self, bits):
        out = 0
        for bit in reversed(bits):
            out = (out << 1) | bit
        return out