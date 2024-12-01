#include "PillManager.h"
#include "NetworkManager.h"

// MQTT回调函数
void mqttCallback(char *topic, byte *payload, unsigned int length) {
    String message;
    for (unsigned int i = 0; i < length; i++) {
        message += (char)payload[i];
    }
    Serial.print("MQTT消息到达 [");
    Serial.print(topic);
    Serial.println("]: " + message);

    // 调用已有指令解析函数
    handleCommand(message);
}

// 发布MQTT消息
void publishMessage(const char *topic, String message) {
    if (client.connected()) {
        client.publish(topic, message.c_str());
        Serial.println("MQTT消息已发布: " + message);
    } else {
        Serial.println("MQTT未连接，无法发送消息");
    }
}

// 网络连接函数
bool connectWifi() {
    Serial.println("尝试连接WiFi...");
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    for (int i = 0; i < MAX_RETRIES; i++) {
        delay(500);
        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("WiFi连接成功");
            return true;
        }
    }
    Serial.println("WiFi连接失败");
    return false;
}

// MQTT服务器连接函数
bool connectMqtt() {
    client.setServer(mqtt_broker, mqtt_port);
    client.setCallback(mqttCallback);

    for (int i = 0; i < MAX_RETRIES; i++) {
        if (client.connect(client_id, mqtt_username, mqtt_password)) {
            logMessage("MQTT连接成功");
            client.subscribe(topic);  // 订阅主题
            return true;
        }
        delay(1000);
    }
    logMessage("MQTT连接失败");
    return false;
}

// 日志消息输出函数
void logMessage(String message) {
    if (useSerial) {
        Serial.println(message);
    }
    if (useWiFi && client.connected()) {
        publishMessage(topic, message);
    }
}

void handleSerialInput() {
    if (Serial.available() > 0) {
        String input = Serial.readStringUntil('\n');
        input.trim();
        handleCommand(input);
    }
}
