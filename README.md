# 智能药盒代码实现设计

## 功能

- [ ] 在规定时间内没有吃药时进行提醒

- [ ] 记录吃药状态（今天是否已吃药）

- [ ] 与手机端APP交互（设置吃药时间等，或者自动识别处方？）

- [ ] 自动取出要吃的药

- [ ] 扫码（处方）自动录入药的种类及吃药时间  

- [ ] 取几天需要的药

## 实现方案

核心：维护一个列表，根据列表以及输入来进行动作的输出
```c++
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
```



列表内容：

|          | 药盒编号pillBoxID | 药盒中药的种类pillType | 吃药时间表pillSchedule[4]       | 上次是否已吃lastTaken | 下次出药时间netTakeTime | 下次提醒时间nextReminderTime |
| -------- | ----------------- | ---------------------- | ------------------------------- | --------------------- | ----------------------- | ---------------------------- |
| 数据类型 | int               | string                 | PillSchedule{int, int, int}数组 | bool                  | PillSchedule            | PillSchedule                 |
| 说明     | 数组下标+1        |                        | 小时，分钟，一次吃的数量        |                       |                         |                              |

- 在规定时间内没有吃药时进行提醒：用RTC计时，如果达到了列表中任意元素的下次提醒时间则进行提醒

  - 下次提醒时间：根据吃药频率计算提醒时间，如果上次未吃隔较短时间（如10min）后进行持续提醒

  - 提醒：语音播报/文字显示，可以把到了提醒时间的药用一个列表记录

    - 例如有一种A药是一天吃两次，那可以在把A药的吃药时间表记为[8:00, 20:00]；还有一种B药是6个小时吃一次，那就把B药的吃药时间表记为[0:00, 6:00, 12:00, 18:00]。

      假如现在是凌晨3点，那A药的下次提醒时间就是8:00，B药的下次提醒时间是6:00。

      到了6点自动取出一粒B药并将B加入提醒列表进行提醒，B药的上次是否已吃状态为未吃，下次提醒时间为6:10。到了6:10还未吃则变为6:20...

      直到8:00B药还未被取走，此时也到了A药的吃药时间，因此取出A药并将A也加入提醒列表进行提醒

      直到该药被取走后下次提醒时间变为12:00

- 与手机端APP交互（设置吃药时间等，或者自动识别处方？）

  - esp32开发板和手机接入同一个wifi以进行通信
  - 手机通过向开发板ip发送消息来设置吃药时间，消息的格式应该包含：药盒编号、药盒中药的种类、吃药时间表。开发板接收到消息之后新增或修改列表条目
  - esp32开发板将列表发送给手机，在手机app上显示
  - 用手机识别处方的功能在手机上完成，然后手机将其转换为上面的消息格式发送给esp32

- 自动取出要吃的药

  - 输入：要取的药的列表

  - 过程：遍历列表，根据要取的药的种类和数量调用电机转动转盘和滑轨及气泵进行吸取

    - 电机转动转盘：函数turn_disk(int pill_index)，其中参数pill_index是要取出的药所在的药盒编号，要求将指定的药盒转到待取位置

    - 吸取：函数pick_pill(int pill_number)，其中pill_number是要取出的数量，要求控制气泵和滑轨从待取位置进行pill_number次吸取动作

### 通信模块
设计中可以使用serial或wifi进行通信，通过`useSerial`和`useWifi`两个变量控制是否启用。  
使用WIFI时，连接到mqtt服务器。
mqtt服务器采用[EMQX的免费公共服务器](https://www.emqx.com/zh/mqtt/public-mqtt5-broker)  
| mqtt_broker  | topic  | username  | password  |
|--------------|--------|-----------|-----------|
|broker-cn.emqx.io|xrobot/pill_box|xrobot|xrobot|

mqtt服务器接受信息时会调用回调函数`mqttCallback`,串口消息则通过在`loop`中调用`handleSerialInput`进行处理。  
接收到的字符串传给`handleCommand`函数进行处理，目前可供使用的指令如下：

| 指令       | 功能                              | 示例                                             |
|------------|-----------------------------------|-------------------------------------------------|
| `show`     | 显示当前所有药盒的药品信息。      | `show`                                          |
| `add`      | 添加新的药品到指定药盒。          | `add 1 A药 8:0:2,12:0:2,-1:0:0,-1:0:0`         |
| `delete`   | 删除指定药盒中的药品信息。        | `delete 1`                                      |
| `update`   | 更新指定药盒中的药品信息。        | `update 1 B药 7:0:1,19:0:1,-1:0:0,-1:0:0`      |
| 其他指令   | 提示未知指令并要求用户检查输入。  | `unknown_command` -> 返回"未知指令，请检查输入。" |

输出则使用`logMessage`函数  

... 待补充

## 调试进度
目前已经实现了与mqtt服务器的通信。RTC模块存在bug，其他模块还未调试。