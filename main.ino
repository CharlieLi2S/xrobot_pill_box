#include "PillManager.h"
#include "NetworkManager.h"
#include "HardwareControl.h"

void setup() {
    Serial.begin(115200);
    // 初始化 WiFi 和 MQTT
    if (!connectWifi() || !connectMqtt()) {
        logMessage("网络或MQTT初始化失败，进入离线模式");
    }

    if (!rtc.begin()) {
        logMessage("RTC初始化失败！程序终止");
        while (true);
    }

    initPillList();

    // 设置传感器引脚为外部中断源
    pinMode(PILL_SENSOR_PIN, INPUT_PULLUP);  // 设置引脚为输入，并启用上拉电阻
    attachInterrupt(digitalPinToInterrupt(PILL_SENSOR_PIN), pillTaken, FALLING);  // 当引脚信号为LOW时触发中断
    
    logMessage("系统初始化完成。");
}

void loop() {
    handleSerialInput();
    if (!WiFi.isConnected()) {
        connectWifi();
    }
    if (!client.connected()) {
        connectMqtt();
    }
    client.loop();  // 处理MQTT消息
    checkPills();
}
