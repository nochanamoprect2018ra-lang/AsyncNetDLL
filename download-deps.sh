#!/bin/bash
# 下载 Windows 版预编译依赖库 (libcurl + OpenSSL)

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
DEPS_DIR="$SCRIPT_DIR/deps-mingw"

GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

info() { echo -e "${GREEN}[INFO]${NC} $1"; }
warn() { echo -e "${YELLOW}[WARN]${NC} $1"; }

mkdir -p "$DEPS_DIR"
cd "$DEPS_DIR"

# 下载 curl for Windows (MinGW 静态库版本)
CURL_VERSION="8.5.0_1"
CURL_FILE="curl-${CURL_VERSION}-win64-mingw.zip"
CURL_URL="https://curl.se/windows/dl-${CURL_VERSION}/${CURL_FILE}"

if [[ ! -f "$CURL_FILE" ]]; then
    info "下载 libcurl ${CURL_VERSION}..."
    curl -L -o "$CURL_FILE" "$CURL_URL" || {
        warn "官方源下载失败，尝试备用源..."
        curl -L -o "$CURL_FILE" "https://github.com/curl/curl-for-win/releases/download/curl-${CURL_VERSION}/${CURL_FILE}" || {
            warn "备用源也失败，请手动下载: $CURL_URL"
            exit 1
        }
    }
fi

# 解压
CURL_DIR="curl-${CURL_VERSION}-win64-mingw"
if [[ ! -d "$CURL_DIR" ]]; then
    info "解压 libcurl..."
    unzip -q "$CURL_FILE"
fi

# 创建统一的目录结构
mkdir -p "$DEPS_DIR/include" "$DEPS_DIR/lib"

# 复制头文件
info "复制头文件..."
cp -r "$CURL_DIR/include/"* "$DEPS_DIR/include/" 2>/dev/null || true

# 复制库文件 (优先静态库)
info "复制库文件..."
if [[ -f "$CURL_DIR/lib/libcurl.a" ]]; then
    cp "$CURL_DIR/lib/libcurl.a" "$DEPS_DIR/lib/"
elif [[ -f "$CURL_DIR/lib/libcurl.dll.a" ]]; then
    cp "$CURL_DIR/lib/libcurl.dll.a" "$DEPS_DIR/lib/libcurl.a"
fi

# 复制 DLL (如果需要动态链接)
if [[ -f "$CURL_DIR/bin/libcurl-x64.dll" ]]; then
    cp "$CURL_DIR/bin/libcurl-x64.dll" "$DEPS_DIR/lib/"
fi

# curl for Windows 通常自带 SSL 支持，检查是否有 OpenSSL
if [[ -d "$CURL_DIR/bin" ]]; then
    # 复制可能的 SSL DLL
    cp "$CURL_DIR/bin/"*ssl*.dll "$DEPS_DIR/lib/" 2>/dev/null || true
    cp "$CURL_DIR/bin/"*crypto*.dll "$DEPS_DIR/lib/" 2>/dev/null || true
fi

info "依赖库设置完成!"
echo ""
info "目录结构:"
ls -la "$DEPS_DIR/include/" 2>/dev/null | head -10
echo "..."
ls -la "$DEPS_DIR/lib/"

echo ""
info "下一步: 运行 ./build-macos.sh full 进行编译"
