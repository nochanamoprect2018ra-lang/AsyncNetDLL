//+------------------------------------------------------------------+
//|                                        ResponseQueue.h          |
//|                  响应队列管理 - 处理异步响应                        |
//|              管理HTTP响应的缓存、分发和清理                         |
//+------------------------------------------------------------------+
#pragma once

#include "AsyncNetDLL.h"
#include "AsyncNetworkManager.h"
#include <queue>
#include <thread>
#include <unordered_map>
#include <mutex>
#include <memory>
#include <atomic>
#include <chrono>

//+------------------------------------------------------------------+
//| 响应上下文结构                                                    |
//+------------------------------------------------------------------+
struct ResponseContext {
    int request_id;                // 请求ID
    RequestType type;              // 请求类型
    RequestStatus status;          // 响应状态
    std::string response_data;     // 响应数据
    std::string error_message;     // 错误信息
    long http_code;                // HTTP状态码
    double latency_ms;             // 响应延迟
    std::chrono::steady_clock::time_point received_time;  // 接收时间
    bool processed;                // 是否已处理

    ResponseContext() : request_id(0), type(REQ_HEARTBEAT), status(static_cast<RequestStatus>(REQ_STATUS_PENDING)),
                       http_code(0), latency_ms(0.0), processed(false) {}

    ResponseContext(int id, RequestType req_type, RequestStatus req_status,
                   const std::string& data, const std::string& error = "")
        : request_id(id), type(req_type), status(req_status), response_data(data),
          error_message(error), http_code(200), latency_ms(0.0), processed(false) {
        received_time = std::chrono::steady_clock::now();
    }
};

//+------------------------------------------------------------------+
//| 响应缓存配置                                                      |
//+------------------------------------------------------------------+
struct ResponseCacheConfig {
    int max_responses_per_type;    // 每种类型最大缓存响应数
    int cache_timeout_seconds;     // 缓存超时时间
    bool auto_cleanup;             // 自动清理过期响应
    int cleanup_interval_ms;       // 清理间隔

    static ResponseCacheConfig Default() {
        return {10, 300, true, 30000}; // 10个响应，5分钟超时，自动清理，30秒间隔
    }

    static ResponseCacheConfig ForHeartbeat() {
        return {5, 60, true, 10000};   // 5个响应，1分钟超时，10秒清理
    }

    static ResponseCacheConfig ForEvents() {
        return {20, 600, true, 60000}; // 20个响应，10分钟超时，1分钟清理
    }

    static ResponseCacheConfig ForParams() {
        return {3, 1800, false, 0};    // 3个响应，30分钟超时，手动清理
    }
};

//+------------------------------------------------------------------+
//| 响应队列类                                                        |
//+------------------------------------------------------------------+
class ResponseQueue {
private:
    // 按类型分组的响应队列
    std::unordered_map<RequestType, std::queue<std::unique_ptr<ResponseContext>>> type_queues_;

    // 最新响应缓存（每种类型保留最新的一个）
    std::unordered_map<RequestType, std::unique_ptr<ResponseContext>> latest_responses_;

    // 线程同步
    mutable std::mutex queue_mutex_;
    mutable std::mutex cache_mutex_;

    // 配置
    ResponseCacheConfig config_;

    // 统计信息
    std::atomic<int> total_responses_;
    std::atomic<int> processed_responses_;
    std::atomic<int> expired_responses_;
    std::atomic<int> heartbeat_responses_;
    std::atomic<int> events_responses_;
    std::atomic<int> params_responses_;

    // 清理线程
    std::thread cleanup_thread_;
    std::atomic<bool> cleanup_running_;

    // 内部方法
    void CleanupExpiredResponses();
    void CleanupThread();
    bool IsResponseExpired(const ResponseContext& response) const;
    void UpdateStatistics(const ResponseContext& response);

public:
    ResponseQueue();
    explicit ResponseQueue(const ResponseCacheConfig& config);
    ~ResponseQueue();

    // 配置
    void SetConfig(const ResponseCacheConfig& config);
    ResponseCacheConfig GetConfig() const;

    // 响应管理
    bool Push(std::unique_ptr<ResponseContext> response);
    std::unique_ptr<ResponseContext> Pop(RequestType type);
    bool GetLatestResponse(RequestType type, std::string& response_data);
    bool GetResponse(RequestType type, std::string& response_data);

    // 批量处理
    int ProcessCompleted();
    std::vector<std::unique_ptr<ResponseContext>> PopAll(RequestType type);

    // 状态查询
    int GetQueueLength(RequestType type) const;
    int GetTotalLength() const;
    bool HasResponse(RequestType type) const;
    bool HasLatestResponse(RequestType type) const;

    // 统计信息
    struct QueueStatistics {
        int total_responses;
        int processed_responses;
        int expired_responses;
        int heartbeat_responses;
        int events_responses;
        int params_responses;
        int queue_lengths[4]; // 按RequestType索引
    };
    void GetStatistics(QueueStatistics& stats) const;
    void ResetStatistics();

    // 清理操作
    int Clear(RequestType type);
    int ClearAll();
    int ClearExpired();
    void Shutdown();
};

//+------------------------------------------------------------------+
//| 响应处理器接口                                                    |
//+------------------------------------------------------------------+
class IResponseHandler {
public:
    virtual ~IResponseHandler() = default;
    virtual void HandleResponse(const ResponseContext& response) = 0;
    virtual bool CanHandle(RequestType type) const = 0;
};

//+------------------------------------------------------------------+
//| 具体响应处理器                                                    |
//+------------------------------------------------------------------+
class HeartbeatResponseHandler : public IResponseHandler {
public:
    void HandleResponse(const ResponseContext& response) override;
    bool CanHandle(RequestType type) const override { return type == REQ_HEARTBEAT; }
};

class EventsResponseHandler : public IResponseHandler {
public:
    void HandleResponse(const ResponseContext& response) override;
    bool CanHandle(RequestType type) const override { return type == REQ_EVENTS; }
};

class ParamsResponseHandler : public IResponseHandler {
public:
    void HandleResponse(const ResponseContext& response) override;
    bool CanHandle(RequestType type) const override { return type == REQ_PARAMS; }
};

//+------------------------------------------------------------------+
//| 响应分发器                                                        |
//+------------------------------------------------------------------+
class ResponseDispatcher {
private:
    std::vector<std::unique_ptr<IResponseHandler>> handlers_;
    std::mutex handlers_mutex_;

public:
    ResponseDispatcher();
    ~ResponseDispatcher();

    // 处理器管理
    void RegisterHandler(std::unique_ptr<IResponseHandler> handler);
    void UnregisterHandler(RequestType type);

    // 响应分发
    bool DispatchResponse(const ResponseContext& response);
    int DispatchAll(const std::vector<std::unique_ptr<ResponseContext>>& responses);
};

//+------------------------------------------------------------------+
//| 响应工厂类                                                        |
//+------------------------------------------------------------------+
class ResponseFactory {
public:
    static std::unique_ptr<ResponseQueue> CreateQueue(const ResponseCacheConfig& config = ResponseCacheConfig::Default());
    static std::unique_ptr<ResponseDispatcher> CreateDispatcher();
    static std::unique_ptr<ResponseContext> CreateResponse(int request_id, RequestType type,
                                                          RequestStatus status, const std::string& data,
                                                          const std::string& error = "");
};