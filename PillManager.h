#ifndef PILL_MANAGER_H
#define PILL_MANAGER_H

#include <Arduino.h>
#include <RTClib.h>

// 药品和时间相关结构体
struct PillSchedule {
    int hour;
    int minute;
    int amount;
};

struct Pill {
    int pillBoxID;
    String pillType;
    PillSchedule pillSchedule[4];
    bool lastTaken;
    PillSchedule nextTakeTime;
    PillSchedule nextReminderTime;
    Pill(int id = 0, String type = "", PillSchedule schedule[4] = {}, bool taken = true, 
         PillSchedule nextTake = {0, 0, 0}, PillSchedule nextReminder = {0, 0});
};

// 全局变量
extern Pill Pills[8];
extern bool havePill[8];
extern RTC_DS3231 rtc;  // 创建RTC对象

// 功能函数
void initPillList();
void showPills();
void addPill(int pillBoxID, String pillType, PillSchedule schedules[4]);
void deletePill(int pillBoxID);
void updatePill(int pillBoxID, String pillType, PillSchedule schedules[4]);
void checkPills();
void setNextTakeTime(int pillBoxID);
void sendReminder(String pillType);
void IRAM_ATTR pillTaken();


#endif
