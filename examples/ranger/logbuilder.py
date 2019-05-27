import io
import sys
import re
import argparse
import glob
import os

parser = argparse.ArgumentParser()
parser.add_argument("rxlog", help="The logfile to which all received messages need to be copied.")
parser.add_argument("txlog", help="The logfile to which all transmitted messages need to be copied.")
args = parser.parse_args()

rx_log_filename = args.rxlog
tx_log_filename = args.txlog

file_list = []

script_dir = os.path.dirname(__file__)
raw_log_dir = "raw-logs"

for file in glob.glob(os.path.join(script_dir,raw_log_dir,"*.log")):
    file_list.append(file)

LOG_REGEXP = re.compile("^.*?csv-log: (?P<descriptor>.+), (?P<packet_nr>\d+), (?P<payload_len>\d+), (?P<rssi>[+-]?\d+), "
                        "(?P<rssi_offset>[+-]?\d+), (?P<lqi>\d+), (?P<tx_power>\d+), (?P<channel>\d+), (?P<channel_center_freq_0>\d+), "
                        "(?P<channel_spacing>\d+), (?P<channel_center_freq_curr>\d+), (?P<bitrate>\d+), (?P<symbol_rate>\d+), "
                        "(?P<rx_filt_bw>\d+), (?P<preamble_nibbles>\d+), (?P<preamble_word>.+), (?P<crc_poly>.+), (?P<crc_init>.+), "
                        "(?P<sync_word>.+), (?P<sync_word_thr>\d+), (?P<dual_sync_en>\d+), (?P<sync_bits>\d+), (?P<freq_dev>\d+), "
                        "(?P<mac_hdr_len>\d+), (?P<rx_link_addr>.+), (?P<tx_link_addr>.+)")

empty_line = ""
with open(tx_log_filename, "w") as tx_log:
    tx_log.write(empty_line)
with open(rx_log_filename, "w") as rx_log:
    rx_log.write(empty_line)

if (len(file_list) >= 1):
    for file_path in file_list:
        with open(file_path, "r") as f:
            for line in f:
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

                    if(rx_link_addr == tx_link_addr):
                        with open(tx_log_filename, "a") as tx_log:
                            tx_log.write(line)
                    else:
                        with open(rx_log_filename, "a") as rx_log:
                            rx_log.write(line)
