/*
 * ESP8266_Blink —— NodeMCU (ESP-12E/12F) 板载 LED 闪灯程序
 *
 * 板载 LED 说明：
 *   - NodeMCU 的板载 LED 接在 GPIO2（即 Arduino 引脚 D4 / LED_BUILTIN）
 *   - 该 LED 为“低电平点亮”：digitalWrite(LED_BUILTIN, LOW) 亮，HIGH 灭
 *
 * 烧录：
 *   Arduino IDE -> 开发板选 "NodeMCU 1.0 (ESP-12E Module)"，直接点上传即可。
 */

void setup() {
  // 将板载 LED 引脚设为输出
  pinMode(LED_BUILTIN, OUTPUT);
}

void loop() {
  // 点亮（低电平有效）
  digitalWrite(LED_BUILTIN, LOW);
  delay(500);

  // 熄灭
  digitalWrite(LED_BUILTIN, HIGH);
  delay(500);
}
