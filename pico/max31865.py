import machine, time
from adafruit_max31865 import MAX31865

# SPI1 pins on Pico W
spi = machine.SPI(1,
    baudrate=500000,
    polarity=0,
    phase=1,
    bits=8,
    firstbit=machine.SPI.MSB,
    sck=machine.Pin(10),
    mosi=machine.Pin(11),
    miso=machine.Pin(12))

cs = machine.Pin(13, machine.Pin.OUT)

# Initialize MAX31865 for 4-wire RTD (use 2 or 3 depending on your wiring)
sensor = MAX31865(spi, cs, wires=4)

while True:
    try:
        temp = sensor.temperature
        print("MAX31865 Temperature: {:.2f} °C".format(temp))
    except Exception as e:
        print("Sensor error:", e)
    time.sleep(2)

