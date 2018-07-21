

#include "mqtt.h"
// #include "config.h"
//#include "FSWebServerLib.h"

#include <Arduino.h>

#define PRINTPORT Serial
#define DEBUGPORT Serial

#define RELEASEMQTT

#define PROGMEM_T __attribute__((section(".irom.text.template")))

#ifndef RELEASEMQTT
#define DEBUGMQTT(fmt, ...)                   \
  {                                           \
    static const char pfmt[] PROGMEM_T = fmt; \
    DEBUGPORT.printf_P(pfmt, ##__VA_ARGS__);  \
  }
#else
#define DEBUGMQTT(...)
#endif

bool gotWifiForMqttFlag = false;
bool wifiDisconnectedForMqttFlag = false;
bool mqttConnectedFlag = false;
bool mqttDisconnectedFlag = false;
bool mqttPublishFlag = false;
bool mqttSubscribeFlag = false;
bool mqttUnsubscribeFlag = false;
bool mqttOnMessageFlag = false;

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

// bool enabled = true;
static const char pgm_default_server[] PROGMEM = "192.168.10.3";
// uint16_t port = 1883;
const char pgm_default_user[] PROGMEM = "test";
const char pgm_default_pass[] PROGMEM = "test";
// const char pgm_default_clientid[] PROGMEM; // will be set during setup
// uint16_t keepalive = 30;
// bool cleansession = true;
const char pgm_default_lwttopicprefix[] PROGMEM = "mqttstatus";
// uint8_t lwtqos = 2;
// bool lwtretain = true;
const char pgm_default_lwtpayload[] PROGMEM = "DISCONNECTED";

// char pub1_basetopic[64] = strConfigMqtt.clientid;
// char pub1_topicprefix[16] = "mqttstatus";
// uint8_t pub1_qos = 2;
// bool pub1_retain = true;
// char pub1_payload[64] = "CONNECTED";
// char sub1_topic[64] = "ESP13579541/meterreading/1s";
// uint8_t sub1_qos = 0;
// char sub2_topic[64] = "/rumah/sts/1s/kwh1/voltage";
// uint8_t sub2_qos = 0;

const char pgm_default_pub1_basetopic[] PROGMEM = "lwtpayload";

// MQTT config struct
strConfigMqtt configMqtt;

// void load_default_mqtt_config()
// {
//   configMqtt.enabled = true;
//   strcpy_P(configMqtt.server, pgm_default_server);
//   configMqtt.port = 1883;
//   char user[32] = "test";
//   char pass[64] = "test";
//   char clientid[24]; // will be set during setup
//   configMqtt.keepalive = 30;
//   bool cleansession = true;
//   char lwttopicprefix[64] = "mqttstatus";
//   configMqtt.lwtqos = 2;
//   configMqtt.lwtretain = true;
//   // configMqtt.lwtpayload[] = "DISCONNECTED";
// }

bool load_config_mqtt()
{
  DEBUGMQTT("%s\r\n", __PRETTY_FUNCTION__);

  // const char *p = pgm_configfilemqtt;
  // JsonObject &root = ParseConfigFile(p, 1024);

  JsonObject &root = ParseConfigFile(FPSTR(pgm_configfilemqtt), 1024);

  if (!root.success())
  {
    PRINT("Failed parsing config MQTT file\r\n");
    return false;
  }

  root.prettyPrintTo(PRINTPORT);
  PRINT("\r\n");

  configMqtt.enabled = root[FPSTR(pgm_mqtt_enabled)];
  strlcpy(configMqtt.server, root[FPSTR(pgm_mqtt_server)], sizeof(configMqtt.server) / sizeof(configMqtt.server[0]));
  configMqtt.port = root[FPSTR(pgm_mqtt_port)];
  strlcpy(configMqtt.user, root[FPSTR(pgm_mqtt_user)], sizeof(configMqtt.user) / sizeof(configMqtt.user[0]));
  strlcpy(configMqtt.pass, root[FPSTR(pgm_mqtt_pass)], sizeof(configMqtt.pass) / sizeof(configMqtt.pass[0]));
  strlcpy(configMqtt.clientid, _config.hostname, sizeof(configMqtt.clientid) / sizeof(configMqtt.clientid[0]));
  configMqtt.keepalive = root[FPSTR(pgm_mqtt_keepalive)];
  configMqtt.cleansession = root[FPSTR(pgm_mqtt_cleansession)];

  //lwt
  strlcpy(configMqtt.lwttopicprefix, root[FPSTR(pgm_mqtt_lwttopicprefix)], sizeof(configMqtt.lwttopicprefix) / sizeof(configMqtt.lwttopicprefix[0]));
  configMqtt.lwtqos = root[FPSTR(pgm_mqtt_lwtqos)];
  configMqtt.lwtretain = root[FPSTR(pgm_mqtt_lwtretain)];
  strlcpy(configMqtt.lwtpayload, root[FPSTR(pgm_mqtt_lwtpayload)], sizeof(configMqtt.lwtpayload) / sizeof(configMqtt.lwttopicprefix[0]));

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
  // DEBUGMQTT("%s\n", __PRETTY_FUNCTION__);
  gotWifiForMqttFlag = true;

  // connectToMqtt();
}

void onWifiDisconnectForMqtt(const WiFiEventStationModeDisconnected &event)
{
  // DEBUGMQTT("Disconnected from Wi-Fi. Detaching mqttReconnectTimer.\r\n");
  mqttReconnectTimer.detach(); // ensure we don't reconnect to MQTT while reconnecting to Wi-Fi

  //wifiReconnectTimer.once(2, connectToWifi);

  wifiDisconnectedForMqttFlag = true;
}

void connectToMqtt()
{
  DEBUGMQTT("%s\n", __PRETTY_FUNCTION__);  

  if (configMqtt.enabled)
  {
    load_config_mqtt();

    PRINT("\r\nConnecting to MQTT...\r\n");

    //lwt
    const char *lwtbasetopic = _config.hostname;
    const char *lwttopicprefix = configMqtt.lwttopicprefix;
    char buflwttopic[64];
    strlcpy(buflwttopic, lwtbasetopic, sizeof(buflwttopic) / sizeof(buflwttopic[0]));

    // check if lwt topic prefix is available or not
    // if yes, add "/" as needed
    if (strncmp(lwttopicprefix, "", 1) == 0)
    {
      //do nothing
      PRINT("lwt topic prefix is not available. Do nothing...\r\n");
    }
    else
    {
      char bufSlash[] = "/";
      strncat(buflwttopic, bufSlash, sizeof(buflwttopic) / sizeof(buflwttopic[0]));
      strncat(buflwttopic, lwttopicprefix, sizeof(buflwttopic) / sizeof(buflwttopic[0]));
    }

    mqttClient.setServer(configMqtt.server, configMqtt.port);

    if (strcmp(configMqtt.user, "") == 0 || strcmp(configMqtt.pass, "") == 0)
    {
      // do nothing
      PRINT("configMqtt.user or configMqtt.pass is empty. Do nothing...\r\n");
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
  mqttConnectedFlag = true;
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason)
{
  mqttDisconnectedFlag = true;
}

void onMqttSubscribe(uint16_t packetId, uint8_t qos)
{
  // DEBUGMQTT("Subscribe acknowledged.\r\n\tpacketId: %d\r\n\tqos: %d\r\n", packetId, qos);
  mqttSubscribeFlag = true;
}

void onMqttUnsubscribe(uint16_t packetId)
{
  // DEBUGMQTT("Unsubscribe acknowledged.\r\n\tpacketId: %d\r\n", packetId);
  mqttUnsubscribeFlag = true;
}

char bufTopic[32];
char bufPayload[256];

void onMqttMessage(char *topic, char *payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total)
{
  mqttOnMessageFlag = true;

  DEBUGMQTT("Payload received\r\n  topic: %s\r\n  qos: %d\r\n  dup: %d\r\n  retain: %d\r\n  len: %d\r\n  index: %d\r\n  total: %d\r\n",
  topic, properties.qos, properties.dup, properties.retain, len, index, total);

  size_t lenTopic = strlen(topic) + 1;
  DEBUGMQTT("topic len:%d, payload len:%d", lenTopic, len);

  if (lenTopic >= 32)
  {
    DEBUGMQTT("Topic length is too large!");
    return;
  }

  if (len >= 256)
  {
    DEBUGMQTT("Payload length is too large!");
    return;
  }

  for (size_t i = 0; i < lenTopic; i++)
  {
    bufTopic[i] = (char)topic[i];
  }
  bufTopic[lenTopic] = '\0';

  for (size_t i = 0; i < len; i++)
  {
    bufPayload[i] = (char)payload[i];
  }
  bufPayload[len] = '\0';
}

void onMqttPublish(uint16_t packetId)
{
  // DEBUGMQTT("\r\n%s\r\n", __PRETTY_FUNCTION__);
  // DEBUGMQTT("Publish acknowledged.\r\n");
  // DEBUGMQTT("  packetId: %d\r\n", packetId);
  mqttPublishFlag = true;
}

bool mqtt_setup()
{
  DEBUGMQTT("\r\n%s\r\n", __PRETTY_FUNCTION__);

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

  // set Client ID
  // SetMqttClientId();

  // load config
  // This will load saved client Id
  // load_config_mqtt();

  return true;
}

void SetMqttClientId()
{
  char bufPrefix[] = "ESP";
  char bufChipId[11];
  itoa(ESP.getChipId(), bufChipId, 10);

  //char bufHostName[32];
  strlcpy(configMqtt.clientid, bufPrefix, sizeof(configMqtt.clientid));
  strncat(configMqtt.clientid, bufChipId, sizeof(bufChipId));
}

JsonObject &ParseConfigFile(const char *filename, size_t allocSize)
{
  PRINT("Opening config file...\r\n");
  File file = SPIFFS.open(filename, "r");
  if (!file)
  {
    PRINT("Failed to open config file\r\n");
    file.close();
    // return false;
  }

  PRINT("Config file size: %d bytes\r\n", file.size());

  if (file.size() > allocSize)
  {
    PRINT("Config file size is larger than %d bytes!\r\n", allocSize);
    file.close();
    // return false;
  }

  // DynamicJsonBuffer jsonBuffer(allocSize);

  StaticJsonBuffer<1024> jsonBuffer;
  JsonObject &json = jsonBuffer.parseObject(file);

  file.close();
  return json;
}

JsonObject &ParseConfigFile(const __FlashStringHelper *filename, size_t allocSize)
{
  PRINT("Opening config file...\r\n");
  File file = SPIFFS.open(filename, "r");
  if (!file)
  {
    PRINT("Failed to open config file\r\n");
    file.close();
    // return false;
  }

  PRINT("Config file size: %d bytes\r\n", file.size());

  if (file.size() > allocSize)
  {
    PRINT("PUBSUB file size is larger than %d bytes!\r\n", allocSize);
    file.close();
    // return false;
  }

  DynamicJsonBuffer jsonBuffer(allocSize);

  // StaticJsonBuffer<1024> jsonBuffer;
  JsonObject &json = jsonBuffer.parseObject(file);

  file.close();
  return json;
}

void MqttConnectedCb()
{
  // PRINT("Connected to MQTT.\r\n\tSession present: %d\r\n", sessionPresent);
  PRINT("Connected to MQTT.\r\n");

  PRINT("Opening PUBSUB config file...\r\n");
  File file = SPIFFS.open(CONFIG_FILE_MQTT_PUBSUB, "r");
  if (!file)
  {
    PRINT("Failed to open PUBSUB file\r\n");
    file.close();
    return;
  }

  PRINT("PUBSUB file size: %d bytes\r\n", file.size());

  const uint16_t allocSize = 512;

  if (file.size() > allocSize)
  {
    PRINT("PUBSUB file size is larger than %d bytes!\r\n", allocSize);
    file.close();
    return;
  }

  StaticJsonBuffer<allocSize> jsonBuffer;
  JsonObject &json = jsonBuffer.parseObject(file);

  file.close();

  if (!json.success())
  {
    PRINT("Failed to parse MQTT config file\r\n");
    return;
  }

  // #ifndef RELEASEMQTT
  json.prettyPrintTo(DEBUGPORT);
  PRINT("\r\n");
  // #endif

  // publish 1
  const char *pub1_basetopic = _config.hostname;
  const char *pub1_topicprefix = json[FPSTR(pgm_pub1_topicprefix)];
  char bufPub1_topic[48];
  strlcpy(bufPub1_topic, pub1_basetopic, sizeof(bufPub1_topic) / sizeof(bufPub1_topic[0]));

  // check if topic prefix is available or not
  // if yes, add "/" as needed
  if (strncmp(pub1_topicprefix, "", 1) == 0)
  {
    //do nothing
  }
  else
  {
    char bufSlash[] = "/";
    strncat(bufPub1_topic, bufSlash, sizeof(bufPub1_topic) / sizeof(bufPub1_topic[0]));
    strncat(bufPub1_topic, pub1_topicprefix, sizeof(bufPub1_topic) / sizeof(bufPub1_topic[0]));
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
      "\r\nPublishing packetId: %d\r\n\ttopic: %s\r\n\tQoS: %d\r\n\tretain: %d\r\n\tpayload: %s\r\n",
      packetIdPub1, bufPub1_topic, pub1_qos, pub1_retain, pub1_payload);

  // subscribe 1
  const char *sub1_topic = json[FPSTR(pgm_sub1_topic)];
  uint8_t sub1_qos = json[FPSTR(pgm_sub1_qos)];
  uint16_t packetIdSub1 = mqttClient.subscribe(sub1_topic, sub1_qos);
  PRINT("\r\nSubscribing packetId: %d\r\n\ttopic: %s\r\n\tQoS: %d\r\n", packetIdSub1, sub1_topic, sub1_qos);

  //subscribe 2
  if (false)
  {
    const char *sub2_topic = json[FPSTR(pgm_sub2_topic)];
    uint8_t sub2_qos = json[FPSTR(pgm_sub2_qos)];
    uint16_t packetIdSub2 = mqttClient.subscribe(sub2_topic, sub2_qos);
    PRINT("\r\nSubscribing packetId: %d\r\n\ttopic: %s\r\n\tQoS: %d\r\n", packetIdSub2, sub2_topic, sub2_qos);
  }
}

void MqttDisconnectedCb()
{
  PRINT("Disconnected from MQTT.\r\n");

  if (configMqtt.enabled)
  {
    if (WiFi.status() == WL_CONNECTED)
    {
      PRINT("Turn on mqttReconnectTimer.\r\n");
      mqttReconnectTimer.once(5, connectToMqtt);
    }
    else
    {
      PRINT("WiFi is disconnected. Cannnot start mqttReconnectTimer.\r\n");
    }
  }
}

void mqtt_loop()
{
  if (gotWifiForMqttFlag)
  {
    gotWifiForMqttFlag = false;
    PRINT("gotWifiForMqttFlag\r\n");

    if (configMqtt.enabled)
    {
      if (!mqttClient.connected())
      {
        PRINT("Turn on mqttReconnectTimer.\r\n");
        mqttReconnectTimer.once(5, connectToMqtt);
      }
    }
  }

  if (wifiDisconnectedForMqttFlag)
  {
    wifiDisconnectedForMqttFlag = false;
    PRINT("wifiDisconnectedForMqttFlag\r\n");
  }

  static bool mqttEnabled = configMqtt.enabled;
  if (mqttEnabled != configMqtt.enabled)
  {
    mqttEnabled = configMqtt.enabled;
    if (configMqtt.enabled)
    {
      if (!mqttClient.connected())
      {
        PRINT("Turn on mqttReconnectTimer.\r\n");
        mqttReconnectTimer.once(5, connectToMqtt);
      }
    }
    else
    {
      PRINT("MQTT is disabled!\r\n");
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

  // if (lastTimePayloadReceived >= 5 || !mqttClient.connected() || !WiFi.isConnected())
  // {
  //   char NA[] = "N/A";
  //   strlcpy(bufWatt, NA, sizeof(bufWatt) / sizeof(bufWatt[0]));
  //   strlcpy(bufVoltage, NA, sizeof(bufVoltage) / sizeof(bufVoltage[0]));
  // }

  if (lastTimePayloadReceived >= 5 || !mqttClient.connected() || WiFi.status() != WL_CONNECTED)
  {
    char NA[] = "N/A";
    strlcpy(bufWatt, NA, sizeof(bufWatt) / sizeof(bufWatt[0]));
    strlcpy(bufVoltage, NA, sizeof(bufVoltage) / sizeof(bufVoltage[0]));
  }

  if (mqttConnectedFlag)
  {
    mqttConnectedFlag = false;
    MqttConnectedCb();
  }

  if (mqttDisconnectedFlag)
  {
    mqttDisconnectedFlag = false;
    MqttDisconnectedCb();
  }

  if (mqttPublishFlag)
  {
    mqttPublishFlag = false;
    DEBUGMQTT("Publish acknowledged.\r\n");
    // DEBUGMQTT("  packetId: %d\r\n", packetId);
  }

  if (mqttSubscribeFlag)
  {
    mqttSubscribeFlag = false;
    // DEBUGMQTT("Subscribe acknowledged.\n  packetId: %d\n  qos: %d\r\n", packetId, qos);
    DEBUGMQTT("Subscribe acknowledged.\r\n");
  }

  if (mqttUnsubscribeFlag)
  {
    mqttUnsubscribeFlag = false;
    // DEBUGMQTT("Unsubscribe acknowledged.\n  packetId: %d\r\n", packetId);
    DEBUGMQTT("Unsubscribe acknowledged.\r\n");
  }

  if (mqttOnMessageFlag)
  {
    mqttOnMessageFlag = false;

    const char *sub1_topic = configMqtt.sub1_topic;

    //handle energy meter payload
    // if (strncmp(const_cast<char *>(topic), sub1_topic, json[FPSTR(sub1_topic)].measureLength()) == 0)
    if (strncmp(bufTopic, sub1_topic, strlen(sub1_topic)) == 0)
    {
      lastTimePayloadReceived = 0;

      DEBUGMQTT("MQTT received [%s]\r\n", bufTopic);
      DEBUGMQTT("Payload: %s\r\n", bufPayload);

      StaticJsonBuffer<256> jsonBuffer;
      JsonObject &root = jsonBuffer.parseObject(bufPayload);

      if (!root.success())
      {
        return;
      }

      const char *voltage;
      const char *watt;
      if (root["voltage"].success())
      {
        voltage = root["voltage"];
        strlcpy(bufVoltage, voltage, sizeof(bufVoltage) / sizeof(bufVoltage[0]));
        dtostrf(atof(bufVoltage), 1, 0, bufVoltage);
      }
      if (root["watt"].success())
      {
        watt = root["watt"];
        strlcpy(bufWatt, watt, sizeof(bufWatt) / sizeof(bufWatt[0]));
        dtostrf(atof(bufWatt), 1, 0, bufWatt);
      }

      if (wsConnected && clientVisitConfigMqttPage)
      {
        size_t len = root.measureLength();
        char buf[len + 1];
        root.printTo(buf, sizeof(buf) / sizeof(buf[0]));
        ws.text(clientID, buf);
      }
    }
  }
}
