# Pulse RTS to run firmware after esptool --after no_reset.
param([string]$Port = "COM7")

python -c @"
import serial, sys, time
port = sys.argv[1]
ser = serial.Serial()
ser.port = port
ser.baudrate = 115200
ser.timeout = 0.2
ser.dtr = False
ser.rts = False
ser.open()
ser.rts = True
time.sleep(0.05)
ser.rts = False
time.sleep(0.05)
ser.close()
print(f'cores3-reset: RTS pulse on {port}')
"@ $Port
