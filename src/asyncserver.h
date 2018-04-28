#ifndef asyncserver_h
#define asyncserver_h

#include "config.h"
#include <pgmspace.h>



void AsyncWSBegin();
void AsyncWSLoop();
void load_running_text();
void WsSendInfoStatic(int clientID);
void WsSendInfoDynamic(int clientID);
void WsSendRunningLedConfig(int clientID);;
void WsSendSholatStatic(int clientID);
void WsSendSholatDynamic(int clientID);
char* formatBytes(size_t bytes);
bool load_config();
bool load_config_time();
bool load_config_sholat();
bool load_config_runningled();
bool load_config_httpauth();
bool save_config();
void onWiFiConnected(WiFiEventStationModeConnected data);
void onWifiGotIP (const WiFiEventStationModeGotIP& event);
void onWiFiDisconnected(WiFiEventStationModeDisconnected data);
//void handleFileList(AsyncWebServerRequest *request);
void send_network_configuration_values_html(AsyncWebServerRequest *request);
//void send_NTP_configuration_html(AsyncWebServerRequest *request);
void restart_esp(AsyncWebServerRequest *request);
void reset_esp(AsyncWebServerRequest *request);
void send_wwwauth_configuration_values_html(AsyncWebServerRequest *request);
void send_wwwauth_configuration_html(AsyncWebServerRequest *request);
void send_update_firmware_values_html(AsyncWebServerRequest *request);
void setUpdateMD5(AsyncWebServerRequest *request);
void updateFirmware(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final);
void send_connection_state_values_html(AsyncWebServerRequest *request);
void send_information_values_html(AsyncWebServerRequest *request);
void send_NTP_configuration_values_html(AsyncWebServerRequest *request);
void send_sholat_values_html(AsyncWebServerRequest *request);
void set_RTC_TIME_html(AsyncWebServerRequest *request);
String urldecode(String input);
void restart_1();
void reset_1();
bool saveHTTPAuth();

//bool save_system_info();

void EventSendNetworkStatus();
void EventSendTimeStatus();
void EventSendSystemStatus();
void EventSendHeap();

void WsSendNetworkStatus();
void WsSendTimeStatus();
void WsSendSystemStatus();
void WsSendHeap();
void WsSendConfigSholat();


#endif




