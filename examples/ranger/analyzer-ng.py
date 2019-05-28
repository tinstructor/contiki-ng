import io
import sys
import re
import argparse
import csv

################################################################################

class message:
    def __init__(self, descriptor, packet_nr, payload_len, rssi, rssi_offset, lqi, tx_power, channel, 
                 channel_center_freq_0, channel_spacing, channel_center_freq_curr, bitrate, symbol_rate,
                 rx_filt_bw, preamble_nibbles, preamble_word, crc_poly, crc_init, sync_word, sync_word_thr,
                 dual_sync_en, sync_bits, freq_dev, mac_hdr_len, rx_link_addr, tx_link_addr):
        self.descriptor = descriptor
        self.packet_nr = packet_nr
        self.payload_len = payload_len
        self.rssi = rssi
        self.rssi_offset = rssi_offset
        self.lqi = lqi
        self.tx_power = tx_power
        self.channel = channel
        self.channel_center_freq_0 = channel_center_freq_0
        self.channel_spacing = channel_spacing
        self.channel_center_freq_curr = channel_center_freq_curr
        self.bitrate = bitrate
        self.symbol_rate = symbol_rate
        self.rx_filt_bw = rx_filt_bw
        self.preamble_nibbles = preamble_nibbles
        self.preamble_word = preamble_word
        self.crc_poly = crc_poly
        self.crc_init = crc_init
        self.sync_word = sync_word
        self.sync_word_thr = sync_word_thr
        self.dual_sync_en = dual_sync_en
        self.sync_bits = sync_bits
        self.freq_dev = freq_dev
        self.mac_hdr_len = mac_hdr_len
        self.rx_link_addr = rx_link_addr
        self.tx_link_addr = tx_link_addr
    
    # def as_list(self):
    #     attribute_list = []
    #     attribute_list.append(self.descriptor)
    #     attribute_list.append(self.packet_nr)
    #     attribute_list.append(self.payload_len)
    #     attribute_list.append(self.rssi)
    #     attribute_list.append(self.rssi_offset)
    #     attribute_list.append(self.lqi)
    #     attribute_list.append(self.tx_power)
    #     attribute_list.append(self.channel)
    #     attribute_list.append(self.channel_center_freq_0)
    #     attribute_list.append(self.channel_spacing)
    #     attribute_list.append(self.channel_center_freq_curr)
    #     attribute_list.append(self.bitrate)
    #     attribute_list.append(self.symbol_rate)
    #     attribute_list.append(self.rx_filt_bw)
    #     attribute_list.append(self.preamble_nibbles)
    #     attribute_list.append(self.preamble_word)
    #     attribute_list.append(self.crc_poly)
    #     attribute_list.append(self.crc_init)
    #     attribute_list.append(self.sync_word)
    #     attribute_list.append(self.sync_word_thr)
    #     attribute_list.append(self.dual_sync_en)
    #     attribute_list.append(self.sync_bits)
    #     attribute_list.append(self.freq_dev)
    #     attribute_list.append(self.mac_hdr_len)
    #     attribute_list.append(self.rx_link_addr)
    #     attribute_list.append(self.tx_link_addr)
    #     return attribute_list

class received_messages:
    def __init__(self):
        self.messages = {}
        self.base = {}

    def get_descriptors(self):
        return list(self.base.keys())

    def add_message(self, message):
        if (not message.descriptor in self.messages):
            self.messages[message.descriptor] = []

        for m in self.messages[message.descriptor]:
            if (m.packet_nr == message.packet_nr):
                raise ValueError("A message with RF descriptor %s and packet nr. %d has already been added" 
                                 % (message.descriptor, message.packet_nr))

        self.messages[message.descriptor].append(message)

    def add_base(self, message):
        if (not message.descriptor in self.base):
            self.base[message.descriptor] = []

        for m in self.base[message.descriptor]:
            if (m.packet_nr == message.packet_nr):
                raise ValueError("A message with RF descriptor %s and packet nr. %d has already been added to base" 
                                 % (message.descriptor, message.packet_nr))
        
        self.base[message.descriptor].append(message)

    def average_rssi(self, amount, descriptor):
        if (not descriptor in self.messages):
            raise RuntimeError("There are no received messages with descriptor %s" % (descriptor))

        if (amount < 0):
            raise ValueError("Cannot process a negative amount of messages")
        elif (amount > len(self.messages[descriptor])):
            raise ValueError("There are not enough messages to process")

        avg_rssi = 0

        for m in self.messages[descriptor][:amount]:
            avg_rssi += m.rssi

        avg_rssi /= amount

        return avg_rssi

    def average_rssi_all(self, descriptor):
        if (not descriptor in self.messages):
            raise RuntimeError("There are no received messages with descriptor %s" % (descriptor))

        return self.average_rssi(len(self.messages[descriptor]), descriptor)

    def packet_loss(self, amount, descriptor):
        if (not descriptor in self.base):
            raise RuntimeError("There were no sent messages with descriptor %s" % (descriptor))

        if (amount < 0):
            raise ValueError("Cannot process a negative amount of messages")
        elif (amount > len(self.base[descriptor])):
            raise ValueError("There are not enough sent messages to process")

        if (not descriptor in self.messages):
            return 1.0

        packets_lost = 0

        for i in range(1, amount):
            if (not any(x.packet_nr == self.base[descriptor][i].packet_nr for x in self.messages[descriptor])):
                packets_lost += 1

        total = self.base[descriptor][amount - 1].packet_nr - self.base[descriptor][0].packet_nr + 1

        return packets_lost / total

    def packet_loss_all(self, descriptor):
        if (not descriptor in self.base):
            raise RuntimeError("There were no sent messages with descriptor %s" % (descriptor))

        return self.packet_loss(len(self.base[descriptor]), descriptor)

    def amount(self, descriptor):
        if (not descriptor in self.messages):
            return 0          
        return len(self.messages[descriptor])

    def as_list_of_dict(self, descriptor):
        if (not descriptor in self.messages):
            raise RuntimeError("There are no received messages with descriptor %s" % (descriptor))

        # message_list = list(map(lambda m: m.as_list(), self.messages[descriptor]))
        message_list = list(map(lambda m: m.__dict__, self.messages[descriptor]))
        
        return message_list

