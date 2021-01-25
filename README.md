# led-display

Displays current time on a 3D LED matrix. Time is retrieved from an NTP server and converted to the correct time (GMT/BST).

## Hardware

The display is running from an Arduino Nano, using shift registers to enable up to eight LEDs to be controlled from each pin. 
Network connectivity is handled by a Wemos D1 Mini, which communicates over serial to the Arduino.
