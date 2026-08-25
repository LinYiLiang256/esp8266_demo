/*
 * ESP8266_LCD_Weather —— ST7735S 1.8" 屏幕 天气时钟
 *
 * 功能:
 *   1. SmartConfig 配网：手机 Esptouch App 一键配网（WiFi 信息自动保存，下次直连）
 *   2. NTP 网络对时：显示 日期 / 星期 / 时间
 *   3. 天气获取：Open-Meteo（免费免 key，响应 ~700B，一次请求拿全）
 *      当前温度 / 湿度 / 天气描述 / 当日最高(红)·最低(绿)
 *   4. ST7735S 128x160 屏幕显示，局部刷新不闪烁，彩色界面
 *
 * 接线 (NodeMCU / ESP-12E):
 *   TFT VCC   -> 3V3            TFT GND -> GND
 *   TFT SCL   -> D5 (GPIO14)    TFT SDA -> D7 (GPIO13)
 *   TFT CS    -> D8 (GPIO15)    TFT DC  -> D1 (GPIO5)
 *   TFT RST   -> D2 (GPIO4)     TFT BL  -> 3V3 (常亮)
 *
 * 城市坐标: 下方 LAT/LON 填你所在城市经纬度（百度搜"城市名 经纬度"）。
 *           默认合肥 (31.82, 117.28)。
 *
 * 屏幕偏色/边缘彩带: 把下方 initR 参数依次换
 *   INITR_18GREENTAB / INITR_18REDTAB / INITR_GREENTAB / INITR_REDTAB / INITR_BLACKTAB
 */

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <ArduinoJson.h>
#include <time.h>

// ================= 屏幕引脚 (ST7735S 1.8" 128x160) =================
#define TFT_CS   15    // D8
#define TFT_DC   5     // D1
#define TFT_RST  4     // D2
#define TFT_MOSI 13    // D7
#define TFT_SCLK 14    // D5
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST);

// ================= 颜色 (RGB565) =================
#define TFT_BLACK   0x0000
#define TFT_WHITE   0xFFFF
#define TFT_RED     0xF800
#define TFT_GREEN   0x07E0
#define TFT_CYAN    0x07FF
#define TFT_YELLOW  0xFFE0
#define TFT_ORANGE  0xFD20
#define TFT_SKY     0x7DFF   // 天蓝

// ================= 可配置项 =================
const float LAT = 31.82;                 // 纬度（默认合肥；改成你所在城市）
const float LON = 117.28;                // 经度（默认合肥）
const int   TIMEZONE = 8;                // 时区（北京时间 UTC+8）
const unsigned int WEATHER_INTERVAL = 600;    // 天气刷新间隔(秒)
const char* NTP_SERVER1 = "ntp.aliyun.com";
const char* NTP_SERVER2 = "pool.ntp.org";

// ================= 数据缓存 =================
char weatherDesc[24] = "--";    // 天气描述
char weatherTemp[8]  = "--";    // 当前温度, 如 "31"
char weatherHum[8]   = "--";    // 湿度, 如 "62%"
char weatherMax[8]   = "--";    // 当日最高
char weatherMin[8]   = "--";    // 当日最低

// ================= 显示状态（局部刷新） =================
int  lastDateKey = -1;
int  lastH = -1, lastM = -1, lastS = -1;
char lastDesc[24] = "", lastTemp[8] = "", lastHum[8] = "";
char lastMax[8] = "", lastMin[8] = "";

// 浮点取整
int iround(float v) { return (int)(v >= 0 ? v + 0.5f : v - 0.5f); }

// ================= WMO 天气码 -> 文本 =================
const char* weatherText(int code) {
  switch (code) {
    case 0:  return "Clear Sky";
    case 1: case 2: case 3:            return "Partly Cloudy";
    case 45: case 48:                  return "Fog";
    case 51: case 53: case 55:         return "Drizzle";
    case 56: case 57:                  return "Freezing Drizzle";
    case 61: case 63: case 65:         return "Rain";
    case 66: case 67:                  return "Freezing Rain";
    case 71: case 73: case 75:         return "Snow";
    case 77:                           return "Snow Grains";
    case 80: case 81: case 82:         return "Showers";
    case 85: case 86:                  return "Snow Showers";
    case 95:                           return "Thunderstorm";
    case 96: case 99:                  return "Storm & Hail";
    default: return "--";
  }
}

// ================= 显示工具 =================
void centerText(const char* s, int y, uint8_t size, uint16_t color) {
  int16_t x1, y1;
  uint16_t w, h;
  tft.setTextSize(size);
  tft.setTextColor(color);
  tft.getTextBounds(s, 0, 0, &x1, &y1, &w, &h);
  tft.setCursor((128 - w) / 2, y);
  tft.print(s);
}

void showMsg(const char* msg) {
  tft.fillScreen(TFT_BLACK);
  tft.setCursor(4, 70);
  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE);
  tft.print(msg);
}

