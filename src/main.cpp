#include <WiFi.h>
#include <WiFiClient.h>
#include <driver/adc.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <OneWire.h>  // https://github.com/stickbreaker/OneWire.git, lib from PaukStoffregen has issues.
#include <DallasTemperature.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <ArduinoJson.h>
#include <secret.h>

#define FIRMWAREVERSION "0.0.2"
#define protocollversion "1"

// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);
// Pass our oneWire reference to Dallas Temperature. 
DallasTemperature sensors(&oneWire);

// variable to hold device addresses
RTC_DATA_ATTR DeviceAddress sensoradresses[10];

// Number of temperature devices found
RTC_DATA_ATTR uint8_t numberOfDevices;

#define uS_TO_S_FACTOR 1000000  /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP  (15 * 60)        /* Time ESP32 will go to sleep (in seconds) */

RTC_DATA_ATTR int bootCount = 0;
RTC_DATA_ATTR int updatecounter;

uint32_t start, stop;
char buffDate[20];
char buffTime[20];

#ifdef ESP32
  #include <HTTPClient.h>
#endif

// change in "secret.h"
const char* ssid     = SSID;
const char* password = PASSWORD;
const char* serverName = SERVERNAME;
const char* updateServer = UPDATESERVER;

String apiKeyValue = APIKEYVALUE;

String MacAddress = "";

const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 3600;
const int   daylightOffset_sec = 3600;

float adcval = 0;
float adcval2 = 0;

volatile int interruptCounter;
int totalInterruptCounter;

WiFiClient client;
 
hw_timer_t * timer = NULL;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;
 
void IRAM_ATTR onTimer() {
  portENTER_CRITICAL_ISR(&timerMux);
  interruptCounter++;
  portEXIT_CRITICAL_ISR(&timerMux);
}

void getfirmwareupdate() {
  delay(1000);
    updatecounter = 0;
    Serial.println("try to get ota update...");
    t_httpUpdate_return ret = httpUpdate.update(client, updateServer, FIRMWAREVERSION);

    switch (ret) {
      case HTTP_UPDATE_FAILED:
        Serial.printf("HTTP_UPDATE_FAILED Error (%d): %s\n", httpUpdate.getLastError(), httpUpdate.getLastErrorString().c_str());
        break;
      case HTTP_UPDATE_NO_UPDATES:
        Serial.println("HTTP_UPDATE_NO_UPDATES");
        break;
      case HTTP_UPDATE_OK:
        Serial.println("HTTP_UPDATE_OK");
        break;
    }
}

void printLocalTime() {
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return;
  }
  Serial.print("localtime: ");
  Serial.println(&timeinfo, "%d.%m.%Y %H:%M:%S");
  Serial.println();
  strftime(buffDate, 20, "%d.%m.%Y", &timeinfo);
  strftime(buffTime, 20, "%H:%M:%S", &timeinfo);
}

void print_wakeup_reason(){
  esp_sleep_wakeup_cause_t wakeup_reason;
  wakeup_reason = esp_sleep_get_wakeup_cause();
  switch(wakeup_reason)
  {
    case ESP_SLEEP_WAKEUP_EXT0 : Serial.println("Wakeup caused by external signal using RTC_IO"); break;
    case ESP_SLEEP_WAKEUP_EXT1 : Serial.println("Wakeup caused by external signal using RTC_CNTL"); break;
    case ESP_SLEEP_WAKEUP_TIMER : Serial.println("Wakeup caused by timer"); break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD : Serial.println("Wakeup caused by touchpad"); break;
    case ESP_SLEEP_WAKEUP_ULP : Serial.println("Wakeup caused by ULP program"); break;
    default : Serial.printf("Wakeup was not caused by deep sleep: %d\n",wakeup_reason); break;
  }
}

// function to get a device address
byte getDeviceAddress(DeviceAddress deviceAddress)
{
  byte type_s = 0;
  for (uint8_t i = 0; i < 8; i++)
  {
    Serial.print("0x");
    if (deviceAddress[i] < 0x10) Serial.print("0");
    Serial.print(deviceAddress[i], HEX);
    if (i < 7) Serial.print(", ");
  }
  
  // the first ROM byte indicates which chip
  switch (deviceAddress[0]) {
    case 0x10:
      Serial.print("  Chip = DS18S20 (old)");  // old DS1820
      type_s = 1;
      break;
    case 0x28:
      Serial.print("  Chip = DS18B20");
      type_s = 1;
      break;
    case 0x22:
      Serial.print("  Chip = DS1822");
      //type_s = 0;
      break;
    default:
      Serial.print("  Device is currently not supported.");
      return 0;
  }
  Serial.println("");
  return type_s;
}

