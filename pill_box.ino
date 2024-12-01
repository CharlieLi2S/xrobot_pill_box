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
const char *topic = "xrobot/pill_box";
const char *mqtt_username = "xrobot";
const char *mqtt_password = "xrobot";
const int mqtt_port = 1883;
const char *client_id = "xrobot";

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

    // 自定义构造函数
    Pill(int id, String type, PillSchedule schedule[4], bool taken, PillSchedule nextTake, PillSchedule nextReminder) {
        pillBoxID = id;
        pillType = type;
        for (int i = 0; i < 4; ++i) {
            pillSchedule[i] = schedule[i];
        }
        lastTaken = taken;
        nextTakeTime = nextTake;
        nextReminderTime = nextReminder;
    }
    
    // 默认构造函数
    Pill() {}
};

Pill Pills[8];
bool havePill[8] = {1, 1, 0, 0, 0, 0, 0, 0};

// 初始化药品列表
void initPillList() {
    PillSchedule scheduleA[4] = {{8, 0, 2}, {20, 0, 2}, {-1, 0, 0}, {-1, 0, 0}};
    PillSchedule scheduleB[4] = {{0, 0, 1}, {6, 0, 1}, {12, 0, 1}, {18, 0, 1}};

    Pills[0] = Pill(1, "A药", scheduleA, false, {8, 0, 2}, {8, 0});
    Pills[1] = Pill(2, "B药", scheduleB, false, {6, 0, 1}, {6, 0});
}

// 显示当前药品列表
void showPills() {
    String pillsInfo = "当前药品列表：\n";
    for (int i = 0; i < 8; i++) {
        if (havePill[i]) {
            pillsInfo += "药盒编号: " + String(Pills[i].pillBoxID) + "\n";
            pillsInfo += "药物种类: " + Pills[i].pillType + "\n";
            pillsInfo += "吃药时间表:\n";
            for (int j = 0; j < 4; j++) {
                if (Pills[i].pillSchedule[j].hour != -1) {
                    pillsInfo += "  时间: " + String(Pills[i].pillSchedule[j].hour) + ":" +
                                 String(Pills[i].pillSchedule[j].minute) + " 数量: " +
                                 String(Pills[i].pillSchedule[j].amount) + "\n";
                }
            }
            pillsInfo += "\n";
        }
    }
    logMessage(pillsInfo);
}

// 添加药品到列表
void addPill(int pillBoxID, String pillType, PillSchedule schedules[4]) {
    if (havePill[pillBoxID - 1]) {
        logMessage("该药盒已存在药品，请删除后重新添加。");
        return;
    }
    Pills[pillBoxID - 1] = Pill(pillBoxID, pillType, {}, true, {0, 0, 0}, {0, 0});
    for (int i = 0; i < 4; i++) {
        Pills[pillBoxID - 1].pillSchedule[i] = schedules[i];
    }
    havePill[pillBoxID - 1] = true;
    logMessage("药品已添加成功。");
}

// 删除药品
void deletePill(int pillBoxID) {
    if (!havePill[pillBoxID - 1]) {
        logMessage("药盒中没有药品。");
        return;
    }
    havePill[pillBoxID - 1] = false;
    logMessage("药品已删除成功。");
}

// 更新药品
void updatePill(int pillBoxID, String pillType, PillSchedule schedules[4]) {
    if (!havePill[pillBoxID - 1]) {
        logMessage("药盒中没有药品，请先添加。");
        return;
    }
    Pills[pillBoxID - 1].pillType = pillType;
    for (int i = 0; i < 4; i++) {
        Pills[pillBoxID - 1].pillSchedule[i] = schedules[i];
    }
    logMessage("药品信息已更新。");
}

