# CapraClock

## Installation
From the *Arduino IDE*, first of all you need to install support for *ESP32* boards.  
Follow the guide at this [*link*](https://docs.espressif.com/projects/arduino-esp32/en/latest/installing.html#windows-manual-installation).  

After that, you need to install the *plugin* to load the files into the *ESP32* memory.  
Follow the guide at this [*link*](https://randomnerdtutorials.com/arduino-ide-2-install-esp32-littlefs/)  


## Settings
To change the *Wi-Fi network* to which the clock will be connected, open the **wifi_settings.h** file and change the *SSID* and *password*. 

**Note**: you can set up multiple SSIDs if you have different networks in your house, but only one password will be used.  
So if you have multiple Wi-Fi networks in your home with different passwords, choose the one you want to be connect to.

## Upload
Open the **CapraClock.ino** with the *Arduino IDE* file and upload the code.  

Now, you need to upload the contents of the data folder to the *ESP32* memory.  
Press **CTRL+SHIFT+P** and search for *"Upload LittleFS to Pico/ESP8266/ESP32"* and press **ENTER**.