StaticJsonDocument<200> handleDS18b20Data(DeviceAddress deviceAddress)
{
  float tempC = sensors.getTempC(deviceAddress);
  String sensorAdresse; 
  Serial.print(tempC);
  Serial.print("°C  |  Adress: ");
  for (uint8_t i = 0; i < 8; i++)
  {
    // zero pad the address if necessary
    if (deviceAddress[i] < 16) Serial.print("0");
    Serial.print(deviceAddress[i], HEX);
  }
  Serial.println("");

  char buffer[20];
  sprintf( buffer, "%02X%02X%02X%02X%02X%02X%02X%02X", (int) deviceAddress[0], (int) deviceAddress[1], (int) deviceAddress[2], (int) deviceAddress[3], (int) deviceAddress[4], (int) deviceAddress[5], (int) deviceAddress[6], (int) deviceAddress[7]);

  StaticJsonDocument<200> sensorData;
  sensorData["typid"] = 1;
  sensorData["sensorAddress"] = buffer;
  sensorData["value1"] = String(tempC);
  sensorData["date"] = String(buffDate);
  sensorData["time"] = String(buffTime);

  return sensorData;
}

void initialSetup() {
  Serial.println("Start after reset");
  btStop();

  // Grab a count of devices on the wire
  numberOfDevices = sensors.getDeviceCount();

  // locate devices on the bus
  Serial.print("1. Locating devices...");
  Serial.print("Found ");
  Serial.print(numberOfDevices, DEC);
  Serial.println(" devices.");

  byte addr[8];
  if ( !oneWire.search(addr)) {
    Serial.println("No more addresses.");
    Serial.println();
    delay(250);
    return;
  }

  Serial.println("Printing addresses...");
  for (int i = 0;  i < numberOfDevices;  i++)
  {
    Serial.print("Sensor ");
    Serial.print(i+1);
    Serial.print(" : ");
    sensors.getAddress(sensoradresses[i], i);
    getDeviceAddress(sensoradresses[i]);
  }

  if (OneWire::crc8(addr, 7) != addr[7]) {
      Serial.println("CRC is not valid!");
      return;
  }
}

void setup(void)
{
  Serial.begin(115200);
  sensors.begin();

  esp_sleep_wakeup_cause_t wakeup_cause; // Variable for wakeup cause
  wakeup_cause = esp_sleep_get_wakeup_cause(); // get wakeup cause
  if (wakeup_cause == ESP_SLEEP_WAKEUP_UNDEFINED) initialSetup();     // if wakeup cause = reset
  //Increment boot number and print it every reboot
  ++bootCount;
  Serial.print("Boot number: " + String(bootCount) + ", ");
  //Print the wakeup reason for ESP32
  print_wakeup_reason();
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  
  Serial.println();

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    delay(2000);
    break;
  }

  int i2 = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Try to connect to the Internet...");
    i2++;
    if (i2 >= 5) {
      break;
    }
  }
  Serial.println("Connected to the internet!");

  MacAddress = WiFi.macAddress();

  if (wakeup_cause == ESP_SLEEP_WAKEUP_UNDEFINED) {
    Serial.print("IP Adresse ");       // displays ip of ESP
    Serial.println(WiFi.localIP());
    Serial.print("ESP Board MAC Address:  ");
    Serial.println(WiFi.macAddress());
  }

  // Init and get the time
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  printLocalTime();
}

void loop(void)
{
  // request to all devices on the bus
  sensors.requestTemperatures(); // Send the command to get temperatures
  
  String httpRequestDataBase = "";
  StaticJsonDocument<200> sensor22;
  StaticJsonDocument<600> allSensors;

  // Loop through each device, print out temperature data
  for (uint8_t i = 0; i < numberOfDevices; i++)
  {
    allSensors[i] = handleDS18b20Data(sensoradresses[i]);
  }
 
    //Check WiFi connection status
    start = millis();
    if(WiFi.status()== WL_CONNECTED){
      HTTPClient http;
      // Your Domain name with URL path or IP address with path
      http.begin(serverName);
      // Specify content-type header
      http.addHeader("Content-Type", "application/json");

      StaticJsonDocument<200> board;
      StaticJsonDocument<1024> docpayload;
      // Add values in the document
      board["api_key"] = apiKeyValue;
      board["protocollversion"] = protocollversion;
      board["macaddress"] = MacAddress;
      docpayload["board"] = board;
      docpayload["sensors"] = allSensors;
        
      String requestBody;
      serializeJson(docpayload, requestBody);
        
      int httpResponseCode = http.POST(requestBody);
      Serial.print("httppost via json: ");
      Serial.println(requestBody);
      Serial.print("httpResponseCode: ");
      Serial.println(httpResponseCode);
      if(httpResponseCode > 0){
        String response = http.getString();
        Serial.println(httpResponseCode);
        Serial.println(response);
      } else {
        Serial.print("Error code: ");
        Serial.println(httpResponseCode);
      }

      // Free resources
      http.end();
      stop = millis();
      Serial.print("task duration: ");
      Serial.println( stop - start);
    } else {
      Serial.println("WiFi Disconnected");
    }
  
  // Check if updates are available on server.
  if (updatecounter >= 20) {
    getfirmwareupdate();
  }
  Serial.print("updatecounter: ");
  Serial.println(updatecounter);
  updatecounter++;

  Serial.println("going to sleep");
  Serial.println("----------------");
  esp_deep_sleep_start();
}

