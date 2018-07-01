#ifndef asyncserver_h
#define asyncserver_h

#include "config.h"
#include <pgmspace.h>

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
void onWifiGotIP (const WiFiEventStationModeGotIP& event);
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



#endif




