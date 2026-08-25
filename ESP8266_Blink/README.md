# ESP8266_Blink

ESP8266（NodeMCU / ESP-12E）板载 LED 闪灯程序，用于验证开发板的基本烧录与运行。

## 功能

让 NodeMCU 板载的蓝色 LED 以 **0.5 秒** 间隔持续闪烁。

## 硬件说明

- 板载 LED 接在 **GPIO2**（Arduino 引脚 **D4**，即宏 `LED_BUILTIN`）
- 该 LED 为**低电平点亮**：`digitalWrite(LED_BUILTIN, LOW)` 点亮，`HIGH` 熄灭

无需外接任何元件，烧录后即可直接看到板载 LED 闪烁。

## 环境要求

- 开发板：NodeMCU（ESP-12E/12F，CP2102 或 CH340 转串口）
- Arduino IDE（或 arduino-cli）
- ESP8266 开发板核心（开发板管理器地址）：
  `http://arduino.esp8266.com/stable/package_esp8266com_index.json`

## 编译与烧录

### 方式一：一条命令（推荐）

```bash
cd ESP8266_Blink
./build_flash.sh          # 自动查找工程 -> 编译 -> 检测串口 -> 烧录
```

> 首次使用前需安装 arduino-cli：`sudo pacman -S arduino-cli`（Arch）
> 串口权限：`sudo usermod -aG uucp $USER` 后重新登录

### 方式二：只用已有固件烧录（跳过编译）

```bash
./flash_esp8266.sh        # 直接烧录 build/ 下的 .bin
```

### 方式三：Arduino IDE

1. 打开 `ESP8266_Blink.ino`
2. 开发板选 **"NodeMCU 1.0 (ESP-12E Module)"**
3. 端口选 `/dev/ttyUSB0`
4. 点上传

## 预期现象

烧录完成后，板载蓝色 LED 以 0.5 秒间隔闪烁。

## 文件结构

```
ESP8266_Blink/
├── ESP8266_Blink.ino      # 主程序
├── build_flash.sh         # 通用「编译+烧录」脚本
├── flash_esp8266.sh       # 纯烧录脚本（跳过编译）
└── build/                 # 编译产物（含 ESP8266_Blink.ino.bin 固件）
```
