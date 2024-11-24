#include <WiFi.h>
#include <WiFiClient.h>
#include <ESP32TimerInterrupt.h>
#include <RTClib.h>  // 引入RTC库

#define REMINDER_INTERVAL 600000  // 提醒间隔10分钟
#define WIFI_SSID "WIFI_SSID"
#define WIFI_PASSWORD "WIFI_PASSWORD"
#define PORT 80
#define PILL_SENSOR_PIN 15  // 假设传感器引脚

ESP32Timer timer(0);
RTC_DS3231 rtc;  // 创建RTC对象

// 指示是否使用 Serial 调试和 WiFi 功能
bool useSerial = true;  // 默认启用串口调试
bool useWiFi = true;    // 默认启用 WiFi 功能

// 吃药时间结构，以天为单位
struct PillSchedule{
    int hour;    // 小时
    int minute;  // 分钟
    int amount;  // 一次吃的数量
};

// 列表中的药品信息结构
struct Pill {
    int pillBoxID;  // 药盒编号
    String pillType;
    PillSchedule pillSchedule[4];  // 吃药时间表
    bool lastTaken = true;  // 上次是否已吃
    PillSchedule nextTakeTime;  // 下次吃药时间
    PillSchedule nextReminderTime;  // 下次提醒时间
};

// 全局药品列表
Pill Pills[8];
bool havePill[8] = {1, 1, 0, 0, 0, 0, 0, 0};

// 初始化WiFi服务器
WiFiServer server(PORT);

void initPillList() {
    Pills[0] = {1, "A药", {{8, 0, 2}, {20, 0, 2}, {-1, 0, 0}, {-1, 0, 0}}, false, {8, 0, 2}, {8, 0}};
    Pills[1] = {2, "B药", {{0, 0, 1}, {6, 0, 1}, {12, 0, 1}, {18, 0, 1}}, false, {6, 0, 1}, {6, 0}};
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
    Pills[pillBoxID - 1] = {pillBoxID, pillType, {}, true, {0, 0, 0}, {0, 0}};
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

void handleWiFiInput() {
    if (!useWiFi) return;

    WiFiClient client = server.available();
    if (client) {
        String input = client.readStringUntil('\n');
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

// 日志消息输出函数
// 日志消息输出函数
void logMessage(String message) {
    if (useSerial) {
        Serial.println(message);
    }
    if (useWiFi) {
        WiFiClient client = server.available();
        if (client && client.connected()) {
            client.println(message);
        }
    }
}

bool timerCallback() {
    checkPills();
    return true;
}

// WiFi和服务器初始化
void initWiFi() {
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        logMessage("正在连接WiFi...");
    }
    logMessage("WiFi已连接");
    server.begin();
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

void setup() {
    Serial.begin(115200);
    if (!rtc.begin()) {
        logMessage("RTC无法找到！");
        while (1);
    }
    if (rtc.lostPower()) {
        rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));  // 设置为编译时的时间
    }

    initWiFi();
    initPillList();
    pinMode(PILL_SENSOR_PIN, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(PILL_SENSOR_PIN), pillTaken, FALLING);

    if (timer.attachInterruptInterval(REMINDER_INTERVAL, timerCallback)) {
        logMessage("定时提醒功能已启动");
    } else {
        logMessage("定时器初始化失败");
    }
}

void loop() {
    handleClient();
    checkPills();
}
