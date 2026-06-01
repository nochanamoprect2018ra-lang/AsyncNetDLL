# MinGW-w64 交叉编译工具链文件 (macOS -> Windows x64)
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

# MinGW-w64 工具链路径 (Homebrew 安装)
set(MINGW_PREFIX "x86_64-w64-mingw32")
set(MINGW_ROOT "/usr/local/opt/mingw-w64/toolchain-x86_64")

# 编译器设置
set(CMAKE_C_COMPILER ${MINGW_ROOT}/bin/${MINGW_PREFIX}-gcc)
set(CMAKE_CXX_COMPILER ${MINGW_ROOT}/bin/${MINGW_PREFIX}-g++)
set(CMAKE_RC_COMPILER ${MINGW_ROOT}/bin/${MINGW_PREFIX}-windres)
set(CMAKE_AR ${MINGW_ROOT}/bin/${MINGW_PREFIX}-ar)
set(CMAKE_RANLIB ${MINGW_ROOT}/bin/${MINGW_PREFIX}-ranlib)

# 搜索路径设置
set(CMAKE_FIND_ROOT_PATH ${MINGW_ROOT}/${MINGW_PREFIX})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# 静态链接运行时库
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static-libgcc -static-libstdc++")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -static-libgcc")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc -static-libstdc++")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -static-libgcc -static-libstdc++")

# Windows 特定定义
add_definitions(-DWIN32 -D_WIN32 -D_WINDOWS)
add_definitions(-DASYNCNETDLL_EXPORTS)
