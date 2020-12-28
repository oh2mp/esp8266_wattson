# ESP8266 Wattson hardware hacking and the circuit

Wattson has a FT232RL USB to serial chip. We want to bypass it and talk directly to the device's 
internal 3.3V TTL serial line. To connect the ESP8266 to Wattson's internal serial line, the soldering 
points and other needed modifications are in the photo below.

![wattson_board_under2_small.jpg](i/wattson_board_under2_small.jpg)

- Connect ESP8266:s GPIO12 to the point marked as TX.
- Connect ESP8266:s GPIO13 to the point marked as TX.

Cut the lead near the point marked as USB TX. If you want to put eg. a toggle switch that you can choose
between ESP and USB, connect it to switch between USB TX and ESP.

Wattson's firmware detects if there's a USB connection. If there's not, it does not talk to the serial 
line. It's sensed from the USB connector if there's +5V DC or not. Near to the connector is a power 
transistor, where +5V can be taken and connected to the USB port tap to fake the connected USB. I soldered
a 1N4148 diode there in case the USB will be used some day. Then it will not feed the emitter of the transistor.

Connect also a push button between ESP's ground and GPIO14. It starts the portal mode. You can also 
connect an optional LED to GPIO16. It flashes always when a MQTT packet is sent. In portal mode it's 
blinking.

The internet is full of documentation how to make ESP8266 connections like needed pullup resistors,
programming circuit etc. so I will not repeat that information here.

----------