// 按单词折行显示
void drawWrap(const char* s, int y, uint8_t size, int maxChars, uint16_t color) {
  String line = "", rest = s;
  int ypos = y;
  while (rest.length() > 0 && ypos < 160) {
    int sp = rest.indexOf(' ');
    String word = (sp >= 0) ? rest.substring(0, sp) : rest;
    if (line.length() > 0 && line.length() + 1 + word.length() > maxChars) {
      centerText(line.c_str(), ypos, size, color);
      ypos += 7 * size + 2;
      line = word;
    } else {
      if (line.length() > 0) line += " ";
      line += word;
    }
    rest = (sp >= 0) ? rest.substring(sp + 1) : "";
  }
  if (line.length() > 0) centerText(line.c_str(), ypos, size, color);
}

// ================= 天气获取 (Open-Meteo, 小 JSON 直接解析) =================
bool fetchWeather() {
  String url = "http://api.open-meteo.com/v1/forecast?latitude="
             + String(LAT, 2) + "&longitude=" + String(LON, 2)
             + "&current=temperature_2m,relative_humidity_2m,weather_code"
             + "&daily=temperature_2m_max,temperature_2m_min"
             + "&timezone=Asia%2FShanghai";

  WiFiClient wclient;
  HTTPClient http;
  http.setTimeout(10000);
  if (!http.begin(wclient, url)) return false;   // 3.1.x 要求 begin(WiFiClient, url)
  int code = http.GET();
  if (code != 200) { http.end(); return false; }
  String body = http.getString();
  http.end();
  if (body.length() < 50) return false;

  JsonDocument doc;                              // v7 自动管理内存，无需容量参数
  if (deserializeJson(doc, body)) return false;

  float t    = doc["current"]["temperature_2m"] | 0.0f;
  int   h    = doc["current"]["relative_humidity_2m"] | 0;
  int   wc   = doc["current"]["weather_code"] | -1;
  float tmax = doc["daily"]["temperature_2m_max"][0] | 0.0f;
  float tmin = doc["daily"]["temperature_2m_min"][0] | 0.0f;

  snprintf(weatherTemp, sizeof(weatherTemp), "%d", iround(t));
  snprintf(weatherHum,  sizeof(weatherHum),  "%d%%", h);
  snprintf(weatherMax,  sizeof(weatherMax),  "%d", iround(tmax));
  snprintf(weatherMin,  sizeof(weatherMin),  "%d", iround(tmin));
  strncpy(weatherDesc, weatherText(wc), sizeof(weatherDesc) - 1);
  weatherDesc[sizeof(weatherDesc) - 1] = 0;
  return true;
}

// ================= 当前温度显示（自绘 ° 符号） =================
void drawTempDeg(const char* tempStr, int y) {
  int n = strlen(tempStr);
  int wText = n * 18;                 // size3: 每字符 6*3=18px
  int degR = 3;                       // ° 圆半径
  int totalW = wText + (degR * 2 + 2) + 18;
  int x = (128 - totalW) / 2;

  tft.setTextSize(3);
  tft.setTextColor(TFT_ORANGE);
  tft.setCursor(x, y);
  tft.print(tempStr);
  x += wText;

  // 自绘 ° 小圆（内置字体无 ° 字符）
  tft.drawCircle(x + degR, y + 6, degR, TFT_ORANGE);
  x += degR * 2 + 2;

  tft.setCursor(x, y);
  tft.print("C");
}

// ================= 最高温(红)/最低温(绿) =================
void drawHilo(const char* tmax, const char* tmin, int y) {
  char sMax[8], sMin[8];
  snprintf(sMax, sizeof(sMax), "H%s", tmax);
  snprintf(sMin, sizeof(sMin), "L%s", tmin);
  int gap = 16;
  int wMax = strlen(sMax) * 12;   // size2: 每字符 6*2=12px
  int wMin = strlen(sMin) * 12;
  int x0 = (128 - (wMax + gap + wMin)) / 2;

  tft.setTextSize(2);
  tft.setCursor(x0, y);
  tft.setTextColor(TFT_RED);
  tft.print(sMax);
  tft.setCursor(x0 + wMax + gap, y);
  tft.setTextColor(TFT_GREEN);
  tft.print(sMin);
}

