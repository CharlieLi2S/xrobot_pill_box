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

void addPill(Pill pill) {
    int pillBoxID = pill.pillBoxID;
    if (havePill[pillBoxID - 1]) {
        Serial.println("该药盒已经有药了");
    } else {
        Pills[pillBoxID - 1] = pill;
        Serial.println("已添加药品");
        havePill[pillBoxID - 1] = 1;
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
    Serial.println("提醒：" + pillType + " 的服药时间到了！");
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
        Serial.println("正在连接WiFi...");
    }
    Serial.println("WiFi已连接");
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
        Serial.println("RTC无法找到！");
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
        Serial.println("定时提醒功能已启动");
    } else {
        Serial.println("定时器初始化失败");
    }
}

void loop() {
    handleClient();
    checkPills();
}
