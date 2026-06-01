//+------------------------------------------------------------------+
//|                                        RequestQueue.cpp         |
//|                  请求队列管理 - 实现文件                           |
//+------------------------------------------------------------------+
#include "RequestQueue.h"
#include <algorithm>
#include <chrono>

//+------------------------------------------------------------------+
//| 构造函数和析构函数                                                |
//+------------------------------------------------------------------+
RequestQueue::RequestQueue()
    : max_size_(100)
    , drop_on_full_(true)
    , total_requests_(0)
    , dropped_requests_(0)
    , heartbeat_count_(0)
    , events_count_(0)
    , params_count_(0)
    , ping_count_(0)
    , total_wait_time_ms_(0.0)
    , processed_requests_(0)
{
}

RequestQueue::~RequestQueue() {
    Shutdown();
}

//+------------------------------------------------------------------+
//| 配置方法                                                          |
//+------------------------------------------------------------------+
void RequestQueue::SetMaxSize(int max_size) {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    max_size_ = std::max(1, std::min(max_size, ASYNCNET_MAX_QUEUE_SIZE));
}

void RequestQueue::SetDropOnFull(bool drop_on_full) {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    drop_on_full_ = drop_on_full;
}

//+------------------------------------------------------------------+
//| 队列操作                                                          |
//+------------------------------------------------------------------+
bool RequestQueue::Push(std::unique_ptr<RequestContext> request) {
    if (!request) {
        return false;
    }

    std::unique_lock<std::mutex> lock(queue_mutex_);

    // 检查队列是否已满
    if (queue_.size() >= max_size_) {
        if (drop_on_full_) {
            // 根据策略决定是否丢弃请求
            if (ShouldDropRequest(request->type)) {
                dropped_requests_.fetch_add(1);
                return false;
            }

            // 如果是高优先级请求，丢弃一个低优先级请求
            if (GetRequestPriority(request->type) == PRIORITY_HIGH && !queue_.empty()) {
                // 找到并移除一个低优先级请求
                std::vector<std::unique_ptr<RequestContext>> temp_requests;
                bool removed_low_priority = false;

                while (!queue_.empty() && !removed_low_priority) {
                    auto top_request = std::move(const_cast<std::unique_ptr<RequestContext>&>(queue_.top()));
                    queue_.pop();

                    if (GetRequestPriority(top_request->type) == PRIORITY_LOW) {
                        // 丢弃这个低优先级请求
                        DecrementCounter(top_request->type);
                        dropped_requests_.fetch_add(1);
                        removed_low_priority = true;
                    } else {
                        temp_requests.push_back(std::move(top_request));
                    }
                }

                // 将临时保存的请求放回队列
                for (auto& temp_req : temp_requests) {
                    queue_.push(std::move(temp_req));
                }

                if (!removed_low_priority) {
                    // 没有找到可丢弃的低优先级请求
                    dropped_requests_.fetch_add(1);
                    return false;
                }
            } else {
                dropped_requests_.fetch_add(1);
                return false;
            }
        } else {
            // 等待队列有空间
            queue_cv_.wait(lock, [this] { return queue_.size() < max_size_; });
        }
    }

    // 添加请求到队列
    IncrementCounter(request->type);
    total_requests_.fetch_add(1);
    queue_.push(std::move(request));

    lock.unlock();
    queue_cv_.notify_one();

    return true;
}

std::unique_ptr<RequestContext> RequestQueue::Pop() {
    std::unique_lock<std::mutex> lock(queue_mutex_);

    if (queue_.empty()) {
        return nullptr;
    }

    auto request = std::move(const_cast<std::unique_ptr<RequestContext>&>(queue_.top()));
    queue_.pop();

    DecrementCounter(request->type);
    UpdateWaitTimeStats(*request);

    lock.unlock();
    queue_cv_.notify_one();

    return request;
}

std::unique_ptr<RequestContext> RequestQueue::PopTimeout(int timeout_ms) {
    std::unique_lock<std::mutex> lock(queue_mutex_);

    if (queue_cv_.wait_for(lock, std::chrono::milliseconds(timeout_ms),
                          [this] { return !queue_.empty(); })) {
        auto request = std::move(const_cast<std::unique_ptr<RequestContext>&>(queue_.top()));
        queue_.pop();

        DecrementCounter(request->type);
        UpdateWaitTimeStats(*request);

        lock.unlock();
        queue_cv_.notify_one();

        return request;
    }

    return nullptr;
}

//+------------------------------------------------------------------+
//| 状态查询                                                          |
//+------------------------------------------------------------------+
bool RequestQueue::HasRequests() const {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    return !queue_.empty();
}

int RequestQueue::GetLength() const {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    return queue_.size();
}

int RequestQueue::GetLength(int request_type) const {
    switch (static_cast<RequestType>(request_type)) {
        case REQ_HEARTBEAT:
            return heartbeat_count_.load();
        case REQ_EVENTS:
            return events_count_.load();
        case REQ_PARAMS:
            return params_count_.load();
        case REQ_PING:
            return ping_count_.load();
        default:
            return 0;
    }
}

bool RequestQueue::IsFull() const {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    return queue_.size() >= max_size_;
}

bool RequestQueue::IsEmpty() const {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    return queue_.empty();
}

