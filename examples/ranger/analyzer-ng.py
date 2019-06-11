import io
import sys
import re
import argparse
import csv
import math

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
parser.add_argument("csvfile", help="The csv file to which log-derived info must be appended.")
parser.add_argument("nodeinfo", help="Configuration file for nodes.")
parser.add_argument("losinfo", help="Configuration file for los conditions.")
args = parser.parse_args()

LOG_REGEXP = re.compile("^.*?csv-log: (?P<descriptor>.+), (?P<packet_nr>\d+), (?P<payload_len>\d+), (?P<rssi>[+-]?\d+), "
                        "(?P<rssi_offset>[+-]?\d+), (?P<lqi>\d+), (?P<tx_power>\d+), (?P<channel>\d+), (?P<channel_center_freq_0>\d+), "
                        "(?P<channel_spacing>\d+), (?P<channel_center_freq_curr>\d+), (?P<bitrate>\d+), (?P<symbol_rate>\d+), "
                        "(?P<rx_filt_bw>\d+), (?P<preamble_nibbles>\d+), (?P<preamble_word>.+), (?P<crc_poly>.+), (?P<crc_init>.+), "
                        "(?P<sync_word>.+), (?P<sync_word_thr>\d+), (?P<dual_sync_en>\d+), (?P<sync_bits>\d+), (?P<freq_dev>\d+), "
                        "(?P<mac_hdr_len>\d+), (?P<rx_link_addr>.+), (?P<tx_link_addr>.+)")

csv_filename = args.csvfile
rx_log_filename = args.rxlog
tx_log_filename = args.txlog
rx_nodes = {}

with open(rx_log_filename, "r") as rx_log:
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

with open(tx_log_filename, "r") as tx_log:
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

NODE_REGEXP = re.compile("^.*?node-info: (?P<link_addr>.+), (?P<node_id>.+), (?P<height>\d+), (?P<antenna>.+), "
                         "(?P<temperature>\d+\.\d+), (?P<x>\d+), (?P<y>\d+), (?P<z>\d+)")

node_info_filename = args.nodeinfo
node_info = {}

with open(node_info_filename, "r") as node_info_file:
    for line in node_info_file:
        chomped_line = line.rstrip()

        match = re.match(NODE_REGEXP, chomped_line)

        if(match):
            link_addr = match.group("link_addr")
            node_id = match.group("node_id")
            height = int(match.group("height"))
            antenna = match.group("antenna")
            temperature = float(match.group("temperature"))
            x = int(match.group("x"))
            y = int(match.group("y"))
            z = int(match.group("z"))

            if (not link_addr in rx_nodes):
                raise ValueError("Illegal node addr.")
            
            if (not link_addr in node_info):
                node_info[link_addr] = [node_id, height, antenna, temperature, x, y, z]

for n in rx_nodes:
    if (not n in node_info):
        print("Node with link addr %s not yet in infobase! Enter now:" % (n))
        node_info[n] = []
        node_info[n].append(input("ID: "))
        node_info[n].append(int(input("Height: ")))
        node_info[n].append(input("Antenna: "))
        node_info[n].append(float(input("Temperature: ")))
        node_info[n].append(int(input("x: ")))
        node_info[n].append(int(input("y: ")))
        node_info[n].append(int(input("z: ")))
        print()

LOS_REGEXP = re.compile("^.*?los-info: (?P<node_id_1>.+), (?P<node_id_2>.+), (?P<los>.+)")

los_info_filename = args.losinfo
los_info = {}

with open(los_info_filename, "r") as los_info_file:
    for line in los_info_file:
        chomped_line = line.rstrip()

        match = re.match(LOS_REGEXP, chomped_line)

        if(match):
            node_id_1 = match.group("node_id_1")
            node_id_2 = match.group("node_id_2")
            los = match.group("los")

            if (not frozenset((node_id_1, node_id_2)) in los_info):
                los_info[frozenset((node_id_1, node_id_2))] = los

