#!/usr/bin/python
#
#   button-press.py   11-12-2019
#    

import pexpect
import sys

import RPi.GPIO as gpio
from subprocess import call
import time

gpio.setmode(gpio.BCM)
gpio.setup(12, gpio.IN, pull_up_down = gpio.PUD_UP)
gpio.setup(16, gpio.IN, pull_up_down = gpio.PUD_UP)

def start_test(channel):
    child.sendline('l')

def reset_node(channel):
    child.sendline('r')

child = pexpect.spawn('make login PORT=/dev/ttyUSB0', encoding='utf-8')
child.logfile = sys.stdout
child.expect('^.*?[OK].*?\r\n')

gpio.add_event_detect(12, gpio.FALLING, callback=start_test, bouncetime=300)
gpio.add_event_detect(16, gpio.FALLING, callback=reset_node, bouncetime=300)

while 1:
    time.sleep(360)