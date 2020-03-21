# ESP32 WiFi credential setup over BLE using NimBLE-Arduino
Setup your ESP32 WiFi credentials over BLE from an Android phone or tablet.
Sometimes you do not want to have your WiFi credentials in the source code, specially if it is open source and maybe accessible as a repository on Github or Bitbucket.

There are already solution like [WiFiManager-ESP32](https://github.com/zhouhan0126/WIFIMANAGER-ESP32) that give you the possibility to setup the WiFi credentials over a captive portal.    
But I wanted to test the possibility to setup the ESP32's WiFi over Bluetooth Low Energy.    
This repository covers the source code for the ESP32. The source code for the Android application are in the [ESP32_WiFi_BLE_Android](https://bitbucket.org/beegee1962/esp32_wifi_ble_android) repository. The ready to use Android application is available on [GooglePlay](https://play.google.com/store/apps/details?id=tk.giesecke.esp32wifible) and [APKfiles](https://www.apkfiles.com/apk-581908/esp32-wifi-setup-over-ble) or [download from here](https://www.desire.giesecke.tk/wp-content/uploads/tk.giesecke.esp32wifible-1.1-release.apk) for manual installation.     

Detailed informations about this project are on my [website](https://desire.giesecke.tk/index.php/2018/04/06/esp32-wifi-setup-over-ble/) 

## Lower memory usage!
The main problem with this application was always the memory usage. When initializing WiFi and BLE together, the ESP32 always runs low on both flash and heap memory. 
Thanks to [h2zero's NimBLE library](https://github.com/h2zero/NimBLE-Arduino) this version of the code is now better usable.
Here a comparison of the identical code, one time using ESP32 BLE library and second time using NimBLE-Arduino library
### Memory usage (compilation output)
#### Arduino BLE library
```log
RAM:   [==        ]  17.7% (used 58156 bytes from 327680 bytes)    
Flash: [========  ]  76.0% (used 1345630 bytes from 1769472 bytes)    
```
#### NimBLE-Arduino library
```log
RAM:   [=         ]  14.5% (used 47476 bytes from 327680 bytes)    
Flash: [=======   ]  69.5% (used 911378 bytes from 1310720 bytes)    
```
### Memory usage after **`setup()`** function
#### Arduino BLE library
**`Internal Total heap 259104, internal Free Heap 91660`**    
#### NimBLE-Arduino library
**`Internal Total heap 290288, internal Free Heap 182344`**    

---
# This is double the free heap for your application!
---

## Development platform
PlatformIO, but as the whole code is in a single file it can be easily copied into a .ino file and used with the Arduino IDE

## Used hardware
- [Elecrow ESP32 WIFI BLE BOARD WROOM](https://circuit.rocks/esp32-wifi-ble-board-wroom.html?search=ESP32)		
- Any Android phone or tablet that is capable of BLE.		

## SW practices used
- Use of BLE for sending and receiving data

## Library dependencies		
- PlatformIO ID64  -  [ArduinoJson by Benoit Blanchon](https://github.com/bblanchon/ArduinoJson)		
- [NimBLE-Arduino](https://github.com/h2zero/NimBLE-Arduino)