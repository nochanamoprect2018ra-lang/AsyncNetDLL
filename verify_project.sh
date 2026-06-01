#!/bin/bash
# AsyncNetDLL 项目验证脚本
# 用于验证项目文件完整性和配置正确性

echo "=========================================="
echo "AsyncNetDLL 项目验证脚本"
echo "=========================================="

PROJECT_ROOT="/Users/abei/Projects/trading-system/AsyncNetDLL"
MT4_PROJECT="/Users/abei/Projects/trading-system/client/ceshi/new61"

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# 验证函数
check_file() {
    if [ -f "$1" ]; then
        echo -e "${GREEN}✓${NC} $1"
        return 0
    else
        echo -e "${RED}✗${NC} $1 (缺失)"
        return 1
    fi
}

check_dir() {
    if [ -d "$1" ]; then
        echo -e "${GREEN}✓${NC} $1/"
        return 0
    else
        echo -e "${RED}✗${NC} $1/ (缺失)"
        return 1
    fi
}

# 计数器
total_checks=0
passed_checks=0

echo
echo "1. 检查项目目录结构..."
echo "----------------------------------------"

# 检查主要目录
directories=(
    "$PROJECT_ROOT"
    "$PROJECT_ROOT/src"
    "$PROJECT_ROOT/include"
    "$PROJECT_ROOT/lib"
    "$PROJECT_ROOT/build"
    "$PROJECT_ROOT/bin"
    "$PROJECT_ROOT/docs"
)

for dir in "${directories[@]}"; do
    total_checks=$((total_checks + 1))
    if check_dir "$dir"; then
        passed_checks=$((passed_checks + 1))
    fi
done

echo
echo "2. 检查核心源文件..."
echo "----------------------------------------"

# 检查源文件
source_files=(
    "$PROJECT_ROOT/src/AsyncNetDLL.cpp"
    "$PROJECT_ROOT/src/AsyncNetworkManager.cpp"
    "$PROJECT_ROOT/src/ConnectionPool.cpp"
    "$PROJECT_ROOT/src/RequestQueue.cpp"
    "$PROJECT_ROOT/src/ResponseQueue.cpp"
    "$PROJECT_ROOT/src/HMACUtils.cpp"
)

for file in "${source_files[@]}"; do
    total_checks=$((total_checks + 1))
    if check_file "$file"; then
        passed_checks=$((passed_checks + 1))
    fi
done

echo
echo "3. 检查头文件..."
echo "----------------------------------------"

# 检查头文件
header_files=(
    "$PROJECT_ROOT/include/AsyncNetDLL.h"
    "$PROJECT_ROOT/include/AsyncNetworkManager.h"
    "$PROJECT_ROOT/include/ConnectionPool.h"
    "$PROJECT_ROOT/include/RequestQueue.h"
    "$PROJECT_ROOT/include/ResponseQueue.h"
    "$PROJECT_ROOT/include/HMACUtils.h"
)

for file in "${header_files[@]}"; do
    total_checks=$((total_checks + 1))
    if check_file "$file"; then
        passed_checks=$((passed_checks + 1))
    fi
done

echo
echo "4. 检查构建文件..."
echo "----------------------------------------"

# 检查构建文件
build_files=(
    "$PROJECT_ROOT/CMakeLists.txt"
    "$PROJECT_ROOT/AsyncNetDLL.vcxproj"
    "$PROJECT_ROOT/AsyncNetDLL.def"
    "$PROJECT_ROOT/build.bat"
)

for file in "${build_files[@]}"; do
    total_checks=$((total_checks + 1))
    if check_file "$file"; then
        passed_checks=$((passed_checks + 1))
    fi
done

echo
echo "5. 检查文档文件..."
echo "----------------------------------------"

# 检查文档文件
doc_files=(
    "$PROJECT_ROOT/README.md"
    "$PROJECT_ROOT/DEPLOYMENT.md"
)

for file in "${doc_files[@]}"; do
    total_checks=$((total_checks + 1))
    if check_file "$file"; then
        passed_checks=$((passed_checks + 1))
    fi
done

echo
echo "6. 检查MQL4适配层..."
echo "----------------------------------------"

# 检查MQL4文件
mql4_files=(
    "$MT4_PROJECT/AsyncNetAdapter.mqh"
    "$MT4_PROJECT/GoldKylin_v1.mq4"
)

for file in "${mql4_files[@]}"; do
    total_checks=$((total_checks + 1))
    if check_file "$file"; then
        passed_checks=$((passed_checks + 1))
    fi
done

echo
echo "7. 检查代码完整性..."
echo "----------------------------------------"

# 检查关键函数是否存在
echo "检查AsyncNetDLL.cpp中的导出函数..."
if grep -q "AsyncNet_Initialize" "$PROJECT_ROOT/src/AsyncNetDLL.cpp"; then
    echo -e "${GREEN}✓${NC} AsyncNet_Initialize 函数存在"
    passed_checks=$((passed_checks + 1))
else
    echo -e "${RED}✗${NC} AsyncNet_Initialize 函数缺失"
fi
total_checks=$((total_checks + 1))

