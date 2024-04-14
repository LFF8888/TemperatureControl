#include <cstring>
#include <EEPROM.h>

// 定义EEPROM的总大小，这里选择足够大的空间来存储100行2列的int数组
// 每个int占用4个字节，100行2列则是100*2*4 = 800字节
#define EEPROM_SIZE 1024
int addr = 0;

//温度段数组
const int max_rows = 100; // 最大段数
int newData[max_rows][2];  // 原始数据数组
int loadData[max_rows][2]; // 用于加载数据的数组

#define LED 13

int KEY1 = 6;   //设定
int KEY2 = 10;  //减
int KEY3 = 3;   //加
int KEY4 = 2;   //左移

// 用于存储从串口读取的字符
char receivedChar;

// 记录当前选择的菜单项
int menuOption = 0;

//用于测试的变量
int i = 0;

//计时相关变量
unsigned long startTime = 0; // 存储计时开始或重置的时间点
unsigned long currentTime = 0; // 存储当前时间
unsigned long lastPrintTime = 0; // 上一次打印时间
int currentEvent = 0; // 当前检查的事件索引

// 定义一个全局变量，用于跟踪上一次插值的时间
unsigned long lastInterpolationTime = 0;
// 每次插值间隔（秒）
const int interpolationInterval = 1;

void setup() {
    delay(5000);  //等待温控器启动

    Serial.begin(9600);

    // 初始化引脚
    pinMode(LED, OUTPUT);
    pinMode(KEY1, OUTPUT);
    pinMode(KEY2, OUTPUT);
    pinMode(KEY3, OUTPUT);
    pinMode(KEY4, OUTPUT);
    // 关闭所有按键
    digitalWrite(KEY1, HIGH);
    digitalWrite(KEY2, HIGH);
    digitalWrite(KEY3, HIGH);
    digitalWrite(KEY4, HIGH);

    setTempZero();  // 设置温度为零

    startTime = millis();   // 重置开始时间

    // 初始化EEPROM
    if (!EEPROM.begin(EEPROM_SIZE)) {
        Serial.println("EEPROM初始化失败");
        while (1); // 如果初始化失败，则停止程序运行
    }
}

void loop() {
    unsigned long startTime = millis(); // 获取当前时间
    bool receivedData = false; // 标记是否接收到数据

    // 打印菜单
    Serial.println(F("\n"));
    Serial.println(F("\n"));
    Serial.println(F("\n"));   
    Serial.println(F("主菜单:"));
    Serial.println(F("a. 执行设定值（15秒后自动执行）"));
    Serial.println(F("b. 重新设定值"));
    Serial.println(F("c. 手动操作"));
    Serial.println(F("请输入a, b或c选择选项\n"));

    // 等待用户选择，最多等待15秒
    while ((millis() - startTime) < 15000) {
        if (Serial.available() > 0) { // 如果串口否有数据
            char receivedChar = Serial.read(); // 读取数据
            Serial.print("您选择了: ");
            Serial.println(receivedChar);
            Serial.print("\n");
            receivedData = true; // 标记已接收到数据
            Serial.read(); // 读取并丢弃缓冲区中的所有剩余数据

            // 根据接收到的字符处理不同的菜单项
            switch (receivedChar) {
                case 'a':
                    menuOption = 1; // 执行设定值
                    Serial.println(F("正在执行设定值，输入z返回主菜单\n"));
                    executeSetting();   // 执行设定值
                    break;

                case 'b':
                    menuOption = 2; // 重设设定值
                    Serial.println(F("请重新设定值，格式为1,23,45（英文逗号）（温度点1，在23分钟设置温度为45度）输入z保存并返回主菜单\n"));
                    resetSetting();     // 重设设定值
                    break;

                case 'c':
                    menuOption = 3; // 手动操作
                    Serial.println(F("温控器由您手动控制，输入z返回主菜单\n"));
                    manualOperation();
                    break;

                default:
                    // 无效输入
                    Serial.println(F("无效输入，请输入a, b或c!\n"));
                    break;
            }

            break; // 跳出循环
        }
    }

    // 如果15秒内未接收到数据，则执行设定值
    if (!receivedData) {
        executeSetting();
    }
}

// 返回检查(z字符)
int returnCheck(){
    if (Serial.available() > 0) { // 如果串口否有数据
        char receivedChar = Serial.read(); // 读取数据
        Serial.read();
        if (receivedChar == 'z') {  // 如果接收到的字符是'z'
            Serial.println(F("检测到输入z，将返回主菜单"));
            return 1;
        }
    return 0;
    }
}

