#!/usr/bin/env bash
# ============================================================
# ESP8266 通用固件烧录脚本（通用版）
#
# 放到任意 ESP8266 工程目录下，直接执行即可自动查找该工程的
# 固件(.bin)并烧录到板子。
#
# 用法:
#   ./flash_esp8266.sh                        # 自动查找固件 + 自动查找串口
#   ./flash_esp8266.sh /dev/ttyUSB0           # 指定串口
#   ./flash_esp8266.sh -f build/xxx.ino.bin   # 指定固件文件
#   ./flash_esp8266.sh -p /dev/ttyUSB0 -f fw.bin
#   ./flash_esp8266.sh -b 115200              # 指定波特率
#   PORT=/dev/ttyUSB0 ./flash_esp8266.sh      # 环境变量指定串口
#   FLASH_SIZE=1MB ./flash_esp8266.sh         # 覆盖 flash 大小(默认 4MB)
#
# 自动查找固件的位置:
#   Arduino:    build/xxx.ino.bin 或 build/<fqbn>/xxx.ino.bin
#   PlatformIO: .pio/build/<env>/firmware.bin
#
# 前提: 已安装 esptool ->  sudo pacman -S esptool (Arch) 或 pip3 install esptool
# ============================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# 工程根目录：优先当前工作目录，回退脚本所在目录
PROJ_DIR="${PWD}"

# ---- 默认烧录参数（ESP8266 通用，可用环境变量覆盖）----
FLASH_MODE="${FLASH_MODE:-dio}"
FLASH_FREQ="${FLASH_FREQ:-40m}"
FLASH_SIZE="${FLASH_SIZE:-4MB}"
FLASH_ADDR="${FLASH_ADDR:-0x00000}"
BAUD="${BAUD:-460800}"

RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[0;33m'; NC='\033[0m'
info()  { echo -e "${GREEN}[信息]${NC} $*"; }
warn()  { echo -e "${YELLOW}[警告]${NC} $*"; }
error() { echo -e "${RED}[错误]${NC} $*" >&2; }

usage() {
  cat <<EOF
ESP8266 通用固件烧录脚本

用法:
  $0                         自动查找固件 + 自动查找串口
  $0 /dev/ttyUSB0            指定串口
  $0 -f <固件.bin>           指定固件文件
  $0 -p <串口> -f <固件.bin>
  $0 -b 115200               指定波特率

选项:
  -p, --port <串口>      串口设备(默认自动检测 /dev/ttyUSB* /dev/ttyACM*)
  -f, --firmware <文件>  固件 .bin 路径(默认自动查找)
  -b, --baud <波特率>    波特率(默认 460800)
  -h, --help             显示帮助

环境变量:
  PORT        串口       FIRMWARE    固件路径
  BAUD        波特率     FLASH_MODE  默认 dio
  FLASH_FREQ  频率       FLASH_SIZE  默认 4MB
  FLASH_ADDR  地址      默认 0x00000
EOF
}

# ---- 查找固件 ----
find_firmware() {
  local root f cand=()
  for root in "$PROJ_DIR" "$SCRIPT_DIR"; do
    [ "$root" = "$SCRIPT_DIR" ] && [ "$PROJ_DIR" != "$SCRIPT_DIR" ] && \
      info "当前目录未找到固件，改用脚本所在目录查找"

    # Arduino: build 下的 .ino.bin
    while IFS= read -r f; do
      [ -n "$f" ] && cand+=("$f")
    done < <(find "$root" -maxdepth 3 -type f -name "*.ino.bin" 2>/dev/null)
    [ "${#cand[@]}" -gt 0 ] && break

    # PlatformIO: .pio 下的 firmware.bin
    while IFS= read -r f; do
      [ -n "$f" ] && cand+=("$f")
    done < <(find "$root" -maxdepth 5 -type f -path "*/.pio/*" -name "firmware.bin" 2>/dev/null)
    [ "${#cand[@]}" -gt 0 ] && break

    # 兜底: 根目录下任意 .bin（排除 bootloader/partitions）
    while IFS= read -r f; do
      case "$f" in
        *bootloader*|*partitions*) continue ;;
        *) [ -n "$f" ] && cand+=("$f") ;;
      esac
    done < <(find "$root" -maxdepth 2 -type f -name "*.bin" 2>/dev/null)
    [ "${#cand[@]}" -gt 0 ] && break
  done

  [ "${#cand[@]}" -eq 0 ] && return 1

  if [ "${#cand[@]}" -gt 1 ]; then
    warn "发现多个固件候选，选用修改时间最新的:"
    for f in "${cand[@]}"; do echo "      $f"; done
  fi
  FIRMWARE="$(ls -t "${cand[@]}" | head -1)"
  return 0
}

