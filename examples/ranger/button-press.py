#!/usr/bin/env python3
#
#   button-press.py   11-12-2019
#    

import shlex
import subprocess
import atexit
import logging
import io
import datetime
import argparse

import RPi.GPIO as gpio
import time

parser = argparse.ArgumentParser()
parser.add_argument("-t","--terminal",action="store_true",help="Also prints the log output to the terminal.")
args = parser.parse_args()

gpio.setmode(gpio.BCM)
gpio.setup(12, gpio.IN, pull_up_down = gpio.PUD_UP)
gpio.setup(16, gpio.IN, pull_up_down = gpio.PUD_UP)

def start_test(channel):
    shell_0.stdin.write("l")
    shell_1.stdin.write("l")

def reset_node(channel):
    shell_0.stdin.write("r")
    shell_1.stdin.write("r")

shell_0 = subprocess.Popen(shlex.split('make login PORT=/dev/ttyUSB0'),stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE,universal_newlines=True,bufsize=0)
shell_1 = subprocess.Popen(shlex.split('make login PORT=/dev/ttyUSB1'),stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE,universal_newlines=True,bufsize=0)

gpio.add_event_detect(12, gpio.FALLING, callback=start_test, bouncetime=300)
gpio.add_event_detect(16, gpio.FALLING, callback=reset_node, bouncetime=300)

def exit_handler():
    shell_0.stdin.close()
    shell_1.stdin.close()
    logging.shutdown()
    print("Exiting")

atexit.register(exit_handler)

log_0 = logging.getLogger('timestamper_0')
log_0.setLevel(logging.INFO)
if args.terminal:
    log_0.addHandler(logging.StreamHandler())
log_0_filename = "%s.log" % (datetime.datetime.now().strftime("log_0_%d-%m-%Y_%H-%M-%S-%f"))
log_0.addHandler(logging.FileHandler(log_0_filename))
log_0.info("Created logfile \"{}\"".format(log_0_filename))

log_1 = logging.getLogger('timestamper_1')
log_1.setLevel(logging.INFO)
if args.terminal:
    log_1.addHandler(logging.StreamHandler())
log_1_filename = "%s.log" % (datetime.datetime.now().strftime("log_1_%d-%m-%Y_%H-%M-%S-%f"))
log_1.addHandler(logging.FileHandler(log_1_filename))
log_1.info("Created logfile \"{}\"".format(log_1_filename))

start_time = datetime.datetime.now()

while True:
    current_time = datetime.datetime.now()
    log_0.info("%s | %s | %s" % (current_time.isoformat(),(current_time - start_time),shell_0.stdout.readline().strip()))
    log_1.info("%s | %s | %s" % (current_time.isoformat(),(current_time - start_time),shell_1.stdout.readline().strip()))