// 执行设定值
void executeSetting() {
    // 从EEPROM读取数据
    addr = 0;
    for (int i = 0; i < 100; i++) {
        for (int j = 0; j < 2; j++) {
        loadData[i][j] = EEPROM.get(addr, loadData[i][j]);
        addr += sizeof(int);  // 更新地址
        }
    }

    // 打印设定值数组
    Serial.println("\n设定值：");
    for (int i = 0; i < max_rows; i++) {
        // 检查当前行是否已经是默认的0值，如果是，则不打印后续的数据
        if (loadData[i][0] == 0 && loadData[i][1] == 0) break;

        Serial.print("温度点");
        Serial.print(i + 1); // 显示温度点的编号，从1开始
        Serial.print("：");
        Serial.print(loadData[i][0]); // 打印分钟数
        Serial.print("分钟 ");
        Serial.print(loadData[i][1]); // 打印温度值
        Serial.println("度");
    }

    startTime = millis(); // 重置开始时间
    currentEvent = 0;

    while (1) {
        if (returnCheck() == 1) return;

        printTime();  // 打印时间
        tempEvent();  // 处理温度事件
    }
}

// 重置设定值
void resetSetting() {
    memset(newData, 0, sizeof(newData));    //初始化数组为0
    while (1) {
        String received = Serial.readStringUntil('\n'); // 读取串口直到遇到换行符
        Serial.read();

        if (received == "z") {
            // 如果接收到的是字符'z'，保存并退出
            // 将数据写入EEPROM
            addr = 0;
            for (int i = 0; i < 100; i++) {
                for (int j = 0; j < 2; j++) {
                EEPROM.put(addr, newData[i][j]);
                addr += sizeof(int);  // 更新地址，每次增加一个int的大小
                }
            }
            EEPROM.commit();  // 确保所有数据都写入到EEPROM
            return; 
        }
        
        int rowIndex, val1, val2;
        char comma1, comma2;
        // 尝试按照 "行索引,值1,值2" 的格式解析接收到的字符串
        if (sscanf(received.c_str(), "%d%c%d%c%d", &rowIndex, &comma1, &val1, &comma2, &val2) == 5) {
            // 检查分隔符是否为逗号且行索引是否在有效范围内
            if (comma1 == ',' && comma2 == ',' && rowIndex >= 1 && rowIndex <= max_rows) {
                // 调整 rowIndex 以适应从 0 开始的索引
                rowIndex -= 1;
                // 更新数组
                newData[rowIndex][0] = val1;
                newData[rowIndex][1] = val2;
                
                // 打印设定值数组
                Serial.println("\n设定值：");
                for (int i = 0; i < max_rows; i++) {
                    // 检查当前行是否已经是默认的0值，如果是，则不打印后续的数据
                    if (newData[i][0] == 0 && newData[i][1] == 0) break;

                    Serial.print("温度点");
                    Serial.print(i + 1); // 显示温度点的编号，从1开始
                    Serial.print("：");
                    Serial.print(newData[i][0]); // 打印分钟数
                    Serial.print("分钟 ");
                    Serial.print(newData[i][1]); // 打印温度值
                    Serial.println("度");
                }
            }
            else {
                Serial.println("解析输入错误");
            }
        }
    }
}

// 手动操作模式
void manualOperation() {
    while (1) {
        if (returnCheck() == 1) return;
    }
}

// // 处理温度事件
// void tempEvent() {
//   if (currentEvent < max_rows && loadData[currentEvent][0] != 0) { // 检查是否还有事件需要处理
//     unsigned long currentTime = (millis() - startTime) / 1000; // 计算当前时间（秒）
//     if (currentTime >= loadData[currentEvent][0]) { // 检查是否到达或超过事件指定的时间
//       setTemp(loadData[currentEvent][1]); // 调用setTemp函数

//       Serial.print("温度已经设定为: ");
//       Serial.println(loadData[currentEvent][1]);

//       currentEvent++; // 移动到下一个事件
//     }
//   }
// }

