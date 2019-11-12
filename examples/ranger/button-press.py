#!/usr/bin/python
#
#   button-press.py   11-12-2019
#    

import RPi.GPIO as gpio
from subprocess import call
import time

gpio.setmode(gpio.BCM)
gpio.setup(26, gpio.IN, pull_up_down = gpio.PUD_UP)

def set_backlight(channel):
    print("Hello world!")
    
gpio.add_event_detect(26, gpio.FALLING, callback=set_backlight, bouncetime=300)

while 1:
    time.sleep(360)