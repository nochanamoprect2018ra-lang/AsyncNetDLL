//+------------------------------------------------------------------+
//|                                        AsyncNetDLL.h            |
//|                  异步网络DLL - 主接口头文件                        |
//|              libcurl异步HTTP客户端，替换WinINet同步实现            |
//+------------------------------------------------------------------+
#pragma once

#ifndef ASYNCNETDLL_H
#define ASYNCNETDLL_H

#ifdef ASYNCNETDLL_EXPORTS
#define ASYNCNET_API __declspec(dllexport)
#else
#define ASYNCNET_API __declspec(dllimport)
#endif

#include <stdint.h>
#include <string>
#include <chrono>

//+------------------------------------------------------------------+
//| 请求类型枚举                                                      |
//+------------------------------------------------------------------+
enum RequestType {
    REQ_HEARTBEAT = 1,    // 心跳请求（高优先级）
    REQ_EVENTS = 2,       // 事件上报（普通优先级）
    REQ_PARAMS = 3,       // 参数拉取（低优先级）
    REQ_PING = 4          // 连接测试
};

//+------------------------------------------------------------------+
//| 请求状态枚举                                                      |
//+------------------------------------------------------------------+
enum RequestStatus {
    REQ_STATUS_PENDING = 0,   // 等待处理
    REQ_STATUS_SUCCESS = 1,   // 成功完成
    REQ_STATUS_ERROR = 2,     // 请求错误
    REQ_STATUS_TIMEOUT = 3,   // 请求超时
    REQ_STATUS_NETWORK_ERROR = 4,  // 网络错误
    REQ_STATUS_AUTH_ERROR = 5      // 认证错误
};

//+------------------------------------------------------------------+
//| 请求上下文结构                                                    |
//+------------------------------------------------------------------+
struct RequestContext {
    int id;                    // 请求ID
    RequestType type;          // 请求类型
    std::string url;           // 请求URL
    std::string data;          // 请求数据
    std::string headers;       // 请求头
    std::chrono::steady_clock::time_point created_time;  // 创建时间
    std::chrono::steady_clock::time_point sent_time;     // 发送时间
    int retry_count;           // 重试次数
    RequestStatus status;      // 请求状态
    std::string response;      // 响应数据
    std::string error_message; // 错误信息
    long response_code;        // HTTP响应码
    double latency_ms;         // 延迟时间(毫秒)

    RequestContext() : id(0), type(REQ_HEARTBEAT), retry_count(0),
                      status(static_cast<RequestStatus>(REQ_STATUS_PENDING)), response_code(0), latency_ms(0.0) {}
};

//+------------------------------------------------------------------+
//| 网络统计结构体                                                    |
//+------------------------------------------------------------------+
struct NetworkStats {
    uint64_t heartbeat_sent;        // 心跳发送次数
    uint64_t heartbeat_success;     // 心跳成功次数
    uint64_t heartbeat_failed;      // 心跳失败次数
    uint64_t events_sent;           // 事件发送次数
    uint64_t events_success;        // 事件成功次数
    uint64_t events_failed;         // 事件失败次数
    uint64_t params_fetched;        // 参数拉取次数
    uint64_t params_success;        // 参数成功次数
    uint64_t params_failed;         // 参数失败次数
    double avg_heartbeat_latency;   // 平均心跳延迟(ms)
    double avg_events_latency;      // 平均事件延迟(ms)
    double avg_params_latency;      // 平均参数延迟(ms)
    uint64_t bytes_sent;            // 总发送字节数
    uint64_t bytes_received;        // 总接收字节数
    uint32_t active_connections;    // 活跃连接数
    uint32_t pool_size;             // 连接池大小
};

//+------------------------------------------------------------------+
//| 紧凑心跳数据结构（80字节）                                         |
//+------------------------------------------------------------------+
#pragma pack(push, 1)
struct CompactHeartbeat {
    uint32_t timestamp;        // 4字节 - Unix时间戳
    uint32_t sequence;         // 4字节 - 序列号
    float equity;              // 4字节 - 净值
    float balance;             // 4字节 - 余额
    uint16_t open_positions;   // 2字节 - 持仓数量
    uint16_t param_version;    // 2字节 - 参数版本
    uint8_t ea_state;          // 1字节 - EA状态
    uint8_t flags;             // 1字节 - 状态标志位
    char symbol[8];            // 8字节 - 交易品种
    uint32_t magic_number;     // 4字节 - 魔术数字
    float floating_pnl;        // 4字节 - 浮动盈亏
    uint16_t timeframe;        // 2字节 - 时间周期
    uint8_t reserved[40];      // 40字节 - 预留扩展
};
#pragma pack(pop)

