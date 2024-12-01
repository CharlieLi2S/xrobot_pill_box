#include "PillManager.h"
#include "NetworkManager.h"

// 初始化药品列表
void initPillList() {
    PillSchedule scheduleA[4] = {{8, 0, 2}, {20, 0, 2}, {-1, 0, 0}, {-1, 0, 0}};
    PillSchedule scheduleB[4] = {{0, 0, 1}, {6, 0, 1}, {12, 0, 1}, {18, 0, 1}};

    Pills[0] = Pill(1, "A药", scheduleA, false, {8, 0, 2}, {8, 0});
    Pills[1] = Pill(2, "B药", scheduleB, false, {6, 0, 1}, {6, 0});

    havePill = {1, 1, 0, 0, 0, 0, 0, 0};
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

void IRAM_ATTR pillTaken() { //TODO: 还没写中断
    for (int i = 0; i < 8; i++) {
        Pills[i].lastTaken = true;
    }
}

