#!/usr/bin/env python3.7

from rtlsdr import RtlSdr
import syslog
import math
from threading import Timer # for watchdog timer
import sys
import datetime
import time
import logging
import signal

NUM_SAMPLES = 32768
BARGRAPH = "################################################################################" \
            + "                                                                                "

loop_flag = True

def main():

    #Setup the SDR device here to match a centre freq; 
    #TODO: Read from noSQL table for settings; Error handling
    sdr = RtlSdr()
    # configure device
    sdr.sample_rate = 1.024e6  # Hz
    sdr.center_freq = 433.9e6     # Hz
    sdr.freq_correction = 20   # PPM
    sdr.gain = 'auto'
    
    signal.signal(signal.SIGTERM, service_shutdown)
    signal.signal(signal.SIGINT, service_shutdown)

    #To get some initial readings and an estimate of averages;
    for i in range(0, 10):
        rssi = MeasureRSSI(sdr)

    # Measure minimum RSSI over a few readings, auto-adjust for dongle gain
    min_rssi = 1000
    avg_rssi = 0
    for i in range(0, 10):
        rssi = MeasureRSSI(sdr)
        min_rssi = min(min_rssi, rssi)
        avg_rssi += rssi
    avg_rssi /= 10
    ampl_offset = avg_rssi
    max_rssi = MeasureRSSI(sdr) - ampl_offset
    avg_rssi = max_rssi + 20;
    counter = 0
    redirect_stderr() #as is the function name; redirect errors to prevent flooding of std-out

    
    #init_dt = datetime.datetime.now() #we want to take an initial time here to check on 1 minute intervals 
    #run the reading in a loop; takes around 6 seconds to converge to a value
    while(loop_flag):          
          #current_dt = datetime.datetime.now()        
          #if ((current_dt - init_dt).total_seconds() > 1 ):
              #print("1 minute passed")
          #init_dt = datetime.datetime.now()
          rssi = MeasureRSSI(sdr) - ampl_offset
          avg_rssi = ((9 * avg_rssi) + rssi) / 10        
          if avg_rssi > 30:
             with open("sms_tx_fd", "w") as sms_wr:
                   sms_wr.write("2");
                   syslog.syslog(LOG_INFOG, 'Sending SMS: Jamming Detected')
             break #as a result of jamming detected, we send the sms and exit gracefully
          time.sleep(0.2) #here so the process does not flood the CPU

    sdr.close() #close the resource

# First attempt: using floating-point complex samples
def MeasureRSSI_1(sdr):
    samples = sdr.read_samples(NUM_SAMPLES)
    power = 0.0
    for sample in samples:
        power += (sample.real * sample.real) + (sample.imag * sample.imag)
    return 10 * (math.log(power) - math.log(NUM_SAMPLES))

# Second go: read raw bytes, square and add those
def MeasureRSSI_2(sdr):
    data_bytes = sdr.read_bytes(NUM_SAMPLES * 2)
    power = 0
    for next_byte in data_bytes:
        signed_byte = next_byte + next_byte - 255
        power += signed_byte * signed_byte
    return 10 * (math.log(power) - math.log(NUM_SAMPLES) - math.log(127)) - 70

# Third go: modify librtlsdr, do the square-and-add calculation in C
def MeasureRSSI_3(sdr):
    while(True):
        try:
            return sdr.read_power_dB(NUM_SAMPLES) - 112
        except OSError: # e.g. SDR unplugged...
            pass  # go round and try again, SDR will be replugged sometime...

# Select the desired implementation here:
def MeasureRSSI(sdr):
    return MeasureRSSI_1(sdr)
#    return sdr.read_offset_I(NUM_SAMPLES) / (NUM_SAMPLES / 2)
#    return sdr.read_offset_Q(NUM_SAMPLES) / (NUM_SAMPLES / 2)

def service_shutdown():
    loop_flag = False
    logging.info('Exiting gracefully')
    syslog.closelog()
    

def redirect_stderr():
    import os, sys
    sys.stderr.flush()
    err = open('/dev/null', 'a+')
    os.dup2(err.fileno(), sys.stderr.fileno())  # send ALSA underrun error messages to /dev/null
    

if __name__ == "__main__":
    import os, sys
#try:
    main()

#except(SystemExit, KeyboardInterrupt):
    exit()