// 解析并处理指令
void handleCommand(String command) {
    if (command.startsWith("show")) {
        // show指令：输出当前药品列表信息
        // 用途：查看药品信息，包括药盒编号、药品名称、服药时间表等
        // Example: 
        // show
        showPills();
    } else if (command.startsWith("add")) {
        // add指令：添加新的药品信息到指定药盒
        // 用途：向指定药盒中添加药品，包含药品名称和每天的服药时间表
        // Example: 
        // add 1 A药 8:0:2,12:0:2,-1:0:0,-1:0:0
        // 解释：向药盒1添加名称为"A药"的药品，服药时间为8:00和12:00，每次2粒，后两时间为空
        int pillBoxID;
        char pillType[20];
        PillSchedule schedules[4];
        sscanf(command.c_str(), "add %d %s %d:%d:%d,%d:%d:%d,%d:%d:%d,%d:%d:%d",
               &pillBoxID, pillType,
               &schedules[0].hour, &schedules[0].minute, &schedules[0].amount,
               &schedules[1].hour, &schedules[1].minute, &schedules[1].amount,
               &schedules[2].hour, &schedules[2].minute, &schedules[2].amount,
               &schedules[3].hour, &schedules[3].minute, &schedules[3].amount);
        addPill(pillBoxID, String(pillType), schedules);
    } else if (command.startsWith("delete")) {
        // delete指令：删除指定药盒中的药品信息
        // 用途：清空指定药盒的药品信息
        // Example:
        // delete 1
        // 解释：删除药盒1中的药品信息
        int pillBoxID;
        sscanf(command.c_str(), "delete %d", &pillBoxID);
        deletePill(pillBoxID);
    } else if (command.startsWith("update")) {
        // update指令：更新指定药盒中的药品信息
        // 用途：修改现有药品的名称和服药时间表
        // Example: 
        // update 1 B药 7:0:1,19:0:1,-1:0:0,-1:0:0
        // 解释：更新药盒1的药品信息，将药品名称改为"B药"，服药时间为7:00和19:00，每次1粒
        int pillBoxID;
        char pillType[20];
        PillSchedule schedules[4];
        sscanf(command.c_str(), "update %d %s %d:%d:%d,%d:%d:%d,%d:%d:%d,%d:%d:%d",
               &pillBoxID, pillType,
               &schedules[0].hour, &schedules[0].minute, &schedules[0].amount,
               &schedules[1].hour, &schedules[1].minute, &schedules[1].amount,
               &schedules[2].hour, &schedules[2].minute, &schedules[2].amount,
               &schedules[3].hour, &schedules[3].minute, &schedules[3].amount);
        updatePill(pillBoxID, String(pillType), schedules);
    } else {
        // 未知指令：提示用户输入的指令格式错误或不存在
        // 用途：在用户输入错误指令时给出提示
        logMessage("未知指令，请检查输入。");
    }
}


void handleSerialInput() {
    if (Serial.available() > 0) {
        String input = Serial.readStringUntil('\n');
        input.trim();
        handleCommand(input);
    }
}

void IRAM_ATTR pillTaken() {
    for (int i = 0; i < 8; i++) {
        Pills[i].lastTaken = true;
    }
}

// 检查当前时间是否到达设定的时间
bool timeReached(PillSchedule timeSet) {
    DateTime now = rtc.now();
    return (now.hour() == timeSet.hour && now.minute() == timeSet.minute);
}

// 设置下次吃药时间
void setNextTakeTime(int pillBoxID) {
    DateTime now = rtc.now();
    for (int j = 0; j < 4; j++) {
        if (Pills[pillBoxID - 1].pillSchedule[j].hour > now.hour() || 
           (Pills[pillBoxID - 1].pillSchedule[j].hour == now.hour() && Pills[pillBoxID - 1].pillSchedule[j].minute > now.minute())) {
            Pills[pillBoxID - 1].nextTakeTime = Pills[pillBoxID - 1].pillSchedule[j];
            break;
        }
    }
}

// 检查是否达到提醒时间并出药
void checkPills() {
    DateTime now = rtc.now();
    for (int i = 0; i < 8; i++) {
        if (timeReached(Pills[i].nextTakeTime) && Pills[i].lastTaken) {
            Pills[i].lastTaken = false;
            autoDispense(Pills[i].pillBoxID, Pills[i].nextTakeTime.amount);
            setNextTakeTime(Pills[i].pillBoxID);
        }
        if (timeReached(Pills[i].nextReminderTime) && !Pills[i].lastTaken) {
            sendReminder(Pills[i].pillType);
            Pills[i].nextReminderTime.minute += 10;
            if (Pills[i].nextReminderTime.minute >= 60) {
                Pills[i].nextReminderTime.hour++;
                Pills[i].nextReminderTime.minute -= 60;
            }
        }
    }
}

void sendReminder(String pillType) {
    logMessage("提醒：" + pillType + " 的服药时间到了！");
}

bool timerCallback(void* param) {
    checkPills();
    return true;
}

void handleClient() {
    // TODO: 处理APP交互
}

void autoDispense(int pillBoxID, int pillAmount) {
    turnDisk(pillBoxID);
    pickPill(pillAmount);
}

void turnDisk(int pillBoxID) {
    // TODO: 实现转动逻辑
}

void pickPill(int pillAmount) {
    // TODO: 实现取药逻辑
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
