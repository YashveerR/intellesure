#!/bin/bash


LOG="logger -t control_main"

ppid=$(grep 'PPid' /proc/$$/status | cut -f2)
pcmd=$(cat /proc/$ppid/cmdline)

$LOG "$1 from [$ppid] $pcmd"
#Current Pi HW is Pi 3 
case "$1" in
	stop)
		#Turn off USB IC INCLUDING THE ETHERNET PORT
		echo '1-1' |sudo tee /sys/bus/usb/drivers/usb/unbind
		#Turn off the HDMI 
		/opt/vc/bin/tvservice -o
		#Turn off the Power indicator LED
		sudo sh -c 'echo 0 >/sys/class/leds/led1/brightness'
		#Turn off the activity LED
		sudo sh -c 'echo 0 > /sys/class/leds/led0/brightness'

		#Turn On the HDMI - honestly not needed in headless Pi
		#sudo /opt/vc/bin/tvservice -p

		#In production environment enbale this, however for testing keep this on
		#sudo ifconfig wlan0 down
		$LOG "Stopping devices"
		;;
	start)
		#Turn ON the USB IC including the Ethernet PORT
		echo '1-1' |sudo tee /sys/bus/usb/drivers/usb/bind

		#Turn on the power indicator light
		sudo sh -c 'echo 1 >/sys/class/leds/led1/brightness'

		sudo sh -c 'echo 1 > /sys/class/leds/led0/brightness'
		
		python3.7 gsm_enable.py

		#sudo ifconfig wlan0 up
		$LOG "Starting up devices again"
		;;
	*)  $LOG "Unable to do anything for power!!"
		;;
esac
exit 0
