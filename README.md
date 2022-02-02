# Intro
**MaritimDataCollector** is the part that reads data from sensors and deliver these data to the **MDS**.

The whole project is a solution made by me, for collecting data from different sensor and have access to the sensor data all around the worl.  
You will find the **MDS** documentation under **https://github.com/bytecrusher/MaritimeDataServer**

## Description
The sensors (g.E. Dalas Temperature, Battery Voltage...) are connected to a ESP32 microcontroller via OneWire.  
The ESP will periodically (ever 15 minutes) or event driven (waterlevel switch) collect data from these sensors and store a smal amount of data local (currently only the last data).  
The **MDC** will send these data even periodically or event driven to the server as JSON via WiFi (Lora is planed).  

![Board1](/images/board1.jpeg)

## Folder description

- **images** contains images for readme.md documentation
- **include** currently empty
- **lib** currently empty (for private librarys)
- **src** source files for ESP32

---
## **MDC** (MaritimeDataCollector)

The **MaritimeDataCollector** is a microcontroller driven device. Here i am using an ESP32 from Espressif.

The controller will be powered from a batterie. I my case it is located in my boat with a 12v batterie and goes most time into sleep mode.

#### Datasheet
- 5V-35V supply voltage
- *W power consumtion
- ESP32 NodeMCU
- WiFi for data transfer to the server
- OTA (Over tha air) Firmware update
- OneWire Bus

## Development
For Development i use as IDE VSC (Visual Studio Code) with PlatformIO as extension.
The Initial upload of the firmware needs to be done via USB.
Now it also possible to update the ESP via OTA (OverTheAir).

## On Compiling Issues
Maybe the OneWire library from Paul Stroffregen will be installed. If yes, uninstall this lib and remove the onewire folder under /.pio/libdeps/esp32dev/ and recompile.

#### functions / ToDos Status
- [x] WiFi for data transfer to the server.
- [x] after read data go to sleep for a while to save energy.  
- [x] OTA (Over tha air) Firmware update.
- [x] DS18B20 Temperature Sensor.
- [ ] Input for Alarm signal.
- [ ] input for tank sensors.
- [ ] Output for Relais.
- [ ] GPS  
- [ ] humidity sensor.  
- [ ] Power ESP32 from solar panel.  
- [x] change server communication to JSON.  
- [ ] store a smal set of sensor data, if connection to the server is available.  
- [ ] Implement LoRa (LoRaWan, TTN).  
- [ ] store user data in Flash.

###### Power states.
The MDC will have different power states, for saving energy.

- *Standby* no data will collect, no data will send to server, saving most energy.
- *datacollect/senddata* cpu will wakeup, collect data from sensors and try to send it (via WiFi).

