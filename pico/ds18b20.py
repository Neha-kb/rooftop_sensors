from machine import Pin
import time
import onewire
import ds18x20

ow = onewire.OneWire(Pin(28))
ds = ds18x20.DS18X20(ow)

devices = ds.scan()
print('found devices:', devices)

while True:
    ds.convert_temp()
    time.sleep_ms(750)
    for device in devices:
        print("Device: {}".format(device))
        print("Temperature= {}".format(ds.read_temp(device)))
