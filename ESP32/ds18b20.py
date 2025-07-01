import machine, onewire, ds18x20, time

# DS18B20 data pin is connected to GPIO 4 (change if needed)
dat = machine.Pin(4)

# Initialize OneWire and DS18B20
ow = onewire.OneWire(dat)
ds = ds18x20.DS18X20(ow)

# Scan for devices
roms = ds.scan()
print('Found DS devices:', roms)

# Main loop to read temperature
while True:
    ds.convert_temp()
    time.sleep_ms(750)  # Wait for conversion
    for rom in roms:
        print('Temperature:', ds.read_temp(rom), '°C')
    time.sleep(2)
