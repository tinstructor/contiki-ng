#!/usr/bin/python
#
#   button-press.py   11-12-2019
#    

import pexpect

import RPi.GPIO as gpio
from subprocess import call
import time

gpio.setmode(gpio.BCM)
gpio.setup(26, gpio.IN, pull_up_down = gpio.PUD_UP)

def start_test(channel):
    child = pexpect.spawn('make login')
    child.expect('^.*?[OK].*?\r\n')
    child.sendline('l')
    child.interact()
    
gpio.add_event_detect(26, gpio.FALLING, callback=start_test, bouncetime=300)

while 1:
    time.sleep(360)