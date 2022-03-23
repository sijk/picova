from ina219 import INA219, DeviceRangeError
from machine import I2C, Pin
import time

led = Pin(25, Pin.OUT, value=1)
i2c = I2C(0, freq=400_000)
ina = INA219(shunt_ohms=0.1, i2c=i2c)
ina.configure()

while True:
    try:
        ina.update()
        print(time.ticks_us(), ina.voltage(), ina.current(), ina.power(), sep=',')
    except DeviceRangeError:
        pass
