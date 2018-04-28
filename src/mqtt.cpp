/*
   -------------------------------------------------------------------
   EmonESP Serial to Emoncms gateway
   -------------------------------------------------------------------
   Adaptation of Chris Howells OpenEVSE ESP Wifi
   by Trystan Lea, Glyn Hudson, OpenEnergyMonitor
   All adaptation GNU General Public License as below.

   -------------------------------------------------------------------

   This file is part of OpenEnergyMonitor.org project.
   EmonESP is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3, or (at your option)
   any later version.
   EmonESP is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   You should have received a copy of the GNU General Public License
   along with EmonESP; see the file COPYING.  If not, write to the
   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
*/

//#include "emonesp.h"
#include "mqtt.h"
#include "config.h"
//#include "FSWebServerLib.h"

#include <Arduino.h>

#define PRINTPORT Serial
#define DEBUGPORT Serial

#define RELEASEMQTT

#define PROGMEM_T __attribute__((section(".irom.text.template")))
#define PRINT(fmt, ...) \
  { static const char pfmt[] PROGMEM_T = fmt; PRINTPORT.printf_P(pfmt, ## __VA_ARGS__); }

#ifndef RELEASEMQTT
#define DEBUGMQTT(fmt, ...) \
  { static const char pfmt[] PROGMEM_T = fmt; DEBUGPORT.printf_P(pfmt, ## __VA_ARGS__); }
#else
#define DEBUGMQTT(...)
#endif


long reconnectInterval = 30000;
int clientTimeout = 0;
int i = 0;

AsyncMqttClient mqttClient;

Ticker mqttReconnectTimer;

WiFiEventHandler wifiConnectHandlerForMqtt;
WiFiEventHandler wifiDisconnectHandlerForMqtt;


//IPAddress mqttServer(192, 168, 10, 3);

const char pgm_txt_subcribedTopic_0[] PROGMEM = "/rumah/sts/1s/kwh1/watt";
const char pgm_txt_subcribedTopic_1[] PROGMEM = "/rumah/sts/1s/kwh1/voltage";

const char pgm_subscribe_1[] PROGMEM = "subscribe_1";
const char pgm_subscribe_2[] PROGMEM = "subscribe_2";
const char pgm_publish_1[] PROGMEM = "publish_1";

const char pgm_mqtt_server[] PROGMEM = "mqtt_server";
const char pgm_mqtt_port[] PROGMEM = "mqtt_port";
const char pgm_mqtt_keepalive[] PROGMEM = "mqtt_keepalive";
const char pgm_mqtt_cleansession[] PROGMEM = "mqtt_cleansession";
const char pgm_mqtt_lwt[] PROGMEM = "mqtt_lwt";
const char pgm_mqtt_user[] PROGMEM = "mqtt_user";
const char pgm_mqtt_pass[] PROGMEM = "mqtt_pass";
const char pgm_mqtt_clientid[] PROGMEM = "mqtt_clientid";

// MQTT config struct
strConfigMqtt configMqtt;

void onWifiConnect(const WiFiEventStationModeGotIP& event) {
  PRINT("Connected to Wi-Fi.\r\n");
  if (WiFi.status() == WL_CONNECTED) {
    connectToMqtt();
  }
}

void onWifiDisconnect(const WiFiEventStationModeDisconnected& event) {
  PRINT("Disconnected from Wi-Fi.\r\n");
  mqttReconnectTimer.detach(); // ensure we don't reconnect to MQTT while reconnecting to Wi-Fi
  //wifiReconnectTimer.once(2, connectToWifi);
}

void connectToMqtt() {
  PRINT("Connecting to MQTT...\r\n");
  mqttClient.connect();
}

void onMqttConnect(bool sessionPresent) {
  PRINT("Connected to MQTT.\n  Session present: %d\r\n", sessionPresent);

  File configFile = SPIFFS.open(CONFIG_FILE_MQTT_PUBSUB, "r");
  if (!configFile) {
    DEBUGMQTT("Failed to open config file\r\n");
    return;
  }

  size_t size = configFile.size();
  DEBUGMQTT("CONFIG_FILE_MQTT_PUBSUB file size: %d bytes\r\n", size);
  if (size > 1024) {
    PRINT("WARNING, file size maybe too large\r\n");

    //configFile.close();

    //return false;
  }

  // Allocate a buffer to store contents of the file.
  std::unique_ptr<char[]> buf(new char[size]);

  // We don't use String here because ArduinoJson library requires the input
  // buffer to be mutable. If you don't use ArduinoJson, you may as well
  // use configFile.readString instead.
  configFile.readBytes(buf.get(), size);
  configFile.close();
  DynamicJsonBuffer jsonBuffer(256);
  // StaticJsonBuffer<1024> jsonBuffer;

  JsonObject& json = jsonBuffer.parseObject(buf.get());

  //  JsonVariant json;
  //  json = jsonBuffer.parse(buf.get());

  if (!json.success()) {
    PRINT("Failed to parse MQTT config file\r\n");
    return;
  }

#ifndef RELEASEMQTT
  // String temp;
  json.prettyPrintTo(DEBUGPORT);
  // Serial.println(temp);
#endif

  // publish 1
  const char* publish_1_topic = json[FPSTR(pgm_publish_1)][0];
  uint8_t publish_1_qos = json[FPSTR(pgm_publish_1)][1];
  bool publish_1_retain = json[FPSTR(pgm_publish_1)][2];
  // const char* publish_1_payload = json[FPSTR(pgm_publish_1)][0];
  uint16_t packetIdPub1 = mqttClient.publish(
                            json[FPSTR(pgm_publish_1)][0],  //topic
                            json[FPSTR(pgm_publish_1)][1],  //qos
                            json[FPSTR(pgm_publish_1)][2],  //retain
                            json[FPSTR(pgm_publish_1)][3]   //payload
                          );
  PRINT(
    "Publishing packetId: %d\n  topic:  %s\n QoS:  %d\n retain:  %d\n payload:  %s\r\n",
    packetIdPub1, publish_1_topic, publish_1_qos, publish_1_retain, publish_1_topic
  );

  // subscribe 1
  const char* subscribe_1_topic = json[FPSTR(pgm_subscribe_1)][0];
  uint8_t subscribe_1_qos = json[FPSTR(pgm_subscribe_1)][1];
  uint16_t packetIdSub1 = mqttClient.subscribe(subscribe_1_topic, subscribe_1_qos);
  PRINT("Subscribing packetId: %d\n  topic: %s\n  QoS: %d\r\n", packetIdSub1, subscribe_1_topic, subscribe_1_qos);

  //subscribe 2
  if (false) {
    const char* subscribe_2_topic = json[FPSTR(pgm_subscribe_2)][0];
    uint8_t subscribe_2_qos = json[FPSTR(pgm_subscribe_2)][1];
    uint16_t packetIdSub2 = mqttClient.subscribe(subscribe_2_topic, subscribe_2_qos);
    PRINT("Subscribing packetId: %d\n  topic: %s\n  QoS: %d\r\n", packetIdSub2, subscribe_2_topic, subscribe_2_qos);
  }
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
  PRINT("Disconnected from MQTT.\r\n");

  if (WiFi.isConnected()) {
    mqttReconnectTimer.once(2, connectToMqtt);
  }
}

void onMqttSubscribe(uint16_t packetId, uint8_t qos) {
  PRINT("Subscribe acknowledged.\n  packetId: %d\n  qos: %d\r\n", packetId, qos);
}

void onMqttUnsubscribe(uint16_t packetId) {
  PRINT("Unsubscribe acknowledged.\n  packetId: %d\r\n", packetId);
}

void onMqttMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total) {
  //  Serial.println("Payload received.");
  //  Serial.print("  topic: ");
  //  Serial.println(topic);
  //  Serial.print("  qos: ");
  //  Serial.println(properties.qos);
  //  Serial.print("  dup: ");
  //  Serial.println(properties.dup);
  //  Serial.print("  retain: ");
  //  Serial.println(properties.retain);
  //  Serial.print("  len: ");
  //  Serial.println(len);
  //  Serial.print("  index: ");
  //  Serial.println(index);
  //  Serial.print("  total: ");
  //  Serial.println(total);

  DEBUGMQTT("MQTT received [%s] ", topic);
  for (size_t i = 0; i < len; i++) {
    DEBUGMQTT("%c", (char)payload[i]);
  }
  DEBUGMQTT("\r\n");

  char t[64];
  strcpy(t, topic);

  File configFile = SPIFFS.open(CONFIG_FILE_MQTT_PUBSUB, "r");
  if (!configFile) {
    DEBUGMQTT("Failed to open config file\r\n");
    return;
  }

  size_t size = configFile.size();
  //DEBUGMQTT("CONFIG_FILE_MQTT_PUBSUB file size: %d bytes\r\n", size);
  if (size > 1024) {
    DEBUGMQTT("WARNING, file size maybe too large\r\n");

    //configFile.close();

    //return false;
  }

  // Allocate a buffer to store contents of the file.
  std::unique_ptr<char[]> buf(new char[size]);

  // We don't use String here because ArduinoJson library requires the input
  // buffer to be mutable. If you don't use ArduinoJson, you may as well
  // use configFile.readString instead.
  configFile.readBytes(buf.get(), size);
  configFile.close();
  DynamicJsonBuffer jsonBuffer(512);
  //StaticJsonBuffer<1024> jsonBuffer;

  JsonObject& json = jsonBuffer.parseObject(buf.get());

  //  JsonVariant json;
  //  json = jsonBuffer.parse(buf.get());

  if (!json.success()) {
    DEBUGMQTT("Failed to parse MQTT config file\r\n");
    return;
  }

  //#ifndef RELEASEMQTT
  //  String temp;
  //  json.prettyPrintTo(temp);
  //  Serial.println(temp);
  //#endif

  const char* subscribe_1_topic = json["subscribe_1"][0];
  // uint8_t subscribe_1_qos =  json["subscribe_1"][1];
  // const char* subscribe_2_topic = json["subscribe_2"][0];
  // uint8_t subscribe_2_qos =  json["subscribe_2"][1];

  //handle energy meter payload
  if (strncmp(const_cast<char*>(topic), subscribe_1_topic, json[FPSTR(pgm_subscribe_1)][0].measureLength()) == 0) {
    //copy payload
    //    for (int i = 0; i < len; i++) {
    //      //DEBUGMQTT("%c", (char)payload[i]);
    //      bufWatt[i] = (char)payload[i];
    //    }
    //    bufWatt[len] = '\0';

    DynamicJsonBuffer jsonBuffer(256);
    //StaticJsonBuffer<512> jsonBuffer;
    JsonObject& root = jsonBuffer.parseObject(payload);

    const char* voltage;
    const char* watt;
    if (root["voltage"].success()) {
      voltage = root["voltage"];
      strlcpy(bufVoltage, voltage, sizeof(bufVoltage));
      dtostrf(atof(bufVoltage), 1, 0, bufVoltage);
    }
    if (root["watt"].success()) {
      watt = root["watt"];
      strlcpy(bufWatt, watt, sizeof(bufWatt));
      dtostrf(atof(bufWatt), 1, 0, bufWatt);
    }
  }

  //  //handle voltage payload
  //  //if (strncmp(t, subscribe_2_topic, json["subscribe_2"][0].measureLength()) == 0) {
  //  if (strncmp(const_cast<char*>(topic), subscribe_2_topic, json[FPSTR(pgm_subscribe_2)][0].measureLength()) == 0) {
  //    //copy payload
  //    //    for (int i = 0; i < len; i++) {
  //    //      //DEBUGMQTT("%c", (char)payload[i]);
  //    //      bufVoltage[i] = (char)payload[i];
  //    //    }
  //    //    bufVoltage[len] = '\0';
  //    strlcpy(bufVoltage, payload, sizeof(bufVoltage));
  //
  //    //roundup value
  //    dtostrf(atof(bufVoltage), 1, 0, bufVoltage);
  //  }
}

void onMqttPublish(uint16_t packetId) {
  DEBUGMQTT("Publish acknowledged.\r\n");
  DEBUGMQTT("  packetId: %d\r\n", packetId);
}

boolean mqtt_setup() {

  File configFile = SPIFFS.open(CONFIG_FILE_MQTT, "r");
  if (!configFile) {
    DEBUGMQTT("Failed to open config file\r\n");
    return false;
  }

  size_t size = configFile.size();
  /*if (size > 1024) {
      DEBUGMQTT("Config file size is too large");
      configFile.close();
      return false;
    }*/

  // Allocate a buffer to store contents of the file.
  std::unique_ptr<char[]> buf(new char[size]);

  // We don't use String here because ArduinoJson library requires the input
  // buffer to be mutable. If you don't use ArduinoJson, you may as well
  // use configFile.readString instead.
  configFile.readBytes(buf.get(), size);
  configFile.close();
  DEBUGMQTT("JSON file size: %d bytes\r\n", size);
  //DynamicJsonBuffer jsonBuffer(1024);
  StaticJsonBuffer<1024> jsonBuffer;

  JsonObject& json = jsonBuffer.parseObject(buf.get());

  //  JsonVariant json;
  //  json = jsonBuffer.parse(buf.get());

  if (!json.success()) {
    DEBUGMQTT("Failed to parse MQTT config file\r\n");
    return false;
  }

#ifndef RELEASEMQTT
  String temp;
  json.prettyPrintTo(temp);
  Serial.println(temp);
#endif

  strlcpy(configMqtt.server, json[FPSTR(pgm_mqtt_server)], sizeof(configMqtt.server));
  configMqtt.port = json[FPSTR(pgm_mqtt_port)];
  strlcpy(configMqtt.user, json[FPSTR(pgm_mqtt_user)], sizeof(configMqtt.user));
  strlcpy(configMqtt.pass, json[FPSTR(pgm_mqtt_pass)], sizeof(configMqtt.pass));
  strlcpy(configMqtt.clientid, json[FPSTR(pgm_mqtt_clientid)], sizeof(configMqtt.clientid));
  configMqtt.keepalive = json[FPSTR(pgm_mqtt_keepalive)];
  configMqtt.cleansession = json[FPSTR(pgm_mqtt_cleansession)];

  //lwt
  strlcpy(configMqtt.lwttopic, json[FPSTR(pgm_mqtt_lwt)][0], sizeof(configMqtt.lwttopic));
  configMqtt.lwtqos = json[FPSTR(pgm_mqtt_lwt)][1];
  configMqtt.lwtretain = json[FPSTR(pgm_mqtt_lwt)][2];
  strlcpy(configMqtt.lwtpayload, json[FPSTR(pgm_mqtt_lwt)][3], sizeof(configMqtt.lwtpayload));


  wifiConnectHandlerForMqtt = WiFi.onStationModeGotIP(onWifiConnect);
  wifiDisconnectHandlerForMqtt = WiFi.onStationModeDisconnected(onWifiDisconnect);

  //register mqtt handler
  mqttClient.onConnect(onMqttConnect);
  mqttClient.onDisconnect(onMqttDisconnect);
  mqttClient.onSubscribe(onMqttSubscribe);
  mqttClient.onUnsubscribe(onMqttUnsubscribe);
  mqttClient.onMessage(onMqttMessage);
  mqttClient.onPublish(onMqttPublish);

  //setup
  mqttClient.setServer(configMqtt.server, configMqtt.port);
  mqttClient.setCredentials(configMqtt.user, configMqtt.pass);
  mqttClient.setClientId(configMqtt.clientid);
  mqttClient.setKeepAlive(configMqtt.keepalive);
  mqttClient.setCleanSession(configMqtt.cleansession);
  mqttClient.setWill(configMqtt.lwttopic, configMqtt.lwtqos,
                     configMqtt.lwtretain, configMqtt.lwtpayload
                    );

  return true;
}
