#include "PillManager.h"
#include "NetworkManager.h"

// 检查当前时间是否到达设定的时间
bool timeReached(PillSchedule timeSet) {
    DateTime now = rtc.now();
    return (now.hour() == timeSet.hour && now.minute() == timeSet.minute);
}