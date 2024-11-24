#include <WiFi.h>
#include <WiFiClient.h>
#include <ESP32TimerInterrupt.h>
#include <RTClib.h>
#include <PubSubClient.h>

#define REMINDER_INTERVAL 600000  // 提醒间隔10分钟
#define WIFI_SSID "WIFI_SSID"     // 替换为WiFi名称
#define WIFI_PASSWORD "WIFI_PASSWORD" // 替换为WiFi密码
#define PORT 80                   // 替换为实际服务器端口
#define PILL_SENSOR_PIN 15        // 替换为实际传感器引脚
#define MAX_RETRIES 20            // 网络最大连接次数
#define MQTT_CALLBACK 1           // 是否开启MQTT回调函数

// 设置MQTT broker信息
const char *mqtt_broker = "broker-cn.emqx.io";
const char *topic = "mqtt_hbb_example";
const char *mqtt_username = "emqx";
const char *mqtt_password = "public";
const int mqtt_port = 1883;
const char *client_id = "mqtt-client-hbb-example";

WiFiClient espClient;
PubSubClient client(espClient);

ESP32Timer timer(0);
RTC_DS3231 rtc;  // 创建RTC对象

bool useSerial = true;
bool useWiFi = true;

// 吃药时间结构
struct PillSchedule {
    int hour;
    int minute;
    int amount;
};

// 列表中的药品信息结构
struct Pill {
    int pillBoxID;
    String pillType;
    PillSchedule pillSchedule[4];
    bool lastTaken = true;
    PillSchedule nextTakeTime;
    PillSchedule nextReminderTime;
};

Pill Pills[8];
bool havePill[8] = {1, 1, 0, 0, 0, 0, 0, 0};

// 初始化药品列表
void initPillList() {
    Pills[0] = {1, "A药", {{8, 0, 2}, {20, 0, 2}, {-1, 0, 0}, {-1, 0, 0}}, false, {8, 0, 2}, {8, 0}};
    Pills[1] = {2, "B药", {{0, 0, 1}, {6, 0, 1}, {12, 0, 1}, {18, 0, 1}}, false, {6, 0, 1}, {6, 0}};
}

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

// 主循环
void loop() {
    if (!WiFi.isConnected()) {
        logMessage("WiFi断开，尝试重新连接...");
        connectWifi();
    }
    if (!client.connected()) {
        logMessage("MQTT断开，尝试重新连接...");
        connectMqtt();
    }

    // 保持 MQTT 的连接
    client.loop();

    // 处理其他功能
    handleSerialInput();
    checkPills();
}

// 初始化函数
void setup() {
    Serial.begin(115200);
    if (!rtc.begin()) {
        logMessage("RTC初始化失败");
        while (1);
    }
    if (rtc.lostPower()) {
        rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }

    initPillList();

    // 初始化 WiFi 和 MQTT
    if (!connectWifi() || !connectMqtt()) {
        logMessage("网络或MQTT初始化失败，进入离线模式");
    }

    // 定时器初始化
    if (timer.attachInterruptInterval(REMINDER_INTERVAL, timerCallback)) {
        logMessage("定时器启动成功");
    } else {
        logMessage("定时器启动失败");
    }
}
