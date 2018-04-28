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

extern AsyncMqttClient mqttClient;
extern IPAddress mqttServer;

extern const char pgm_txt_subcribedTopic_0[];
extern const char pgm_txt_subcribedTopic_1[];

// MQTT config
typedef struct {
  char server[45];
  uint16_t port;
  char user[32];
  char pass[64];
  char clientid[16];
  uint16_t keepalive;
  bool cleansession;
  char lwttopic[64];
  uint8_t lwtqos;
  bool lwtretain;
  char lwtpayload[64];
  //char publish_1_topic[64];
  //uint8_t publish_1_qos;
  //bool publish_1_retain;
  //char publish_1_payload[64];
  //char subscribe_1_topic[64];
  //uint8_t subscribe_1_qos;
  //char subscribe_2_topic[64];
  //uint8_t subscribe_2_qos;
} strConfigMqtt;

extern  strConfigMqtt configMqtt;

//
void connectToMqtt();

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
extern boolean mqtt_connected();

// -------------------------------------------------------------------
// Initialize MQTT connection
// -------------------------------------------------------------------
extern boolean mqtt_setup();

extern boolean mqtt_reconnect();




#endif // _LEDMATRIX_MQTT_H








