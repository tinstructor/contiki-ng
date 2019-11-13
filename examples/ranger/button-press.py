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
    child_0.sendline('l')
    child_1.sendline('l')

def reset_node(channel):
    child_0.sendline('r')
    child_1.sendline('r')

shell_cmd_0 = 'make login PORT=/dev/ttyUSB0 | python3 timestamper.py -f test_0'
child_0 = pexpect.spawn('/bin/bash', ['-c', shell_cmd_0], encoding='utf-8')
child_0.expect('^.*?[OK].*?\r\n')

shell_cmd_1 = 'make login PORT=/dev/ttyUSB1 | python3 timestamper.py -f test_1'
child_1 = pexpect.spawn('/bin/bash', ['-c', shell_cmd_1], encoding='utf-8')
child_1.expect('^.*?[OK].*?\r\n')

gpio.add_event_detect(12, gpio.FALLING, callback=start_test, bouncetime=300)
gpio.add_event_detect(16, gpio.FALLING, callback=reset_node, bouncetime=300)

while 1:
    time.sleep(360)