class receiver_node:
    def __init__(self, rx_node_addr):
        self.transmissions = {}
        self.node_addr = rx_node_addr

    def get_node_addr(self):
        return self.node_addr
    
    def get_transmissions(self):
        return self.transmissions

    def add_transmission(self, message):
        if (message.rx_link_addr == message.tx_link_addr and message.rx_link_addr != self.node_addr):
            if (not message.tx_link_addr in self.transmissions):
                self.transmissions[message.tx_link_addr] = received_messages()
            self.transmissions[message.tx_link_addr].add_base(message)
        elif (message.rx_link_addr != message.tx_link_addr and message.rx_link_addr == self.node_addr):
            if (not message.tx_link_addr in self.transmissions):
                self.transmissions[message.tx_link_addr] = received_messages()
            self.transmissions[message.tx_link_addr].add_message(message)

################################################################################

parser = argparse.ArgumentParser()
parser.add_argument("rxlog", help="The logfile containing all received messages.")
parser.add_argument("txlog", help="The logfile containing all transmitted messages.")
parser.add_argument("-c", "--csvfile", help="The csv file to which log-derived info must be appended.")
args = parser.parse_args()

LOG_REGEXP = re.compile("^.*?csv-log: (?P<descriptor>.+), (?P<packet_nr>\d+), (?P<payload_len>\d+), (?P<rssi>[+-]?\d+), "
                        "(?P<rssi_offset>[+-]?\d+), (?P<lqi>\d+), (?P<tx_power>\d+), (?P<channel>\d+), (?P<channel_center_freq_0>\d+), "
                        "(?P<channel_spacing>\d+), (?P<channel_center_freq_curr>\d+), (?P<bitrate>\d+), (?P<symbol_rate>\d+), "
                        "(?P<rx_filt_bw>\d+), (?P<preamble_nibbles>\d+), (?P<preamble_word>.+), (?P<crc_poly>.+), (?P<crc_init>.+), "
                        "(?P<sync_word>.+), (?P<sync_word_thr>\d+), (?P<dual_sync_en>\d+), (?P<sync_bits>\d+), (?P<freq_dev>\d+), "
                        "(?P<mac_hdr_len>\d+), (?P<rx_link_addr>.+), (?P<tx_link_addr>.+)")

if (args.csvfile):
    csv_filename = args.csvfile
rx_log_filename = args.rxlog
tx_log_filename = args.txlog
rx_log = open(rx_log_filename, "r")
tx_log = open(tx_log_filename, "r")
rx_nodes = {}