void tempEvent() {
  if (currentEvent < max_rows - 1 && loadData[currentEvent][0] != 0) { // 确保至少还有一个后续事件
    unsigned long currentTime = (millis() - startTime) / 1000 / 60; // 计算当前时间（分钟）

    // 当时间达到当前事件指定的时间时
    if (currentTime >= loadData[currentEvent][0]) {
      // 设置初始温度
      setTemp(loadData[currentEvent][1]);
      Serial.print("温度已经设定为: ");
      Serial.println(loadData[currentEvent][1]);

      // 准备开始插值到下一个事件的温度
      currentEvent++;
      lastInterpolationTime = currentTime; // 重置插值时间
    }
    else if (currentTime - lastInterpolationTime >= interpolationInterval && currentEvent > 0) {
      // 插值计算
      float timeDiff = loadData[currentEvent][0] - loadData[currentEvent - 1][0];
      if (timeDiff > 0) {
        float tempDiff = loadData[currentEvent][1] - loadData[currentEvent - 1][1];
        float fraction = (currentTime - loadData[currentEvent - 1][0]) / timeDiff;
        int interpolatedTemp = loadData[currentEvent - 1][1] + round(tempDiff * fraction);

        // 更新温度
        setTemp(interpolatedTemp);
        Serial.print("插值温度更新为: ");
        Serial.println(interpolatedTemp);
      }
      lastInterpolationTime = currentTime; // 更新插值时间
    }
  }
}

// 打印时间戳
void printTime() {
  currentTime = millis(); // 获取当前时间点

  if (currentTime - lastPrintTime >= 10000) { // 每隔至少1000毫秒更新一次
    digitalWrite(LED, HIGH);  // turn the KEY on (HIGH is the voltage level)
    
    lastPrintTime = currentTime; // 更新上一次打印时间
    
    unsigned long elapsed = currentTime - startTime; // 计算经过的时间
    unsigned long seconds = elapsed / 1000; // 总秒数
    unsigned long minutes = seconds / 60; // 总分钟数
    unsigned long hours = minutes / 60; // 总小时数
    unsigned long days = hours / 24; // 总天数

    // 计算剩余的小时、分钟和秒
    hours = hours % 24;
    minutes = minutes % 60;
    seconds = seconds % 60;

    // 打印当前运行时间
    Serial.print("程序已运行 ");
    Serial.print(days);
    Serial.print(" 天 ");
    Serial.print(hours);
    Serial.print(" 小时 ");
    Serial.print(minutes);
    Serial.print(" 分 ");
    Serial.print(seconds);
    Serial.println(" 秒");
    delay(10);
    digitalWrite(LED, LOW);  // turn the KEY on (HIGH is the voltage level)
  }
}

// 设置温控器温度为0
void setTempZero(){
  // KEY1按一次
  digitalWrite(KEY1, LOW);delay(50);digitalWrite(KEY1, HIGH);delay(50);

  // KEY4按3次
  for (int i = 0; i < 3; i++) {
    digitalWrite(KEY4, LOW);delay(25);digitalWrite(KEY4, HIGH);delay(25);
  }

  // KEY2按2次
  for (int i = 0; i < 2; i++) {
    digitalWrite(KEY2, LOW);delay(25);digitalWrite(KEY2, HIGH);delay(25);
  }

  // KEY1按一次
  digitalWrite(KEY1, LOW);delay(50);digitalWrite(KEY1, HIGH);delay(50);
}

// 设置温控器温度
void setTemp(int a) {
  setTempZero();    // 设置温度为零

  // KEY1按一次,表示启动
  digitalWrite(KEY1, LOW);delay(50);digitalWrite(KEY1, HIGH);delay(50);

  // 获取三位数的个位数
  int units = a % 10;
  // 让KEY3闪烁units次
  for (int i = 0; i < units; i++) {
    digitalWrite(KEY3, LOW);delay(25);digitalWrite(KEY3, HIGH);delay(25);
  }

  // KEY4按一次,表示前移一位
  digitalWrite(KEY4, LOW);delay(25);digitalWrite(KEY4, HIGH);delay(25);

  // 获取三位数的十位数
  int tens = (a / 10) % 10;
  // 让KEY3闪烁tens次
  for (int i = 0; i < tens; i++) {
    digitalWrite(KEY3, LOW);delay(25);digitalWrite(KEY3, HIGH);delay(25);
  }

  // KEY4按一次,表示前移一位
  digitalWrite(KEY4, LOW);delay(25);digitalWrite(KEY4, HIGH);delay(25);

  // 获取三位数的百位数
  int hundreds = a / 100;
  // 让KEY3闪烁hundreds次
  for (int i = 0; i < hundreds; i++) {
    digitalWrite(KEY3, LOW);delay(25);digitalWrite(KEY3, HIGH);delay(25);
  }

  // KEY1按一次,表示结束
  digitalWrite(KEY1, LOW);delay(50);digitalWrite(KEY1, HIGH);delay(50);
}