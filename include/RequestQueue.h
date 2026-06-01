//+------------------------------------------------------------------+
//|                                        RequestQueue.h           |
//|                  请求队列管理 - 优先级队列实现                      |
//|              管理异步请求的排队、优先级和调度                       |
//+------------------------------------------------------------------+
#pragma once

#include "AsyncNetDLL.h"
#include <queue>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <atomic>

//+------------------------------------------------------------------+
//| 请求优先级定义                                                    |
//+------------------------------------------------------------------+
enum RequestPriority {
    PRIORITY_HIGH = 1,    // 心跳请求
    PRIORITY_NORMAL = 2,  // 事件上报
    PRIORITY_LOW = 3      // 参数拉取
};

//+------------------------------------------------------------------+
//| 请求比较器（用于优先级队列）                                       |
//+------------------------------------------------------------------+
struct RequestComparator {
    bool operator()(const std::unique_ptr<RequestContext>& a,
                   const std::unique_ptr<RequestContext>& b) const {
        // 优先级数字越小，优先级越高
        RequestPriority priority_a = GetRequestPriority(a->type);
        RequestPriority priority_b = GetRequestPriority(b->type);

        if (priority_a != priority_b) {
            return priority_a > priority_b; // 优先级队列是最大堆，所以反转比较
        }

        // 相同优先级按创建时间排序（FIFO）
        return a->created_time > b->created_time;
    }

private:
    RequestPriority GetRequestPriority(RequestType type) const {
        switch (type) {
            case REQ_HEARTBEAT:
            case REQ_PING:
                return PRIORITY_HIGH;
            case REQ_EVENTS:
                return PRIORITY_NORMAL;
            case REQ_PARAMS:
                return PRIORITY_LOW;
            default:
                return PRIORITY_NORMAL;
        }
    }
};

//+------------------------------------------------------------------+
//| 队列统计信息                                                      |
//+------------------------------------------------------------------+
struct QueueStats {
    int total_requests;
    int heartbeat_requests;
    int events_requests;
    int params_requests;
    int ping_requests;
    int max_queue_size;
    double avg_wait_time_ms;
    int dropped_requests;
};

//+------------------------------------------------------------------+
//| 请求队列类                                                        |
//+------------------------------------------------------------------+
class RequestQueue {
private:
    // 优先级队列
    std::priority_queue<std::unique_ptr<RequestContext>,
                       std::vector<std::unique_ptr<RequestContext>>,
                       RequestComparator> queue_;

    // 线程同步
    mutable std::mutex queue_mutex_;
    std::condition_variable queue_cv_;

    // 配置参数
    int max_size_;
    bool drop_on_full_;

    // 统计信息
    std::atomic<int> total_requests_;
    std::atomic<int> dropped_requests_;
    std::atomic<int> heartbeat_count_;
    std::atomic<int> events_count_;
    std::atomic<int> params_count_;
    std::atomic<int> ping_count_;

    // 等待时间统计
    mutable std::mutex stats_mutex_;
    double total_wait_time_ms_;
    int processed_requests_;

    // 内部方法
    bool ShouldDropRequest(RequestType type) const;
    void UpdateWaitTimeStats(const RequestContext& request);
    void IncrementCounter(RequestType type);
    void DecrementCounter(RequestType type);
    RequestPriority GetRequestPriority(RequestType type) const;

public:
    RequestQueue();
    ~RequestQueue();

    // 配置
    void SetMaxSize(int max_size);
    void SetDropOnFull(bool drop_on_full);

    // 队列操作
    bool Push(std::unique_ptr<RequestContext> request);
    std::unique_ptr<RequestContext> Pop();
    std::unique_ptr<RequestContext> PopTimeout(int timeout_ms);

    // 状态查询
    bool HasRequests() const;
    int GetLength() const;
    int GetLength(int request_type) const;
    bool IsFull() const;
    bool IsEmpty() const;

    // 统计信息
    void GetStatistics(QueueStats& stats) const;
    void ResetStatistics();

    // 清理操作
    int Clear();
    int Clear(int request_type);
    void Shutdown();
};

//+------------------------------------------------------------------+
//| 队列工厂类                                                        |
//+------------------------------------------------------------------+
class QueueFactory {
public:
    static std::unique_ptr<RequestQueue> CreateQueue(int max_size = 100, bool drop_on_full = true);
    static std::unique_ptr<RequestQueue> CreateHeartbeatQueue(int max_size = 10);
    static std::unique_ptr<RequestQueue> CreateEventsQueue(int max_size = 50);
    static std::unique_ptr<RequestQueue> CreateParamsQueue(int max_size = 5);
};