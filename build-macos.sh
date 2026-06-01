#!/bin/bash
# macOS 交叉编译脚本 - 使用 MinGW-w64 编译 Windows DLL

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build-mingw"
DEPS_DIR="$SCRIPT_DIR/deps-mingw"
OUTPUT_DIR="$SCRIPT_DIR/bin"

# 颜色输出
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

info() { echo -e "${GREEN}[INFO]${NC} $1"; }
warn() { echo -e "${YELLOW}[WARN]${NC} $1"; }
error() { echo -e "${RED}[ERROR]${NC} $1"; exit 1; }

# 检查 MinGW-w64
check_mingw() {
    info "检查 MinGW-w64..."
    if ! command -v x86_64-w64-mingw32-gcc &> /dev/null; then
        warn "MinGW-w64 未找到，尝试安装..."
        brew install mingw-w64
    fi

    MINGW_GCC=$(which x86_64-w64-mingw32-gcc)
    info "MinGW GCC: $MINGW_GCC"
    x86_64-w64-mingw32-gcc --version | head -1
}

# 下载并编译 Windows 版依赖库
setup_deps() {
    info "设置依赖库..."
    mkdir -p "$DEPS_DIR"
    cd "$DEPS_DIR"

    # 检查是否已有预编译库
    if [[ -f "$DEPS_DIR/lib/libcurl.a" && -f "$DEPS_DIR/lib/libssl.a" ]]; then
        info "依赖库已存在，跳过下载"
        return 0
    fi

    info "下载预编译的 Windows 静态库..."

    # 下载 curl for Windows (静态库)
    if [[ ! -f "curl-8.5.0_1-win64-mingw.zip" ]]; then
        info "下载 libcurl..."
        curl -L -o curl-8.5.0_1-win64-mingw.zip \
            "https://curl.se/windows/dl-8.5.0_1/curl-8.5.0_1-win64-mingw.zip"
    fi

    # 解压 curl
    if [[ ! -d "curl-8.5.0_1-win64-mingw" ]]; then
        unzip -q curl-8.5.0_1-win64-mingw.zip
    fi

    # 复制库文件
    mkdir -p "$DEPS_DIR/lib" "$DEPS_DIR/include"
    cp -r curl-8.5.0_1-win64-mingw/include/* "$DEPS_DIR/include/" 2>/dev/null || true
    cp curl-8.5.0_1-win64-mingw/lib/*.a "$DEPS_DIR/lib/" 2>/dev/null || true

    # 下载 OpenSSL for Windows (使用 curl 自带的)
    # curl 的 mingw 版本通常包含 OpenSSL

    info "依赖库设置完成"
    ls -la "$DEPS_DIR/lib/"
}

# 简化版：不依赖外部库，使用 WinINet
build_simple() {
    info "构建简化版 DLL (使用 WinINet，无外部依赖)..."

    mkdir -p "$BUILD_DIR" "$OUTPUT_DIR"
    cd "$BUILD_DIR"

    # 编译所有源文件
    SOURCES=(
        "$SCRIPT_DIR/src/AsyncNetDLL.cpp"
        "$SCRIPT_DIR/src/AsyncNetworkManager.cpp"
        "$SCRIPT_DIR/src/ConnectionPool.cpp"
        "$SCRIPT_DIR/src/RequestQueue.cpp"
        "$SCRIPT_DIR/src/ResponseQueue.cpp"
        "$SCRIPT_DIR/src/HMACUtils.cpp"
    )

    OBJECTS=()
    for src in "${SOURCES[@]}"; do
        obj=$(basename "${src%.cpp}.o")
        info "编译: $(basename $src)"
        x86_64-w64-mingw32-g++ -c "$src" -o "$obj" \
            -I"$SCRIPT_DIR/include" \
            -DWIN32 -D_WIN32 -D_WINDOWS \
            -DASYNCNETDLL_EXPORTS \
            -DUSE_WININET \
            -std=c++17 -O2 -Wall \
            -static-libgcc -static-libstdc++
        OBJECTS+=("$obj")
    done

    # 链接 DLL
    info "链接 DLL..."
    x86_64-w64-mingw32-g++ -shared -o "$OUTPUT_DIR/AsyncNetDLL.dll" \
        "${OBJECTS[@]}" \
        -Wl,--out-implib,"$OUTPUT_DIR/libAsyncNetDLL.a" \
        -lwininet -lws2_32 -lcrypt32 \
        -static-libgcc -static-libstdc++ \
        -Wl,--enable-stdcall-fixup

    info "构建完成!"
    ls -la "$OUTPUT_DIR/"
}

# 完整版：使用 libcurl
build_full() {
    info "构建完整版 DLL (使用 libcurl)..."

    if [[ ! -f "$DEPS_DIR/lib/libcurl.a" ]]; then
        error "缺少 libcurl，请先运行: $0 deps"
    fi

    mkdir -p "$BUILD_DIR" "$OUTPUT_DIR"
    cd "$BUILD_DIR"

    # 使用 CMake 构建
    cmake "$SCRIPT_DIR" \
        -DCMAKE_TOOLCHAIN_FILE="$SCRIPT_DIR/toolchain-mingw64.cmake" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCURL_INCLUDE_DIR="$DEPS_DIR/include" \
        -DCURL_LIBRARY="$DEPS_DIR/lib/libcurl.a" \
        -DOPENSSL_ROOT_DIR="$DEPS_DIR" \
        -DCMAKE_INSTALL_PREFIX="$OUTPUT_DIR"

    cmake --build . --config Release -j$(sysctl -n hw.ncpu)

    cp -f bin/*.dll "$OUTPUT_DIR/" 2>/dev/null || true

    info "构建完成!"
    ls -la "$OUTPUT_DIR/"
}

# 清理
clean() {
    info "清理构建目录..."
    rm -rf "$BUILD_DIR"
    rm -rf "$OUTPUT_DIR"/*.dll
    rm -rf "$OUTPUT_DIR"/*.a
    info "清理完成"
}

# 验证 DLL
verify() {
    info "验证 DLL..."

    if [[ ! -f "$OUTPUT_DIR/AsyncNetDLL.dll" ]]; then
        error "DLL 不存在: $OUTPUT_DIR/AsyncNetDLL.dll"
    fi

    info "文件信息:"
    file "$OUTPUT_DIR/AsyncNetDLL.dll"

    info "导出函数:"
    x86_64-w64-mingw32-nm -C "$OUTPUT_DIR/AsyncNetDLL.dll" 2>/dev/null | grep " T " | head -20 || \
    x86_64-w64-mingw32-objdump -p "$OUTPUT_DIR/AsyncNetDLL.dll" | grep -A 100 "Export Table" | head -30

    info "文件大小:"
    ls -lh "$OUTPUT_DIR/AsyncNetDLL.dll"
}

# 帮助
usage() {
    echo "用法: $0 [命令]"
    echo ""
    echo "命令:"
    echo "  simple    构建简化版 (WinINet, 无外部依赖) [推荐]"
    echo "  deps      下载依赖库"
    echo "  full      构建完整版 (libcurl)"
    echo "  clean     清理构建文件"
    echo "  verify    验证生成的 DLL"
    echo "  all       deps + full"
    echo ""
    echo "示例:"
    echo "  $0 simple   # 快速构建，推荐首次使用"
    echo "  $0 verify   # 检查 DLL 是否正确"
}

# 主入口
case "${1:-simple}" in
    simple)
        check_mingw
        build_simple
        verify
        ;;
    deps)
        setup_deps
        ;;
    full)
        check_mingw
        build_full
        verify
        ;;
    all)
        check_mingw
        setup_deps
        build_full
        verify
        ;;
    clean)
        clean
        ;;
    verify)
        verify
        ;;
    help|--help|-h)
        usage
        ;;
    *)
        error "未知命令: $1"
        ;;
esac
