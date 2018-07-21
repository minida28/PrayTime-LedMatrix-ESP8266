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

#ifndef _LEDMATRIX_MQTT_H
#define _LEDMATRIX_MQTT_H

// -------------------------------------------------------------------
// MQTT support
// -------------------------------------------------------------------

#include <Arduino.h>
//#include <WiFiClient.h>
#include <AsyncMqttClient.h>
#include <ESP8266WiFi.h>
#include <Ticker.h>
#include "config.h"
#include "asyncserver.h"
#include <ArduinoJson.h>

#define CONFIG_FILE_MQTT "/configmqtt.json"
#define CONFIG_FILE_MQTT_PUBSUB "/configmqttpubsub.json"

extern AsyncMqttClient mqttClient;
extern IPAddress mqttServer;

extern const char pgm_txt_subcribedTopic_0[];
extern const char pgm_txt_subcribedTopic_1[];

// extern const char pgm_pub1_basetopic[];
extern const char pgm_pub1_topicprefix[];
extern const char pgm_pub1_qos[];
extern const char pgm_pub1_retain[];
extern const char pgm_pub1_payload[];
extern const char pgm_sub1_topic[];
extern const char pgm_sub1_qos[];
extern const char pgm_sub2_topic[];
extern const char pgm_sub2_qos[];

extern const char pgm_mqtt_enabled[];
extern const char pgm_mqtt_server[];
extern const char pgm_mqtt_port[];
extern const char pgm_mqtt_user[];
extern const char pgm_mqtt_pass[];
extern const char pgm_mqtt_clientid[];
extern const char pgm_mqtt_keepalive[];
extern const char pgm_mqtt_cleansession[];
extern const char pgm_mqtt_lwttopicprefix[];
extern const char pgm_mqtt_lwtqos[];
extern const char pgm_mqtt_lwtretain[];
extern const char pgm_mqtt_lwtpayload[];

// MQTT config
typedef struct
{
  bool enabled = true;
  char server[64] = "192.168.10.3";
  uint16_t port = 1883;
  char user[32] = "test";
  char pass[64] = "test";
  char clientid[24] = "SHOLAT_2937814"; // will be set during setup
  uint16_t keepalive = 30;
  bool cleansession = true;
  char lwttopicprefix[64] = "mqttstatus";
  uint8_t lwtqos = 2;
  bool lwtretain = true;
  char lwtpayload[64] = "DISCONNECTED";

  char pub1_basetopic[64];
  char pub1_topicprefix[16] = "mqttstatus";
  uint8_t pub1_qos = 2;
  bool pub1_retain = true;
  char pub1_payload[64] = "CONNECTED";
  char sub1_topic[64] = "ESP13579541/meterreading/1s";
  uint8_t sub1_qos = 0;
  char sub2_topic[64] = "/rumah/sts/1s/kwh1/voltage";
  uint8_t sub2_qos = 0;

} strConfigMqtt;

extern strConfigMqtt configMqtt;

extern uint32_t lastTimePayloadReceived;

//
void connectToMqtt();
void SetMqttClientId();
void MqttConnectedCb();
void MqttDisconnectedCb();

JsonObject &ParseConfigFile(const char *filename, size_t allocSize);
JsonObject &ParseConfigFile(const __FlashStringHelper *filename, size_t allocSize);

// -------------------------------------------------------------------
// Perform the background MQTT operations. Must be called in the main
// loop function
// -------------------------------------------------------------------
extern void mqtt_loop();

// -------------------------------------------------------------------
// Publish values to MQTT
//
// data: a comma seperated list of name:value pairs to send
// -------------------------------------------------------------------
extern void mqtt_publish(String data);

// -------------------------------------------------------------------
// Restart the MQTT connection
// -------------------------------------------------------------------
extern void mqtt_restart();

// -------------------------------------------------------------------
// Return true if we are connected to an MQTT broker, false if not
// -------------------------------------------------------------------
extern bool mqtt_connected();

// -------------------------------------------------------------------
// Initialize MQTT connection
// -------------------------------------------------------------------
extern bool load_config_mqtt();

extern bool mqtt_setup();

extern bool mqtt_reconnect();

#endif // _LEDMATRIX_MQTT_H
