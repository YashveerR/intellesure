#!/usr/bin/env python3.7

import RPi.GPIO as GPIO
import time

GSM_EN = 19


#This indicates what style of input we are using: https://pinout.xyz
#Here PIN number are used rather than actual GPIO references to make this portable
GPIO.setmode(GPIO.BOARD)

GPIO.setup(GSM_EN, GPIO.OUT)

while True:
	GPIO.output(GSM_EN, GPIO.LOW)
	time.sleep(4)
	GPIO.output(GSM_EN, GPIO.HIGH)
	break
GPIO.cleanup()
