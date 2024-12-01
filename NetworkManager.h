#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

#include <WiFi.h>
#include <PubSubClient.h>

#define WIFI_SSID "WIFI_SSID"     // 替换为WiFi名称
#define WIFI_PASSWORD "WIFI_PASSWORD" // 替换为WiFi密码
#define PORT 80                   // 替换为实际服务器端口
#define MQTT_CALLBACK 1           // 是否开启MQTT回调函数
#define MAX_RETRIES 20            // 网络最大连接次数

extern bool useSerial = true;
extern bool useWiFi = true;

extern PubSubClient client;

bool connectWifi();
bool connectMqtt();
void logMessage(String message);
void publishMessage(const char *topic, String message);
void mqttCallback(char *topic, byte *payload, unsigned int length);
void handleCommand(String command);
void handleSerialInput();

#endif
