# ESP8266 Wattson

__ESP8266 based MQTT gateway for DIY Kyoto Wattson__

[DIY Kyoto Wattson](http://www.diykyoto.com/uk/aboutus/wattson-classic) is a three phase Electricity 
monitor that came to market about 2010. The manufacturer did not provide other software than "Holmes" for 
Windows. Then I reverse engineered the serial protocol and wrote a 
[Linux daemon](https://pikarinen.com/rrdwattsond/) for it in Perl. The daemon worked well all these years 
but I got an idea that it should use just ESP8266 and MQTT. The graph could be drawn by Grafana.

