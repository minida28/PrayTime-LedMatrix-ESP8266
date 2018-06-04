


#include "mqtt.h"
// #include "config.h"
//#include "FSWebServerLib.h"

#include <Arduino.h>

#define PRINTPORT Serial
#define DEBUGPORT Serial

#define RELEASEMQTT

#define PROGMEM_T __attribute__((section(".irom.text.template")))
#define PRINT(fmt, ...)                       \
  {                                           \
    static const char pfmt[] PROGMEM_T = fmt; \
    PRINTPORT.printf_P(pfmt, ##__VA_ARGS__);  \
  }

#ifndef RELEASEMQTT
#define DEBUGMQTT(fmt, ...)                   \
  {                                           \
    static const char pfmt[] PROGMEM_T = fmt; \
    DEBUGPORT.printf_P(pfmt, ##__VA_ARGS__);  \
  }
#else
#define DEBUGMQTT(...)
#endif

uint32_t lastTimePayloadReceived = 0;
uint32_t reconnectInterval = 30000;
uint32_t clientTimeout = 0;
uint32_t i = 0;

AsyncMqttClient mqttClient;

Ticker mqttReconnectTimer;

WiFiEventHandler wifiConnectHandlerForMqtt;
WiFiEventHandler wifiDisconnectHandlerForMqtt;

//IPAddress mqttServer(192, 168, 10, 3);

const char pgm_txt_subcribedTopic_0[] PROGMEM = "/rumah/sts/1s/kwh1/watt";
const char pgm_txt_subcribedTopic_1[] PROGMEM = "/rumah/sts/1s/kwh1/voltage";

const char pgm_pub1_topicprefix[] PROGMEM = "pub1_topicprefix";
const char pgm_pub1_qos[] PROGMEM = "pub1_qos";
const char pgm_pub1_retain[] PROGMEM = "pub1_retain";
const char pgm_pub1_payload[] PROGMEM = "pub1_payload";
const char pgm_sub1_topic[] PROGMEM = "sub1_topic";
const char pgm_sub1_qos[] PROGMEM = "sub1_qos";
const char pgm_sub2_topic[] PROGMEM = "sub2_topic";
const char pgm_sub2_qos[] PROGMEM = "sub2_qos";

const char pgm_mqtt_enabled[] = "enabled";
const char pgm_mqtt_server[] PROGMEM = "server";
const char pgm_mqtt_port[] PROGMEM = "port";
const char pgm_mqtt_user[] PROGMEM = "user";
const char pgm_mqtt_pass[] PROGMEM = "pass";
const char pgm_mqtt_clientid[] PROGMEM = "clientid";
const char pgm_mqtt_keepalive[] PROGMEM = "keepalive";
const char pgm_mqtt_cleansession[] PROGMEM = "cleansession";
const char pgm_mqtt_lwttopicprefix[] PROGMEM = "lwttopicprefix";
const char pgm_mqtt_lwtqos[] PROGMEM = "lwtqos";
const char pgm_mqtt_lwtretain[] PROGMEM = "lwtretain";
const char pgm_mqtt_lwtpayload[] PROGMEM = "lwtpayload";

// MQTT config struct
strConfigMqtt configMqtt;

bool load_config_mqtt()
{
  DEBUGMQTT("%s\r\n", __PRETTY_FUNCTION__);

  File file = SPIFFS.open(FPSTR(pgm_configfilemqtt), "r");
  if (!file)
  {
    DEBUGMQTT("Failed to open config MQTT file\n");
    file.close();
    return false;
  }

  size_t size = file.size();
  DEBUGMQTT("MQTT file size: %d bytes\r\n", size);

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
    DEBUGMQTT("Failed to parse config MQTT file\r\n");
    return false;
  }

#ifndef RELEASEMQTT
  root.prettyPrintTo(DEBUGPORT);
#endif

  configMqtt.enabled = root[FPSTR(pgm_mqtt_enabled)];
  strlcpy(configMqtt.server, root[FPSTR(pgm_mqtt_server)], sizeof(configMqtt.server));
  configMqtt.port = root[FPSTR(pgm_mqtt_port)];
  strlcpy(configMqtt.user, root[FPSTR(pgm_mqtt_user)], sizeof(configMqtt.user));
  strlcpy(configMqtt.pass, root[FPSTR(pgm_mqtt_pass)], sizeof(configMqtt.pass));
  strlcpy(configMqtt.clientid, _config.hostname, sizeof(configMqtt.clientid));
  configMqtt.keepalive = root[FPSTR(pgm_mqtt_keepalive)];
  configMqtt.cleansession = root[FPSTR(pgm_mqtt_cleansession)];

  //lwt
  strlcpy(configMqtt.lwttopicprefix, root[FPSTR(pgm_mqtt_lwttopicprefix)], sizeof(configMqtt.lwttopicprefix));
  configMqtt.lwtqos = root[FPSTR(pgm_mqtt_lwtqos)];
  configMqtt.lwtretain = root[FPSTR(pgm_mqtt_lwtretain)];
  strlcpy(configMqtt.lwtpayload, root[FPSTR(pgm_mqtt_lwtpayload)], sizeof(configMqtt.lwtpayload));

  DEBUGMQTT("\r\nConfig MQTT loaded successfully.\r\n");
  DEBUGMQTT("server: %s\r\n", configMqtt.server);
  DEBUGMQTT("port: %d\r\n", configMqtt.port);
  DEBUGMQTT("user: %s\r\n", configMqtt.user);
  DEBUGMQTT("pass: %s\r\n", configMqtt.pass);
  DEBUGMQTT("clientid: %s\r\n", configMqtt.clientid);
  DEBUGMQTT("keepalive: %d\r\n", configMqtt.keepalive);
  DEBUGMQTT("cleansession: %d\r\n", configMqtt.cleansession);
  DEBUGMQTT("lwttopicprefix: %s\r\n", configMqtt.lwttopicprefix);
  DEBUGMQTT("lwtqos: %d\r\n", configMqtt.lwtqos);
  DEBUGMQTT("lwtretain: %d\r\n", configMqtt.lwtretain);
  DEBUGMQTT("lwtpayload: %s\r\n", configMqtt.lwtpayload);

  return true;
}

void onWifiGotIPForMqtt(const WiFiEventStationModeGotIP &event)
{
  PRINT("Wifi got IP. Connecting to MQTT...\r\n");

  connectToMqtt();
}

void onWifiDisconnectForMqtt(const WiFiEventStationModeDisconnected &event)
{
  PRINT("Disconnected from Wi-Fi. Detaching mqttReconnectTimer.\r\n");
  mqttReconnectTimer.detach(); // ensure we don't reconnect to MQTT while reconnecting to Wi-Fi
  //wifiReconnectTimer.once(2, connectToWifi);
}

void connectToMqtt()
{
  DEBUGMQTT("%s\n", __PRETTY_FUNCTION__);

  if (configMqtt.enabled)
  {
    PRINT("Connecting to MQTT...\r\n");

    //load config
    load_config_mqtt();

    //lwt
    const char *lwtbasetopic = _config.hostname;
    const char *lwttopicprefix = configMqtt.lwttopicprefix;
    char buflwttopic[64];
    strlcpy(buflwttopic, lwtbasetopic, sizeof(buflwttopic));

    // check if lwt topic prefix is available or not
    // if yes, add "/" as needed
    if (strncmp(lwttopicprefix, "", 1) == 0)
    {
      //do nothing
    }
    else
    {
      char bufSlash[] = "/";
      strncat(buflwttopic, bufSlash, sizeof(buflwttopic));
      strncat(buflwttopic, lwttopicprefix, sizeof(buflwttopic));
    }

    mqttClient.setServer(configMqtt.server, configMqtt.port);

    if (strcmp(configMqtt.user, "") == 0 || strcmp(configMqtt.pass, "") == 0)
    {
      // do nothing
    }
    else
    {
      mqttClient.setCredentials(configMqtt.user, configMqtt.pass);
    }

    mqttClient.setClientId(configMqtt.clientid);
    mqttClient.setKeepAlive(configMqtt.keepalive);
    mqttClient.setCleanSession(configMqtt.cleansession);
    mqttClient.setWill(buflwttopic,
                       configMqtt.lwtqos,
                       configMqtt.lwtretain,
                       configMqtt.lwtpayload);

    mqttClient.connect();
  }
  else
  {
    PRINT("MQTT is NOT enabled in the config.\r\n");
  }
}

void onMqttConnect(bool sessionPresent)
{
  PRINT("Connected to MQTT.\r\n\tSession present: %d\r\n", sessionPresent);

  File configFile = SPIFFS.open(CONFIG_FILE_MQTT_PUBSUB, "r");
  if (!configFile)
  {
    DEBUGMQTT("Failed to open config file\r\n");
    configFile.close();
    return;
  }

  size_t size = configFile.size();
  DEBUGMQTT("CONFIG_FILE_MQTT_PUBSUB file size: %d bytes\r\n", size);
  if (size > 1024)
  {
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
  DynamicJsonBuffer jsonBuffer;

  JsonObject &json = jsonBuffer.parseObject(buf.get());

  //  JsonVariant json;
  //  json = jsonBuffer.parse(buf.get());

  if (!json.success())
  {
    PRINT("Failed to parse MQTT config file\r\n");
    return;
  }

#ifndef RELEASEMQTT
  // String temp;
  json.prettyPrintTo(DEBUGPORT);
  // Serial.println(temp);
#endif

  // publish 1
  const char *pub1_basetopic = _config.hostname;
  const char *pub1_topicprefix = json[FPSTR(pgm_pub1_topicprefix)];
  char bufPub1_topic[48];
  strlcpy(bufPub1_topic, pub1_basetopic, sizeof(bufPub1_topic));

  // check if topic prefix is available or not
  // if yes, add "/" as needed
  if (strncmp(pub1_topicprefix, "", 1) == 0)
  {
    //do nothing
  }
  else
  {
    char bufSlash[] = "/";
    strncat(bufPub1_topic, bufSlash, sizeof(bufPub1_topic));
    strncat(bufPub1_topic, pub1_topicprefix, sizeof(bufPub1_topic));
  }

  uint8_t pub1_qos = json[FPSTR(pgm_pub1_qos)];
  bool pub1_retain = json[FPSTR(pgm_pub1_retain)];
  const char *pub1_payload = json[FPSTR(pgm_pub1_payload)];
  uint16_t packetIdPub1 = mqttClient.publish(
      bufPub1_topic,                //topic
      json[FPSTR(pgm_pub1_qos)],    //qos
      json[FPSTR(pgm_pub1_retain)], //retain
      json[FPSTR(pgm_pub1_payload)] //payload
  );
  PRINT(
      "Publishing packetId: %d\r\n\ttopic: %s\r\n\tQoS: %d\r\n\tretain: %d\r\n\tpayload: %s\r\n",
      packetIdPub1, bufPub1_topic, pub1_qos, pub1_retain, pub1_payload);

  // subscribe 1
  const char *sub1_topic = json[FPSTR(pgm_sub1_topic)];
  uint8_t sub1_qos = json[FPSTR(pgm_sub1_qos)];
  uint16_t packetIdSub1 = mqttClient.subscribe(sub1_topic, sub1_qos);
  PRINT("Subscribing packetId: %d\r\n\ttopic: %s\r\n\tQoS: %d\r\n", packetIdSub1, sub1_topic, sub1_qos);

  //subscribe 2
  if (false)
  {
    const char *sub2_topic = json[FPSTR(pgm_sub2_topic)];
    uint8_t sub2_qos = json[FPSTR(pgm_sub2_qos)];
    uint16_t packetIdSub2 = mqttClient.subscribe(sub2_topic, sub2_qos);
    PRINT("Subscribing packetId: %d\r\n\ttopic: %s\r\n\tQoS: %d\r\n", packetIdSub2, sub2_topic, sub2_qos);
  }
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason)
{
  PRINT("Disconnected from MQTT.\r\n");

  if (WiFi.isConnected())
  {
    mqttReconnectTimer.once(2, connectToMqtt);
  }
}

void onMqttSubscribe(uint16_t packetId, uint8_t qos)
{
  PRINT("Subscribe acknowledged.\r\n\tpacketId: %d\r\n\tqos: %d\r\n", packetId, qos);
}

void onMqttUnsubscribe(uint16_t packetId)
{
  PRINT("Unsubscribe acknowledged.\r\n\tpacketId: %d\r\n", packetId);
}

void onMqttMessage(char *topic, char *payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total)
{
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
  for (size_t i = 0; i < len; i++)
  {
    DEBUGMQTT("%c", (char)payload[i]);
  }
  DEBUGMQTT("\r\n");

  char t[64];
  strcpy(t, topic);

  File configFile = SPIFFS.open(CONFIG_FILE_MQTT_PUBSUB, "r");
  if (!configFile)
  {
    DEBUGMQTT("Failed to open MQTT config file\r\n");
    configFile.close();
    return;
  }

  size_t size = configFile.size();
  //DEBUGMQTT("CONFIG_FILE_MQTT_PUBSUB file size: %d bytes\r\n", size);
  if (size > 1024)
  {
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
  DynamicJsonBuffer jsonBuffer;
  //StaticJsonBuffer<1024> jsonBuffer;

  JsonObject &json = jsonBuffer.parseObject(buf.get());

  //  JsonVariant json;
  //  json = jsonBuffer.parse(buf.get());

  if (!json.success())
  {
    DEBUGMQTT("Failed to parse MQTT config file\r\n");
    return;
  }

  //#ifndef RELEASEMQTT
  //  String temp;
  //  json.prettyPrintTo(temp);
  //  Serial.println(temp);
  //#endif

  const char *sub1_topic = json["sub1_topic"];
  // uint8_t subscribe_1_qos =  json["subscribe_1"][1];
  // const char* subscribe_2_topic = json["subscribe_2"][0];
  // uint8_t subscribe_2_qos =  json["subscribe_2"][1];

  //handle energy meter payload
  if (strncmp(const_cast<char *>(topic), sub1_topic, json[FPSTR(sub1_topic)].measureLength()) == 0)
  {
    lastTimePayloadReceived = 0;
    
    //copy payload
    //    for (int i = 0; i < len; i++) {
    //      //DEBUGMQTT("%c", (char)payload[i]);
    //      bufWatt[i] = (char)payload[i];
    //    }
    //    bufWatt[len] = '\0';

    DynamicJsonBuffer jsonBuffer;
    //StaticJsonBuffer<512> jsonBuffer;
    JsonObject &root = jsonBuffer.parseObject(payload);

    const char *voltage;
    const char *watt;
    if (root["voltage"].success())
    {
      voltage = root["voltage"];
      strlcpy(bufVoltage, voltage, sizeof(bufVoltage));
      dtostrf(atof(bufVoltage), 1, 0, bufVoltage);
    }
    if (root["watt"].success())
    {
      watt = root["watt"];
      strlcpy(bufWatt, watt, sizeof(bufWatt));
      dtostrf(atof(bufWatt), 1, 0, bufWatt);
    }

    if (wsConnected && clientVisitConfigMqttPage)
    {
      size_t len = root.measureLength();
      char buf[len + 1];
      root.printTo(buf, sizeof(buf));
      ws.text(clientID, buf);
    }
  }
}

void onMqttPublish(uint16_t packetId)
{
  DEBUGMQTT("Publish acknowledged.\r\n");
  DEBUGMQTT("  packetId: %d\r\n", packetId);
}

bool mqtt_setup()
{
  //load config
  load_config_mqtt();

  // register wifi handler for mqtt
  wifiConnectHandlerForMqtt = WiFi.onStationModeGotIP(onWifiGotIPForMqtt);
  wifiDisconnectHandlerForMqtt = WiFi.onStationModeDisconnected(onWifiDisconnectForMqtt);

  //register mqtt handler
  mqttClient.onConnect(onMqttConnect);
  mqttClient.onDisconnect(onMqttDisconnect);
  mqttClient.onSubscribe(onMqttSubscribe);
  mqttClient.onUnsubscribe(onMqttUnsubscribe);
  mqttClient.onMessage(onMqttMessage);
  mqttClient.onPublish(onMqttPublish);

  return true;
}

void mqtt_loop()
{
  static bool mqttEnabled = configMqtt.enabled;
  if (mqttEnabled != configMqtt.enabled)
  {
    mqttEnabled = configMqtt.enabled;
    if (configMqtt.enabled)
    {
      if (!mqttClient.connected())
      {
        connectToMqtt();
        // mqttClient.connect();
      }
    }
    else
    {
      if (mqttClient.connected())
      {
        mqttClient.disconnect();
      }
    }
  }

  static time_t prevDisplay;
  if (utcTime != prevDisplay)
  {
    prevDisplay = utcTime;
    lastTimePayloadReceived++;
  }

  if (lastTimePayloadReceived >= 5 || !mqttClient.connected() || !WiFi.isConnected())
  {
    char NA[] = "N/A";
    strlcpy(bufWatt, NA, sizeof(bufWatt));
    strlcpy(bufVoltage, NA, sizeof(bufWatt));
  }
}