//+------------------------------------------------------------------+
//| 统计信息                                                          |
//+------------------------------------------------------------------+
void RequestQueue::GetStatistics(QueueStats& stats) const {
    std::lock_guard<std::mutex> stats_lock(stats_mutex_);
    std::lock_guard<std::mutex> queue_lock(queue_mutex_);

    stats.total_requests = total_requests_.load();
    stats.heartbeat_requests = heartbeat_count_.load();
    stats.events_requests = events_count_.load();
    stats.params_requests = params_count_.load();
    stats.ping_requests = ping_count_.load();
    stats.max_queue_size = max_size_;
    stats.dropped_requests = dropped_requests_.load();

    if (processed_requests_ > 0) {
        stats.avg_wait_time_ms = total_wait_time_ms_ / processed_requests_;
    } else {
        stats.avg_wait_time_ms = 0.0;
    }
}

void RequestQueue::ResetStatistics() {
    std::lock_guard<std::mutex> lock(stats_mutex_);

    total_requests_.store(0);
    dropped_requests_.store(0);
    heartbeat_count_.store(0);
    events_count_.store(0);
    params_count_.store(0);
    ping_count_.store(0);
    total_wait_time_ms_ = 0.0;
    processed_requests_ = 0;
}

//+------------------------------------------------------------------+
//| 清理操作                                                          |
//+------------------------------------------------------------------+
int RequestQueue::Clear() {
    std::lock_guard<std::mutex> lock(queue_mutex_);

    int cleared_count = queue_.size();

    // 清空队列并重置计数器
    while (!queue_.empty()) {
        auto request = std::move(const_cast<std::unique_ptr<RequestContext>&>(queue_.top()));
        queue_.pop();
        DecrementCounter(request->type);
    }

    queue_cv_.notify_all();
    return cleared_count;
}

int RequestQueue::Clear(int request_type) {
    std::lock_guard<std::mutex> lock(queue_mutex_);

    RequestType target_type = static_cast<RequestType>(request_type);
    std::vector<std::unique_ptr<RequestContext>> remaining_requests;
    int cleared_count = 0;

    // 移除指定类型的请求
    while (!queue_.empty()) {
        auto request = std::move(const_cast<std::unique_ptr<RequestContext>&>(queue_.top()));
        queue_.pop();

        if (request->type == target_type) {
            DecrementCounter(request->type);
            cleared_count++;
        } else {
            remaining_requests.push_back(std::move(request));
        }
    }

    // 将剩余请求放回队列
    for (auto& request : remaining_requests) {
        queue_.push(std::move(request));
    }

    queue_cv_.notify_all();
    return cleared_count;
}

void RequestQueue::Shutdown() {
    Clear();
    queue_cv_.notify_all();
}

//+------------------------------------------------------------------+
//| 内部方法                                                          |
//+------------------------------------------------------------------+
bool RequestQueue::ShouldDropRequest(RequestType type) const {
    // 高优先级请求不丢弃
    if (GetRequestPriority(type) == PRIORITY_HIGH) {
        return false;
    }

    // 根据队列中的请求类型分布决定是否丢弃
    int total_in_queue = heartbeat_count_.load() + events_count_.load() +
                        params_count_.load() + ping_count_.load();

    if (total_in_queue == 0) {
        return false;
    }

    // 如果队列中低优先级请求过多，允许丢弃
    int low_priority_count = params_count_.load();
    double low_priority_ratio = static_cast<double>(low_priority_count) / total_in_queue;

    return (type == REQ_PARAMS && low_priority_ratio > 0.5);
}

void RequestQueue::UpdateWaitTimeStats(const RequestContext& request) {
    std::lock_guard<std::mutex> lock(stats_mutex_);

    auto now = std::chrono::steady_clock::now();
    auto wait_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - request.created_time).count();

    total_wait_time_ms_ += wait_time;
    processed_requests_++;
}

void RequestQueue::IncrementCounter(RequestType type) {
    switch (type) {
        case REQ_HEARTBEAT:
            heartbeat_count_.fetch_add(1);
            break;
        case REQ_EVENTS:
            events_count_.fetch_add(1);
            break;
        case REQ_PARAMS:
            params_count_.fetch_add(1);
            break;
        case REQ_PING:
            ping_count_.fetch_add(1);
            break;
    }
}

void RequestQueue::DecrementCounter(RequestType type) {
    switch (type) {
        case REQ_HEARTBEAT:
            heartbeat_count_.fetch_sub(1);
            break;
        case REQ_EVENTS:
            events_count_.fetch_sub(1);
            break;
        case REQ_PARAMS:
            params_count_.fetch_sub(1);
            break;
        case REQ_PING:
            ping_count_.fetch_sub(1);
            break;
    }
}

RequestPriority RequestQueue::GetRequestPriority(RequestType type) const {
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

//+------------------------------------------------------------------+
//| 队列工厂类实现                                                    |
//+------------------------------------------------------------------+
std::unique_ptr<RequestQueue> QueueFactory::CreateQueue(int max_size, bool drop_on_full) {
    auto queue = std::make_unique<RequestQueue>();
    queue->SetMaxSize(max_size);
    queue->SetDropOnFull(drop_on_full);
    return queue;
}

std::unique_ptr<RequestQueue> QueueFactory::CreateHeartbeatQueue(int max_size) {
    auto queue = std::make_unique<RequestQueue>();
    queue->SetMaxSize(max_size);
    queue->SetDropOnFull(false); // 心跳队列不丢弃请求
    return queue;
}

std::unique_ptr<RequestQueue> QueueFactory::CreateEventsQueue(int max_size) {
    auto queue = std::make_unique<RequestQueue>();
    queue->SetMaxSize(max_size);
    queue->SetDropOnFull(true); // 事件队列可以丢弃旧请求
    return queue;
}

std::unique_ptr<RequestQueue> QueueFactory::CreateParamsQueue(int max_size) {
    auto queue = std::make_unique<RequestQueue>();
    queue->SetMaxSize(max_size);
    queue->SetDropOnFull(true); // 参数队列可以丢弃重复请求
    return queue;
}