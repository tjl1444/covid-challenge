# Covid Challenge

# Project Description

Contacts the covid19 API via TLS v1.2 and simply prints out the response and attempts to do some JSON parsing using basic C libraries.

# Build Instructions

To build this project using esp-idf:
* Within the command line, change into the directory of this project
* Run `idf.py menuconfig` to open the project configuration menu and set the WiFi SSID and password. This will generate your sdkconfig file
* Run `idf.py build` to build the project 
* Flash the generated binary onto the ESP32 by running `idf.py -p PORT flash`