if grep -q "AsyncNet_SendHeartbeat" "$PROJECT_ROOT/src/AsyncNetDLL.cpp"; then
    echo -e "${GREEN}✓${NC} AsyncNet_SendHeartbeat 函数存在"
    passed_checks=$((passed_checks + 1))
else
    echo -e "${RED}✗${NC} AsyncNet_SendHeartbeat 函数缺失"
fi
total_checks=$((total_checks + 1))

echo "检查MQL4适配层函数..."
if grep -q "InitAsyncNetwork" "$MT4_PROJECT/AsyncNetAdapter.mqh"; then
    echo -e "${GREEN}✓${NC} InitAsyncNetwork 函数存在"
    passed_checks=$((passed_checks + 1))
else
    echo -e "${RED}✗${NC} InitAsyncNetwork 函数缺失"
fi
total_checks=$((total_checks + 1))

echo "检查MT4代码集成..."
if grep -q "AsyncNetAdapter.mqh" "$MT4_PROJECT/GoldKylin_v1.mq4"; then
    echo -e "${GREEN}✓${NC} AsyncNetAdapter.mqh 已集成到EA"
    passed_checks=$((passed_checks + 1))
else
    echo -e "${RED}✗${NC} AsyncNetAdapter.mqh 未集成到EA"
fi
total_checks=$((total_checks + 1))

if grep -q "ProcessAsyncResponses" "$MT4_PROJECT/GoldKylin_v1.mq4"; then
    echo -e "${GREEN}✓${NC} ProcessAsyncResponses 调用已添加"
    passed_checks=$((passed_checks + 1))
else
    echo -e "${RED}✗${NC} ProcessAsyncResponses 调用缺失"
fi
total_checks=$((total_checks + 1))

echo
echo "8. 检查配置文件..."
echo "----------------------------------------"

# 检查Visual Studio项目配置
if grep -q "ASYNCNETDLL_EXPORTS" "$PROJECT_ROOT/AsyncNetDLL.vcxproj"; then
    echo -e "${GREEN}✓${NC} DLL导出宏已配置"
    passed_checks=$((passed_checks + 1))
else
    echo -e "${RED}✗${NC} DLL导出宏未配置"
fi
total_checks=$((total_checks + 1))

# 检查依赖库配置
if grep -q "libcurl.lib" "$PROJECT_ROOT/AsyncNetDLL.vcxproj"; then
    echo -e "${GREEN}✓${NC} libcurl依赖已配置"
    passed_checks=$((passed_checks + 1))
else
    echo -e "${RED}✗${NC} libcurl依赖未配置"
fi
total_checks=$((total_checks + 1))

if grep -q "libssl.lib" "$PROJECT_ROOT/AsyncNetDLL.vcxproj"; then
    echo -e "${GREEN}✓${NC} OpenSSL依赖已配置"
    passed_checks=$((passed_checks + 1))
else
    echo -e "${RED}✗${NC} OpenSSL依赖未配置"
fi
total_checks=$((total_checks + 1))

echo
echo "9. 检查代码质量..."
echo "----------------------------------------"

# 检查代码中的关键安全特性
if grep -q "std::mutex" "$PROJECT_ROOT/src/AsyncNetworkManager.cpp"; then
    echo -e "${GREEN}✓${NC} 线程安全机制已实现"
    passed_checks=$((passed_checks + 1))
else
    echo -e "${RED}✗${NC} 线程安全机制缺失"
fi
total_checks=$((total_checks + 1))

if grep -q "HMAC_SHA256" "$PROJECT_ROOT/src/HMACUtils.cpp"; then
    echo -e "${GREEN}✓${NC} HMAC签名验证已实现"
    passed_checks=$((passed_checks + 1))
else
    echo -e "${RED}✗${NC} HMAC签名验证缺失"
fi
total_checks=$((total_checks + 1))

if grep -q "curl_easy_setopt.*CURLOPT_TCP_KEEPALIVE" "$PROJECT_ROOT/src/ConnectionPool.cpp"; then
    echo -e "${GREEN}✓${NC} Keep-Alive连接复用已配置"
    passed_checks=$((passed_checks + 1))
else
    echo -e "${RED}✗${NC} Keep-Alive连接复用未配置"
fi
total_checks=$((total_checks + 1))

echo
echo "10. 生成验证报告..."
echo "----------------------------------------"

# 计算通过率
pass_rate=$((passed_checks * 100 / total_checks))

echo "验证完成!"
echo "总检查项: $total_checks"
echo "通过项: $passed_checks"
echo "失败项: $((total_checks - passed_checks))"
echo "通过率: $pass_rate%"

if [ $pass_rate -ge 90 ]; then
    echo -e "${GREEN}✓ 项目验证通过！可以进行编译和部署。${NC}"
    exit 0
elif [ $pass_rate -ge 70 ]; then
    echo -e "${YELLOW}⚠ 项目基本完整，但存在一些问题需要修复。${NC}"
    exit 1
else
    echo -e "${RED}✗ 项目存在严重问题，需要修复后再进行部署。${NC}"
    exit 2
fi