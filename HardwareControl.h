#ifndef HARDWARE_CONTROL_H
#define HARDWARE_CONTROL_H

#include <Arduino.h>
#define PILL_SENSOR_PIN 15        // 替换为实际传感器引脚

// 硬件控制功能
void turnDisk(int pillBoxID);
void pickPill(int pillAmount);
void autoDispense(int pillBoxID, int pillAmount);

#endif
