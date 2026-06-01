# AsyncNetDLL - 异步网络DLL项目
cmake_minimum_required(VERSION 3.16)
project(AsyncNetDLL VERSION 1.0.0 LANGUAGES CXX)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# 编译选项
if(MSVC)
    add_compile_options(/W4 /WX- /MP)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
    add_compile_definitions(ASYNCNETDLL_EXPORTS)
endif()

# 查找依赖库
find_package(PkgConfig REQUIRED)

# 查找libcurl
find_package(CURL REQUIRED)
if(CURL_FOUND)
    message(STATUS "Found CURL: ${CURL_LIBRARIES}")
else()
    message(FATAL_ERROR "CURL not found")
endif()

# 查找OpenSSL
find_package(OpenSSL REQUIRED)
if(OPENSSL_FOUND)
    message(STATUS "Found OpenSSL: ${OPENSSL_LIBRARIES}")
else()
    message(FATAL_ERROR "OpenSSL not found")
endif()

# 包含目录
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CURL_INCLUDE_DIRS}
    ${OPENSSL_INCLUDE_DIR}
)

# 源文件
set(SOURCES
    src/AsyncNetDLL.cpp
    src/AsyncNetworkManager.cpp
    src/ConnectionPool.cpp
    src/RequestQueue.cpp
    src/ResponseQueue.cpp
    src/HMACUtils.cpp
)

# 头文件
set(HEADERS
    include/AsyncNetDLL.h
    include/AsyncNetworkManager.h
    include/ConnectionPool.h
    include/RequestQueue.h
    include/ResponseQueue.h
    include/HMACUtils.h
)

# 创建DLL
add_library(AsyncNetDLL SHARED ${SOURCES} ${HEADERS})

# 链接库
target_link_libraries(AsyncNetDLL
    ${CURL_LIBRARIES}
    ${OPENSSL_LIBRARIES}
    ws2_32
    wininet
)

# 设置DLL属性
set_target_properties(AsyncNetDLL PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
    OUTPUT_NAME "AsyncNetDLL"
    PREFIX ""
)

# 安装规则
install(TARGETS AsyncNetDLL
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(FILES ${HEADERS}
    DESTINATION include
)

# 创建测试可执行文件（可选）
option(BUILD_TESTS "Build test programs" OFF)
if(BUILD_TESTS)
    add_subdirectory(tests)
endif()

# 打包配置
set(CPACK_PACKAGE_NAME "AsyncNetDLL")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Async Network DLL for MT4/MT5")
set(CPACK_PACKAGE_VENDOR "Trading System")
set(CPACK_GENERATOR "ZIP")

include(CPack)