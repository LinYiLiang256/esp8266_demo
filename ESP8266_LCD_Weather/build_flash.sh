#!/usr/bin/env bash
# ============================================================
# ESP 通用「编译 + 烧录」脚本（ESP8266 / ESP32 通用）
#
# 一条命令完成：查找 .ino 工程 -> 编译 -> 自动检测串口 -> 烧录
#
# 用法:
#   ./build_flash.sh                 # 编译并烧录（最常用）
#   ./build_flash.sh --compile-only  # 只编译，不烧录
#   ./build_flash.sh --flash-only    # 只烧录（用已存在的 build 产物）
#   ./build_flash.sh /dev/ttyUSB0    # 指定串口
#   ./build_flash.sh -p /dev/ttyUSB0
#
# 切换板子（环境变量 FQBN）:
#   ESP8266 NodeMCU(默认):  esp8266:esp8266:nodemcuv2
#   ESP32 通用:             FQBN=esp32:esp32:esp32 ./build_flash.sh
#   ESP32 S3:               FQBN=esp32:esp32:esp32s3 ./build_flash.sh
#
# 前提: 已安装 arduino-cli
#   Arch:          sudo pacman -S arduino-cli
#   Debian/Ubuntu: sudo apt install arduino-cli
#   其他:          curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | sh
# ============================================================
set -euo pipefail

PROJ_DIR="${PWD}"
FQBN="${FQBN:-esp8266:esp8266:nodemcuv2}"
BUILD_DIR="${BUILD_DIR:-build}"

RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[0;33m'; NC='\033[0m'
info()  { echo -e "${GREEN}[信息]${NC} $*"; }
warn()  { echo -e "${YELLOW}[警告]${NC} $*"; }
error() { echo -e "${RED}[错误]${NC} $*" >&2; }

usage() {
  cat <<EOF
ESP 通用编译+烧录脚本

用法:
  $0                         编译并烧录
  $0 --compile-only          只编译
  $0 --flash-only            只烧录(用已有 build 产物)
  $0 /dev/ttyUSB0            指定串口
  $0 -p /dev/ttyUSB0

选项:
  -p, --port <串口>      串口(默认自动检测)
  -b, --fqbn <板子>      板子类型(默认 esp8266:esp8266:nodemcuv2)
  --compile-only         只编译
  --flash-only           只烧录
  -h, --help             帮助

环境变量:
  FQBN    板子类型    PORT    串口    BUILD_DIR  编译输出目录(默认 build)
EOF
}

# 查找 sketch 工程（.ino 所在目录）
find_sketch() {
  local base f
  base="$(basename "$PROJ_DIR")"
  if [ -f "$PROJ_DIR/$base.ino" ]; then
    SKETCH_DIR="$PROJ_DIR"; return 0
  fi
  f=$(find "$PROJ_DIR" -maxdepth 2 -name "*.ino" -type f 2>/dev/null | head -1)
  [ -n "$f" ] && { SKETCH_DIR="$(dirname "$f")"; return 0; }
  return 1
}

# 查找串口
find_port() {
  local p found=()
  for p in /dev/ttyUSB* /dev/ttyACM*; do
    [ -e "$p" ] && found+=("$p")
  done
  case "${#found[@]}" in
    0) return 1 ;;
    1) PORT="${found[0]}" ;;
    *) error "检测到多个串口设备: ${found[*]}"; return 2 ;;
  esac
  return 0
}

# ---- 参数解析 ----
opt_port=""; opt_fqbn=""; do_compile=1; do_flash=1
while [ $# -gt 0 ]; do
  case "$1" in
    -p|--port)         opt_port="$2"; shift 2 ;;
    -b|--fqbn)         opt_fqbn="$2"; shift 2 ;;
    --compile-only)    do_flash=0;    shift ;;
    --flash-only)      do_compile=0;  shift ;;
    -h|--help)         usage; exit 0 ;;
    -p=*|--port=*)     opt_port="${1#*=}"; shift ;;
    -b=*|--fqbn=*)     opt_fqbn="${1#*=}"; shift ;;
    --)                shift; break ;;
    -*)                error "未知选项: $1"; usage; exit 1 ;;
    *)                 break ;;
  esac
done
PORT="${PORT:-}"
[ -n "$opt_fqbn" ] && FQBN="$opt_fqbn"

# 位置参数：/dev/ 开头视为串口
if [ $# -gt 0 ]; then
  case "$1" in
    /dev/*) [ -z "$opt_port" ] && PORT="$1" ;;
    *)      error "未知参数: $1"; usage; exit 1 ;;
  esac
fi
[ -n "$opt_port" ] && PORT="$opt_port"

# ---- 检查 arduino-cli ----
if ! command -v arduino-cli >/dev/null 2>&1; then
  error "未找到 arduino-cli，请先安装:"
  echo "    Arch:          sudo pacman -S arduino-cli"
  echo "    Debian/Ubuntu: sudo apt install arduino-cli"
  exit 1
fi

# ---- 查找 sketch ----
if ! find_sketch; then
  error "未找到 .ino 工程文件。请在工程目录下运行本脚本。"
  exit 1
fi
info "工程: $SKETCH_DIR"
info "板子: $FQBN"

# ---- 编译 ----
if [ "$do_compile" -eq 1 ]; then
  info "编译中 ..."
  arduino-cli compile --fqbn "$FQBN" --build-path "$BUILD_DIR" "$SKETCH_DIR"
  info "编译完成"
fi

# ---- 烧录 ----
if [ "$do_flash" -eq 1 ]; then
  if [ -z "$PORT" ]; then
    find_port || { error "未检测到串口设备。请指定: $0 /dev/ttyUSB0"; exit 1; }
  fi
  info "烧录到 $PORT ..."
  arduino-cli upload --port "$PORT" --fqbn "$FQBN" --input-dir "$BUILD_DIR" "$SKETCH_DIR"
  info "烧录完成！"
fi