// ================= 主界面（局部刷新 + 彩色） =================
void drawDisplay(struct tm* t) {
  static const char* wd[] = {"SUN","MON","TUE","WED","THU","FRI","SAT"};
  char buf[24];
  int dateKey = (t->tm_mon + 1) * 100 + t->tm_mday;

  // ---- 日期+星期（青色）：跨天或首次 ----
  if (dateKey != lastDateKey) {
    lastDateKey = dateKey;
    tft.fillRect(0, 0, 128, 20, TFT_BLACK);
    snprintf(buf, sizeof(buf), "%02d-%02d %s", t->tm_mon + 1, t->tm_mday, wd[t->tm_wday]);
    centerText(buf, 4, 2, TFT_CYAN);
  }

  // ---- 时间（黄色大字）：变化时 ----
  if (t->tm_hour != lastH || t->tm_min != lastM) {
    lastH = t->tm_hour; lastM = t->tm_min;
    tft.fillRect(0, 22, 128, 36, TFT_BLACK);
    snprintf(buf, sizeof(buf), "%02d:%02d", t->tm_hour, t->tm_min);
    centerText(buf, 30, 4, TFT_YELLOW);
  }

  // ---- 秒(白) + 湿度(绿)：每秒或湿度变化 ----
  if (t->tm_sec != lastS || strcmp(weatherHum, lastHum) != 0) {
    lastS = t->tm_sec;
    strncpy(lastHum, weatherHum, sizeof(lastHum) - 1);
    lastHum[sizeof(lastHum) - 1] = 0;
    tft.fillRect(0, 58, 128, 18, TFT_BLACK);
    snprintf(buf, sizeof(buf), "%02d", t->tm_sec);
    tft.setTextSize(1);
    tft.setTextColor(TFT_WHITE);
    tft.setCursor(8, 62);
    tft.print(buf);
    tft.setTextColor(TFT_GREEN);
    tft.setCursor(96, 62);
    tft.print(weatherHum);
  }

  // ---- 天气描述（天蓝色）：内容变化时 ----
  if (strcmp(weatherDesc, lastDesc) != 0) {
    strncpy(lastDesc, weatherDesc, sizeof(lastDesc) - 1);
    lastDesc[sizeof(lastDesc) - 1] = 0;
    tft.fillRect(0, 76, 128, 36, TFT_BLACK);
    drawWrap(weatherDesc, 90, 1, 21, TFT_SKY);
  }

  // ---- 当前温度（橙色大字 + °C）：内容变化时 ----
  if (strcmp(weatherTemp, lastTemp) != 0) {
    strncpy(lastTemp, weatherTemp, sizeof(lastTemp) - 1);
    lastTemp[sizeof(lastTemp) - 1] = 0;
    tft.fillRect(0, 112, 128, 30, TFT_BLACK);
    drawTempDeg(weatherTemp, 118);
  }

  // ---- 当日最高(红)/最低(绿)：内容变化时 ----
  if (strcmp(weatherMax, lastMax) != 0 || strcmp(weatherMin, lastMin) != 0) {
    strncpy(lastMax, weatherMax, sizeof(lastMax) - 1);
    lastMax[sizeof(lastMax) - 1] = 0;
    strncpy(lastMin, weatherMin, sizeof(lastMin) - 1);
    lastMin[sizeof(lastMin) - 1] = 0;
    tft.fillRect(0, 142, 128, 18, TFT_BLACK);
    drawHilo(weatherMax, weatherMin, 144);
  }
}

// ================= 主程序 =================
void setup() {
  Serial.begin(115200);
  delay(100);

  // ST7735S 1.8": 偏色/边缘彩带就换 INITR_18REDTAB / GREENTAB / REDTAB / BLACKTAB
  tft.initR(INITR_18GREENTAB);
  tft.setRotation(0);          // 竖屏 128x160
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE);
  tft.setTextWrap(false);

  // ---- 配网：已保存 WiFi 优先，否则 SmartConfig ----
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);

  showMsg("WiFi...");
  WiFi.begin();                       // 尝试已保存的网络
  unsigned long w0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - w0 < 8000) delay(300);

  if (WiFi.status() != WL_CONNECTED) {
    // 进入 SmartConfig，等待手机配网
    showMsg("SmartConfig");
    tft.println("Esptouch App");
    Serial.println("Waiting SmartConfig...");
    WiFi.beginSmartConfig();
    unsigned long sc0 = millis();
    while (!(WiFi.smartConfigDone() && WiFi.status() == WL_CONNECTED)) {
      if (millis() - sc0 > 120000) {   // 120 秒超时自动重启，重新进入配网
        ESP.restart();
      }
      delay(200);
    }
    while (WiFi.status() != WL_CONNECTED) delay(200);
  }

  Serial.print("Connected, IP: ");
  Serial.println(WiFi.localIP());

  // ---- NTP 对时 ----
  showMsg("Syncing...");
  configTime(TIMEZONE * 3600, 0, NTP_SERVER1, NTP_SERVER2);
  unsigned long n0 = millis();
  while (time(nullptr) < 1600000000 && millis() - n0 < 15000) delay(300);
  Serial.println("NTP synced");

  // ---- 首次获取天气 ----
  if (fetchWeather()) Serial.println("Weather OK");
}

void loop() {
  static unsigned long lastWeather = 0;
  static unsigned long lastTick = 0;

  // 断线重连（含提示）
  if (WiFi.status() != WL_CONNECTED) {
    showMsg("Reconnect...");
    WiFi.reconnect();
    delay(3000);
    return;
  }

  // 定时刷新天气
  if (millis() - lastWeather > WEATHER_INTERVAL * 1000UL) {
    lastWeather = millis();
    fetchWeather();
  }

  // 每秒刷新界面（局部刷新，不整屏清空）
  time_t now = time(nullptr);
  if (now > 1600000000 && millis() - lastTick >= 1000) {
    lastTick = millis();
    drawDisplay(localtime(&now));
  }

  delay(50);
}