//+------------------------------------------------------------------+
//| DLL导出接口函数                                                   |
//+------------------------------------------------------------------+
extern "C" {
    //--- 初始化和配置
    ASYNCNET_API int AsyncNet_Initialize(const char* server_host, int timeout_ms);
    ASYNCNET_API int AsyncNet_SetAuth(const char* account, const char* license, const char* secret_key);
    ASYNCNET_API int AsyncNet_SetConfig(int max_connections, int worker_threads, int queue_size);

    //--- 异步请求接口
    ASYNCNET_API int AsyncNet_SendHeartbeat(const char* data, int data_len);
    ASYNCNET_API int AsyncNet_SendEvents(const char* data, int data_len);
    ASYNCNET_API int AsyncNet_FetchParams();
    ASYNCNET_API int AsyncNet_SendPing();

    //--- 响应处理
    ASYNCNET_API int AsyncNet_ProcessResponses();
    ASYNCNET_API int AsyncNet_GetHeartbeatResponse(char* buffer, int buffer_size);
    ASYNCNET_API int AsyncNet_GetEventsResponse(char* buffer, int buffer_size);
    ASYNCNET_API int AsyncNet_GetParamsResponse(char* buffer, int buffer_size);
    ASYNCNET_API int AsyncNet_GetPingResponse(char* buffer, int buffer_size);

    //--- 状态查询
    ASYNCNET_API int AsyncNet_GetConnectionStatus();
    ASYNCNET_API int AsyncNet_GetQueueLength(int request_type);
    ASYNCNET_API int AsyncNet_GetStatistics(NetworkStats* stats);
    ASYNCNET_API int AsyncNet_GetLastError(char* error_buffer, int buffer_size);

    //--- 控制接口
    ASYNCNET_API int AsyncNet_Pause();
    ASYNCNET_API int AsyncNet_Resume();
    ASYNCNET_API int AsyncNet_ClearQueue(int request_type);

    //--- 清理
    ASYNCNET_API void AsyncNet_Cleanup();

    //--- 版本信息
    ASYNCNET_API const char* AsyncNet_GetVersion();
    ASYNCNET_API int AsyncNet_GetBuildNumber();
}

//+------------------------------------------------------------------+
//| 错误代码定义                                                      |
//+------------------------------------------------------------------+
#define ASYNCNET_SUCCESS           0    // 成功
#define ASYNCNET_ERROR_INIT       -1    // 初始化失败
#define ASYNCNET_ERROR_AUTH       -2    // 认证失败
#define ASYNCNET_ERROR_NETWORK    -3    // 网络错误
#define ASYNCNET_ERROR_TIMEOUT    -4    // 超时
#define ASYNCNET_ERROR_QUEUE_FULL -5    // 队列满
#define ASYNCNET_ERROR_INVALID_PARAM -6 // 无效参数
#define ASYNCNET_ERROR_NOT_INIT   -7    // 未初始化
#define ASYNCNET_ERROR_MEMORY     -8    // 内存错误
#define ASYNCNET_ERROR_CURL       -9    // libcurl错误
#define ASYNCNET_ERROR_JSON       -10   // JSON解析错误

//+------------------------------------------------------------------+
//| 配置常量                                                          |
//+------------------------------------------------------------------+
#define ASYNCNET_VERSION          "1.0.0"
#define ASYNCNET_BUILD_NUMBER     1001
#define ASYNCNET_MAX_CONNECTIONS  10
#define ASYNCNET_MAX_WORKERS      4
#define ASYNCNET_MAX_QUEUE_SIZE   1000
#define ASYNCNET_DEFAULT_TIMEOUT  5000  // 5秒
#define ASYNCNET_HEARTBEAT_SIZE   80    // 紧凑心跳大小

#endif // ASYNCNETDLL_H