for i in node_info:
    for j in node_info:
        if (i != j and not frozenset((node_info[i][0], node_info[j][0])) in los_info):
            print("Los conditions between %s and %s not yet in infobase! Enter now:" % (node_info[i][0], node_info[j][0]))
            los_info[frozenset((node_info[i][0], node_info[j][0]))] = input("Los (yes/no): ")

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

                try:
                    for foo in transmissions[t].as_list_of_dict(descriptor):
                        new_dict = {}
                        new_dict["Receiver_id"] = node_info[foo["rx_link_addr"]][0]
                        new_dict["Transmitter_id"] = node_info[foo["tx_link_addr"]][0]
                        new_dict["Receiver_height"] = node_info[foo["rx_link_addr"]][1]
                        new_dict["Transmitter_height"] = node_info[foo["tx_link_addr"]][1]
                        new_dict["Is_line_of_sight"] = los_info[frozenset((node_info[foo["rx_link_addr"]][0],node_info[foo["tx_link_addr"]][0]))]
                        new_dict["Receiver_location"] = "%d, %d, %d" % (node_info[foo["rx_link_addr"]][4], node_info[foo["rx_link_addr"]][5], \
                            node_info[foo["rx_link_addr"]][6])
                        new_dict["Transmitter_location"] = "%d, %d, %d" % (node_info[foo["tx_link_addr"]][4], node_info[foo["tx_link_addr"]][5], \
                            node_info[foo["tx_link_addr"]][6])
                        new_dict["Antenna_type"] = node_info[foo["rx_link_addr"]][2]
                        new_dict["Distance_from_tx"] = math.sqrt(math.pow(node_info[foo["tx_link_addr"]][4] - node_info[foo["rx_link_addr"]][4],2) \
                            + math.pow(node_info[foo["tx_link_addr"]][5] - node_info[foo["rx_link_addr"]][5],2) + math.pow(node_info[foo["tx_link_addr"]][6] \
                            - node_info[foo["rx_link_addr"]][6],2))
                        new_dict["MCS"] = foo["descriptor"]
                        new_dict["Center_frequency"] = foo["channel_center_freq_curr"]
                        new_dict["Spreading_factor"] = ""
                        new_dict["Bandwidth"] = foo["rx_filt_bw"] # REVIEW channel spacing, filter bandwidth or actual calculated bandwidth?
                        new_dict["Bitrate"] = foo["bitrate"]
                        new_dict["Packet_size"] = foo["preamble_nibbles"] / 2 + foo["sync_bits"] / 8 + 1 + foo["mac_hdr_len"] \
                                                  + foo["payload_len"] + ((len(foo["crc_poly"]) - 2) / 2 if int(foo["crc_poly"],16) else 0)
                        new_dict["Packet_id"] = foo["packet_nr"]
                        new_dict["Received_power"] = foo["rssi"]
                        new_dict["SNR"] = ""
                        new_dict["LQI"] = foo["lqi"]
                        new_dict["Temperature"] = node_info[foo["rx_link_addr"]][3]
                        new_dict["Sync_word"] = foo["sync_word"]
                        new_dict["Frequency_deviation"] = foo["freq_dev"]
                        new_dict["CRC_polynomial"] = foo["crc_poly"]
                        new_dict["CRC_init_vector"] = foo["crc_init"]
                        new_dict["Packet_loss"] = (calculated_packet_loss * 100)
                        new_list.append(new_dict)
                except (ValueError, KeyError) as e:
                    print(e)
            
            else:
                print("No messages received by %s from %s with descriptor %s" % (n, t, descriptor))
                print()

keys = new_list[0].keys()
with open(csv_filename, "w", newline='') as output_file:
    dict_writer = csv.DictWriter(output_file, keys)
    dict_writer.writeheader()
    dict_writer.writerows(new_list)
