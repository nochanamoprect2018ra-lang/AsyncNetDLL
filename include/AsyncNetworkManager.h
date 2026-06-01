//+------------------------------------------------------------------+
//|                                   AsyncNetworkManager.h         |
//|                  异步网络管理器 - 核心管理类                        |
//|              管理连接池、请求队列、工作线程                         |
//+------------------------------------------------------------------+
#pragma once

#include "AsyncNetDLL.h"
#include "ConnectionPool.h"
#include "RequestQueue.h"
#include "ResponseQueue.h"
#include <memory>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <string>

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
                      status(static_cast<RequestStatus>(STATUS_PENDING)), response_code(0), latency_ms(0.0) {}
};

//+------------------------------------------------------------------+
//| 异步网络管理器类                                                  |
//+------------------------------------------------------------------+
class AsyncNetworkManager {
private:
    // 核心组件
    std::unique_ptr<ConnectionPool> connection_pool_;
    std::unique_ptr<RequestQueue> request_queue_;
    std::unique_ptr<ResponseQueue> response_queue_;

    // 线程管理
    std::vector<std::thread> worker_threads_;
    std::atomic<bool> shutdown_flag_;
    std::atomic<bool> paused_flag_;
    std::condition_variable worker_cv_;
    std::mutex worker_mutex_;

    // 配置参数
    std::string server_host_;
    int timeout_ms_;
    int max_connections_;
    int worker_thread_count_;
    int max_queue_size_;

    // 认证信息
    std::string account_;
    std::string license_;
    std::string secret_key_;
    std::mutex auth_mutex_;

    // 统计信息
    NetworkStats stats_;
    std::mutex stats_mutex_;
    std::atomic<int> next_request_id_;

    // 内部方法
    void WorkerThread();
    void ExecuteRequest(RequestContext* request);
    std::string BuildRequestUrl(RequestType type, const std::string& data);
    std::string GenerateSignature(const std::string& data);
    void UpdateStatistics(RequestContext* request);
    bool ShouldRetry(RequestContext* request);
    void HandleRequestError(RequestContext* request, const std::string& error);

public:
    AsyncNetworkManager();
    ~AsyncNetworkManager();

    // 初始化和配置
    bool Initialize(const std::string& server_host, int timeout_ms);
    void SetAuth(const std::string& account, const std::string& license, const std::string& secret_key);
    void SetConfig(int max_connections, int worker_threads, int queue_size);

    // 请求管理
    int SendRequest(RequestType type, const char* data, int data_len);
    int ProcessResponses();
    bool GetResponse(RequestType type, std::string& response);

    // 状态查询
    int GetConnectionStatus() const;
    int GetQueueLength(int request_type) const;
    void GetStatistics(NetworkStats& stats) const;

    // 控制接口
    void Pause();
    void Resume();
    int ClearQueue(int request_type);
    void Shutdown();
};

//+------------------------------------------------------------------+
//| 重试配置结构                                                      |
//+------------------------------------------------------------------+
struct RetryConfig {
    int max_retries;
    int base_delay_ms;
    double backoff_multiplier;

    static RetryConfig ForHeartbeat() {
        return {2, 200, 1.5}; // 心跳快速重试
    }

    static RetryConfig ForEvents() {
        return {3, 500, 2.0}; // 事件标准重试
    }

    static RetryConfig ForParams() {
        return {5, 1000, 2.0}; // 参数重要数据多重试
    }

    static RetryConfig ForPing() {
        return {1, 100, 1.0}; // Ping最少重试
    }
};

//+------------------------------------------------------------------+
//| URL路径常量                                                       |
//+------------------------------------------------------------------+
namespace UrlPaths {
    const std::string HEARTBEAT = "/v1/heartbeat";
    const std::string EVENTS = "/v1/events";
    const std::string PARAMS = "/v1/params";
    const std::string PING = "/v1/ping";
}