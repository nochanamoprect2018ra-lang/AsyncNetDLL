@echo off
REM AsyncNetDLL 构建脚本
REM 用于自动化编译和部署异步网络DLL

setlocal enabledelayedexpansion

echo ========================================
echo AsyncNetDLL 构建脚本 v1.0
echo ========================================

REM 设置环境变量
set PROJECT_DIR=%~dp0
set BUILD_DIR=%PROJECT_DIR%build
set BIN_DIR=%PROJECT_DIR%bin
set LIB_DIR=%PROJECT_DIR%lib

REM 检查Visual Studio环境
where cl >nul 2>&1
if %errorlevel% neq 0 (
    echo 错误: 未找到Visual Studio编译器
    echo 请运行 "Developer Command Prompt for VS" 或设置VS环境变量
    pause
    exit /b 1
)

REM 创建构建目录
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"
if not exist "%BIN_DIR%" mkdir "%BIN_DIR%"

echo.
echo 1. 清理旧的构建文件...
if exist "%BUILD_DIR%\*" del /q "%BUILD_DIR%\*"

echo.
echo 2. 检查依赖库...

REM 检查libcurl
if not exist "%LIB_DIR%\libcurl" (
    echo 警告: 未找到libcurl库，请确保已下载并解压到 %LIB_DIR%\libcurl
    echo 下载地址: https://curl.se/windows/
)

REM 检查OpenSSL
if not exist "%LIB_DIR%\openssl" (
    echo 警告: 未找到OpenSSL库，请确保已下载并解压到 %LIB_DIR%\openssl
    echo 下载地址: https://slproweb.com/products/Win32OpenSSL.html
)

echo.
echo 3. 开始编译...

REM 设置编译参数
set INCLUDE_DIRS=/I"%PROJECT_DIR%include" /I"%LIB_DIR%\libcurl\include" /I"%LIB_DIR%\openssl\include"
set LIB_DIRS=/LIBPATH:"%LIB_DIR%\libcurl\lib" /LIBPATH:"%LIB_DIR%\openssl\lib"
set LIBS=libcurl.lib libssl.lib libcrypto.lib ws2_32.lib wininet.lib crypt32.lib wldap32.lib normaliz.lib

REM 编译源文件
set SOURCE_FILES="%PROJECT_DIR%src\AsyncNetDLL.cpp" "%PROJECT_DIR%src\AsyncNetworkManager.cpp" "%PROJECT_DIR%src\ConnectionPool.cpp" "%PROJECT_DIR%src\RequestQueue.cpp" "%PROJECT_DIR%src\ResponseQueue.cpp" "%PROJECT_DIR%src\HMACUtils.cpp"

REM Release版本编译
echo 编译Release版本...
cl /nologo /O2 /MD /DNDEBUG /DASYNCNETDLL_EXPORTS /D_WINDOWS /D_USRDLL ^
   %INCLUDE_DIRS% ^
   %SOURCE_FILES% ^
   /link /DLL /OUT:"%BIN_DIR%\AsyncNetDLL.dll" /DEF:"%PROJECT_DIR%AsyncNetDLL.def" ^
   %LIB_DIRS% %LIBS%

if %errorlevel% neq 0 (
    echo 错误: Release版本编译失败
    pause
    exit /b 1
)

echo Release版本编译成功: %BIN_DIR%\AsyncNetDLL.dll

REM Debug版本编译
echo.
echo 编译Debug版本...
cl /nologo /Od /MDd /D_DEBUG /DASYNCNETDLL_EXPORTS /D_WINDOWS /D_USRDLL /Zi ^
   %INCLUDE_DIRS% ^
   %SOURCE_FILES% ^
   /link /DLL /OUT:"%BIN_DIR%\AsyncNetDLL_Debug.dll" /DEF:"%PROJECT_DIR%AsyncNetDLL.def" /DEBUG ^
   %LIB_DIRS% %LIBS%

if %errorlevel% neq 0 (
    echo 警告: Debug版本编译失败，但Release版本可用
) else (
    echo Debug版本编译成功: %BIN_DIR%\AsyncNetDLL_Debug.dll
)

echo.
echo 4. 复制到MT4目录...

REM 查找MT4安装目录
set MT4_DIRS="%APPDATA%\MetaQuotes\Terminal" "C:\Program Files\MetaTrader 4" "C:\Program Files (x86)\MetaTrader 4"

for %%d in (%MT4_DIRS%) do (
    if exist %%d (
        echo 找到MT4目录: %%d
        REM 复制DLL到Libraries目录
        for /d %%f in ("%%d\*") do (
            if exist "%%f\MQL4\Libraries" (
                copy "%BIN_DIR%\AsyncNetDLL.dll" "%%f\MQL4\Libraries\" >nul 2>&1
                if !errorlevel! equ 0 (
                    echo 已复制到: %%f\MQL4\Libraries\
                )
            )
        )
    )
)

echo.
echo 5. 复制MQL4适配层文件...

REM 复制适配层文件到项目目录
copy "%PROJECT_DIR%..\client\ceshi\new61\AsyncNetAdapter.mqh" "%PROJECT_DIR%include\" >nul 2>&1

echo.
echo ========================================
echo 构建完成!
echo ========================================
echo.
echo 生成的文件:
echo   Release DLL: %BIN_DIR%\AsyncNetDLL.dll
if exist "%BIN_DIR%\AsyncNetDLL_Debug.dll" (
    echo   Debug DLL:   %BIN_DIR%\AsyncNetDLL_Debug.dll
)
echo   适配层文件:  AsyncNetAdapter.mqh
echo.
echo 使用说明:
echo 1. 将AsyncNetDLL.dll复制到MT4的Libraries目录
echo 2. 将AsyncNetAdapter.mqh包含到您的EA中
echo 3. 调用InitAsyncNetwork()初始化异步网络
echo.

pause