# ---- 参数解析（支持长短选项）----
opt_port=""; opt_firmware=""; opt_baud=""
while [ $# -gt 0 ]; do
  case "$1" in
    -p|--port)         opt_port="$2";     shift 2 ;;
    -f|--firmware)     opt_firmware="$2"; shift 2 ;;
    -b|--baud)         opt_baud="$2";     shift 2 ;;
    -h|--help)         usage; exit 0 ;;
    -p=*|--port=*)     opt_port="${1#*=}";     shift ;;
    -f=*|--firmware=*) opt_firmware="${1#*=}"; shift ;;
    -b=*|--baud=*)     opt_baud="${1#*=}";     shift ;;
    --)                shift; break ;;
    -*)                error "未知选项: $1"; usage; exit 1 ;;
    *)                 break ;;   # 位置参数，交给下面处理
  esac
done

# 环境变量兜底
PORT="${PORT:-}"
FIRMWARE="${FIRMWARE:-}"
[ -n "$opt_baud" ] && BAUD="$opt_baud"

# 位置参数：/dev/ 开头视为串口，否则视为固件
if [ $# -gt 0 ]; then
  case "$1" in
    /dev/*) [ -z "$opt_port" ] && PORT="$1" ;;
    *)      [ -z "$opt_firmware" ] && FIRMWARE="$1" ;;
  esac
fi
[ -n "$opt_port" ] && PORT="$opt_port"
[ -n "$opt_firmware" ] && FIRMWARE="$opt_firmware"

# ---- 1. 确定固件 ----
if [ -z "$FIRMWARE" ]; then
  find_firmware || {
    error "未找到固件 .bin 文件。"
    error "请先编译，或用 -f 指定固件路径，例如:"
    echo "    $0 -f build/你的工程名.ino.bin"
    exit 1
  }
fi
if [ ! -f "$FIRMWARE" ]; then
  error "固件文件不存在: $FIRMWARE"
  exit 1
fi
info "固件: $FIRMWARE ($(du -h "$FIRMWARE" | cut -f1))"

# ---- 2. 定位 esptool ----
ESPTOOL=()
if command -v esptool >/dev/null 2>&1; then
  ESPTOOL=(esptool)
elif command -v esptool.py >/dev/null 2>&1; then
  ESPTOOL=(esptool.py)
elif python3 -c 'import esptool' >/dev/null 2>&1; then
  ESPTOOL=(python3 -m esptool)
else
  error "未找到 esptool，请先安装:"
  echo "    Arch:          sudo pacman -S esptool"
  echo "    Debian/Ubuntu: sudo apt install esptool"
  echo "    其他:          pip3 install esptool"
  exit 1
fi
info "esptool: ${ESPTOOL[*]}"

# ---- 3. 确定串口 ----
if [ -z "$PORT" ]; then
  FOUND=()
  for p in /dev/ttyUSB* /dev/ttyACM*; do
    [ -e "$p" ] && FOUND+=("$p")
  done
  case "${#FOUND[@]}" in
    0)
      error "未检测到串口设备。请确认板子已连接(USB 为数据线)，然后:"
      echo "    1) 查看: ls /dev/ttyUSB* /dev/ttyACM*"
      echo "    2) 或指定端口: $0 /dev/ttyUSB0"
      exit 1
      ;;
    1)
      PORT="${FOUND[0]}"
      ;;
    *)
      error "检测到多个串口设备: ${FOUND[*]}"
      error "请指定端口重试: $0 /dev/ttyUSB0"
      exit 1
      ;;
  esac
fi
info "串口: $PORT"

# ---- 4. 检查串口权限 ----
if [ ! -r "$PORT" ] || [ ! -w "$PORT" ]; then
  warn "当前用户可能无权限访问 $PORT"
  warn "解决: 加入串口组后注销重新登录，或临时用 sudo 运行本脚本"
  warn "  Arch:          sudo usermod -aG uucp $USER"
  warn "  Debian/Ubuntu: sudo usermod -aG dialout $USER"
fi

# ---- 5. 烧录 ----
info "开始烧录 (mode=$FLASH_MODE freq=$FLASH_FREQ size=$FLASH_SIZE addr=$FLASH_ADDR baud=$BAUD)"
"${ESPTOOL[@]}" \
  --port "$PORT" \
  --baud "$BAUD" \
  write-flash \
  --flash-mode "$FLASH_MODE" \
  --flash-freq "$FLASH_FREQ" \
  --flash-size "$FLASH_SIZE" \
  "$FLASH_ADDR" "$FIRMWARE"

# ---- 6. 完成 ----
info "烧录完成！"