for line in rx_log:
    chomped_line = line.rstrip()

    match = re.match(LOG_REGEXP, chomped_line)
    
    if (match):
        descriptor = match.group("descriptor")
        packet_nr = int(match.group("packet_nr"))
        payload_len = int(match.group("payload_len"))
        rssi = int(match.group("rssi"))
        rssi_offset = int(match.group("rssi_offset"))
        lqi = int(match.group("lqi"))
        tx_power = int(match.group("tx_power"))
        channel = int(match.group("channel"))
        channel_center_freq_0 = int(match.group("channel_center_freq_0"))
        channel_spacing = int(match.group("channel_spacing"))
        channel_center_freq_curr = int(match.group("channel_center_freq_curr"))
        bitrate = int(match.group("bitrate"))
        symbol_rate = int(match.group("symbol_rate"))
        rx_filt_bw = int(match.group("rx_filt_bw"))
        preamble_nibbles = int(match.group("preamble_nibbles"))
        preamble_word = match.group("preamble_word")
        crc_poly = match.group("crc_poly")
        crc_init = match.group("crc_init")
        sync_word = match.group("sync_word")
        sync_word_thr = int(match.group("sync_word_thr"))
        dual_sync_en = int(match.group("dual_sync_en"))
        sync_bits = int(match.group("sync_bits"))
        freq_dev = int(match.group("freq_dev"))
        mac_hdr_len = int(match.group("mac_hdr_len"))
        rx_link_addr = match.group("rx_link_addr")
        tx_link_addr = match.group("tx_link_addr")

        try:
            if (not rx_link_addr in rx_nodes):
                rx_nodes[rx_link_addr] = receiver_node(rx_link_addr)
            
            rx_nodes[rx_link_addr].add_transmission(message(descriptor, packet_nr, payload_len, rssi, rssi_offset, lqi, tx_power, 
                                                            channel, channel_center_freq_0, channel_spacing, channel_center_freq_curr,
                                                            bitrate, symbol_rate, rx_filt_bw, preamble_nibbles, preamble_word,
                                                            crc_poly, crc_init, sync_word, sync_word_thr, dual_sync_en, sync_bits,
                                                            freq_dev, mac_hdr_len, rx_link_addr, tx_link_addr))

        except ValueError as e:
            print(e)

for line in tx_log:
    chomped_line = line.rstrip()

    match = re.match(LOG_REGEXP, chomped_line)
    
    if (match):
        descriptor = match.group("descriptor")
        packet_nr = int(match.group("packet_nr"))
        payload_len = int(match.group("payload_len"))
        rssi = int(match.group("rssi"))
        rssi_offset = int(match.group("rssi_offset"))
        lqi = int(match.group("lqi"))
        tx_power = int(match.group("tx_power"))
        channel = int(match.group("channel"))
        channel_center_freq_0 = int(match.group("channel_center_freq_0"))
        channel_spacing = int(match.group("channel_spacing"))
        channel_center_freq_curr = int(match.group("channel_center_freq_curr"))
        bitrate = int(match.group("bitrate"))
        symbol_rate = int(match.group("symbol_rate"))
        rx_filt_bw = int(match.group("rx_filt_bw"))
        preamble_nibbles = int(match.group("preamble_nibbles"))
        preamble_word = match.group("preamble_word")
        crc_poly = match.group("crc_poly")
        crc_init = match.group("crc_init")
        sync_word = match.group("sync_word")
        sync_word_thr = int(match.group("sync_word_thr"))
        dual_sync_en = int(match.group("dual_sync_en"))
        sync_bits = int(match.group("sync_bits"))
        freq_dev = int(match.group("freq_dev"))
        mac_hdr_len = int(match.group("mac_hdr_len"))
        rx_link_addr = match.group("rx_link_addr")
        tx_link_addr = match.group("tx_link_addr")

        try:
            if (not rx_link_addr in rx_nodes):
                rx_nodes[rx_link_addr] = receiver_node(rx_link_addr)
            
            for n in rx_nodes:
                rx_nodes[n].add_transmission(message(descriptor, packet_nr, payload_len, rssi, rssi_offset, lqi, tx_power, 
                                                     channel, channel_center_freq_0, channel_spacing, channel_center_freq_curr,
                                                     bitrate, symbol_rate, rx_filt_bw, preamble_nibbles, preamble_word,
                                                     crc_poly, crc_init, sync_word, sync_word_thr, dual_sync_en, sync_bits,
                                                     freq_dev, mac_hdr_len, rx_link_addr, tx_link_addr))
            
        except ValueError as e:
            print(e)

print()
print("Results for %s" % (rx_log_filename))
print("-" * 80)

new_list = []

for n in rx_nodes:
    transmissions = rx_nodes[n].get_transmissions()
    for t in transmissions:
        for descriptor in transmissions[t].get_descriptors():
            if (transmissions[t].amount(descriptor) > 0):
                print("Amount of received_messages by %s from %s with descriptor %s: %d" 
                      % (n, t, descriptor, transmissions[t].amount(descriptor)))
                
                calculated_average_rssi = transmissions[t].average_rssi_all(descriptor)
                calculated_packet_loss = transmissions[t].packet_loss_all(descriptor)

                print("Average RSSI: %.2f" % (calculated_average_rssi))
                print("Packet loss rate: %.2f %%" % (calculated_packet_loss * 100))
                print()

                if (args.csvfile):
                    try:
                        for foo in transmissions[t].as_list_of_dict(descriptor):
                            # TODO perform calculations here
                            foo["packet loss"] = (calculated_packet_loss * 100)
                            new_list.append(foo)
                    except ValueError as e:
                        print(e)
            
            else:
                print("No messages received by %s from %s with descriptor %s" % (n, t, descriptor))
                print()

keys = new_list[0].keys()
with open(csv_filename, "w", newline='') as output_file:
    dict_writer = csv.DictWriter(output_file, keys)
    dict_writer.writeheader()
    dict_writer.writerows(new_list)
# TODO write / append to csv file from list of dicts