# Firmware for ESP32 GPS Vehicle Tracker with SIM800L Modem

## Installation Procedure

1. **Clone the Repository:**
   Clone the repository to your local environment.

2. **Install Arduino IDE:**
   If you haven't installed the Arduino IDE, download and install it from [this link](https://support.arduino.cc/hc/en-us/articles/360019833020-Download-and-install-Arduino-IDE).

3. **Compile Code:**
   Open the Arduino IDE and compile the code for the ESP32 hardware board.

4. **Connect Hardware:**
   Connect the ESP32 to the SIM800L modem and GPS module according to the pinout descriptions provided in the code. The GPS module will send coordinates to the MQTT server.

5. **Set Up MQTT Server:**
   In a separate repository, you'll find an MQTT server built with Django and Channels. This server will handle requests and provide an API to external services and apps.

Happy testing! :)
