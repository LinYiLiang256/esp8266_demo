# ESP8266_LED_Web

ESP8266 开启 WiFi 热点，手机连上后通过网页按钮控制 LED 亮灭。

## 功能

1. ESP8266 上电后开启 **AP 热点**（默认 `ESP8266-LED` / 密码 `12345678`）
2. 手机连上该热点，浏览器打开 `http://192.168.4.1`
3. 页面按钮控制 **D2（GPIO4）** 引脚上 LED 的亮灭，状态实时同步

## 硬件接线

默认**高电平点亮**接法：

```
D2 (GPIO4) ── 220Ω 电阻 ── LED 阳极
LED 阴极 ── GND
```

若你的 LED 接在 `3V3 ── 电阻 ── LED ── D2`（低电平点亮），把 `ESP8266_LED_Web.ino` 里 `handleLedOn/Off` 中的 `HIGH/LOW` 对调即可（源码内有注释标注）。

## 使用步骤

1. 烧录完成后，手机 WiFi 列表搜索并连接 **`ESP8266-LED`**，密码 `12345678`
2. 浏览器打开 **`http://192.168.4.1`**
3. 点页面按钮：绿灯"开灯" / 红灯"关灯"，LED 随之亮灭

## 自定义配置

修改 `ESP8266_LED_Web.ino` 顶部的常量：

| 常量 | 含义 | 默认值 |
|------|------|--------|
| `AP_SSID` | 热点名称 | `ESP8266-LED` |
| `AP_PASS` | 热点密码（≥8 位；空串 `""` 为开放网络） | `12345678` |
| `LED_PIN` | LED 引脚 | `D2`（GPIO4） |

## 环境要求

- 开发板：NodeMCU（ESP-12E/12F）
- Arduino IDE（或 arduino-cli）
- ESP8266 开发板核心（开发板管理器地址）：
  `http://arduino.esp8266.com/stable/package_esp8266com_index.json`

## 编译与烧录

### 方式一：一条命令（推荐）

```bash
cd ESP8266_LED_Web
./build_flash.sh          # 自动查找工程 -> 编译 -> 检测串口 -> 烧录
```

> 首次使用前需安装 arduino-cli：`sudo pacman -S arduino-cli`（Arch）
> 串口权限：`sudo usermod -aG uucp $USER` 后重新登录

### 方式二：只用已有固件烧录（跳过编译）

```bash
./flash_esp8266.sh        # 直接烧录 build/ 下的 .bin
```

### 方式三：Arduino IDE

1. 打开 `ESP8266_LED_Web.ino`
2. 开发板选 **"NodeMCU 1.0 (ESP-12E Module)"**
3. 端口选 `/dev/ttyUSB0`
4. 点上传

## 网页接口说明

| 路由 | 功能 |
|------|------|
| `GET /`        | 返回控制页面 |
| `GET /led/on`  | 开灯，返回 `ON` |
| `GET /led/off` | 关灯，返回 `OFF` |
| `GET /status`  | 查询状态，返回 `ON` / `OFF` |

## 文件结构

```
ESP8266_LED_Web/
├── ESP8266_LED_Web.ino    # 主程序
├── build_flash.sh         # 通用「编译+烧录」脚本
├── flash_esp8266.sh       # 纯烧录脚本（跳过编译）
└── build/                 # 编译产物（含 ESP8266_LED_Web.ino.bin 固件）
```
