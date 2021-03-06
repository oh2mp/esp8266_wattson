# ESP8266 Wattson hardware hacking and the circuit

Wattson has a FT232RL USB to serial chip. We want to bypass it and talk directly to the device's 
internal 3.3V TTL serial line. To connect the ESP8266 to Wattson's internal serial line, the soldering 
points and other needed modifications are in the photo below.

![wattson_board_under2_small.jpg](images/wattson_board_under2_small.jpg)

- Connect ESP8266:s GPIO12 to the point marked as TX.
- Connect ESP8266:s GPIO13 to the point marked as RX.

Cut the lead near the point marked as USB TX. If you want to put eg. a toggle switch that you can choose
between ESP and USB, connect it to switch between USB TX and ESP.

Wattson's firmware detects if there's a USB connection. If there's not, it does not talk to the serial 
line. It's sensed from the USB connector if there's +5V DC or not. Near to the connector is a power 
transistor, where +5V can be taken and connected to the USB port +5V solderin point to fake the connected 
USB. I soldered a 1N4148 diode there in case the USB will be used some day. Then it will not feed the 
emitter of the transistor.

Connect also a push button between ESP's ground and GPIO14. It starts the portal mode. You can also 
connect an optional LED to GPIO16. It flashes always when a MQTT packet is sent. In portal mode it's 
blinking.

![esp8266_wattson_schema.jpg](images/esp8266_wattson_schema.jpg)

----------

## Powering the ESP8266

The device is powered with 9V AC adapter. There are two extra soldering points next to where it's connected
so it's almost too easy to take power there. See red arrow on the image.

![wattson_board.jpg](images/wattson_board.jpg)

I used a 3.3V buck converter to take power for my generic ESP8266 module. There's plenty of space on
the right side of the case. The buck converter and ESP are mounted just with two sided tape.

![wattson_and_esp_small.jpg](images/wattson_and_esp_small.jpg)


