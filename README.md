# CapraClock

## Hardware setup
Hardware I used:
- RGB led matrix WS2812B 8x32 5V (ensure that it operates at 5v)
- ESP32
- LDR light sensor GL5516
- Temperature and humidity sensor HTU21

Connections are the following:
| ESP32 PIN | GPIO | Usage or part |
|---|---|---|
| 7 | ADC7 / GPIO35 | LDR light sensor (GL5516) (optional) |
| 8 | GPIO32 | Matrix |
| 33/36 | 21 (SDA) / 22(SCL) | Temperature and Humidity Sensors (BME280, BMP280, HTU21, SHT31) (optional) |

## Installation
From the *Arduino IDE*, first of all you need to install support for *ESP32* boards.  
Follow the guide at this [*link*](https://docs.espressif.com/projects/arduino-esp32/en/latest/installing.html#windows-manual-installation).  

After that, you need to install the *plugin* to upload the files into the *ESP32* memory.  
Follow the guide at this [*link*](https://randomnerdtutorials.com/arduino-ide-2-install-esp32-littlefs/)  

## Settings
To change the *Wi-Fi network* to which the clock will be connected, open the **wifi_settings.h** file and change the *SSID* and *password*. 

**Note**: you can set up multiple SSIDs if you have different networks in your house, but only one password will be used.  
So if you have multiple Wi-Fi networks in your home with different passwords, choose the one you want to be connect to.

You can also enable or disable screen data, temperature/humidity, or the logo at startup by changing **ENABLE_DATE**, **ENABLE_SENSORS**, or **SHOW_LOGO** from *true* to *false* or vice versa.

## Upload
Open the **CapraClock.ino** with the *Arduino IDE* file and upload the code.  

Go to **Tools -> Erase all flash before sketch upload** on the Arduino IDE and *enable* it.  

Now, you need to upload the contents of the data folder to the *ESP32* memory.  
Press **CTRL+SHIFT+P** and search for *"Upload LittleFS to Pico/ESP8266/ESP32"* and press **ENTER**.
