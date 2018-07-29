#ifndef asyncserver_h
#define asyncserver_h

// #include <Arduino.h>
#ifdef ESP8266
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <FS.h>
#include <Hash.h>
#include <Ticker.h>
#include <DNSServer.h>
#include <ESP8266SSDP.h>
#include <ESP8266NetBIOS.h>
#include <ESPAsyncTCP.h>
#include <ESP8266SSDP.h>
#include <time.h>
// extern "C" {
// #include "sntp.h"
// }
#elif defined ESP32
#include <WiFi.h>
#include <ESPmDNS.h>
#include "SPIFFS.h"
#include <ESP32Ticker.h>
#include "time.h"
#include "AsyncTCP.h"
#endif

#include <ESPAsyncWebServer.h>

// #include <TimeLib.h>
// #include "sholat.h"
// #include "hub08.h"
// #include "mqtt.h"
#include <Wire.h>
#include <RtcDS3231.h>      //RTC library
#include <StreamString.h>
#include "AsyncJson.h"
#include <ArduinoJson.h>

#include <ArduinoOTA.h>
#include <SPIFFSEditor.h>


#include "progmemmatrix.h"
#include "displayhelper.h"

#include <pgmspace.h>


typedef struct {
  char hostname[32] = "ESP_XXXX";
  char ssid[33] = "your_wifi_ssid";
  char password[65] = "your_wifi_password";
  bool dhcp = true;
  char static_ip[16] = "192.168.10.15";
  char netmask[16] = "255.255.255.0";
  char gateway[16] = "192.168.10.1";
  char dns0[16] = "192.168.10.1";
  char dns1[16] = "8.8.8.8";
} strConfig;
extern strConfig _config; // General and WiFi configuration

typedef struct
{
  const char* ssid = _config.hostname; // ChipID is appended to this name
  char password[10] = ""; // NULL means no password
  bool APenable = false; // AP disabled by default
} strApConfig;
extern strApConfig _configAP; // Static AP config settings

typedef struct {
  bool auth;
  char wwwUsername[32];
  char wwwPassword[32];
} strHTTPAuth;
extern strHTTPAuth _httpAuth;

extern bool wsConnected;
extern bool wifiGotIpFlag;
extern bool wifiDisconnectedFlag;
extern bool stationConnectedFlag;
extern bool stationDisconnectedFlag;

extern bool clientVisitConfigMqttPage;

extern AsyncWebServer server;
extern AsyncWebSocket ws;
extern AsyncEventSource events;

extern uint32_t clientID;

extern FSInfo fs_info;

void AsyncWSBegin();
void AsyncWSLoop();
void load_running_text();
void load_running_text_2();
char* formatBytes(size_t bytes);
bool load_config_location();
bool load_config_network();
bool load_config_time();
bool load_config_sholat();
bool load_config_ledmatrix();
bool load_config_httpauth();
bool save_config_network();
bool save_config_location();
bool save_config_time();
bool save_config_sholat();
bool save_config_ledmatrix();
void onWiFiConnected(WiFiEventStationModeConnected data);
// void onWifiGotIP (const WiFiEventStationModeGotIP& event);
void onWifiGotIP (WiFiEventStationModeGotIP ipInfo);
void onWiFiDisconnected(WiFiEventStationModeDisconnected data);
void onStationConnected(const WiFiEventSoftAPModeStationConnected& evt);
void onStationDisconnected(const WiFiEventSoftAPModeStationDisconnected& evt);
void onProbeRequestPrint(const WiFiEventSoftAPModeProbeRequestReceived& evt);
void onProbeRequestBlink(const WiFiEventSoftAPModeProbeRequestReceived&);
char* macToString(const unsigned char* mac);
void handleFileList(AsyncWebServerRequest *request);
void send_network_configuration_values_html(AsyncWebServerRequest *request);
void restart_esp(AsyncWebServerRequest *request);
void reset_esp(AsyncWebServerRequest *request);
void send_wwwauth_configuration_values_html(AsyncWebServerRequest *request);
void send_wwwauth_configuration_html(AsyncWebServerRequest *request);
void send_update_firmware_values_html(AsyncWebServerRequest *request);
void setUpdateMD5(AsyncWebServerRequest *request);
void updateFirmware(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final);
void send_connection_state_values_html(AsyncWebServerRequest *request);
void send_information_values_html(AsyncWebServerRequest *request);
void set_time_html(AsyncWebServerRequest *request);
void send_config_network(AsyncWebServerRequest *request);
void send_config_location(AsyncWebServerRequest *request);
void send_config_time(AsyncWebServerRequest *request);
void send_config_sholat(AsyncWebServerRequest *request);
void send_config_ledmatrix(AsyncWebServerRequest *request);
void send_config_mqtt(AsyncWebServerRequest *request);
// String urldecode(String input);
void restart_1();
void reset_1();
bool saveHTTPAuth();

void sendNetworkStatus(uint8_t mode);
void sendConfigSholat(uint8_t mode);
void sendSholatSchedule(uint8_t mode);
void sendHeap(uint8_t mode);
void sendDateTime(uint8_t mode);
void sendTimeStatus(uint8_t mode);

bool save_system_info();



#endif




