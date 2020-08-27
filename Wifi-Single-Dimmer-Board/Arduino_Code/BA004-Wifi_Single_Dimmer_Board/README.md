# Wifi-Single-Dimmer-Board

For Atmega firmare,
we have to upload the Atmega_Single_DimmerV0_6_with_tasmota code.
In tools we have to select Board:Arduino/Genuino Uno.

for ESP8266 firmware,
we have to flash the sonoff_betaV0.3.ino.generic.bin file either by terminal or by flasher tool.
for linux user, first we have to go to the file path and left click, open the terminal and run the following command: 
"esptool.py --chip esp8266 --port /dev/ttyUSB0 --baud 115200 write_flash -z 0x00000000 sonoff_betaV0.3.ino.generic.bin"
Note: here we have to choose the correct port which is in use.
