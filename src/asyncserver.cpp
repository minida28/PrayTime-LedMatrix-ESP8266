//#include <Ticker.h>

#include <ESP8266NetBIOS.h>
//#include <DNSServer.h>
//#include <ESP8266SSDP.h>

//#include <StreamString.h>
//#include <ESP8266WiFi.h>
//#include <ESP8266mDNS.h>
//#include <ArduinoOTA.h>
//#include <FS.h>
//#include <Hash.h>
//#include <ESPAsyncTCP.h>
//#include <ESPAsyncWebServer.h>
//#include <SPIFFSEditor.h>
//#include "AsyncJson.h"
//#include "ArduinoJson.h"
//#include "config.h"

#include "lwip/inet.h"

#include <pgmspace.h>
#include "asyncserver.h"
#include "buzzer.h"

#if defined(ARDUINO) && ARDUINO >= 100
#include "Arduino.h"
#else
#include "WProgram.h"
#endif

#define PRINTPORT Serial
#define DEBUGPORT Serial

// #define RELEASE
//#define RELEASEASYNCWS

#define PROGMEM_T __attribute__((section(".irom.text.template")))

#ifndef RELEASE
#define DEBUGLOG(fmt, ...)                    \
  {                                           \
    static const char pfmt[] PROGMEM_T = fmt; \
    DEBUGPORT.printf_P(pfmt, ##__VA_ARGS__);  \
  }
#else
#define DEBUGLOG(...)
#endif

#ifndef RELEASEASYNCWS
#define DEBUGASYNCWS(fmt, ...)                \
  {                                           \
    static const char pfmt[] PROGMEM_T = fmt; \
    DEBUGPORT.printf_P(pfmt, ##__VA_ARGS__);  \
  }
#else
#define DEBUGASYNCWS(...)
#endif

#define AP_ENABLE_BUTTON 4 // Button pin to enable AP during startup for configuration. -1 to disable
//#define CONFIG_FILE "/config.json"
#define USER_CONFIG_FILE "/userconfig.json"
#define GENERIC_CONFIG_FILE "/genericconfig.json"
#define SECRET_FILE "/secret.json"
#define RUNNING_TEXT_FILE "/runningtext.txt"
#define RUNNING_TEXT_FILE_2 "/runningtext_2.txt"
//#define CONFIG_FILE_MQTT "/configMqtt.json"

Ticker _secondTk;
Ticker restartESP;

//FS* _fs;
unsigned long wifiDisconnectedSince = 0;
String _browserMD5 = "";
uint32_t _updateSize = 0;

bool eventsourceTriggered = false;
bool wsConnected = false;
bool configNetworkUpdatedFlag = false;

WiFiEventHandler onStationModeConnectedHandler, onStationModeGotIPHandler, onStationModeDisconnectedHandler;
WiFiEventHandler stationConnectedHandler;
WiFiEventHandler stationDisconnectedHandler;
WiFiEventHandler probeRequestPrintHandler;
WiFiEventHandler probeRequestBlinkHandler;

bool wifiGotIpFlag = false;
bool wifiDisconnectedFlag = false;
bool stationConnectedFlag = false;
bool stationDisconnectedFlag = false;

bool firmwareUpdated = false;

// SKETCH BEGIN

FSInfo fs_info;

DNSServer dnsServer;

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
AsyncEventSource events("/events");

uint32_t clientID;

bool clientVisitConfigRunningLedPage = false;
bool clientVisitConfigSholatPage = false;
bool clientVisitConfigMqttPage = false;

bool clientVisitStatusNetworkPage = false;
bool clientVisitStatusTimePage = false;
bool clientVisitStatusSystemPage = false;
bool clientVisitSetTimePage = false;

bool clientVisitInfoPage = false;
bool clientVisitSholatTimePage = false;

void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len)
{
  if (type == WS_EVT_CONNECT)
  {
    DEBUGASYNCWS("ws[%s][%u] connect\r\n", server->url(), client->id());
    client->printf("Hello Client %u :)", client->id());
    client->ping();
    wsConnected = true;
    //wsSendTimeData();
    clientID = client->id();

    client->status();

    // reset state
    clientVisitConfigRunningLedPage = false;
    clientVisitConfigSholatPage = false;
    clientVisitConfigMqttPage = false;
    clientVisitStatusNetworkPage = false;
    clientVisitStatusTimePage = false;
    clientVisitStatusSystemPage = false;
    clientVisitInfoPage = false;
    clientVisitSholatTimePage = false;

    static uint32_t clientID_old = 0;
    if (clientID != clientID_old && clientID != 0)
    {
      DEBUGASYNCWS("New client is connected, ID  %d\r\n", clientID);

      // if old client exist, instruct old client to 'kill' its connection :)
      if (ws.hasClient(clientID_old))
      {
        DEBUGASYNCWS("Disconnect old client, ID %d\r\n", clientID_old);
        // ws.text(clientID_old, FPSTR(F("suicide")));

        //client->text(PSTR("text"));
        //const char flash_text[] PROGMEM = "Text to send";
        //client->text(FPSTR(flash_text));
        //const_cast<char *>(_config.deviceName.c_str());
        //const char flash_text[] PROGMEM = "Text to send";
        //ws.close(clientID_old, 1013, const_cast<char *>PSTR("text"));

        //String text = pgm_txt_serverloseconnection;
        //ws.close(clientID_old, 4000, const_cast<char *>(text.c_str()));

        int len = strlen_P(pgm_txt_serverloseconnection);
        char text[len];
        strcpy_P(text, pgm_txt_serverloseconnection);
        text[len] = '\0';
        ws.close(clientID_old, 4000, text);
      }

      clientID_old = clientID;
    }
    //zulu = client;
    //request->client()->remoteIP();
    //Serial.println(zulu->remoteIP());
    //Serial.println(client->request->url());
    //Serial.println(client->request->url().c_str());
    //AsyncWebHeader* h = request->getHeader(i);
    //AsyncWebServerRequest *request;
    //Serial.println(request->url().c_str());
  }
  else if (type == WS_EVT_DISCONNECT)
  {
    DEBUGASYNCWS("ws[%s][%u] disconnect: [%u\r\n", server->url(), client->id(), client->id());
    //_secondTk.detach();
    //eventsourceTriggered = false;
    wsConnected = false;
    //clientID = 0;
  }
  else if (type == WS_EVT_ERROR)
  {
    DEBUGASYNCWS("ws[%s][%u] error(%u): %s\r\n", server->url(), client->id(), *((uint16_t *)arg), (char *)data);
  }
  else if (type == WS_EVT_PONG)
  {
    DEBUGASYNCWS("ws[%s][%u] pong[%u]: %s\r\n", server->url(), client->id(), len, (len) ? (char *)data : "");
  }
  else if (type == WS_EVT_DATA)
  {
    AwsFrameInfo *info = (AwsFrameInfo *)arg;
    char msg[len + 1];
    msg[0] = '\0';
    if (info->final && info->index == 0 && info->len == len)
    {
      //the whole message is in a single frame and we got all of it's data
      DEBUGLOG("ws[%s][%u] %s-message[%lu]: ", server->url(), client->id(), (info->opcode == WS_TEXT) ? "text" : "binary", info->len);

      if (info->opcode == WS_TEXT)
      {
        client->text(FPSTR(pgm_txt_gotyourtextmessage));
      }
      else
      {
        client->binary(FPSTR(pgm_txt_gotyourbinarymessage));
      }

      if (info->opcode == WS_TEXT)
      {
        // check if start with '{' i.e. data in JSON format
        if (data[0] == '{')
        {
          DynamicJsonBuffer jsonBuffer;
          JsonObject &root = jsonBuffer.parseObject(data);
          if (root.success())
          {
            //DEBUGLOG("JSON received: ");
            root.printTo(PRINTPORT);
            PRINT("\r\n");

            // check if JSON has certain key/parameter 'type'
            if (root[FPSTR(pgm_url)].success())
            {

              const char *url = root[FPSTR(pgm_url)];
              // int len = strlen(url);

              if (strcmp_P(url, pgm_statuspagenetwork) == 0)
              {
                clientVisitStatusNetworkPage = true;

                //  WsSendInfoStatic(clientID);
                //  WsSendInfoDynamic(clientID);
                //  //WsSendSholatStatic(clientID);
                //  //WsSendSholatDynamic(clientID);
                //  //WsSendRunningLedConfig(clientID);

                WsSendNetworkStatus();
              }
              else if (strcmp_P(url, pgm_statuspagetime) == 0)
              {
                clientVisitStatusTimePage = true;

                //  WsSendInfoStatic(clientID);
                //  WsSendInfoDynamic(clientID);
                //  //WsSendSholatStatic(clientID);
                //  //WsSendSholatDynamic(clientID);
                //  //WsSendRunningLedConfig(clientID);

                WsSendTimeStatus();
              }
              else if (strcmp_P(url, pgm_statuspagesystem) == 0)
              {
                clientVisitStatusSystemPage = true;

                //WsSendSystemStatus();

                //  WsSendInfoStatic(clientID);
                //  WsSendInfoDynamic(clientID);
                //  //WsSendSholatStatic(clientID);
                //  //WsSendSholatDynamic(clientID);
                //  //WsSendRunningLedConfig(clientID);
              }
              else if (strcmp_P(url, pgm_configpagesholat) == 0)
              {
                clientVisitConfigSholatPage = true;

                // WsSendConfigSholat();
              }
              else if (strcmp_P(url, pgm_configpageledmatrix) == 0)
              {
                clientVisitConfigRunningLedPage = true;

                //WsSendInfoStatic(clientID);
                //WsSendInfoDynamic(clientID);
                //WsSendSholatStatic(clientID);
                //WsSendSholatDynamic(clientID);
                //WsSendRunningLedConfig(clientID);
              }
              else if (strcmp_P(url, pgm_configpagemqtt) == 0)
              {
                clientVisitConfigMqttPage = true;

                //WsSendInfoStatic(clientID);
                //WsSendInfoDynamic(clientID);
                //WsSendSholatStatic(clientID);
                //WsSendSholatDynamic(clientID);
                //WsSendRunningLedConfig(clientID);
              }
              else if (strcmp_P(url, pgm_schedulepagesholat) == 0)
              {
                clientVisitSholatTimePage = true;

                //WsSendInfoStatic(clientID);
                //WsSendInfoDynamic(clientID);
                //WsSendSholatStatic(clientID);
                //WsSendSholatDynamic(clientID);
                //WsSendRunningLedConfig(clientID);
              }
              else if (strcmp_P(url, pgm_settimepage) == 0)
              {
                clientVisitSetTimePage = true;

                //WsSendInfoStatic(clientID);
                //WsSendInfoDynamic(clientID);
                //WsSendSholatStatic(clientID);
                //WsSendSholatDynamic(clientID);
                //WsSendRunningLedConfig(clientID);
              }
            }

            //***********************
            // handle long text JSON
            //***********************
            if (root[FPSTR(pgm_longtext)].success())
            {
              const char *longtext = root[FPSTR(pgm_longtext)];

              File file = SPIFFS.open(RUNNING_TEXT_FILE, "w");

              if (!file)
              {
                DEBUGASYNCWS("Failed to open MQTT_PUBSUB config file\r\n");
                file.close();
                return;
              }

              for (uint16_t j = 0; j < strlen(longtext); j++)
              {
                file.write(longtext[j]);
              }
              file.close();
              tone1 = HIGH;
            }

            //******************************
            // handle SAVE CONFIG (not fast)
            //******************************
            if (root[FPSTR(pgm_saveconfig)].success())
            {

              const char *saveconfig = root[FPSTR(pgm_saveconfig)];
              //int len = root[FPSTR(pgm_saveconfig)].measureLength();

              //remove json key before saving
              root.remove(FPSTR(pgm_saveconfig));

              //******************************
              // save LOCATION config
              //******************************
              if (strcmp_P(saveconfig, pgm_configpagelocation) == 0)
              {
                File file = SPIFFS.open(FPSTR(pgm_configfilelocation), "w");

                if (!file)
                {
                  DEBUGASYNCWS("Failed to open LOCATION config file\r\n");
                  file.close();
                  return;
                }

                root.prettyPrintTo(file);
                file.flush();
                file.close();

                load_config_location();
                process_sholat();

                //beep
                tone1 = HIGH;
              }
              //******************************
              // save NETWORK config
              //******************************
              else if (strcmp_P(saveconfig, pgm_configpagenetwork) == 0)
              {
                File file = SPIFFS.open(FPSTR(pgm_configfilenetwork), "w");

                if (!file)
                {
                  DEBUGASYNCWS("Failed to open NETWORK config file\r\n");
                  file.close();
                  return;
                }

                root.prettyPrintTo(file);
                file.flush();
                file.close();

                load_config_network();
                configNetworkUpdatedFlag = true;

                //beep
                tone1 = HIGH;
              }
              //******************************
              // save TIME config
              //******************************
              else if (strcmp_P(saveconfig, pgm_configpagetime) == 0)
              {
                File file = SPIFFS.open(FPSTR(pgm_configfiletime), "w");

                if (!file)
                {
                  DEBUGASYNCWS("Failed to open TIME config file\r\n");
                  file.close();
                  return;
                }

                root.prettyPrintTo(file);
                file.flush();
                file.close();

                load_config_time();
                process_sholat();

                //beep
                tone1 = HIGH;
              }
              //******************************
              // save SHOLAT config
              //******************************
              else if (strcmp_P(saveconfig, pgm_configpagesholat) == 0)
              {
                File file = SPIFFS.open(FPSTR(pgm_configfilesholat), "w");

                if (!file)
                {
                  DEBUGASYNCWS("Failed to open SHOLAT config file\r\n");
                  file.close();
                  return;
                }

                root.prettyPrintTo(file);
                file.flush();
                file.close();

                load_config_sholat();
                process_sholat();

                //beep
                tone1 = HIGH;
              }
              //******************************
              // save LED MATRIX config
              //******************************
              else if (strcmp_P(saveconfig, pgm_configpageledmatrix) == 0)
              {
                File file = SPIFFS.open(FPSTR(pgm_configfileledmatrix), "w");

                if (!file)
                {
                  DEBUGASYNCWS("Failed to open LED MATRIX config file\r\n");
                  file.close();
                  return;
                }

                root.prettyPrintTo(file);
                file.flush();
                file.close();

                load_config_ledmatrix();

                //beep
                tone1 = HIGH;
              }
              //******************************
              // save Mqtt config
              //******************************
              else if (strcmp_P(saveconfig, pgm_configpagemqtt) == 0)
              {
                File file = SPIFFS.open(FPSTR(pgm_configfilemqtt), "w");

                if (!file)
                {
                  DEBUGASYNCWS("Failed to open MQTT config file\r\n");
                  file.close();
                  return;
                }

                root.prettyPrintTo(file);
                file.flush();
                file.close();

                load_config_mqtt();

                //beep
                tone1 = HIGH;
              }

              //******************************
              // save MqttPubSub config
              //******************************
              else if (strcmp_P(saveconfig, pgm_configpagemqttpubsub) == 0)
              {
                File file = SPIFFS.open(FPSTR(pgm_configfilemqttpubsub), "w");

                if (!file)
                {
                  DEBUGASYNCWS("Failed to open MQTT_PUBSUB config file\r\n");
                  file.close();
                  return;
                }

                root.prettyPrintTo(file);
                file.flush();
                file.close();

                //beep
                tone1 = HIGH;
              }

              return;
            }

            //saveFastConfig; for led matrix settings page
            if (root[FPSTR(pgm_type)].success())
            {

              const char *type = root[FPSTR(pgm_type)];
              // int len = strlen(type);

              if (strcmp_P(type, pgm_matrixConfig) == 0)
              {

                const char *param = root[FPSTR(pgm_param)];
                const char *value = root[FPSTR(pgm_value)];
                // const char* text = root[FPSTR(pgm_text)];

                //int len = strlen(param);
                //int lenValue = strlen(value);

                if (strcmp_P(param, pgm_operatingmode) == 0)
                {
                  if (atoi(value) == 0)
                  {
                    _ledMatrixSettings.operatingmode = Normal;
                  }
                  else if (atoi(value) == 1)
                  {
                    _ledMatrixSettings.operatingmode = Config;
                  }
                  else if (atoi(value) == 2)
                  {
                    _ledMatrixSettings.operatingmode = Edit;
                  }
                  MODE = _ledMatrixSettings.operatingmode;
                }
                if (strcmp_P(param, pgm_pagemode0) == 0)
                {
                  _ledMatrixSettings.pagemode0 = atoi(value);
                  currentPageMode0 = _ledMatrixSettings.pagemode0;
                }
                if (strcmp_P(param, pgm_pagemode1) == 0)
                {
                  _ledMatrixSettings.pagemode1 = atoi(value);
                  currentPageMode1 = _ledMatrixSettings.pagemode1;
                }
                if (strcmp_P(param, pgm_pagemode2) == 0)
                {
                  _ledMatrixSettings.pagemode2 = atoi(value);
                  currentPageMode2 = _ledMatrixSettings.pagemode2;
                }
                if (strcmp_P(param, pgm_pagemode) == 0)
                {
                  if (strcmp_P(value, pgm_automatic) == 0)
                  {
                    _ledMatrixSettings.pagemode = Automatic;
                  }
                  if (strcmp_P(value, pgm_manual) == 0)
                  {
                    _ledMatrixSettings.pagemode = Manual;
                  }
                }
                if (strcmp_P(param, pgm_scrollrow_0) == 0)
                {
                  if (strcmp_P(value, pgm_true) == 0)
                  {
                    _ledMatrixSettings.scrollrow_0 = 1;
                  }
                  else
                  {
                    _ledMatrixSettings.scrollrow_0 = 0;
                  }
                }
                if (strcmp_P(param, pgm_scrollrow_1) == 0)
                {
                  if (strcmp_P(value, pgm_true) == 0)
                  {
                    _ledMatrixSettings.scrollrow_1 = 1;
                  }
                  else
                  {
                    _ledMatrixSettings.scrollrow_1 = 0;
                  }
                }
                if (strcmp_P(param, pgm_scrollspeedslider) == 0)
                {
                  _ledMatrixSettings.scrollspeed = atoi(value);
                }
                if (strcmp_P(param, pgm_brightnessslider) == 0)
                {
                  //_ledMatrixSettings.brightness = value.toInt();
                  _ledMatrixSettings.brightness = atoi(value);
                }
                if (strcmp_P(param, pgm_longtext) == 0)
                {
                  File runningTextFile = SPIFFS.open(RUNNING_TEXT_FILE, "w");
                  for (uint16_t j = 0; j < strlen(value); j++)
                  {
                    runningTextFile.write(value[j]);
                  }
                  runningTextFile.close();
                  tone1 = HIGH;
                }
                if (strcmp_P(param, pgm_btntesttone0) == 0)
                {
                  tone0 = HIGH;
                }
                if (strcmp_P(param, pgm_btntesttone1) == 0)
                {
                  tone1 = HIGH;
                }
                if (strcmp_P(param, pgm_btntesttone2) == 0)
                {
                  alarmState = HIGH;
                }
                if (strcmp_P(param, pgm_btntesttone10) == 0)
                {
                  tone10 = HIGH;
                }
                if (strcmp_P(param, pgm_btnsaveconfig) == 0)
                {
                  // config_save_runnningled();
                  tone1 = HIGH;
                }
              }
            }
          }
          else
          {
            // Parsing failed :-(
            DEBUGLOG("Parsing failed, not a JSON object or wrong JSON format.\r\n");
          }
        }
        else if (data[0] == 't' && data[1] == ' ')
        {
          char *token = strtok((char *)&data[2], " ");
          time_t utcTimestamp = (unsigned long)strtol(token, '\0', 10);

          RtcDateTime timeToSetToRTC;
          timeToSetToRTC.InitWithEpoch32Time(utcTimestamp);

          Rtc.SetDateTime(timeToSetToRTC);

          lastSyncRTC = utcTimestamp;

          setTime(utcTimestamp);
          _lastSyncd = utcTimestamp;

          DEBUGLOG("timeSync ");
        }
        else if (data[0] == 'm' && data[1] == ' ')
        {
          char *token = strtok((char *)&data[2], " ");
          time_t utcTimestamp = (unsigned long)strtol(token, '\0', 10);

          RtcDateTime timeToSetToRTC;
          timeToSetToRTC.InitWithEpoch32Time(utcTimestamp);

          Rtc.SetDateTime(timeToSetToRTC);

          lastSyncRTC = utcTimestamp;

          setTime(utcTimestamp);
          _lastSyncd = utcTimestamp;

          DEBUGLOG("timeSync ");
        }
        for (size_t i = 0; i < info->len; i++)
        {
          //msg += (char) data[i];
          char _data[2];
          _data[0] = (char)data[i];
          _data[1] = '\0';
          if (i == 0)
          {
            strcpy(msg, _data);
          }
          else
          {
            strcat(msg, _data);
          }
        }
      }
      else
      {
        char buff[3];
        for (size_t i = 0; i < info->len; i++)
        {
          sprintf(buff, "%02x ", (uint8_t)data[i]);
          //msg += buff ;
          if (i == 0)
          {
            strcpy(msg, buff);
          }
          else
          {
            strcat(msg, buff);
          }
        }
      }
      msg[len + 1] = '\0';
      DEBUGASYNCWS("%s\r\n", msg);
    }
    else
    {
      //message is comprised of multiple frames or the frame is split into multiple packets
      if (info->index == 0)
      {
        if (info->num == 0)
          DEBUGASYNCWS("ws[%s][%u] %s-message start\r\n", server->url(), client->id(), (info->message_opcode == WS_TEXT) ? "text" : "binary");
        DEBUGASYNCWS("ws[%s][%u] frame[%u] start[%llu]\r\n", server->url(), client->id(), info->num, info->len);
      }

      DEBUGASYNCWS("ws[%s][%u] frame[%u] %s[%llu - %llu]: ", server->url(), client->id(), info->num, (info->message_opcode == WS_TEXT) ? "text" : "binary", info->index, info->index + len);

      if (info->opcode == WS_TEXT)
      {
        for (size_t i = 0; i < info->len; i++)
        {
          //msg += (char) data[i];
          //strcat(msg, const_cast<char*>((char)data[i]));
          char _data[2];
          _data[0] = (char)data[i];
          _data[1] = '\0';
          if (i == 0)
          {
            strcpy(msg, _data);
          }
          else
          {
            strcat(msg, _data);
          }
        }
      }
      else
      {
        char buff[3];
        for (size_t i = 0; i < info->len; i++)
        {
          sprintf(buff, "%02x ", (uint8_t)data[i]);
          //msg += buff ;
          if (i == 0)
          {
            strcpy(msg, buff);
          }
          else
          {
            strcat(msg, buff);
          }
        }
      }
      msg[len + 1] = '\0';
      DEBUGASYNCWS("%s\r\n", msg);

      if ((info->index + len) == info->len)
      {
        DEBUGASYNCWS("ws[%s][%u] frame[%u] end[%llu]\r\n", server->url(), client->id(), info->num, info->len);
        if (info->final)
        {
          DEBUGASYNCWS("ws[%s][%u] %s-message end\r\n", server->url(), client->id(), (info->message_opcode == WS_TEXT) ? "text" : "binary");
          if (info->message_opcode == WS_TEXT)
            client->text(pgm_txt_gotyourtextmessage);
          else
            client->binary(pgm_txt_gotyourbinarymessage);
        }
      }
    }
  }
}

void AsyncWSBegin()
{
  DEBUGASYNCWS("Async WebServer Init...\r\n");

  int reading = analogRead(A0);
  DEBUGASYNCWS("Analog Read: %d\r\n", reading);
  if (reading >= 768)
  {                            // analog read 3 volts or more
    _configAP.APenable = true; // Read AP button. If button is pressed, activate AP
    DEBUGASYNCWS("AP Enable = %d\r\n", _configAP.APenable);
  }

  //  if (!_fs) // If SPIFFS is not started
  //    _fs->begin();

  // SPIFFS.format();

  SPIFFS.begin();

  SPIFFS.info(fs_info);

  PRINT("totalBytes: %u\r\n", fs_info.totalBytes);
  PRINT("usedBytes: %u\r\n", fs_info.totalBytes);
  PRINT("blockSize: %u\r\n", fs_info.totalBytes);
  PRINT("pageSize: %u\r\n", fs_info.totalBytes);
  PRINT("maxOpenFiles: %u\r\n", fs_info.totalBytes);
  PRINT("maxPathLength: %u\r\n", fs_info.totalBytes);

  // start update firmware
  // Dir dir = SPIFFS.openDir("/");

  // pinMode(LED_BUILTIN, OUTPUT);
  // digitalWrite(LED_BUILTIN, LOW);

  if (SPIFFS.exists("/firmware.bin"))
  {
    File file = SPIFFS.open("/firmware.bin", "r");
    if (!file)
    {
      PRINT("Failed to open FIRMWARE file\r\n");
      file.close();
      return;
    }
    uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
    if (!Update.begin(maxSketchSpace, U_FLASH))
    { //start with max available size
      Update.printError(Serial);
      Serial.println("ERROR");
      file.close();
      return;
    }
    while (file.available())
    {
      uint8_t ibuffer[128];
      file.read((uint8_t *)ibuffer, 128);
      Serial.println((char *)ibuffer);
      Update.write(ibuffer, sizeof(ibuffer));
      // Serial.print("#");
    }
    Serial.print(Update.end(true));
    // digitalWrite(LED_BUILTIN, HIGH);
    file.close();
    SPIFFS.remove("/firmware.bin");
  }
  else
  {
    PRINT("Path to FIRMWARE file not exist.\r\n");
  }
  // end update firmware

#ifndef RELEASE
  { // List files
    //Dir dir = _fs->openDir("/");
    Dir dir = SPIFFS.openDir("/");
    while (dir.next())
    {
      String fileName = dir.fileName();
      size_t fileSize = dir.fileSize();

      PRINT("FS File: %s, size: %s\r\n", fileName.c_str(), formatBytes(fileSize));
    }
    PRINT("\r\n");
  }
#endif // RELEASE

  //save_config();

  //Set the host name
  char bufPrefix[] = "SHOLAT_";
  char bufChipId[11];
  itoa(ESP.getChipId(), bufChipId, 10);

  //char bufHostName[32];
  strlcpy(_config.hostname, bufPrefix, sizeof(_config.hostname));
  strncat(_config.hostname, bufChipId, sizeof(bufChipId));

  if (!load_config_network())
  { // Try to load configuration from file system
    //defaultConfig(); // Load defaults if any error

    save_config_network();

    _configAP.APenable = true;
  }

  if (!load_config_location())
  {
    //create file if file not found
    save_config_location();
  }

  if (!load_config_time())
  {
    //create file if file not found
    save_config_time();
  }

  if (!load_config_sholat())
  {
    //create file if file not found
    save_config_sholat();
  }

  if (!load_config_ledmatrix())
  {
    //create file if file not found
    save_config_ledmatrix();
  }

  if (!load_config_httpauth())
  {
    //create file if file not found
  }

  //  loadHTTPAuth();

  // Register wifi Event to control connection LED
  onStationModeConnectedHandler = WiFi.onStationModeConnected([](WiFiEventStationModeConnected data) {
    onWiFiConnected(data);
  });
  onStationModeGotIPHandler = WiFi.onStationModeGotIP([](WiFiEventStationModeGotIP data) {
    onWifiGotIP(data);
  });
  // onStationModeGotIPHandler = WiFi.onStationModeGotIP(onWifiGotIP);
  onStationModeDisconnectedHandler = WiFi.onStationModeDisconnected([](WiFiEventStationModeDisconnected data) {
    onWiFiDisconnected(data);
  });

  // Register event handlers.
  // Callback functions will be called as long as these handler objects exist.
  // Call "onStationConnected" each time a station connects
  stationConnectedHandler = WiFi.onSoftAPModeStationConnected(&onStationConnected);
  // Call "onStationDisconnected" each time a station disconnects
  stationDisconnectedHandler = WiFi.onSoftAPModeStationDisconnected(&onStationDisconnected);
  // Call "onProbeRequestPrint" and "onProbeRequestBlink" each time
  // a probe request is received.
  // Former will print MAC address of the station and RSSI to Serial,
  // latter will blink an LED.
  probeRequestPrintHandler = WiFi.onSoftAPModeProbeRequestReceived(&onProbeRequestPrint);
  probeRequestBlinkHandler = WiFi.onSoftAPModeProbeRequestReceived(&onProbeRequestBlink);

  // _configAP.APenable = true;

  // WiFi.persistent(true);
  // WiFi.mode(WIFI_OFF);

  bool autoConnect = true;
  WiFi.setAutoConnect(autoConnect);

  bool autoReconnect = true;
  WiFi.setAutoReconnect(autoReconnect);

  // WiFi.hostname(_config.hostname);

  if (_configAP.APenable || strcmp(_config.ssid, "") == 0)
  {
    PRINT("Starting wifi in WIFI_AP mode.\r\n");
    WiFi.mode(WIFI_OFF);
    // WiFi.mode(WIFI_AP); // bikin error
    WiFi.softAP(_configAP.ssid, _configAP.password);
    WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
  }
  else if (!_configAP.APenable)
  {
    if (!_config.dhcp)
    {
      PRINT("DHCP disabled, starting wifi in WIFI_STA mode with static IP.\r\n");
      WiFi.mode(WIFI_OFF);
      // WiFi.mode(WIFI_STA);

      IPAddress static_ip;
      IPAddress gateway;
      IPAddress netmask;
      IPAddress dns0;
      IPAddress dns1;

      static_ip.fromString(_config.static_ip);
      gateway.fromString(_config.gateway);
      netmask.fromString(_config.netmask);
      dns0.fromString(_config.dns0);
      dns1.fromString(_config.dns1);

      WiFi.config(static_ip, gateway, netmask, dns0, dns1);
      WiFi.hostname(_config.hostname);
      WiFi.begin(_config.ssid, _config.password);
      WiFi.waitForConnectResult();
    }
    else
    {
      WiFi.hostname(_config.hostname);
      WiFi.begin(_config.ssid, _config.password);
      WiFi.waitForConnectResult();
    }
  }

  dnsServer.start(53, "*", WiFi.softAPIP());

  MDNS.addService("http", "tcp", 80);

  ArduinoOTA.setHostname(_config.hostname);
  ArduinoOTA.begin();

  NBNS.begin(_config.hostname);

  SSDP.setSchemaURL(FPSTR(pgm_descriptionxml));
  SSDP.setHTTPPort(80);
  SSDP.setDeviceType(FPSTR(pgm_upnprootdevice));
  //  SSDP.setModelName(_config.deviceName.c_str());
  //  SSDP.setModelNumber(FPSTR(modelNumber));
  SSDP.begin();

  //SPIFFS.begin();

  ws.onEvent(onWsEvent);

  server.addHandler(&ws);

  events.onConnect([](AsyncEventSourceClient *client) {
    //client->send("hello!", NULL, millis(), 1000);
    if (client->lastId())
    {
      DEBUGLOG("Client reconnected! Last message ID that it gat is: %u\r\n", client->lastId());
    }
    //send event with message "hello!", id current millis
    // and set reconnect delay to 1 second
    //client->send("hello!", NULL, millis(), 1000);
    client->send("hello!");
    //EVENTS_send_information_values_html(client);

    if (clientVisitStatusSystemPage)
    {
      //EventSendSystemStatus();
    }

    // eventsourceTriggered = true;

    //_secondTk.attach(1.0f, s_secondTick, static_cast<void*>(client)); // Task to run periodic things every second
    //SendTimeData();
    //SendSholatTime();
  });
  server.addHandler(&events);

  //  sholattimeevent.onConnect([](AsyncEventSourceClient * client) {
  //    //client->send("hello!", NULL, millis(), 1000);
  //    //_secondTk.attach(1.0f, s_secondTick, static_cast<void*>(client)); // Task to run periodic things every second
  //  });
  //  server.addHandler(&sholattimeevent);

  server.addHandler(new SPIFFSEditor());

  server.on("/list", HTTP_GET, [](AsyncWebServerRequest *request) {
    DEBUGASYNCWS("%s\r\n", __PRETTY_FUNCTION__);
    handleFileList(request);
  });

  server.on("/description.xml", HTTP_GET, [](AsyncWebServerRequest *request) {
    DEBUGASYNCWS("%s\r\n", __PRETTY_FUNCTION__);

    File configFile = SPIFFS.open(DESCRIPTION_XML_FILE, "r");
    if (!configFile)
    {
      PRINT("Failed to open config file\r\n");
      return;
    }

    size_t size = configFile.size();
    PRINT("DESCRIPTION_XML_FILE file size: %d bytes\r\n", size);
    if (size > 1024)
    {
      PRINT("WARNING, file size maybe too large\r\n");
    }

    // Allocate a buffer to store contents of the file
    char buf[size];

    //copy file to buffer
    configFile.readBytes(buf, size);

    //add termination character at the end
    buf[size] = '\0';

    //close the file, save your memory, keep healthy :-)
    configFile.close();

    PRINT("%s\r\n", buf);

    StreamString output;

    if (output.reserve(1024))
    {
      //convert IP address to char array
      size_t len = strlen(WiFi.localIP().toString().c_str());
      char URLBase[len + 1];
      strlcpy(URLBase, WiFi.localIP().toString().c_str(), sizeof(URLBase));

      const char *friendlyName = _config.hostname;
      char presentationURL[] = "index.htm";
      uint32_t serialNumber = ESP.getChipId();
      char modelName[] = "LS-01";
      const char *modelNumber = _config.hostname;
      //output.printf(ssdpTemplate,
      output.printf(buf,
                    URLBase,
                    friendlyName,
                    presentationURL,
                    serialNumber,
                    modelName,
                    modelNumber, //modelNumber
                    (uint8_t)((serialNumber >> 16) & 0xff),
                    (uint8_t)((serialNumber >> 8) & 0xff),
                    (uint8_t)serialNumber & 0xff);
      request->send(200, "text/xml", output);
    }
    else
    {
      request->send(500);
    }
  });

  server.serveStatic("/", SPIFFS, "/").setDefaultFile("index.htm");

  server.onNotFound([](AsyncWebServerRequest *request) {
    DEBUGLOG("NOT_FOUND: ");
    if (request->method() == HTTP_GET)
    {
      DEBUGLOG("GET");
    }
    else if (request->method() == HTTP_POST)
    {
      DEBUGLOG("POST");
    }
    else if (request->method() == HTTP_DELETE)
    {
      DEBUGLOG("DELETE");
    }
    else if (request->method() == HTTP_PUT)
    {
      DEBUGLOG("PUT");
    }
    else if (request->method() == HTTP_PATCH)
    {
      DEBUGLOG("PATCH");
    }
    else if (request->method() == HTTP_HEAD)
    {
      DEBUGLOG("HEAD");
    }
    else if (request->method() == HTTP_OPTIONS)
    {
      DEBUGLOG("OPTIONS");
    }
    else
    {
      DEBUGLOG("UNKNOWN");
    }
    DEBUGLOG("http://%s%s\r\n", request->host().c_str(), request->url().c_str());

    if (request->contentLength())
    {
      DEBUGLOG("_CONTENT_TYPE: %s\r\n", request->contentType().c_str());
      DEBUGLOG("_CONTENT_LENGTH: %u\r\n", request->contentLength());
    }

    int headers = request->headers();
    int i;
    for (i = 0; i < headers; i++)
    {
      AsyncWebHeader *h = request->getHeader(i);
      DEBUGASYNCWS("_HEADER[%s]: %s\r\n", h->name().c_str(), h->value().c_str());
    }

    int params = request->params();
    for (i = 0; i < params; i++)
    {
      AsyncWebParameter *p = request->getParam(i);
      if (p->isFile())
      {
        DEBUGLOG("_FILE[%s]: %s, size: %u\r\n", p->name().c_str(), p->value().c_str(), p->size());
      }
      else if (p->isPost())
      {
        DEBUGLOG("_POST[%s]: %s\r\n", p->name().c_str(), p->value().c_str());
      }
      else
      {
        DEBUGLOG("_GET[%s]: %s\r\n", p->name().c_str(), p->value().c_str());
      }
    }

    request->send(404);
  });
  server.onFileUpload([](AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data, size_t len, bool final) {
    if (!index)
      DEBUGLOG("UploadStart: %s\r\n", filename.c_str());
    DEBUGLOG("%s", (const char *)data);
    if (final)
      DEBUGLOG("UploadEnd: %s (%u)\r\n", filename.c_str(), index + len);
  });
  server.onRequestBody([](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    if (!index)
      DEBUGLOG("BodyStart: %u\r\n", total);
    DEBUGLOG("%s", (const char *)data);
    if (index + len == total)
      DEBUGLOG("BodyEnd: %u\r\n", total);
  });

  server.on("/scan", HTTP_GET, [](AsyncWebServerRequest *request) {
    DEBUGASYNCWS("%s\r\n", request->url().c_str());

    AsyncResponseStream *response = request->beginResponseStream("application/json");
    DynamicJsonBuffer jsonBuffer;
    JsonArray &root = jsonBuffer.createArray();

    int numberOfNetworks = WiFi.scanComplete();
    if (numberOfNetworks == -2)
    {                          //this may also works: WiFi.scanComplete() == WIFI_SCAN_FAILED
      WiFi.scanNetworks(true); //async enabled
    }
    else if (numberOfNetworks)
    {
      for (int i = 0; i < numberOfNetworks; ++i)
      {
        JsonObject &wifi = root.createNestedObject();
        wifi["ssid"] = WiFi.SSID(i);
        wifi["rssi"] = WiFi.RSSI(i);
        wifi["bssid"] = WiFi.BSSIDstr(i);
        wifi["channel"] = WiFi.channel(i);
        wifi["secure"] = WiFi.encryptionType(i);
        wifi["hidden"] = WiFi.isHidden(i) ? true : false;
      }
      WiFi.scanDelete();
      if (WiFi.scanComplete() == -2)
      { //this may also works: WiFi.scanComplete() == WIFI_SCAN_FAILED
        WiFi.scanNetworks(true);
      }
    }
    root.printTo(*response);
    request->send(response);
  });

  server.on("/admin/restart", [](AsyncWebServerRequest *request) {
    DEBUGASYNCWS("%s\r\n", request->url().c_str());
    restart_esp(request);
  });

  server.on("/reset", [](AsyncWebServerRequest *request) {
    DEBUGASYNCWS("%s\r\n", request->url().c_str());
    reset_esp(request);
  });

  server.on("/admin/wwwauth", [](AsyncWebServerRequest *request) {
    send_wwwauth_configuration_values_html(request);
  });

  server.on("/system.html", [](AsyncWebServerRequest *request) {
    send_wwwauth_configuration_html(request);
  });

  server.on("/update/updatepossible", [](AsyncWebServerRequest *request) {
    send_update_firmware_values_html(request);
  });

  server.on("/setmd5", [](AsyncWebServerRequest *request) {
    setUpdateMD5(request);
  });

  // server.on("/update", HTTP_GET, [](AsyncWebServerRequest *request) {
  //   request->send(SPIFFS, "/update.html");
  // });

  server.on("/update", HTTP_POST, [](AsyncWebServerRequest *request) {
    AsyncWebServerResponse *response = request->beginResponse(200, "text/html", (Update.hasError()) ? "FAIL" : "<META http-equiv=\"refresh\" content=\"15;URL=/update\">Update correct. Restarting...");
    response->addHeader("Connection", "close");
    response->addHeader("Access-Control-Allow-Origin", "*");
    request->send(response); }, [](AsyncWebServerRequest *request, const String filename, size_t index, uint8_t *data, size_t len, bool final) { updateFirmware(request, filename, index, data, len, final); });

  server.on("/admin/connectionstate", [](AsyncWebServerRequest *request) {
    send_connection_state_values_html(request);
  });

  server.on("/admin/sholatvalues", [](AsyncWebServerRequest *request) {
    send_sholat_values_html(request);
  });

  // server.on("/settime.html", [](AsyncWebServerRequest *request) {
  //   set_time_html(request);
  // });

  server.on("/config/network", [](AsyncWebServerRequest *request) {
    DEBUGASYNCWS("%s\r\n", request->url().c_str());
    send_config_network(request);
  });

  server.on("/config/location", [](AsyncWebServerRequest *request) {
    DEBUGASYNCWS("%s\r\n", request->url().c_str());
    send_config_location(request);
  });

  server.on("/config/time", [](AsyncWebServerRequest *request) {
    DEBUGASYNCWS("%s\r\n", request->url().c_str());
    send_config_time(request);
  });

  server.on("/config/sholat", [](AsyncWebServerRequest *request) {
    DEBUGASYNCWS("%s\r\n", request->url().c_str());
    send_config_sholat(request);
  });

  server.on("/config/ledmatrix", [](AsyncWebServerRequest *request) {
    DEBUGASYNCWS("%s\r\n", request->url().c_str());
    send_config_ledmatrix(request);
  });

  server.on("/config/mqtt", [](AsyncWebServerRequest *request) {
    DEBUGASYNCWS("%s\r\n", request->url().c_str());
    send_config_mqtt(request);
  });

  server.on("/status/heap", [](AsyncWebServerRequest *request) {
    DEBUGASYNCWS("%s\r\n", request->url().c_str());

    uint32_t heap = ESP.getFreeHeap();

    DynamicJsonBuffer jsonBuffer;
    JsonObject &root = jsonBuffer.createObject();

    root[FPSTR(pgm_heap)] = heap;

    AsyncResponseStream *response = request->beginResponseStream("application/json");
    root.prettyPrintTo(*response);
    request->send(response);
  });

  server.on("/status/datetime", [](AsyncWebServerRequest *request) {
    DEBUGASYNCWS("%s\r\n", request->url().c_str());

    DynamicJsonBuffer jsonBuffer;
    JsonObject &root = jsonBuffer.createObject();

    root["d"] = day(localTime);
    root["m"] = month(localTime);
    root["y"] = year(localTime);
    root["hr"] = hour(localTime);
    root["min"] = minute(localTime);
    root["sec"] = second(localTime);
    root["tz"] = _configLocation.timezone;
    root["utc"] = utcTime;
    root["local"] = localTime;

    AsyncResponseStream *response = request->beginResponseStream("application/json");
    root.prettyPrintTo(*response);
    request->send(response);
  });

  server.begin();
}

// -------------------------------------------------
// *** FUNCTIONS
// -------------------------------------------------
void onWiFiConnected(WiFiEventStationModeConnected data)
{
  PRINT("\r\nWifi Connected\r\n");
  wifiDisconnectedSince = 0;
  PRINT("WiFi status: %s\r\n", WiFi.status() == WL_CONNECTED ? "WL_CONNECTED" : String(WiFi.status()).c_str());
}

// Start NTP only after IP network is connected
//void onWifiGotIP (WiFiEventStationModeGotIP ipInfo) {
void onWifiGotIP(const WiFiEventStationModeGotIP &event)
{
  wifiGotIpFlag = true;

  //Serial.printf_P(PSTR("\r\nWifi Got IP: %s\r\n"), ipInfo.ip.toString().c_str ());
  PRINT("\r\nWifi Got IP\r\n");
  PRINT("IP Address:\t%s\r\n", WiFi.localIP().toString().c_str());
}

void onWiFiDisconnected(WiFiEventStationModeDisconnected data)
{
  wifiDisconnectedFlag = true;
  PRINT("\r\nWifi disconnected.\r\n");
}

void onStationConnected(const WiFiEventSoftAPModeStationConnected &evt)
{
  stationConnectedFlag = true;

  PRINT("Station connected: %s\r\n", macToString(evt.mac));
}

void onStationDisconnected(const WiFiEventSoftAPModeStationDisconnected &evt)
{
  stationDisconnectedFlag = true;

  PRINT("Station disconnected: %s\r\n", macToString(evt.mac));
}

void onProbeRequestPrint(const WiFiEventSoftAPModeProbeRequestReceived &evt)
{
  PRINT("Probe request from: %s, RSSI: %d\r\n", macToString(evt.mac), evt.rssi);
}

void onProbeRequestBlink(const WiFiEventSoftAPModeProbeRequestReceived &)
{
  // We can't use "delay" or other blocking functions in the event handler.
  // Therefore we set a flag here and then check it inside "loop" function.
  // blinkFlag = true;
}

char *macToString(const unsigned char *mac)
{
  static char buf[20];
  snprintf(buf, sizeof(buf), "%02x:%02x:%02x:%02x:%02x:%02x",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return buf;
}

void handleFileList(AsyncWebServerRequest *request)
{
  if (!request->hasArg("dir"))
  {
    request->send(500, "text/plain", "BAD ARGS");
    return;
  }

  String path = request->arg("dir");
  DEBUGLOG("handleFileList: %s\r\n", path.c_str());
  Dir dir = SPIFFS.openDir(path);
  path = String();

  String output = "[";
  while (dir.next())
  {
    File entry = dir.openFile("r");
    if (true) //entry.name()!="secret.json") // Do not show secrets
    {
      if (output != "[")
        output += ',';
      bool isDir = false;
      output += "{\"type\":\"";
      output += (isDir) ? "dir" : "file";
      output += "\",\"name\":\"";
      output += String(entry.name()).substring(1);
      output += "\"}";
    }
    entry.close();
  }

  output += "]";
  DEBUGLOG("%s\r\n", output.c_str());
  request->send(200, "application/json", output);
}

void send_network_configuration_values_html(AsyncWebServerRequest *request)
{
  DEBUGASYNCWS("%s\r\n", __PRETTY_FUNCTION__);

  File configFile = SPIFFS.open(FPSTR(pgm_configfilenetwork), "r");
  if (!configFile)
  {
    DEBUGASYNCWS("Failed to open config file");
    return;
  }

  size_t size = configFile.size();
  DEBUGLOG("JSON file size: %d bytes\r\n", size);
  char buf[size];
  configFile.readBytes(buf, size);
  buf[size] = '\0';
  configFile.close();

  request->send(200, "text/plain", buf);
}

void send_connection_state_values_html(AsyncWebServerRequest *request)
{
  String state = "N/A";
  String Networks = "";
  if (WiFi.status() == 0)
    state = "Idle";
  else if (WiFi.status() == 1)
    state = "NO SSID AVAILBLE";
  else if (WiFi.status() == 2)
    state = "SCAN COMPLETED";
  else if (WiFi.status() == 3)
    state = "CONNECTED";
  else if (WiFi.status() == 4)
    state = "CONNECT FAILED";
  else if (WiFi.status() == 5)
    state = "CONNECTION LOST";
  else if (WiFi.status() == 6)
    state = "DISCONNECTED";

  WiFi.scanNetworks(true);

  String values = "";
  values += "connectionstate|" + state + "|div\r\n";
  //values += "networks|Scanning networks ...|div\r\n";
  request->send(200, "text/plain", values);
  state = "";
  values = "";
  Networks = "";
  DEBUGLOG("%s\r\n", __PRETTY_FUNCTION__);
}

void EventSendNetworkStatus()
{
  DEBUGLOG("%s\r\n", __PRETTY_FUNCTION__);

  DynamicJsonBuffer jsonBuffer;
  JsonObject &root = jsonBuffer.createObject();

  root[FPSTR(pgm_chipid)] = ESP.getChipId();
  root[FPSTR(pgm_hostname)] = _config.hostname;
  uint8_t mode = WiFi.getMode();
  if (mode == WIFI_AP)
  {
    root[FPSTR(pgm_mode)] = FPSTR(pgm_WIFI_AP);
  }
  else if (mode == WIFI_STA)
  {
    root[FPSTR(pgm_mode)] = FPSTR(pgm_WIFI_STA);
  }
  else if (mode == WIFI_AP_STA)
  {
    root[FPSTR(pgm_mode)] = FPSTR(pgm_WIFI_AP_STA);
  }
  else if (mode == WIFI_OFF)
  {
    root[FPSTR(pgm_mode)] = FPSTR(pgm_WIFI_OFF);
  }
  else
  {
    root[FPSTR(pgm_mode)] = FPSTR(pgm_NA);
  }
  root[FPSTR(pgm_ssid)] = WiFi.SSID();
  root[FPSTR(pgm_sta_ip)] = WiFi.localIP().toString();
  root[FPSTR(pgm_sta_mac)] = WiFi.macAddress();
  root[FPSTR(pgm_ap_ip)] = WiFi.softAPIP().toString();
  root[FPSTR(pgm_ap_mac)] = WiFi.softAPmacAddress();
  root[FPSTR(pgm_gateway)] = WiFi.gatewayIP().toString();
  root[FPSTR(pgm_netmask)] = WiFi.subnetMask().toString();
  root[FPSTR(pgm_dns0)] = WiFi.dnsIP().toString();
  root[FPSTR(pgm_dns1)] = WiFi.dnsIP(1).toString();

  size_t len = root.measureLength();
  char buf[len + 1];
  root.printTo(buf, sizeof(buf));
  events.send(buf, "timeDate", millis());
}

void WsSendNetworkStatus()
{
  DEBUGLOG("%s\r\n", __PRETTY_FUNCTION__);

  DynamicJsonBuffer jsonBuffer;
  JsonObject &root = jsonBuffer.createObject();

  root[FPSTR(pgm_chipid)] = ESP.getChipId();
  root[FPSTR(pgm_hostname)] = _config.hostname;
  uint8_t mode = WiFi.getMode();
  if (mode == WIFI_AP)
  {
    root[FPSTR(pgm_mode)] = FPSTR(pgm_WIFI_AP);
  }
  else if (mode == WIFI_STA)
  {
    root[FPSTR(pgm_mode)] = FPSTR(pgm_WIFI_STA);
  }
  else if (mode == WIFI_AP_STA)
  {
    root[FPSTR(pgm_mode)] = FPSTR(pgm_WIFI_AP_STA);
  }
  else if (mode == WIFI_OFF)
  {
    root[FPSTR(pgm_mode)] = FPSTR(pgm_WIFI_OFF);
  }
  else
  {
    root[FPSTR(pgm_mode)] = FPSTR(pgm_NA);
  }
  root[FPSTR(pgm_ssid)] = WiFi.SSID();
  root[FPSTR(pgm_sta_ip)] = WiFi.localIP().toString();
  root[FPSTR(pgm_sta_mac)] = WiFi.macAddress();
  root[FPSTR(pgm_ap_ip)] = WiFi.softAPIP().toString();
  root[FPSTR(pgm_ap_mac)] = WiFi.softAPmacAddress();
  root[FPSTR(pgm_gateway)] = WiFi.gatewayIP().toString();
  root[FPSTR(pgm_netmask)] = WiFi.subnetMask().toString();
  root[FPSTR(pgm_dns0)] = WiFi.dnsIP().toString();
  root[FPSTR(pgm_dns1)] = WiFi.dnsIP(1).toString();

  size_t len = root.measureLength();
  char buf[len + 1];
  root.printTo(buf, sizeof(buf));
  ws.text(clientID, buf);
}

void EventSendTimeStatus()
{
  DEBUGLOG("%s\r\n", __PRETTY_FUNCTION__);

  DynamicJsonBuffer jsonBuffer;
  JsonObject &root = jsonBuffer.createObject();

  root[FPSTR(pgm_date)] = getDateStr(localTime);
  root[FPSTR(pgm_time)] = getTimeStr(localTime);
  root[FPSTR(pgm_uptime)] = getUptimeStr();
  if (_lastSyncd != 0)
  {
    root[FPSTR(pgm_lastsync)] = getLastSyncStr();
    root[FPSTR(pgm_nextsync)] = getNextSyncStr();
  }
  else
  {
    root[FPSTR(pgm_lastsync)] = FPSTR(pgm_never);
    root[FPSTR(pgm_nextsync)] = getNextSyncStr();
  }

  size_t len = root.measureLength();
  char buf[len + 1];
  root.printTo(buf, sizeof(buf));
  events.send(buf);
}

void WsSendTimeStatus()
{
  DEBUGLOG("%s\r\n", __PRETTY_FUNCTION__);

  DynamicJsonBuffer jsonBuffer;
  JsonObject &root = jsonBuffer.createObject();

  root[FPSTR(pgm_date)] = getDateStr(localTime);
  root[FPSTR(pgm_time)] = getTimeStr(localTime);
  root[FPSTR(pgm_uptime)] = getUptimeStr();
  if (_lastSyncd != 0)
  {
    root[FPSTR(pgm_lastsync)] = getLastSyncStr();
    root[FPSTR(pgm_nextsync)] = getNextSyncStr();
  }
  else
  {
    root[FPSTR(pgm_lastsync)] = FPSTR(pgm_never);
    root[FPSTR(pgm_nextsync)] = getNextSyncStr();
  }

  root["ping_seq_num_send"] = ping_seq_num_send;
  root["ping_seq_num_recv"] = ping_seq_num_recv;

  size_t len = root.measureLength();
  char buf[len + 1];
  root.printTo(buf, sizeof(buf));
  ws.text(clientID, buf);
}

int apgm_lastIndexOf(uint8_t c, const char *p)
{
  int last_index = -1; // -1 indicates no match
  uint8_t b;
  for (int i = 0; true; i++)
  {
    b = pgm_read_byte(p++);
    if (b == c)
      last_index = i;
    else if (b == 0)
      break;
  }
  return last_index;
}

void EventSendSystemStatus()
{
  DEBUGLOG("%s\r\n", __PRETTY_FUNCTION__);

  uint32_t heap = ESP.getFreeHeap();

  /*
    const char *the_path = PSTR(__FILE__);

    int slash_loc = apgm_lastIndexOf('/', the_path); // index of last '/'
    if (slash_loc < 0) slash_loc = apgm_lastIndexOf('\\', the_path); // or last '\' (windows, ugh)

    int dot_loc = apgm_lastIndexOf('.', the_path);  // index of last '.'
    if (dot_loc < 0) dot_loc = apgm_lastIndexOf(0, the_path); // if no dot, return end of string

    char sketchname[64];
    int i;
    int n;
    for (i = slash_loc + 1; i < dot_loc; i++) {
    uint8_t b = pgm_read_byte(&the_path[i]);
    if (b != 0) {
      sketchname[n] = (char) b;
      n++;
    }
    else {
      break;
    }
    }
    sketchname[64] = '\0';
  */

  DynamicJsonBuffer jsonBuffer;
  JsonObject &root = jsonBuffer.createObject();

  root[FPSTR(pgm_heap)] = heap;
  //root[FPSTR(pgm_sketchname)] = sketchname;
  root[FPSTR(pgm_lastboot)] = getLastBootStr();
  root[FPSTR(pgm_chipid)] = ESP.getChipId();
  root[FPSTR(pgm_cpufreq)] = ESP.getCpuFreqMHz();
  root[FPSTR(pgm_sketchsize)] = ESP.getSketchSize();
  root[FPSTR(pgm_freesketchspace)] = ESP.getFreeSketchSpace();
  root[FPSTR(pgm_flashchipid)] = ESP.getFlashChipId();
  root[FPSTR(pgm_flashchipsize)] = ESP.getFlashChipSize();
  root[FPSTR(pgm_flashchiprealsize)] = ESP.getFlashChipRealSize();
  root[FPSTR(pgm_flashchipspeed)] = ESP.getFlashChipSpeed();
  root[FPSTR(pgm_cyclecount)] = ESP.getCycleCount();
  root[FPSTR(pgm_corever)] = ESP.getCoreVersion();
  root[FPSTR(pgm_sdkver)] = ESP.getSdkVersion();
  root[FPSTR(pgm_resetreason)] = ESP.getResetReason();

  size_t len = root.measureLength();
  char buf[len + 1];
  root.printTo(buf, len + 1);
  events.send(buf);
}

void WsSendSystemStatus()
{
  DEBUGLOG("%s\r\n", __PRETTY_FUNCTION__);

  uint32_t heap = ESP.getFreeHeap();

  /*
    const char *the_path = PSTR(__FILE__);

    int slash_loc = apgm_lastIndexOf('/', the_path); // index of last '/'
    if (slash_loc < 0) slash_loc = apgm_lastIndexOf('\\', the_path); // or last '\' (windows, ugh)

    int dot_loc = apgm_lastIndexOf('.', the_path);  // index of last '.'
    if (dot_loc < 0) dot_loc = apgm_lastIndexOf(0, the_path); // if no dot, return end of string

    char sketchname[64];
    int i;
    int n;
    for (i = slash_loc + 1; i < dot_loc; i++) {
    uint8_t b = pgm_read_byte(&the_path[i]);
    if (b != 0) {
      sketchname[n] = (char) b;
      n++;
    }
    else {
      break;
    }
    }
    sketchname[64] = '\0';
  */

  DynamicJsonBuffer jsonBuffer;
  JsonObject &root = jsonBuffer.createObject();

  root[FPSTR(pgm_heap)] = heap;
  //root[FPSTR(pgm_sketchname)] = sketchname;
  root[FPSTR(pgm_lastboot)] = getLastBootStr();
  root[FPSTR(pgm_chipid)] = ESP.getChipId();
  root[FPSTR(pgm_cpufreq)] = ESP.getCpuFreqMHz();
  root[FPSTR(pgm_sketchsize)] = ESP.getSketchSize();
  root[FPSTR(pgm_freesketchspace)] = ESP.getFreeSketchSpace();
  root[FPSTR(pgm_flashchipid)] = ESP.getFlashChipId();
  root[FPSTR(pgm_flashchipsize)] = ESP.getFlashChipSize();
  root[FPSTR(pgm_flashchiprealsize)] = ESP.getFlashChipRealSize();
  root[FPSTR(pgm_flashchipspeed)] = ESP.getFlashChipSpeed();
  root[FPSTR(pgm_cyclecount)] = ESP.getCycleCount();
  root[FPSTR(pgm_corever)] = ESP.getCoreVersion();
  root[FPSTR(pgm_sdkver)] = ESP.getSdkVersion();
  root[FPSTR(pgm_resetreason)] = ESP.getResetReason();

  size_t len = root.measureLength();
  char buf[len + 1];
  root.printTo(buf, len + 1);
  ws.text(clientID, buf);
}

// void EventSendHeap()
// {
//   DEBUGLOG("%s\r\n", __PRETTY_FUNCTION__);

//   uint32_t heap = ESP.getFreeHeap();

//   DynamicJsonBuffer jsonBuffer;
//   JsonObject &root = jsonBuffer.createObject();

//   root[FPSTR(pgm_heap)] = heap;

//   size_t len = root.measureLength();
//   char buf[len + 1];
//   root.printTo(buf, len + 1);
//   events.send(buf);
// }

void sendHeap(uint8_t mode)
{
  DEBUGASYNCWS("%s\r\n", __PRETTY_FUNCTION__);

  uint32_t heap = ESP.getFreeHeap();

  DynamicJsonBuffer jsonBuffer;
  JsonObject &root = jsonBuffer.createObject();

  root[FPSTR(pgm_heap)] = heap;

  size_t len = root.measureLength();
  char buf[len + 1];
  root.printTo(buf, len + 1);
  if (mode == 0)
  {
    //
  }
  else if (mode == 1)
  {
    events.send(buf);
  }
  else if (mode == 2)
  {
    ws.text(clientID, buf);
  }
}

void sendDateTime(uint8_t mode)
{
  DEBUGASYNCWS("%s\r\n", __PRETTY_FUNCTION__);

  DynamicJsonBuffer jsonBuffer;
  JsonObject &root = jsonBuffer.createObject();

  root["d"] = day(localTime);
  root["m"] = month(localTime);
  root["y"] = year(localTime);
  root["hr"] = hour(localTime);
  root["min"] = minute(localTime);
  root["sec"] = second(localTime);
  root["tz"] = _configLocation.timezone;
  root["utc"] = utcTime;
  root["local"] = localTime;

  size_t len = root.measurePrettyLength();
  char buf[len + 1];
  root.prettyPrintTo(buf, sizeof(buf));

  if (mode == 0)
  {
    //
  }
  else if (mode == 1)
  {
    events.send(buf);
    // events.send(buf, "timeDate", millis());
  }
  else if (mode == 2)
  {
    ws.text(clientID, buf);
  }
}

void WsSendFsInfo()
{
  DEBUGASYNCWS("%s\r\n", __PRETTY_FUNCTION__);

  DynamicJsonBuffer jsonBuffer;
  JsonObject &root = jsonBuffer.createObject();

  SPIFFS.info(fs_info);

  // root[FPSTR(pgm_heap)] = heap;
  root["totalBytes"] = fs_info.totalBytes;
  root["usedBytes"] = fs_info.usedBytes;
  root["blockSize"] = fs_info.blockSize;
  root["pageSize"] = fs_info.pageSize;
  root["maxOpenFiles"] = fs_info.maxOpenFiles;
  root["maxPathLength"] = fs_info.maxPathLength;

  size_t len = root.measureLength();
  char buf[len + 1];
  root.printTo(buf, len + 1);
  ws.text(clientID, buf);
}

void restart_esp(AsyncWebServerRequest *request)
{
  request->send_P(200, "text/html", Page_WaitAndReload);
  DEBUGLOG("%s\r\n", __PRETTY_FUNCTION__);
  SPIFFS.end(); // SPIFFS.end();

  /*
    GPIO 0  GPIO 2  GPIO 15
    UART Download Mode (Programming)  0       1       0
    Flash Startup (Normal)            1       1       0
    SD - Card Boot                      0       0       1
  */

  //WiFi.forceSleepBegin(); wdt_reset(); ESP.restart(); while(1)wdt_reset();

  //ESP.restart();

  restartESP.attach(1.0f, restart_1); // Task to run periodic things every second
}

void restart_1()
{
  // digitalWrite(0, HIGH); //GPIO0
  // digitalWrite(2, HIGH); //GPIO2
  // digitalWrite(15, LOW); //GPIO15
  ESP.restart();
}

void reset_esp(AsyncWebServerRequest *request)
{
  request->send_P(200, "text/html", Page_WaitAndReload);
  DEBUGLOG("%s\r\n", __PRETTY_FUNCTION__);
  SPIFFS.end(); // SPIFFS.end();
  //delay(1000);
  //ESP.reset();
  restartESP.attach(1.0f, reset_1); // Task to run periodic things every second
}

void reset_1()
{
  // digitalWrite(0, HIGH); //GPIO0
  // digitalWrite(2, HIGH); //GPIO2
  // digitalWrite(15, LOW); //GPIO15
  ESP.reset();
  //ESP.restart();
}

void send_wwwauth_configuration_values_html(AsyncWebServerRequest *request)
{
  DEBUGASYNCWS("%s\r\n", __PRETTY_FUNCTION__);

  AsyncResponseStream *response = request->beginResponseStream("text/plain");
  DynamicJsonBuffer jsonBuffer;
  JsonObject &root = jsonBuffer.createObject();
  if (_httpAuth.auth)
  {
    root["wwwauth"] = true;
  }
  else
  {
    root["wwwauth"] = false;
  }
  root["wwwuser"] = _httpAuth.wwwUsername;
  root["wwwpass"] = _httpAuth.wwwPassword;

  root.printTo(*response);
  request->send(response);
}

void send_wwwauth_configuration_html(AsyncWebServerRequest *request)
{
  DEBUGASYNCWS("%s %d\r\n", __FUNCTION__, request->args());

  //List all parameters
  int params = request->params();
  if (params)
  {
    for (int i = 0; i < params; i++)
    {
      AsyncWebParameter *p = request->getParam(i);
      if (p->isFile())
      { //p->isPost() is also true
        DEBUGASYNCWS("FILE[%s]: %s, size: %u\r\n", p->name().c_str(), p->value().c_str(), p->size());
      }
      else if (p->isPost())
      {
        DEBUGASYNCWS("POST[%s]: %s\r\n", p->name().c_str(), p->value().c_str());
        //      if (p->name() == "wwwauth") {
        //        _httpAuth.auth = p->value();
        //      }
        if (request->hasParam("wwwauth", true))
        {
          if (p->name() == "wwwauth")
          {
            _httpAuth.auth = p->value();
          }
          if (p->name() == "wwwuser")
          {
            strlcpy(_httpAuth.wwwUsername, p->value().c_str(), sizeof(_httpAuth.wwwUsername));
          }
          if (p->name() == "wwwpass")
          {
            strlcpy(_httpAuth.wwwPassword, p->value().c_str(), sizeof(_httpAuth.wwwPassword));
          }
        }
      }
      else
      {
        DEBUGASYNCWS("GET[%s]: %s\r\n", p->name().c_str(), p->value().c_str());
      }
    }
  }
  else
  {
    _httpAuth.auth = false;
  }

  //save settings
  saveHTTPAuth();

  // //Check if POST (but not File) parameter exists
  // if (request->hasParam("wwwauth", true))
  //   AsyncWebParameter* p = request->getParam("wwwauth", true);

  // //Check if POST (but not File) parameter exists
  // if (request->hasParam("wwwuser", true))
  //   AsyncWebParameter* p = request->getParam("wwwuser", true);

  // //Check if POST (but not File) parameter exists
  // if (request->hasParam("wwwpass", true))
  //   AsyncWebParameter* p = request->getParam("wwwpass", true);

  request->send(SPIFFS, request->url());
}

bool saveHTTPAuth()
{
  DEBUGASYNCWS("%s\r\n", __PRETTY_FUNCTION__);
  DEBUGASYNCWS("Save secret\r\n");
  DynamicJsonBuffer jsonBuffer;
  JsonObject &json = jsonBuffer.createObject();
  json["wwwauth"] = _httpAuth.auth;
  json["wwwuser"] = _httpAuth.wwwUsername;
  json["wwwpass"] = _httpAuth.wwwPassword;

  //TODO add AP data to html
  File file = SPIFFS.open(SECRET_FILE, "w");
  if (!file)
  {
    DEBUGASYNCWS("Failed to open secret file for writing\r\n");
    file.close();
    return false;
  }

#ifndef RELEASEASYNCWS
  json.prettyPrintTo(DEBUGPORT);
  DEBUGASYNCWS("\r\n");
#endif // RELEASE

  json.prettyPrintTo(file);
  file.flush();
  file.close();
  return true;
}

void send_update_firmware_values_html(AsyncWebServerRequest *request)
{
  DEBUGASYNCWS("%s\r\n", __PRETTY_FUNCTION__);
  String values = "";
  uint32_t freeSketchSpace = ESP.getFreeSketchSpace();
  uint32_t maxSketchSpace = (ESP.getSketchSize() - 0x1000) & 0xFFFFF000;
  //bool updateOK = Update.begin(maxSketchSpace);
  bool updateOK = maxSketchSpace < freeSketchSpace;
  StreamString result;
  Update.printError(result);
  DEBUGASYNCWS("--FreeSketchSpace: %d\r\n", freeSketchSpace);
  DEBUGASYNCWS("--MaxSketchSpace: %d\r\n", maxSketchSpace);
  DEBUGASYNCWS("--Update error = %s\r\n", result.c_str());
  values += "remupd|" + (String)((updateOK) ? "OK" : "ERROR") + "|div\n";

  if (Update.hasError())
  {
    result.trim();
    values += "remupdResult|" + result + "|div\n";
  }
  else
  {
    values += "remupdResult||div\n";
  }

  request->send(200, "text/plain", values);
}

void setUpdateMD5(AsyncWebServerRequest *request)
{
  _browserMD5 = "";
  DEBUGLOG("Arg number: %d\r\n", request->args());
  if (request->args() > 0) // Read hash
  {
    for (uint8_t i = 0; i < request->args(); i++)
    {
      DEBUGLOG("Arg %s: %s\r\n", request->argName(i).c_str(), request->arg(i).c_str());
      if (request->argName(i) == "md5")
      {
        _browserMD5 = urldecode(request->arg(i));
        Update.setMD5(_browserMD5.c_str());
        continue;
      }
      if (request->argName(i) == "size")
      {
        _updateSize = request->arg(i).toInt();
        DEBUGLOG("Update size: %l\r\n", _updateSize);
        continue;
      }
    }
    request->send(200, "text/html", "OK --> MD5: " + _browserMD5);
  }
}

void updateFirmware(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final)
{
  // handler for the file upload, get's the sketch bytes, and writes
  // them through the Update object
  static unsigned long totalSize = 0;
  if (!index)
  { //UPLOAD_FILE_START
    // SPIFFS.end();
    Update.runAsync(true);
    DEBUGASYNCWS("Update start: %s\r\n", filename.c_str());
    uint32_t maxSketchSpace = ESP.getSketchSize();
    DEBUGASYNCWS("Max free scketch space: %u\r\n", maxSketchSpace);
    DEBUGASYNCWS("New scketch size: %u\r\n", _updateSize);
    if (_browserMD5 != NULL && _browserMD5 != "")
    {
      Update.setMD5(_browserMD5.c_str());
      DEBUGASYNCWS("Hash from client: %s\r\n", _browserMD5.c_str());
    }
    if (!Update.begin(_updateSize))
    { //start with max available size
      Update.printError(DEBUGPORT);
    }
  }

  // Get upload file, continue if not start
  totalSize += len;
  DEBUGASYNCWS(".");
  size_t written = Update.write(data, len);
  if (written != len)
  {
    DEBUGASYNCWS("len = %d, written = %zu, totalSize = %lu\r\n", len, written, totalSize);
    //Update.printError(PRINTPORT);
    //return;
  }
  if (final)
  { // UPLOAD_FILE_END
    String updateHash;
    DEBUGASYNCWS("\r\nApplying update...\r\n");
    if (Update.end(true))
    { //true to set the size to the current progress
      updateHash = Update.md5String();
      DEBUGASYNCWS("Upload finished. Calculated MD5: %s\r\n", updateHash.c_str());
      DEBUGASYNCWS("Update Success: %u\r\nRebooting...\r\n", request->contentLength());
      // SPIFFS.end();
      // mqttClient.disconnect();
      // WiFi.mode(WIFI_OFF);
      // restartESP.attach(1.0f, restart_1);
      // restartESP.attach(1.0f, reset_1);
      firmwareUpdated = true;
    }
    else
    {
      updateHash = Update.md5String();
      DEBUGASYNCWS("Upload failed. Calculated MD5: %s\r\n", updateHash.c_str());
      Update.printError(PRINTPORT);
    }
  }

  //delay(2);
}

void send_sholat_values_html(AsyncWebServerRequest *request)
{
  DEBUGASYNCWS("%s\r\n", __PRETTY_FUNCTION__);

  AsyncResponseStream *response = request->beginResponseStream("application/json");
  DynamicJsonBuffer jsonBuffer;
  JsonObject &root = jsonBuffer.createObject();

  root[FPSTR(pgm_day)] = dayNameStr(weekday(local_time()));
  root[FPSTR(pgm_date)] = getDateStr(local_time());
  root[FPSTR(pgm_fajr)] = sholatTimeArray[0];
  root[FPSTR(pgm_syuruq)] = sholatTimeArray[1];
  root[FPSTR(pgm_dhuhr)] = sholatTimeArray[2];
  root[FPSTR(pgm_ashr)] = sholatTimeArray[3];
  root[FPSTR(pgm_maghrib)] = sholatTimeArray[5];
  root[FPSTR(pgm_isya)] = sholatTimeArray[6];
  root[FPSTR(pgm_h)] = bufHOUR;
  root[FPSTR(pgm_m)] = bufMINUTE;
  root[FPSTR(pgm_s)] = bufSECOND;
  root[FPSTR(pgm_curr)] = sholatNameStr(CURRENTTIMEID);
  root[FPSTR(pgm_next)] = sholatNameStr(NEXTTIMEID);
  root.printTo(*response);
  request->send(response);
}

void send_config_network(AsyncWebServerRequest *request)
{
  DEBUGASYNCWS("%s\r\n", __PRETTY_FUNCTION__);

  AsyncResponseStream *response = request->beginResponseStream("application/json");
  DynamicJsonBuffer jsonBuffer;
  JsonObject &root = jsonBuffer.createObject();

  root[FPSTR(pgm_hostname)] = _config.hostname;
  root[FPSTR(pgm_ssid)] = _config.ssid;
  root[FPSTR(pgm_password)] = _config.password;
  root[FPSTR(pgm_dhcp)] = _config.dhcp;
  root[FPSTR(pgm_static_ip)] = _config.static_ip;
  root[FPSTR(pgm_netmask)] = _config.netmask;
  root[FPSTR(pgm_gateway)] = _config.gateway;
  root[FPSTR(pgm_dns0)] = _config.dns0;
  root[FPSTR(pgm_dns1)] = _config.dns1;

  root.prettyPrintTo(*response);
  request->send(response);
}

void send_config_location(AsyncWebServerRequest *request)
{
  DEBUGASYNCWS("%s\r\n", __PRETTY_FUNCTION__);

  AsyncResponseStream *response = request->beginResponseStream("application/json");
  DynamicJsonBuffer jsonBuffer;
  JsonObject &root = jsonBuffer.createObject();

  root[FPSTR(pgm_city)] = _configLocation.city;
  root[FPSTR(pgm_timezone)] = _configLocation.timezone;
  root[FPSTR(pgm_latitude)] = _configLocation.latitude;
  root[FPSTR(pgm_longitude)] = _configLocation.longitude;

  root.prettyPrintTo(*response);
  request->send(response);
}

void send_config_time(AsyncWebServerRequest *request)
{
  DEBUGASYNCWS("%s\r\n", __PRETTY_FUNCTION__);

  AsyncResponseStream *response = request->beginResponseStream("application/json");
  DynamicJsonBuffer jsonBuffer;
  JsonObject &root = jsonBuffer.createObject();

  root[FPSTR(pgm_dst)] = _configTime.dst;
  root[FPSTR(pgm_enablertc)] = _configTime.enablertc;
  root[FPSTR(pgm_enablentp)] = _configTime.enablentp;
  root[FPSTR(pgm_ntpserver_0)] = _configTime.ntpserver_0;
  root[FPSTR(pgm_ntpserver_1)] = _configTime.ntpserver_1;
  root[FPSTR(pgm_ntpserver_2)] = _configTime.ntpserver_2;
  root[FPSTR(pgm_syncinterval)] = _configTime.syncinterval;

  root.prettyPrintTo(*response);
  request->send(response);
}

void send_config_sholat(AsyncWebServerRequest *request)
{
  DEBUGASYNCWS("%s\r\n", __PRETTY_FUNCTION__);

  AsyncResponseStream *response = request->beginResponseStream("application/json");
  DynamicJsonBuffer jsonBuffer;
  JsonObject &root = jsonBuffer.createObject();

  // root[FPSTR(pgm_province)] = _config.province;
  root[FPSTR(pgm_city)] = _configLocation.city;
  root[FPSTR(pgm_timezone)] = _configLocation.timezone;
  root[FPSTR(pgm_latitude)] = _configLocation.latitude;
  root[FPSTR(pgm_longitude)] = _configLocation.longitude;

  // calcMethod
  if (_sholatConfig.calcMethod == Jafari)
  {
    root[FPSTR(pgm_calcMethod)] = FPSTR(pgm_Jafari);
  }
  else if (_sholatConfig.calcMethod == Karachi)
  {
    root[FPSTR(pgm_calcMethod)] = FPSTR(pgm_Karachi);
  }
  else if (_sholatConfig.calcMethod == ISNA)
  {
    root[FPSTR(pgm_calcMethod)] = FPSTR(pgm_ISNA);
  }
  else if (_sholatConfig.calcMethod == MWL)
  {
    root[FPSTR(pgm_calcMethod)] = FPSTR(pgm_MWL);
  }
  else if (_sholatConfig.calcMethod == Makkah)
  {
    root[FPSTR(pgm_calcMethod)] = FPSTR(pgm_Makkah);
  }
  else if (_sholatConfig.calcMethod == Egypt)
  {
    root[FPSTR(pgm_calcMethod)] = FPSTR(pgm_Egypt);
  }
  else if (_sholatConfig.calcMethod == Custom)
  {
    root[FPSTR(pgm_calcMethod)] = FPSTR(pgm_Custom);
  }

  //asrMethod
  if (_sholatConfig.asrJuristic == Shafii)
  {
    root[FPSTR(pgm_asrJuristic)] = FPSTR(pgm_Shafii);
  }
  else if (_sholatConfig.asrJuristic == Hanafi)
  {
    root[FPSTR(pgm_asrJuristic)] = FPSTR(pgm_Hanafi);
  }

  // highLatsAdjustMethod
  //uint8_t highLatsAdjustMethod = root[FPSTR(pgm_highLatsAdjustMethod)];
  if (_sholatConfig.highLatsAdjustMethod == None)
  {
    root[FPSTR(pgm_highLatsAdjustMethod)] = FPSTR(pgm_None);
  }
  else if (_sholatConfig.highLatsAdjustMethod == MidNight)
  {
    root[FPSTR(pgm_highLatsAdjustMethod)] = FPSTR(pgm_MidNight);
  }
  else if (_sholatConfig.highLatsAdjustMethod == OneSeventh)
  {
    root[FPSTR(pgm_highLatsAdjustMethod)] = FPSTR(pgm_OneSeventh);
  }
  else if (_sholatConfig.highLatsAdjustMethod == AngleBased)
  {
    root[FPSTR(pgm_highLatsAdjustMethod)] = FPSTR(pgm_AngleBased);
  }

  root[FPSTR(pgm_fajrAngle)] = _sholatConfig.fajrAngle;
  root[FPSTR(pgm_maghribAngle)] = _sholatConfig.maghribAngle;
  root[FPSTR(pgm_ishaAngle)] = _sholatConfig.ishaAngle;
  root[FPSTR(pgm_offsetImsak)] = _sholatConfig.offsetImsak;
  root[FPSTR(pgm_offsetFajr)] = _sholatConfig.offsetFajr;
  root[FPSTR(pgm_offsetSunrise)] = _sholatConfig.offsetSunrise;
  root[FPSTR(pgm_offsetDhuhr)] = _sholatConfig.offsetDhuhr;
  root[FPSTR(pgm_offsetAsr)] = _sholatConfig.offsetAsr;
  root[FPSTR(pgm_offsetSunset)] = _sholatConfig.offsetSunset;
  root[FPSTR(pgm_offsetMaghrib)] = _sholatConfig.offsetMaghrib;
  root[FPSTR(pgm_offsetIsha)] = _sholatConfig.offsetIsha;

  root.prettyPrintTo(*response);
  request->send(response);
}

void send_config_ledmatrix(AsyncWebServerRequest *request)
{
  DEBUGASYNCWS("%s\r\n", __PRETTY_FUNCTION__);

  AsyncResponseStream *response = request->beginResponseStream("application/json");
  DynamicJsonBuffer jsonBuffer;
  JsonObject &root = jsonBuffer.createObject();

  // root[FPSTR(pgm_type)] = FPSTR(pgm_matrixconfig);
  root[FPSTR(pgm_pagemode0)] = _ledMatrixSettings.pagemode0;
  root[FPSTR(pgm_pagemode1)] = _ledMatrixSettings.pagemode1;
  root[FPSTR(pgm_pagemode2)] = _ledMatrixSettings.pagemode2;
  root[FPSTR(pgm_scrollrow_0)] = _ledMatrixSettings.scrollrow_0;
  root[FPSTR(pgm_scrollrow_1)] = _ledMatrixSettings.scrollrow_1;
  root[FPSTR(pgm_scrollspeed)] = _ledMatrixSettings.scrollspeed;
  root[FPSTR(pgm_brightness)] = _ledMatrixSettings.brightness;

  char temp[4];
  root[FPSTR(pgm_operatingmode)] = itoa(_ledMatrixSettings.operatingmode, temp, 10);
  if (_ledMatrixSettings.pagemode == Automatic)
  {
    root[FPSTR(pgm_pagemode)] = FPSTR(pgm_automatic);
  }
  else if (_ledMatrixSettings.pagemode == Manual)
  {
    root[FPSTR(pgm_pagemode)] = FPSTR(pgm_manual);
  }

  root[FPSTR(pgm_adzanwaittime)] = _ledMatrixSettings.adzanwaittime;
  root[FPSTR(pgm_iqamahwaittime)] = _ledMatrixSettings.iqamahwaittime;

  root.prettyPrintTo(*response);
  request->send(response);
}

void send_config_mqtt(AsyncWebServerRequest *request)
{
  DEBUGASYNCWS("%s\r\n", __PRETTY_FUNCTION__);

  AsyncResponseStream *response = request->beginResponseStream("application/json");
  DynamicJsonBuffer jsonBuffer;
  JsonObject &root = jsonBuffer.createObject();

  root[FPSTR(pgm_mqtt_enabled)] = configMqtt.enabled;
  root[FPSTR(pgm_mqtt_server)] = configMqtt.server;
  root[FPSTR(pgm_mqtt_port)] = configMqtt.port;
  root[FPSTR(pgm_mqtt_user)] = configMqtt.user;
  root[FPSTR(pgm_mqtt_pass)] = configMqtt.pass;
  root[FPSTR(pgm_mqtt_clientid)] = _config.hostname;
  root[FPSTR(pgm_mqtt_keepalive)] = configMqtt.keepalive;
  root[FPSTR(pgm_mqtt_cleansession)] = configMqtt.cleansession;
  root[FPSTR(pgm_mqtt_lwttopicprefix)] = configMqtt.lwttopicprefix;
  root[FPSTR(pgm_mqtt_lwtqos)] = configMqtt.lwtqos;
  root[FPSTR(pgm_mqtt_lwtretain)] = configMqtt.lwtretain;
  root[FPSTR(pgm_mqtt_lwtpayload)] = configMqtt.lwtpayload;

  root.prettyPrintTo(*response);
  request->send(response);
}

void WsSendConfigSholat()
{
  //  DEBUGASYNCWS("%s\r\n", __PRETTY_FUNCTION__);
  //
  //  DynamicJsonBuffer jsonBuffer;
  //  JsonObject &root = jsonBuffer.createObject();
  //
  //  root[FPSTR(pgm_location)] = _sholatConfig.citySholat;
  //  root[FPSTR(pgm_latitude)] = _sholatConfig.latitudeSholat;
  //  root[FPSTR(pgm_longitude)] = _sholatConfig.longitudeSholat;
  //  root[FPSTR(pgm_timezone)] = _sholatConfig.timeZoneSholat;
  //
  //  // calcMethod
  //  if (_sholatConfig.calcMethod == Jafari) {
  //    root[FPSTR(pgm_calcMethod)] = FPSTR(pgm_Jafari);
  //  }
  //  else if (_sholatConfig.calcMethod == Karachi) {
  //    root[FPSTR(pgm_calcMethod)] = FPSTR(pgm_Karachi);
  //  }
  //  else if (_sholatConfig.calcMethod == ISNA) {
  //    root[FPSTR(pgm_calcMethod)] = FPSTR(pgm_ISNA);
  //  }
  //  else if (_sholatConfig.calcMethod == MWL) {
  //    root[FPSTR(pgm_calcMethod)] = FPSTR(pgm_MWL);
  //  }
  //  else if (_sholatConfig.calcMethod == Makkah) {
  //    root[FPSTR(pgm_calcMethod)] = FPSTR(pgm_Makkah);
  //  }
  //  else if (_sholatConfig.calcMethod == Egypt) {
  //    root[FPSTR(pgm_calcMethod)] = FPSTR(pgm_Egypt);
  //  }
  //  else if (_sholatConfig.calcMethod == Custom) {
  //    root[FPSTR(pgm_calcMethod)] = FPSTR(pgm_Custom);
  //  }
  //
  //  //asrMethod
  //  if (_sholatConfig.asrJuristic == Shafii) {
  //    root[FPSTR(pgm_asrMethod)] = 0;
  //  }
  //  else if (_sholatConfig.asrJuristic == Hanafi) {
  //    root[FPSTR(pgm_asrMethod)] = 1;
  //  }
  //
  //  // highLatsAdjustMethod
  //  uint8_t highLatsAdjustMethod = root[FPSTR(pgm_highLatsAdjustMethod)];
  //  if (_sholatConfig.highLatsAdjustMethod == None) {
  //    root[FPSTR(pgm_highLatsAdjustMethod)] = 0;
  //  }
  //  else if (_sholatConfig.highLatsAdjustMethod == MidNight) {
  //    root[FPSTR(pgm_highLatsAdjustMethod)] = 1;
  //  }
  //  else if (_sholatConfig.highLatsAdjustMethod == OneSeventh) {
  //    root[FPSTR(pgm_highLatsAdjustMethod)] = 2;
  //  }
  //  else if (_sholatConfig.highLatsAdjustMethod == AngleBased) {
  //    root[FPSTR(pgm_highLatsAdjustMethod)] = 3;
  //  }
  //
  //  root[FPSTR(pgm_fajrAngle)] = _sholatConfig.fajrAngle;
  //  root[FPSTR(pgm_maghribAngle)] = _sholatConfig.maghribAngle;
  //  root[FPSTR(pgm_ishaAngle)] = _sholatConfig.ishaAngle;
  //  root[FPSTR(pgm_offsetImsak)] = _sholatConfig.offsetImsak;
  //  root[FPSTR(pgm_offsetFajr)] = _sholatConfig.offsetFajr;
  //  root[FPSTR(pgm_offsetSunrise)] = _sholatConfig.offsetSunrise;
  //  root[FPSTR(pgm_offsetDhuhr)] = _sholatConfig.offsetDhuhr;
  //  root[FPSTR(pgm_offsetAsr)] = _sholatConfig.offsetAsr;
  //  root[FPSTR(pgm_offsetSunset)] = _sholatConfig.offsetSunset;
  //  root[FPSTR(pgm_offsetMaghrib)] = _sholatConfig.offsetMaghrib;
  //  root[FPSTR(pgm_offsetIsha)] = _sholatConfig.offsetIsha;
  //
  //  size_t len = root.measureLength();
  //  char buf[len + 1];
  //  root.printTo(buf, sizeof(buf));
  //  ws.text(clientID, buf);
}

void set_time_html(AsyncWebServerRequest *request)
{
  DEBUGASYNCWS("%s\r\n", __PRETTY_FUNCTION__);

  //List all parameters
  int params = request->params();
  for (int i = 0; i < params; i++)
  {
    AsyncWebParameter *p = request->getParam(i);
    if (p->isFile())
    { //p->isPost() is also true
      Serial.printf("FILE[%s]: %s, size: %u\r\n", p->name().c_str(), p->value().c_str(), p->size());
    }
    else if (p->isPost())
    {
      Serial.printf("POST[%s]: %s\r\n", p->name().c_str(), p->value().c_str());
    }
    else
    {
      Serial.printf("GET[%s]: %s\r\n", p->name().c_str(), p->value().c_str());
    }
  }

  // //Check if GET parameter exists
  // if (request->hasParam("download"))
  //   AsyncWebParameter* p = request->getParam("download");

  // //Check if POST (but not File) parameter exists
  // if (request->hasParam("download", true))
  //   AsyncWebParameter* p = request->getParam("download", true);

  // //Check if FILE was uploaded
  // if (request->hasParam("download", true, true))
  //   AsyncWebParameter* p = request->getParam("download", true, true);

  //handleFileRead("/setrtctime.html", request);
  request->send(SPIFFS, request->url());
}

bool load_config_location()
{
  DEBUGASYNCWS("%s\r\n", __PRETTY_FUNCTION__);

  File file = SPIFFS.open(FPSTR(pgm_configfilelocation), "r");
  if (!file)
  {
    DEBUGASYNCWS("Failed to open config LOCATION file\n");
    file.close();
    return false;
  }

  size_t size = file.size();
  DEBUGASYNCWS("JSON file size: %d bytes\r\n", size);

  // Allocate a buffer to store contents of the file
  char buf[size];

  //copy file to buffer
  file.readBytes(buf, size);

  //add termination character at the end
  buf[size] = '\0';

  //close the file, save your memory, keep healthy :-)
  file.close();

  DynamicJsonBuffer jsonBuffer;
  JsonObject &root = jsonBuffer.parseObject(buf);

  if (!root.success())
  {
    DEBUGASYNCWS("Failed to parse config LOCATION file\r\n");
    return false;
  }

#ifndef RELEASEASYNCWS
  root.prettyPrintTo(DEBUGPORT);
#endif

  strlcpy(_configLocation.city, root[FPSTR(pgm_city)], sizeof(_configLocation.city));
  _configLocation.timezone = root[FPSTR(pgm_timezone)];
  _configLocation.latitude = root[FPSTR(pgm_latitude)];
  _configLocation.longitude = root[FPSTR(pgm_longitude)];

  DEBUGASYNCWS("\r\nConfig LOCATION loaded successfully.\r\n");
  DEBUGASYNCWS("city: %s\r\n", _configLocation.city);
  DEBUGASYNCWS("timezone: %d\r\n", _configLocation.timezone);
  DEBUGASYNCWS("latitude: %f\r\n", _configLocation.latitude);
  DEBUGASYNCWS("longitude: %f\r\n", _configLocation.longitude);

  return true;
}

bool load_config_network()
{
  DEBUGASYNCWS("%s\r\n", __PRETTY_FUNCTION__);

  File file = SPIFFS.open(FPSTR(pgm_configfilenetwork), "r");
  if (!file)
  {
    DEBUGASYNCWS("Failed to open config file\r\n");
    file.close();
    return false;
  }

  size_t size = file.size();
  DEBUGASYNCWS("JSON file size: %d bytes\r\n", size);

  // Allocate a buffer to store contents of the file
  char buf[size];

  //copy file to buffer
  file.readBytes(buf, size);

  //add termination character at the end
  buf[size] = '\0';

  //close the file, save your memory, keep healthy :-)
  file.close();

  DynamicJsonBuffer jsonBuffer;
  JsonObject &root = jsonBuffer.parseObject(buf);

  if (!root.success())
  {
    DEBUGASYNCWS("Failed to parse config NETWORK file\r\n");
    return false;
  }

#ifndef RELEASEASYNCWS
  root.prettyPrintTo(DEBUGPORT);
#endif

  // strlcpy(_config.hostname, root[FPSTR(pgm_hostname)], sizeof(_config.hostname));
  strlcpy(_config.ssid, root[FPSTR(pgm_ssid)], sizeof(_config.ssid));
  strlcpy(_config.password, root[FPSTR(pgm_password)], sizeof(_config.password));
  _config.dhcp = root[FPSTR(pgm_dhcp)];
  strlcpy(_config.static_ip, root[FPSTR(pgm_static_ip)], sizeof(_config.static_ip));
  strlcpy(_config.netmask, root[FPSTR(pgm_netmask)], sizeof(_config.netmask));
  strlcpy(_config.gateway, root[FPSTR(pgm_gateway)], sizeof(_config.gateway));
  strlcpy(_config.dns0, root[FPSTR(pgm_dns0)], sizeof(_config.dns0));
  strlcpy(_config.dns1, root[FPSTR(pgm_dns1)], sizeof(_config.dns1));

  DEBUGASYNCWS("\r\nNetwork settings loaded successfully.\r\n");
  DEBUGASYNCWS("hostname: %s\r\n", _config.hostname);
  DEBUGASYNCWS("ssid: %s\r\n", _config.ssid);
  DEBUGASYNCWS("pass: %s\r\n", _config.password);
  DEBUGASYNCWS("dhcp: %d\r\n", _config.dhcp);
  DEBUGASYNCWS("static_ip: %s\r\n", _config.static_ip);
  DEBUGASYNCWS("netmask: %s\r\n", _config.netmask);
  DEBUGASYNCWS("gateway: %s\r\n", _config.gateway);
  DEBUGASYNCWS("dns0: %s\r\n", _config.dns0);
  DEBUGASYNCWS("dns1: %s\r\n", _config.dns1);

  return true;
}

bool load_config_time()
{
  DEBUGASYNCWS("%s\r\n", __PRETTY_FUNCTION__);

  File file = SPIFFS.open(FPSTR(pgm_configfiletime), "r");
  if (!file)
  {
    DEBUGASYNCWS("Failed to open config TIME file\r\n");
    file.close();
    return false;
  }

  size_t size = file.size();
  DEBUGASYNCWS("config TIME file size: %d bytes\r\n", size);

  // Allocate a buffer to store contents of the file
  char buf[size];

  //copy file to buffer
  file.readBytes(buf, size);

  //add termination character at the end
  buf[size] = '\0';

  //close the file, save your memory, keep healthy :-)
  file.close();

  DynamicJsonBuffer jsonBuffer;
  JsonObject &root = jsonBuffer.parseObject(buf);

  if (!root.success())
  {
    DEBUGASYNCWS("Failed to parse config TIME file\r\n");
    return false;
  }

#ifndef RELEASEASYNCWS
  root.prettyPrintTo(DEBUGPORT);
  DEBUGASYNCWS("\r\n");

#endif

  _configTime.dst = root[FPSTR(pgm_dst)];
  _configTime.enablertc = root[FPSTR(pgm_enablertc)];
  _configTime.enablentp = root[FPSTR(pgm_enablentp)];
  strlcpy(_configTime.ntpserver_0, root[FPSTR(pgm_ntpserver_0)], sizeof(_configTime.ntpserver_0));
  strlcpy(_configTime.ntpserver_1, root[FPSTR(pgm_ntpserver_1)], sizeof(_configTime.ntpserver_1));
  strlcpy(_configTime.ntpserver_2, root[FPSTR(pgm_ntpserver_2)], sizeof(_configTime.ntpserver_2));
  _configTime.syncinterval = root[FPSTR(pgm_syncinterval)];

  //check
  DEBUGASYNCWS("Time settings loaded successfully.\r\n");
  DEBUGASYNCWS("dst: %d\r\n", _configTime.dst);
  DEBUGASYNCWS("syncinterval: %d\r\n", _configTime.syncinterval);
  DEBUGASYNCWS("enablertc: %d\r\n", _configTime.enablertc);
  DEBUGASYNCWS("enablentp: %d\r\n", _configTime.enablentp);
  DEBUGASYNCWS("ntpserver_0: %s\r\n", _configTime.ntpserver_0);
  DEBUGASYNCWS("ntpserver_1: %s\r\n", _configTime.ntpserver_1);
  DEBUGASYNCWS("ntpserver_2: %s\r\n", _configTime.ntpserver_2);

  return true;
}

//*************************
// LOAD SHOLAT CONFIG
//*************************
bool load_config_sholat()
{
  DEBUGASYNCWS("\r\n%s\r\n", __PRETTY_FUNCTION__);

  File file = SPIFFS.open(FPSTR(pgm_configfilesholat), "r");
  if (!file)
  {
    DEBUGASYNCWS("Failed to open config file\r\n");
    file.close();
    return false;
  }

  size_t size = file.size();
  DEBUGASYNCWS("JSON file size: %d bytes\r\n", size);

  // Allocate a buffer to store contents of the file
  char buf[size];

  //copy file to buffer
  file.readBytes(buf, size);

  //add termination character at the end
  buf[size] = '\0';

  //close the file, save your memory, keep healthy :-)
  file.close();

  DynamicJsonBuffer jsonBuffer;
  JsonObject &root = jsonBuffer.parseObject(buf);

  if (!root.success())
  {
    DEBUGASYNCWS("Failed to parse config file\r\n");
    return false;
  }

#ifndef RELEASEASYNCWS
  root.prettyPrintTo(DEBUGPORT);
  DEBUGASYNCWS("\r\n");

#endif

  //strlcpy(_sholatConfig.location, _config.city, sizeof(_sholatConfig.location));
  //const char *city = _config.city;
  //_sholatConfig.city = city;
  // _sholatConfig.timezone = _configLocation.timezone;
  // _sholatConfig.latitude = _configLocation.latitude;
  // _sholatConfig.longitude = _configLocation.longitude;

  uint8_t len;

  const char *calcMethod = root[FPSTR(pgm_calcMethod)];
  len = strlen(calcMethod);
  if (strncmp_P(calcMethod, pgm_Jafari, len) == 0)
  {
    _sholatConfig.calcMethod = Jafari;
  }
  else if (strncmp_P(calcMethod, pgm_Karachi, len) == 0)
  {
    _sholatConfig.calcMethod = Karachi;
  }
  else if (strncmp_P(calcMethod, pgm_ISNA, len) == 0)
  {
    _sholatConfig.calcMethod = ISNA;
  }
  else if (strncmp_P(calcMethod, pgm_MWL, len) == 0)
  {
    _sholatConfig.calcMethod = MWL;
  }
  else if (strncmp_P(calcMethod, pgm_Makkah, len) == 0)
  {
    _sholatConfig.calcMethod = Makkah;
  }
  else if (strncmp_P(calcMethod, pgm_Egypt, len) == 0)
  {
    _sholatConfig.calcMethod = Egypt;
  }
  else if (strncmp_P(calcMethod, pgm_Custom, len) == 0)
  {
    _sholatConfig.calcMethod = Custom;
  }

  //asrMethod
  const char *asrJuristic = root[FPSTR(pgm_asrJuristic)];
  len = strlen(asrJuristic);
  if (strncmp_P(asrJuristic, pgm_Shafii, len) == 0)
  {
    _sholatConfig.asrJuristic = Shafii;
  }
  else if (strncmp_P(asrJuristic, pgm_Hanafi, len) == 0)
  {
    _sholatConfig.asrJuristic = Hanafi;
  }

  // highLatsAdjustMethod
  const char *highLatsAdjustMethod = root[FPSTR(pgm_highLatsAdjustMethod)];
  len = strlen(highLatsAdjustMethod);
  if (strncmp_P(highLatsAdjustMethod, pgm_None, len) == 0)
  {
    _sholatConfig.highLatsAdjustMethod = None;
  }
  else if (strncmp_P(highLatsAdjustMethod, pgm_MidNight, len) == 0)
  {
    _sholatConfig.highLatsAdjustMethod = MidNight;
  }
  else if (strncmp_P(highLatsAdjustMethod, pgm_OneSeventh, len) == 0)
  {
    _sholatConfig.highLatsAdjustMethod = OneSeventh;
  }
  else if (strncmp_P(highLatsAdjustMethod, pgm_AngleBased, len) == 0)
  {
    _sholatConfig.highLatsAdjustMethod = AngleBased;
  }

  _sholatConfig.fajrAngle = root[FPSTR(pgm_fajrAngle)].as<uint16_t>();
  _sholatConfig.maghribAngle = root[FPSTR(pgm_maghribAngle)].as<uint16_t>();
  _sholatConfig.ishaAngle = root[FPSTR(pgm_ishaAngle)].as<uint16_t>();
  _sholatConfig.offsetImsak = root[FPSTR(pgm_offsetImsak)].as<float>();
  _sholatConfig.offsetFajr = root[FPSTR(pgm_offsetFajr)].as<float>();
  _sholatConfig.offsetSunrise = root[FPSTR(pgm_offsetSunrise)].as<float>();
  _sholatConfig.offsetDhuhr = root[FPSTR(pgm_offsetDhuhr)].as<float>();
  _sholatConfig.offsetAsr = root[FPSTR(pgm_offsetAsr)].as<float>();
  _sholatConfig.offsetSunset = root[FPSTR(pgm_offsetSunset)].as<float>();
  _sholatConfig.offsetMaghrib = root[FPSTR(pgm_offsetMaghrib)].as<float>();
  _sholatConfig.offsetIsha = root[FPSTR(pgm_offsetIsha)].as<float>();

  //check
  DEBUGASYNCWS("\r\nSholat settings loaded successfully.\r\n");
  //DEBUGASYNCWS("location: %s\r\n", _sholatConfig.location);
  DEBUGASYNCWS("timezone: %d\r\n", _configLocation.timezone);
  DEBUGASYNCWS("latitude: %f\r\n", _configLocation.latitude);
  DEBUGASYNCWS("longitude: %f\r\n", _configLocation.longitude);
  DEBUGASYNCWS("calcMethod: %d\r\n", _sholatConfig.calcMethod);
  DEBUGASYNCWS("asrJuristic: %d\r\n", _sholatConfig.asrJuristic);
  DEBUGASYNCWS("highLatsAdjustMethod: %d\r\n", _sholatConfig.highLatsAdjustMethod);
  DEBUGASYNCWS("fajrAngle: %d\r\n", _sholatConfig.fajrAngle);
  DEBUGASYNCWS("maghribAngle: %d\r\n", _sholatConfig.maghribAngle);
  DEBUGASYNCWS("ishaAngle: %d\r\n", _sholatConfig.ishaAngle);
  DEBUGASYNCWS("offsetImsak: %.1f\r\n", _sholatConfig.offsetImsak);
  DEBUGASYNCWS("offsetFajr: %.1f\r\n", _sholatConfig.offsetFajr);
  DEBUGASYNCWS("offsetSunrise: %.1f\r\n", _sholatConfig.offsetSunrise);
  DEBUGASYNCWS("offsetDhuhr: %.1f\r\n", _sholatConfig.offsetDhuhr);
  DEBUGASYNCWS("offsetAsr: %.1f\r\n", _sholatConfig.offsetAsr);
  DEBUGASYNCWS("offsetSunset: %.1f\r\n", _sholatConfig.offsetSunset);
  DEBUGASYNCWS("offsetMaghrib: %.1f\r\n", _sholatConfig.offsetMaghrib);
  DEBUGASYNCWS("offsetIsha: %.1f\r\n", _sholatConfig.offsetIsha);

  return true;
}

//*************************
// LOAD CONFIG LEDMATRIX
//*************************
bool load_config_ledmatrix()
{
  DEBUGASYNCWS("\r\n%s\r\n", __PRETTY_FUNCTION__);

  File file = SPIFFS.open(FPSTR(pgm_configfileledmatrix), "r");
  if (!file)
  {
    DEBUGASYNCWS("Failed to open config LEDMATRIX file\r\n");
    file.close();
    return false;
  }

  size_t size = file.size();
  DEBUGASYNCWS("JSON file size: %d bytes\r\n", size);

  // Allocate a buffer to store contents of the file
  char buf[size];

  //copy file to buffer
  file.readBytes(buf, size);

  //add termination character at the end
  buf[size] = '\0';

  //close the file, save your memory, keep healthy :-)
  file.close();

  DynamicJsonBuffer jsonBuffer;
  JsonObject &root = jsonBuffer.parseObject(buf);

  if (!root.success())
  {
    DEBUGASYNCWS("Failed to parse config LEDMATRIX file\r\n");
    return false;
  }

#ifndef RELEASEASYNCWS
  root.prettyPrintTo(DEBUGPORT);
  DEBUGASYNCWS("\r\n");

#endif

  uint8_t op = root[FPSTR(pgm_operatingmode)];

  if (op == 0)
  {
    _ledMatrixSettings.operatingmode = Normal;
  }
  else if (op == 1)
  {
    _ledMatrixSettings.operatingmode = Config;
  }
  else if (op == 2)
  {
    _ledMatrixSettings.operatingmode = Edit;
  }

  const char *pagemode = root[FPSTR(pgm_pagemode)];
  int len = strlen(pagemode);
  if (strncmp_P(pagemode, pgm_automatic, len) == 0)
  {
    _ledMatrixSettings.pagemode = Automatic;
  }
  else
  {
    _ledMatrixSettings.pagemode = Manual;
  }

  _ledMatrixSettings.pagemode0 = root[FPSTR(pgm_pagemode0)];
  _ledMatrixSettings.pagemode1 = root[FPSTR(pgm_pagemode1)];
  _ledMatrixSettings.pagemode2 = root[FPSTR(pgm_pagemode2)];
  _ledMatrixSettings.scrollrow_0 = root[FPSTR(pgm_scrollrow_0)];
  _ledMatrixSettings.scrollrow_1 = root[FPSTR(pgm_scrollrow_1)];
  _ledMatrixSettings.scrollspeed = root[FPSTR(pgm_scrollspeed)];
  _ledMatrixSettings.brightness = root[FPSTR(pgm_brightness)];
  _ledMatrixSettings.adzanwaittime = root[FPSTR(pgm_adzanwaittime)];
  _ledMatrixSettings.iqamahwaittime = root[FPSTR(pgm_iqamahwaittime)];

  MODE = op;
  currentPageMode0 = _ledMatrixSettings.pagemode0;
  currentPageMode1 = _ledMatrixSettings.pagemode1;
  currentPageMode2 = _ledMatrixSettings.pagemode2;

  //check
  DEBUGASYNCWS("\r\nLEDMATRIX settings loaded successfully.\r\n");
  DEBUGASYNCWS("operatingmode: %d\r\n", _ledMatrixSettings.operatingmode);
  DEBUGASYNCWS("pagemode: %d\r\n", _ledMatrixSettings.pagemode);
  DEBUGASYNCWS("pagemode0: %d\r\n", _ledMatrixSettings.pagemode0);
  DEBUGASYNCWS("pagemode1: %d\r\n", _ledMatrixSettings.pagemode1);
  DEBUGASYNCWS("pagemode2: %d\r\n", _ledMatrixSettings.pagemode2);
  DEBUGASYNCWS("scrollrow_0: %d\r\n", _ledMatrixSettings.scrollrow_0);
  DEBUGASYNCWS("scrollrow_1: %d\r\n", _ledMatrixSettings.scrollrow_1);
  DEBUGASYNCWS("scrollspeed: %d\r\n", _ledMatrixSettings.scrollspeed);
  DEBUGASYNCWS("brightness: %d\r\n", _ledMatrixSettings.brightness);

  return true;
}

//*************************
// LOAD HTTPAUTH CONFIG
//*************************
bool load_config_httpauth()
{
  DEBUGASYNCWS("\r\n%s\r\n", __PRETTY_FUNCTION__);

  File file = SPIFFS.open("/secret.json", "r");
  if (!file)
  {
    DEBUGASYNCWS("Failed to open HTTPAUTH file\r\n");
    file.close();
    return false;
  }

  size_t size = file.size();
  DEBUGASYNCWS("JSON file size: %d bytes\r\n", size);

  // Allocate a buffer to store contents of the file
  char buf[size];

  //copy file to buffer
  file.readBytes(buf, size);

  //add termination character at the end
  buf[size] = '\0';

  //close the file, save your memory, keep healthy :-)
  file.close();

  DynamicJsonBuffer jsonBuffer;
  JsonObject &root = jsonBuffer.parseObject(buf);

  if (!root.success())
  {
    DEBUGASYNCWS("Failed to parse json file\r\n");
    return false;
  }

#ifndef RELEASEASYNCWS
  root.prettyPrintTo(DEBUGPORT);
  DEBUGASYNCWS("\r\n");
#endif

  if (
      !root["wwwauth"].success() ||
      !root["wwwuser"].success() ||
      !root["wwwpass"].success())
  {
    DEBUGASYNCWS("Failed to parse httpAuth json file\r\n");
    return false;
  }

  _httpAuth.auth = root["wwwauth"];
  strlcpy(_httpAuth.wwwUsername, root["wwwuser"], sizeof(_httpAuth.wwwUsername));
  strlcpy(_httpAuth.wwwPassword, root["wwwpass"], sizeof(_httpAuth.wwwPassword));

  //check
  DEBUGASYNCWS("httpAuth settings loaded successfully.\r\n");
  DEBUGASYNCWS("Read httpAuth settings from memory:\r\n");
  DEBUGASYNCWS("wwwauth: %d\r\n", _httpAuth.auth);
  DEBUGASYNCWS("wwwuser: %s\r\n", _httpAuth.wwwUsername);
  DEBUGASYNCWS("wwwpass: %s\r\n", _httpAuth.wwwPassword);

  return true;
}

//*************************
// SAVE NETWORK CONFIG
//*************************
bool save_config_network()
{
  DEBUGASYNCWS("%s\r\n", __PRETTY_FUNCTION__);

  DynamicJsonBuffer jsonBuffer;
  JsonObject &json = jsonBuffer.createObject();
  // json[FPSTR(pgm_hostname)] = _config.hostname;
  json[FPSTR(pgm_ssid)] = _config.ssid;
  json[FPSTR(pgm_password)] = _config.password;
  json[FPSTR(pgm_dhcp)] = _config.dhcp;
  json[FPSTR(pgm_static_ip)] = _config.static_ip;
  json[FPSTR(pgm_netmask)] = _config.netmask;
  json[FPSTR(pgm_gateway)] = _config.gateway;
  json[FPSTR(pgm_dns0)] = _config.dns0;
  json[FPSTR(pgm_dns1)] = _config.dns1;

  //TODO add AP data to html
  File configFile = SPIFFS.open(FPSTR(pgm_configfilenetwork), "w");

#ifndef RELEASEASYNCWS
  json.prettyPrintTo(DEBUGPORT);
#endif

  json.prettyPrintTo(configFile);
  configFile.flush();
  configFile.close();
  return true;
}

//*************************
// SAVE CONFIG LOCATION
//*************************
bool save_config_location()
{
  DEBUGASYNCWS("%s\r\n", __PRETTY_FUNCTION__);

  DynamicJsonBuffer jsonBuffer;
  JsonObject &json = jsonBuffer.createObject();

  json[FPSTR(pgm_city)] = _configLocation.city;
  json[FPSTR(pgm_timezone)] = _configLocation.timezone;
  json[FPSTR(pgm_latitude)] = _configLocation.latitude;
  json[FPSTR(pgm_longitude)] = _configLocation.longitude;

  File configFile = SPIFFS.open(FPSTR(pgm_configfilelocation), "w");

#ifndef RELEASEASYNCWS
  json.prettyPrintTo(DEBUGPORT);
#endif

  json.prettyPrintTo(configFile);
  configFile.flush();
  configFile.close();
  return true;
}

//*************************
// SAVE CONFIG TIME
//*************************
bool save_config_time()
{
  DEBUGASYNCWS("%s\r\n", __PRETTY_FUNCTION__);

  DynamicJsonBuffer jsonBuffer;
  JsonObject &json = jsonBuffer.createObject();

  json[FPSTR(pgm_dst)] = _configTime.dst;
  json[FPSTR(pgm_enablertc)] = _configTime.enablertc;
  json[FPSTR(pgm_syncinterval)] = _configTime.syncinterval;
  json[FPSTR(pgm_enablentp)] = _configTime.enablentp;
  json[FPSTR(pgm_ntpserver_0)] = _configTime.ntpserver_0;
  json[FPSTR(pgm_ntpserver_1)] = _configTime.ntpserver_1;
  json[FPSTR(pgm_ntpserver_2)] = _configTime.ntpserver_2;

  //json["led"] = config.connectionLed;

  //TODO add AP data to html
  File configFile = SPIFFS.open(FPSTR(pgm_configfiletime), "w");

#ifndef RELEASEASYNCWS
  json.prettyPrintTo(DEBUGPORT);
#endif

  json.prettyPrintTo(configFile);
  configFile.flush();
  configFile.close();
  return true;
}

//*************************
// SAVE SHOLAT CONFIG
//*************************
bool save_config_sholat()
{
  DEBUGASYNCWS("%s\r\n", __PRETTY_FUNCTION__);

  DynamicJsonBuffer jsonBuffer;
  JsonObject &root = jsonBuffer.createObject();

  //root[FPSTR(pgm_location)] = _sholatConfig.location;
  //root[FPSTR(pgm_timezone)] = _sholatConfig.timezone;
  //root[FPSTR(pgm_latitude)] = _sholatConfig.latitude;
  //root[FPSTR(pgm_longitude)] = _sholatConfig.longitude;

  // calcMethod
  if (_sholatConfig.calcMethod == Jafari)
  {
    root[FPSTR(pgm_calcMethod)] = FPSTR(pgm_Jafari);
  }
  else if (_sholatConfig.calcMethod == Karachi)
  {
    root[FPSTR(pgm_calcMethod)] = FPSTR(pgm_Karachi);
  }
  else if (_sholatConfig.calcMethod == ISNA)
  {
    root[FPSTR(pgm_calcMethod)] = FPSTR(pgm_ISNA);
  }
  else if (_sholatConfig.calcMethod == MWL)
  {
    root[FPSTR(pgm_calcMethod)] = FPSTR(pgm_MWL);
  }
  else if (_sholatConfig.calcMethod == Makkah)
  {
    root[FPSTR(pgm_calcMethod)] = FPSTR(pgm_Makkah);
  }
  else if (_sholatConfig.calcMethod == Egypt)
  {
    root[FPSTR(pgm_calcMethod)] = FPSTR(pgm_Egypt);
  }
  else if (_sholatConfig.calcMethod == Custom)
  {
    root[FPSTR(pgm_calcMethod)] = FPSTR(pgm_Custom);
  }

  //asrJuristic
  if (_sholatConfig.asrJuristic == Shafii)
  {
    root[FPSTR(pgm_asrJuristic)] = FPSTR(pgm_Shafii);
  }
  else if (_sholatConfig.asrJuristic == Hanafi)
  {
    root[FPSTR(pgm_asrJuristic)] = FPSTR(pgm_Hanafi);
  }

  // highLatsAdjustMethod
  if (_sholatConfig.highLatsAdjustMethod == None)
  {
    root[FPSTR(pgm_highLatsAdjustMethod)] = FPSTR(pgm_None);
  }
  else if (_sholatConfig.highLatsAdjustMethod == MidNight)
  {
    root[FPSTR(pgm_highLatsAdjustMethod)] = FPSTR(pgm_MidNight);
  }
  else if (_sholatConfig.highLatsAdjustMethod == OneSeventh)
  {
    root[FPSTR(pgm_highLatsAdjustMethod)] = FPSTR(pgm_OneSeventh);
  }
  else if (_sholatConfig.highLatsAdjustMethod == AngleBased)
  {
    root[FPSTR(pgm_highLatsAdjustMethod)] = FPSTR(pgm_AngleBased);
  }

  root[FPSTR(pgm_fajrAngle)] = _sholatConfig.fajrAngle;
  root[FPSTR(pgm_maghribAngle)] = _sholatConfig.maghribAngle;
  root[FPSTR(pgm_ishaAngle)] = _sholatConfig.ishaAngle;
  root[FPSTR(pgm_offsetImsak)] = _sholatConfig.offsetImsak;
  root[FPSTR(pgm_offsetFajr)] = _sholatConfig.offsetFajr;
  root[FPSTR(pgm_offsetSunrise)] = _sholatConfig.offsetSunrise;
  root[FPSTR(pgm_offsetDhuhr)] = _sholatConfig.offsetDhuhr;
  root[FPSTR(pgm_offsetAsr)] = _sholatConfig.offsetAsr;
  root[FPSTR(pgm_offsetSunset)] = _sholatConfig.offsetSunset;
  root[FPSTR(pgm_offsetMaghrib)] = _sholatConfig.offsetMaghrib;
  root[FPSTR(pgm_offsetIsha)] = _sholatConfig.offsetIsha;

#ifndef RELEASEASYNCWS
  root.prettyPrintTo(DEBUGPORT);
#endif

  File configFile = SPIFFS.open(FPSTR(pgm_configfilesholat), "w");
  root.prettyPrintTo(configFile);
  configFile.flush();
  configFile.close();
  return true;
}

//*************************
// SAVE CONFIG LEDMATRIX
//*************************
bool save_config_ledmatrix()
{
  DEBUGASYNCWS("%s\r\n", __PRETTY_FUNCTION__);

  DynamicJsonBuffer jsonBuffer;
  JsonObject &root = jsonBuffer.createObject();

  uint8_t opmode = 0;

  if (_ledMatrixSettings.operatingmode == Normal)
  {
    opmode = 0;
  }
  else if (_ledMatrixSettings.operatingmode == Config)
  {
    opmode = 1;
  }
  else if (_ledMatrixSettings.operatingmode == Edit)
  {
    opmode = 2;
  }

  root[FPSTR(pgm_operatingmode)] = opmode;

  if (_ledMatrixSettings.pagemode == Automatic)
  {
    root[FPSTR(pgm_pagemode)] = FPSTR(pgm_automatic);
  }
  else if (_ledMatrixSettings.pagemode == Manual)
  {
    root[FPSTR(pgm_pagemode)] = FPSTR(pgm_manual);
  }

  root[FPSTR(pgm_pagemode0)] = _ledMatrixSettings.pagemode0;
  root[FPSTR(pgm_pagemode1)] = _ledMatrixSettings.pagemode1;
  root[FPSTR(pgm_pagemode2)] = _ledMatrixSettings.pagemode2;
  root[FPSTR(pgm_scrollrow_0)] = _ledMatrixSettings.scrollrow_0;
  root[FPSTR(pgm_scrollrow_1)] = _ledMatrixSettings.scrollrow_1;
  root[FPSTR(pgm_scrollspeed)] = _ledMatrixSettings.scrollspeed;
  root[FPSTR(pgm_brightness)] = _ledMatrixSettings.brightness;
  root[FPSTR(pgm_adzanwaittime)] = _ledMatrixSettings.adzanwaittime;
  root[FPSTR(pgm_iqamahwaittime)] = _ledMatrixSettings.iqamahwaittime;

#ifndef RELEASEASYNCWS
  root.prettyPrintTo(DEBUGPORT);
#endif

  File configFile = SPIFFS.open(FPSTR(pgm_configfileledmatrix), "w");
  root.prettyPrintTo(configFile);
  configFile.flush();
  configFile.close();
  return true;
}

// convert a single hex digit character to its integer value (from https://code.google.com/p/avr-netino/)
unsigned char h2int(char c)
{
  if (c >= '0' && c <= '9')
  {
    return ((unsigned char)c - '0');
  }
  if (c >= 'a' && c <= 'f')
  {
    return ((unsigned char)c - 'a' + 10);
  }
  if (c >= 'A' && c <= 'F')
  {
    return ((unsigned char)c - 'A' + 10);
  }
  return (0);
}

String urldecode(String input) // (based on https://code.google.com/p/avr-netino/)
{
  char c;
  String ret = "";

  for (byte t = 0; t < input.length(); t++)
  {
    c = input[t];
    if (c == '+')
      c = ' ';
    if (c == '%')
    {

      t++;
      c = input[t];
      t++;
      c = (h2int(c) << 4) | h2int(input[t]);
    }

    ret.concat(c);
  }
  return ret;
}

void load_running_text()
{

  //String contents = "fail";
  //char contents[512];

  //File runningTextFile = _fs->open(RUNNING_TEXT_FILE, "r");
  File runningTextFile = SPIFFS.open(RUNNING_TEXT_FILE, "r");
  if (!runningTextFile)
  {
    DEBUGLOG("Failed to open running text file");
    //return F("failed");
    //    return false;
    return;
  }

  // this is going to get the number of bytes in the file and give us the value in an integer
  int fileSize = runningTextFile.size();
  //Serial.println(fileSize);
  //int chunkSize=1;
  int chunkSize = 512;
  //This is a character array to store a chunk of the file.
  //We'll store 1024 characters at a time
  // char contents[fileSize];
  char buf[chunkSize];
  int numberOfChunks = (fileSize / chunkSize) + 1;

  // int count = 0;
  int remainingChunks = fileSize;

  int16_t x1Temp, y1Temp;
  uint16_t wTemp, hTemp;

  wText = 0;

  for (int i = 0; i < numberOfChunks; i++)
  {
    if (remainingChunks - chunkSize < 0)
    {
      chunkSize = remainingChunks;
    }
    runningTextFile.read((uint8_t *)buf, chunkSize);
    buf[chunkSize] = '\0';

    //Convert UTF8-string to Extended ASCII
    //utf8ascii(buf);

    matrix.print(buf);

    //contents += String(buf);
    //strcat(contents, buf);
    //buf[0] = (char)0; // clear buffer
    remainingChunks = remainingChunks - chunkSize;
  }

  //char* chr = const_cast<char*>(contents.c_str());
  //matrix.getTextBounds(chr, 0, 0, &x1Temp, &y1Temp, &wTemp, &hTemp);
  matrix.getTextBounds(buf, 0, 0, &x1Temp, &y1Temp, &wTemp, &hTemp);
  //matrix.getTextBounds(contents, 0, 0, &x1Temp, &y1Temp, &wTemp, &hTemp);
  wText = wTemp;

  runningTextFile.close();

  //contents = "";
  //return contents;

  DEBUGLOG("%s\r\n", __PRETTY_FUNCTION__);

  //return F("success");
}

void load_running_text_2()
{
  File runningTextFile = SPIFFS.open(RUNNING_TEXT_FILE_2, "r");
  if (!runningTextFile)
  {
    DEBUGLOG("Failed to open running text file");
    //return F("failed");
    //    return false;
    return;
  }

  // this is going to get the number of bytes in the file and give us the value in an integer
  int fileSize = runningTextFile.size();
  //Serial.println(fileSize);
  //int chunkSize=1;
  int chunkSize = 512;
  //This is a character array to store a chunk of the file.
  //We'll store 1024 characters at a time
  // char contents[fileSize];
  char buf[chunkSize];
  int numberOfChunks = (fileSize / chunkSize) + 1;

  // int count = 0;
  int remainingChunks = fileSize;

  int16_t x1Temp, y1Temp;
  uint16_t wTemp, hTemp;

  wText = 0;

  for (int i = 0; i < numberOfChunks; i++)
  {
    if (remainingChunks - chunkSize < 0)
    {
      chunkSize = remainingChunks;
    }
    runningTextFile.read((uint8_t *)buf, chunkSize);
    buf[chunkSize] = '\0';

    //Convert UTF8-string to Extended ASCII
    //utf8ascii(buf);

    matrix.print(buf);

    //contents += String(buf);
    //strcat(contents, buf);
    //buf[0] = (char)0; // clear buffer
    remainingChunks = remainingChunks - chunkSize;
  }

  //char* chr = const_cast<char*>(contents.c_str());
  //matrix.getTextBounds(chr, 0, 0, &x1Temp, &y1Temp, &wTemp, &hTemp);
  matrix.getTextBounds(buf, 0, 0, &x1Temp, &y1Temp, &wTemp, &hTemp);
  //matrix.getTextBounds(contents, 0, 0, &x1Temp, &y1Temp, &wTemp, &hTemp);
  wText = wTemp;

  runningTextFile.close();

  //contents = "";
  //return contents;

  DEBUGLOG("%s\r\n", __PRETTY_FUNCTION__);

  //return F("success");
}

char *formatBytes(size_t bytes)
{
  static char buf[10];
  if (bytes < 1024)
  {
    itoa(bytes, buf, 10);
    strcat(buf, "B");
  }
  else if (bytes < (1024 * 1024))
  {
    itoa(bytes / 1024.0, buf, 10);
    strcat(buf, "KB");
  }
  else if (bytes < (1024 * 1024 * 1024))
  {
    itoa(bytes / 1024.0 / 1024.0, buf, 10);
    strcat(buf, "MB");
  }
  else
  {
    itoa(bytes / 1024.0 / 1024.0 / 1024.0, buf, 10);
    strcat(buf, "GB");
  }
  return buf;
}

bool loadHTTPAuth()
{
  DEBUGASYNCWS("%s\r\n", __PRETTY_FUNCTION__);

  File configFile = SPIFFS.open(SECRET_FILE, "r");
  if (!configFile)
  {
    DEBUGASYNCWS("Failed to open secret file\r\n");
    _httpAuth.auth = false;
    _httpAuth.wwwUsername[0] = '\0';
    _httpAuth.wwwPassword[0] = '\0';
    configFile.close();
    return false;
  }

  size_t size = configFile.size();

  // Allocate a buffer to store contents of the file.
  std::unique_ptr<char[]> buf(new char[size]);

  // We don't use String here because ArduinoJson library requires the input
  // buffer to be mutable. If you don't use ArduinoJson, you may as well
  // use configFile.readString instead.
  configFile.readBytes(buf.get(), size);
  configFile.close();
  DEBUGASYNCWS("JSON secret file size: %d bytes\r\n", size);
  DynamicJsonBuffer jsonBuffer(256);
  //StaticJsonBuffer<256> jsonBuffer;
  JsonObject &json = jsonBuffer.parseObject(buf.get());

  if (!json.success())
  {
#ifndef RELEASE
    //String temp;
    json.prettyPrintTo(DEBUGPORT);
    PRINT("\r\n");
    PRINT("Failed to parse secret file\r\n");
#endif // RELEASE
    _httpAuth.auth = false;
    return false;
  }
#ifndef RELEASE
  //String temp;
  json.prettyPrintTo(DEBUGPORT);
  PRINT("\r\n");
#endif // RELEASE

  _httpAuth.auth = json["auth"];
  strlcpy(_httpAuth.wwwUsername, json["user"], sizeof(_httpAuth.wwwUsername));
  strlcpy(_httpAuth.wwwPassword, json["pass"], sizeof(_httpAuth.wwwPassword));

  if (_httpAuth.auth)
  {
    DEBUGASYNCWS("User: %s\r\n", _httpAuth.wwwUsername);
    DEBUGASYNCWS("Pass: %s\r\n", _httpAuth.wwwPassword);
  }

  return true;
}

void AsyncWSLoop()
{
  ArduinoOTA.handle();
  dnsServer.processNextRequest();

  // static time_t oldTime = localTime;
  //if (localTime != oldTime) {
  if (tick1000ms)
  {
    if (firmwareUpdated)
    { // check the flag here to determine if a restart is required
      Serial.printf("Restarting ESP\n\r");
      firmwareUpdated = false;
      ESP.restart();
    }

    //if (ws.hasClient(clientID)) {
    if (ws.hasClient(clientID))
    {
      if (clientVisitStatusNetworkPage)
      {
        //WsSendInfoDynamic(clientID);
        //EventSendNetworkStatus();
        WsSendNetworkStatus();
      }
      else if (clientVisitStatusTimePage)
      {
        //WsSendInfoDynamic(clientID);
        WsSendTimeStatus();
      }
      else if (clientVisitStatusSystemPage)
      {
        //WsSendInfoDynamic(clientID);
        //EventSendHeap();
        sendHeap(2);
        WsSendFsInfo();
      }
      else if (clientVisitSholatTimePage)
      {
        WsSendSholatDynamic(clientID);
      }
      else if (clientVisitConfigRunningLedPage)
      {
        // do nothing
      }
      else if (clientVisitConfigMqttPage)
      {
        // do nothing
      }
      else if (clientVisitSetTimePage)
      {
        // sendDateTime(1);
        sendDateTime(2);
      }
    }
    else
    {
      clientID = 0;
      clientVisitInfoPage = false;
      clientVisitSholatTimePage = false;
      clientVisitConfigRunningLedPage = false;
      clientVisitConfigMqttPage = false;
      clientVisitSetTimePage = false;
    }

    static char ssid_old[32];
    static char password_old[32];
    static bool dhcp_old;
    static char static_ip_old[16];
    static char netmask_old[16];
    static char gateway_old[16];
    static char dns0_old[16];
    static char dns1_old[16];

    static bool doOnce = true;
    if (doOnce)
    {
      doOnce = false;
      strlcpy(ssid_old, _config.ssid, sizeof(ssid_old));
      strlcpy(password_old, _config.password, sizeof(password_old));
      dhcp_old = _config.dhcp;
      strlcpy(static_ip_old, _config.static_ip, sizeof(static_ip_old));
      strlcpy(netmask_old, _config.netmask, sizeof(netmask_old));
      strlcpy(gateway_old, _config.gateway, sizeof(gateway_old));
      strlcpy(dns0_old, _config.dns0, sizeof(dns0_old));
      strlcpy(dns1_old, _config.dns1, sizeof(dns1_old));
    }

    if (configNetworkUpdatedFlag)
    {
      configNetworkUpdatedFlag = false;

      if (strcmp(ssid_old, _config.ssid) != 0 ||
          strcmp(password_old, _config.password) != 0 ||
          dhcp_old != _config.dhcp ||
          strcmp(static_ip_old, _config.static_ip) != 0 ||
          strcmp(netmask_old, _config.netmask) != 0 ||
          strcmp(gateway_old, _config.gateway) != 0 ||
          strcmp(dns0_old, _config.dns0) != 0 ||
          strcmp(dns1_old, _config.dns1) != 0)
      {

        PRINT("\r\n*** Network Settings Updated ***\r\nApllying new settings...\r\n\r\n");

        // update old values
        strlcpy(ssid_old, _config.ssid, sizeof(ssid_old));
        strlcpy(password_old, _config.password, sizeof(password_old));
        dhcp_old = _config.dhcp;
        strlcpy(static_ip_old, _config.static_ip, sizeof(static_ip_old));
        strlcpy(netmask_old, _config.netmask, sizeof(netmask_old));
        strlcpy(gateway_old, _config.gateway, sizeof(gateway_old));
        strlcpy(dns0_old, _config.dns0, sizeof(dns0_old));
        strlcpy(dns1_old, _config.dns1, sizeof(dns1_old));

        if (!_config.dhcp)
        {
          PRINT("DHCP disabled, starting wifi in WIFI_STA mode with static IP.\r\n");
          WiFi.mode(WIFI_OFF);
          WiFi.mode(WIFI_STA);

          IPAddress static_ip;
          IPAddress gateway;
          IPAddress netmask;
          IPAddress dns0;
          IPAddress dns1;

          static_ip.fromString(_config.static_ip);
          gateway.fromString(_config.gateway);
          netmask.fromString(_config.netmask);
          dns0.fromString(_config.dns0);
          dns1.fromString(_config.dns1);

          WiFi.config(static_ip, gateway, netmask, dns0, dns1);
          WiFi.hostname(_config.hostname);
          WiFi.begin(_config.ssid, _config.password);
          // WiFi.waitForConnectResult();
        }
        else
        {
          if (_config.ssid)
          {
            WiFi.mode(WIFI_OFF);
            IPAddress zeroIp(0, 0, 0, 0);
            WiFi.config(zeroIp, zeroIp, zeroIp);
            WiFi.hostname(_config.hostname);
            WiFi.begin(_config.ssid, _config.password);
            // WiFi.waitForConnectResult();
          }
        }
      }
      else
      {
        if (WiFi.status() != WL_CONNECTED)
        {
          if (_config.ssid)
          {
            PRINT("\r\nApllying new NETWORK settings...\r\n\r\n");
            WiFi.mode(WIFI_OFF);
            IPAddress zeroIp(0, 0, 0, 0);
            WiFi.config(zeroIp, zeroIp, zeroIp);
            WiFi.hostname(_config.hostname);
            WiFi.begin(_config.ssid, _config.password);
            // WiFi.waitForConnectResult();
          }
        }
      }
    }
  }

  if (wifiGotIpFlag)
  {
    wifiGotIpFlag = false;

    uint8_t mode = WiFi.getMode();
    if (mode == WIFI_AP)
    {
      // root[FPSTR(pgm_mode)] = FPSTR(pgm_WIFI_AP);
    }
    else if (mode == WIFI_STA)
    {
      // root[FPSTR(pgm_mode)] = FPSTR(pgm_WIFI_STA);
    }
    else if (mode == WIFI_AP_STA)
    {
      // root[FPSTR(pgm_mode)] = FPSTR(pgm_WIFI_AP_STA);
      PRINT("\r\nTurning Off Wifi Access Point...\r\n");
      WiFi.softAPdisconnect(true);
    }
    else if (mode == WIFI_OFF)
    {
      // root[FPSTR(pgm_mode)] = FPSTR(pgm_WIFI_OFF);
    }
    else
    {
      // root[FPSTR(pgm_mode)] = FPSTR(pgm_NA);
    }

    /*
    https://github.com/esp8266/Arduino/blob/4897e0006b5b0123a2fa31f67b14a3fff65ce561/libraries/ESP8266WiFi/src/include/wl_definitions.h

    typedef enum {
    WL_NO_SHIELD        = 255,   // for compatibility with WiFi Shield library
    WL_IDLE_STATUS      = 0,
    WL_NO_SSID_AVAIL    = 1,
    WL_SCAN_COMPLETED   = 2,
    WL_CONNECTED        = 3,
    WL_CONNECT_FAILED   = 4,
    WL_CONNECTION_LOST  = 5,
    WL_DISCONNECTED     = 6
    } wl_status_t;
  */
  }

  if (wifiDisconnectedFlag)
  {
    wifiDisconnectedFlag = false;

    // WiFi.reconnect();
    uint8_t client = WiFi.softAPgetStationNum();

    Serial.printf("\r\nStations connected to soft-AP = %d\r\n", client);

    if (!client)
    {
      WiFi.reconnect();
    }

    static bool apEnable = false;

    if (wifiDisconnectedSince == 0)
    {
      wifiDisconnectedSince = millis();
      apEnable = true;
    }

    if (apEnable == true && (int)((millis() - wifiDisconnectedSince) / 1000) >= 30)
    {
      PRINT("\r\nStarting Wifi Access Point...\r\n");
      // DEBUGPORT.printf_P(PSTR("\r\nStarting Wifi Access Point...\r\n"));
      apEnable = false;
      WiFi.mode(WIFI_OFF);
      WiFi.mode(WIFI_AP_STA);
      WiFi.softAP(_configAP.ssid, _configAP.password);
      WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
    }

    // PRINT("\r\nWifi disconnected for %d seconds\r\n", (int)((millis() - wifiDisconnectedSince) / 1000);
    DEBUGPORT.printf_P(PSTR("\r\nWifi disconnected for %d seconds\r\n"), (int)((millis() - wifiDisconnectedSince) / 1000));
  }

  if (stationConnectedFlag)
  {
    stationConnectedFlag = false;
  }

  if (stationDisconnectedFlag)
  {
    stationDisconnectedFlag = false;
    uint8_t client = WiFi.softAPgetStationNum();

    if (!client)
    {
      WiFi.reconnect();
    }
  }
}

void WsSendInfoStatic(int clientID)
{
  //  DEBUGLOG("%s\r\n", __PRETTY_FUNCTION__);
  //
  //  DynamicJsonBuffer jsonBuffer;
  //  JsonObject &root = jsonBuffer.createObject();
  //
  //  root[FPSTR(pgm_type)] = FPSTR(pgm_txt_infoStatic);
  //  root[FPSTR(pgm_loc)] = _sholatConfig.citySholat.c_str();
  //  root[FPSTR(pgm_ssid)] = WiFi.SSID();
  //  root[FPSTR(pgm_sta_ip)] = WiFi.localIP().toString();
  //  root[FPSTR(pgm_sta_mac)] = WiFi.macAddress();
  //  root[FPSTR(pgm_ap_ip)] = WiFi.softAPIP().toString();
  //  root[FPSTR(pgm_ap_mac)] = WiFi.softAPmacAddress();
  //  root[FPSTR(pgm_gateway)] = WiFi.gatewayIP().toString();
  //  root[FPSTR(pgm_netmask)] = WiFi.subnetMask().toString();
  //  root[FPSTR(pgm_dns0)] = WiFi.dnsIP().toString();
  //  root[FPSTR(pgm_dns1)] = WiFi.dnsIP(1).toString();
  //  root[FPSTR(pgm_chipid)] = ESP.getChipId();
  //  root[FPSTR(pgm_cpufreq)] = ESP.getCpuFreqMHz();
  //  root[FPSTR(pgm_sketchsize)] = ESP.getSketchSize();
  //  root[FPSTR(pgm_freesketchspace)] = ESP.getFreeSketchSpace();
  //  root[FPSTR(pgm_flashchipid)] = ESP.getFlashChipId();
  //  root[FPSTR(pgm_flashchipsize)] = ESP.getFlashChipSize();
  //  root[FPSTR(pgm_flashchiprealsize)] = ESP.getFlashChipRealSize();
  //  root[FPSTR(pgm_flashchipspeed)] = ESP.getFlashChipSpeed();
  //  root[FPSTR(pgm_cyclecount)] = ESP.getCycleCount();
  //  root[FPSTR(pgm_corever)] = ESP.getCoreVersion();
  //  root[FPSTR(pgm_sdkver)] = ESP.getSdkVersion();
  //  root[FPSTR(pgm_resetreason)] = ESP.getResetReason();
  //  root[FPSTR(pgm_lastboot)] = getLastBootStr();
  //
  //  size_t len = root.measureLength();
  //  char buf[len + 1];
  //  root.printTo(buf, len + 1);
  //
  //  ws.text(clientID, buf);
}

void WsSendInfoDynamic(int clientID)
{
  DEBUGLOG("%s\r\n", __PRETTY_FUNCTION__);

  uint32_t heapstart = ESP.getFreeHeap();

  DynamicJsonBuffer jsonBuffer;
  JsonObject &root = jsonBuffer.createObject();

  root[FPSTR(pgm_type)] = FPSTR(pgm_infodynamic);
  root[FPSTR(pgm_date)] = dayShortStr(weekday(local_time()));
  root[FPSTR(pgm_time)] = getTimeStr(localTime);
  if (_lastSyncd != 0)
  {
    root[FPSTR(pgm_lastsync)] = getLastSyncStr();
    root[FPSTR(pgm_nextsync)] = getNextSyncStr();
  }
  else
  {
    root[FPSTR(pgm_lastsync)] = FPSTR(pgm_never);
    root[FPSTR(pgm_nextsync)] = FPSTR(pgm_never);
  }
  root[FPSTR(pgm_uptime)] = getUptimeStr();
  root[FPSTR(pgm_roomtemp)] = bufTempSensor;
  root[FPSTR(pgm_rtctemp)] = bufRtcTemp;
  root[FPSTR(pgm_heapstart)] = heapstart;
  root[FPSTR(pgm_heapend)] = heapstart;
  uint32_t heapend = ESP.getFreeHeap();
  root[FPSTR(pgm_heapend)] = heapend;

  size_t len = root.measureLength();
  char buf[len + 1];
  root.printTo(buf, len + 1);

  ws.text(clientID, buf);
}

void WsSendSholatStatic(int clientID)
{
  //  DEBUGLOG("%s\r\n", __PRETTY_FUNCTION__);
  //
  //  DynamicJsonBuffer jsonBuffer;
  //  JsonObject &root = jsonBuffer.createObject();
  //
  //  root[FPSTR(pgm_type)] = FPSTR(pgm_sholatstatic);
  //  root[FPSTR(pgm_loc)] = _sholatConfig.citySholat;
  //  root[FPSTR(pgm_day)] = dayNameStr(weekday(local_time()));
  //  root[FPSTR(pgm_date)] = getDateStr(local_time());
  //  root[FPSTR(pgm_fajr)] = sholatTimeArray[0];
  //  root[FPSTR(pgm_syuruq)] = sholatTimeArray[1];
  //  root[FPSTR(pgm_dhuhr)] = sholatTimeArray[2];
  //  root[FPSTR(pgm_ashr)] = sholatTimeArray[3];
  //  root[FPSTR(pgm_maghrib)] = sholatTimeArray[5];
  //  root[FPSTR(pgm_isya)] = sholatTimeArray[6];
  //
  //  size_t len = root.measureLength();
  //  char buf[len + 1];
  //  root.printTo(buf, len + 1);
  //
  //  ws.text(clientID, buf);
}

void WsSendSholatDynamic(int clientID)
{
  DEBUGLOG("%s\r\n", __PRETTY_FUNCTION__);

  DynamicJsonBuffer jsonBuffer;
  JsonObject &root = jsonBuffer.createObject();

  root[FPSTR(pgm_type)] = FPSTR(pgm_sholatdynamic);
  root[FPSTR(pgm_h)] = HOUR;
  root[FPSTR(pgm_m)] = MINUTE;
  root[FPSTR(pgm_s)] = SECOND;
  root[FPSTR(pgm_curr)] = sholatNameStr(CURRENTTIMEID);
  root[FPSTR(pgm_next)] = sholatNameStr(NEXTTIMEID);

  size_t len = root.measureLength();
  char buf[len + 1];
  root.printTo(buf, len + 1);

  ws.text(clientID, buf);
}

void WsSendRunningLedConfig(int clientID)
{
  DEBUGLOG("%s\r\n", __PRETTY_FUNCTION__);

  DynamicJsonBuffer jsonBuffer;
  JsonObject &root = jsonBuffer.createObject();

  root[FPSTR(pgm_type)] = FPSTR(pgm_matrixconfig);
  root[FPSTR(pgm_pagemode0)] = _ledMatrixSettings.pagemode0;
  root[FPSTR(pgm_pagemode1)] = _ledMatrixSettings.pagemode1;
  root[FPSTR(pgm_pagemode2)] = _ledMatrixSettings.pagemode2;
  root[FPSTR(pgm_scrollrow_0)] = _ledMatrixSettings.scrollrow_0;
  root[FPSTR(pgm_scrollrow_1)] = _ledMatrixSettings.scrollrow_1;
  root[FPSTR(pgm_scrollspeedslider)] = _ledMatrixSettings.scrollspeed;
  root[FPSTR(pgm_scrollspeedvalue)] = _ledMatrixSettings.scrollspeed;
  root[FPSTR(pgm_brightnessslider)] = _ledMatrixSettings.brightness;
  root[FPSTR(pgm_brightnessvalue)] = _ledMatrixSettings.brightness;
  char temp[4];
  root[FPSTR(pgm_operatingmode)] = itoa(_ledMatrixSettings.operatingmode, temp, 10);
  if (_ledMatrixSettings.pagemode == Automatic)
  {
    root[FPSTR(pgm_pagemode)] = FPSTR(pgm_automatic);
  }
  else if (_ledMatrixSettings.pagemode == Manual)
  {
    root[FPSTR(pgm_pagemode)] = FPSTR(pgm_manual);
  }

  size_t len = root.measureLength();
  char buf[len + 1];
  root.printTo(buf, len + 1);

  ws.text(clientID, buf);
}
