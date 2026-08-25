/*
 * ESP8266_LED_Web —— WiFi 热点 + 网页控制 LED
 *
 * 功能:
 *   1. ESP8266 开启 AP 热点（默认 SSID: ESP8266-LED，密码: 12345678）
 *   2. 手机连上该热点后，浏览器打开 http://192.168.4.1
 *   3. 页面上的按钮控制 D2(GPIO4) 引脚上 LED 的亮灭
 *
 * 接线（常见接法，高电平点亮）:
 *   D2(GPIO4) -- 220Ω 电阻 -- LED 阳极
 *   LED 阴极 -- GND
 *
 * 若你的 LED 接在 3V3 和 D2 之间（低电平点亮），
 * 把下面 digitalWrite 里的 HIGH/LOW 对调即可。
 */

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

// ---------- 热点配置 ----------
const char* AP_SSID = "ESP8266-LED";   // 热点名称（手机里搜索这个）
const char* AP_PASS = "12345678";      // 热点密码（至少 8 位；留空 "" 则为开放网络）

// ---------- LED 引脚 ----------
const int LED_PIN = D2;                // NodeMCU 丝印 D2 = GPIO4

ESP8266WebServer server(80);

// ---------- 网页（移动端友好） ----------
const char index_html[] = R"rawliteral(
<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>ESP8266 LED 控制</title>
<style>
  body { font-family: -apple-system, sans-serif; text-align: center; background:#f4f4f4; margin:0; padding:20px; }
  .card { background:#fff; border-radius:14px; padding:30px 20px; max-width:340px; margin:40px auto; box-shadow:0 2px 10px rgba(0,0,0,.12); }
  h1 { color:#333; font-size:22px; margin-top:0; }
  #status { font-weight:bold; color:#555; }
  #btn { width:100%; padding:26px; font-size:22px; border:none; border-radius:12px; color:#fff; cursor:pointer; margin-top:20px; }
  #btn.on  { background:#e53935; }   /* 红灯 = 已开，点击关闭 */
  #btn.off { background:#43a047; }   /* 绿灯 = 已关，点击打开 */
  .tip { color:#999; font-size:12px; margin-top:16px; }
</style>
</head>
<body>
<div class="card">
  <h1>ESP8266 LED 控制</h1>
  <p>LED 当前状态：<span id="status">读取中…</span></p>
  <button id="btn" class="off" onclick="toggle()">开灯</button>
  <p class="tip">D2 (GPIO4)</p>
</div>
<script>
let on = false;
function refresh() {
  document.getElementById('status').textContent = on ? '已打开' : '已关闭';
  const btn = document.getElementById('btn');
  btn.textContent = on ? '关灯' : '开灯';
  btn.className = on ? 'on' : 'off';
}
function toggle() {
  fetch(on ? '/led/off' : '/led/on')
    .then(r => r.text())
    .then(t => { on = (t === 'ON'); refresh(); });
}
// 页面加载时同步真实状态
fetch('/status')
  .then(r => r.text())
  .then(t => { on = (t === 'ON'); refresh(); });
</script>
</body>
</html>
)rawliteral";

// ---------- 处理函数 ----------
void handleRoot() {
  server.send(200, "text/html", index_html);
}

void handleLedOn() {
  digitalWrite(LED_PIN, HIGH);   // 高电平点亮（若低电平点亮改为 LOW）
  server.send(200, "text/plain", "ON");
}

void handleLedOff() {
  digitalWrite(LED_PIN, LOW);    // 低电平熄灭（若低电平点亮改为 HIGH）
  server.send(200, "text/plain", "OFF");
}

void handleStatus() {
  bool on = (digitalRead(LED_PIN) == HIGH);
  server.send(200, "text/plain", on ? "ON" : "OFF");
}

void setup() {
  Serial.begin(115200);
  delay(100);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);   // 初始熄灭

  // 开启热点（AP 模式）
  WiFi.mode(WIFI_AP);
  bool ok = WiFi.softAP(AP_SSID, AP_PASS);
  Serial.println();
  Serial.print("热点启动: ");
  Serial.println(ok ? "成功" : "失败");
  Serial.print("热点名称: ");
  Serial.println(AP_SSID);
  Serial.print("IP 地址: ");
  Serial.println(WiFi.softAPIP());   // 一般为 192.168.4.1

  // 注册路由
  server.on("/", handleRoot);
  server.on("/led/on", handleLedOn);
  server.on("/led/off", handleLedOff);
  server.on("/status", handleStatus);

  server.begin();
  Serial.println("Web 服务器已启动，手机连上热点后访问 http://192.168.4.1");
}

void loop() {
  server.handleClient();
}
