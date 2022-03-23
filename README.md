# PicoVA

Firmware for a 
[Raspberry Pi Pico](https://www.raspberrypi.com/documentation/microcontrollers/raspberry-pi-pico.html) 
to read current/voltage/power from an
[INA219](https://www.ti.com/product/INA219). 
This makes for a dirt-cheap power meter – you can get a Pico for NZ$7 and an
INA219 module with a shunt resistor for NZ$3.

The firmware reads from the INA219 as fast as possible and transmits the
measurements over USB-CDC. Currently the configuration is hard-coded but I plan
to add auto-gain if I get time, and maybe a configuration interface over USB.

There's a fairly simple Python script to run on the host to plot the received
data in real time. 
