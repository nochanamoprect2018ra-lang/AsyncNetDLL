//+------------------------------------------------------------------+
//|                                        ResponseQueue.cpp        |
//|                  响应队列管理 - 实现文件                           |
//+------------------------------------------------------------------+
#include "ResponseQueue.h"
#include <algorithm>
#include <thread>
#include <chrono>

//+------------------------------------------------------------------+
//| 构造函数和析构函数                                                |
//+------------------------------------------------------------------+
ResponseQueue::ResponseQueue()
    : config_(ResponseCacheConfig::Default())
    , total_responses_(0)
    , processed_responses_(0)
    , expired_responses_(0)
    , heartbeat_responses_(0)
    , events_responses_(0)
    , params_responses_(0)
    , cleanup_running_(false)
{
    if (config_.auto_cleanup && config_.cleanup_interval_ms > 0) {
        cleanup_running_.store(true);
        cleanup_thread_ = std::thread(&ResponseQueue::CleanupThread, this);
    }
}

ResponseQueue::ResponseQueue(const ResponseCacheConfig& config)
    : config_(config)
    , total_responses_(0)
    , processed_responses_(0)
    , expired_responses_(0)
    , heartbeat_responses_(0)
    , events_responses_(0)
    , params_responses_(0)
    , cleanup_running_(false)
{
    if (config_.auto_cleanup && config_.cleanup_interval_ms > 0) {
        cleanup_running_.store(true);
        cleanup_thread_ = std::thread(&ResponseQueue::CleanupThread, this);
    }
}

ResponseQueue::~ResponseQueue() {
    Shutdown();
}

//+------------------------------------------------------------------+
//| 配置方法                                                          |
//+------------------------------------------------------------------+
void ResponseQueue::SetConfig(const ResponseCacheConfig& config) {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    config_ = config;
}

ResponseCacheConfig ResponseQueue::GetConfig() const {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    return config_;
}

//+------------------------------------------------------------------+
//| 响应管理                                                          |
//+------------------------------------------------------------------+
bool ResponseQueue::Push(std::unique_ptr<ResponseContext> response) {
    if (!response) {
        return false;
    }

    RequestType type = response->type;
    ResponseContext* response_ptr = response.get();

    {
        std::lock_guard<std::mutex> lock(queue_mutex_);

        // 检查队列大小限制
        auto& queue = type_queues_[type];
        while (queue.size() >= config_.max_responses_per_type) {
            queue.pop(); // 移除最旧的响应
            expired_responses_.fetch_add(1);
        }

        // 添加到类型队列
        queue.push(std::move(response));
    }

    {
        std::lock_guard<std::mutex> cache_lock(cache_mutex_);
        // 更新最新响应缓存
        latest_responses_[type] = std::make_unique<ResponseContext>(*response_ptr);
    }

    // 更新统计信息
    total_responses_.fetch_add(1);
    UpdateStatistics(response_ptr);

    return true;
}

std::unique_ptr<ResponseContext> ResponseQueue::Pop(RequestType type) {
    std::lock_guard<std::mutex> lock(queue_mutex_);

    auto it = type_queues_.find(type);
    if (it == type_queues_.end() || it->second.empty()) {
        return nullptr;
    }

    auto response = std::move(it->second.front());
    it->second.pop();

    if (response) {
        response->processed = true;
        processed_responses_.fetch_add(1);
    }

    return response;
}

bool ResponseQueue::GetLatestResponse(RequestType type, std::string& response_data) {
    std::lock_guard<std::mutex> lock(cache_mutex_);

    auto it = latest_responses_.find(type);
    if (it == latest_responses_.end() || !it->second) {
        return false;
    }

    // 检查响应是否过期
    if (IsResponseExpired(*it->second)) {
        latest_responses_.erase(it);
        return false;
    }

    response_data = it->second->response_data;
    it->second->processed = true;
    processed_responses_.fetch_add(1);

    return true;
}

bool ResponseQueue::GetResponse(RequestType type, std::string& response_data) {
    // 优先从最新响应缓存获取
    if (GetLatestResponse(type, response_data)) {
        return true;
    }

    // 从队列获取
    auto response = Pop(type);
    if (response && response->status == REQ_STATUS_SUCCESS) {
        response_data = response->response_data;
        return true;
    }

    return false;
}

//+------------------------------------------------------------------+
//| 批量处理                                                          |
//+------------------------------------------------------------------+
int ResponseQueue::ProcessCompleted() {
    int processed_count = 0;

    std::lock_guard<std::mutex> lock(queue_mutex_);

    // 处理所有类型的响应队列
    for (auto& pair : type_queues_) {
        auto& queue = pair.second;

        while (!queue.empty()) {
            auto& response = queue.front();
            if (response && response->status != static_cast<RequestStatus>(REQ_STATUS_PENDING)) {
                response->processed = true;
                processed_responses_.fetch_add(1);
                processed_count++;
            }
            break; // 只处理队列前端的完成响应
        }
    }

    return processed_count;
}

std::vector<std::unique_ptr<ResponseContext>> ResponseQueue::PopAll(RequestType type) {
    std::vector<std::unique_ptr<ResponseContext>> responses;

    std::lock_guard<std::mutex> lock(queue_mutex_);

    auto it = type_queues_.find(type);
    if (it == type_queues_.end()) {
        return responses;
    }

    auto& queue = it->second;
    while (!queue.empty()) {
        auto response = std::move(queue.front());
        queue.pop();

        if (response) {
            response->processed = true;
            processed_responses_.fetch_add(1);
            responses.push_back(std::move(response));
        }
    }

    return responses;
}

//+------------------------------------------------------------------+
//| 状态查询                                                          |
//+------------------------------------------------------------------+
int ResponseQueue::GetQueueLength(RequestType type) const {
    std::lock_guard<std::mutex> lock(queue_mutex_);

    auto it = type_queues_.find(type);
    if (it == type_queues_.end()) {
        return 0;
    }

    return it->second.size();
}

int ResponseQueue::GetTotalLength() const {
    std::lock_guard<std::mutex> lock(queue_mutex_);

    int total = 0;
    for (const auto& pair : type_queues_) {
        total += pair.second.size();
    }

    return total;
}

bool ResponseQueue::HasResponse(RequestType type) const {
    std::lock_guard<std::mutex> lock(queue_mutex_);

    auto it = type_queues_.find(type);
    return it != type_queues_.end() && !it->second.empty();
}

bool ResponseQueue::HasLatestResponse(RequestType type) const {
    std::lock_guard<std::mutex> lock(cache_mutex_);

    auto it = latest_responses_.find(type);
    if (it == latest_responses_.end() || !it->second) {
        return false;
    }

    return !IsResponseExpired(*it->second);
}

//+------------------------------------------------------------------+
//| 统计信息                                                          |
//+------------------------------------------------------------------+
void ResponseQueue::GetStatistics(QueueStatistics& stats) const {
    stats.total_responses = total_responses_.load();
    stats.processed_responses = processed_responses_.load();
    stats.expired_responses = expired_responses_.load();
    stats.heartbeat_responses = heartbeat_responses_.load();
    stats.events_responses = events_responses_.load();
    stats.params_responses = params_responses_.load();

    // 获取各类型队列长度
    std::lock_guard<std::mutex> lock(queue_mutex_);
    stats.queue_lengths[REQ_HEARTBEAT] = GetQueueLength(REQ_HEARTBEAT);
    stats.queue_lengths[REQ_EVENTS] = GetQueueLength(REQ_EVENTS);
    stats.queue_lengths[REQ_PARAMS] = GetQueueLength(REQ_PARAMS);
    stats.queue_lengths[REQ_PING] = GetQueueLength(REQ_PING);
}

void ResponseQueue::ResetStatistics() {
    total_responses_.store(0);
    processed_responses_.store(0);
    expired_responses_.store(0);
    heartbeat_responses_.store(0);
    events_responses_.store(0);
    params_responses_.store(0);
}

//+------------------------------------------------------------------+
//| 清理操作                                                          |
//+------------------------------------------------------------------+
int ResponseQueue::Clear(RequestType type) {
    std::lock_guard<std::mutex> queue_lock(queue_mutex_);
    std::lock_guard<std::mutex> cache_lock(cache_mutex_);

    int cleared_count = 0;

    // 清理队列
    auto it = type_queues_.find(type);
    if (it != type_queues_.end()) {
        cleared_count = it->second.size();
        while (!it->second.empty()) {
            it->second.pop();
        }
    }

    // 清理最新响应缓存
    auto cache_it = latest_responses_.find(type);
    if (cache_it != latest_responses_.end()) {
        latest_responses_.erase(cache_it);
    }

    return cleared_count;
}

int ResponseQueue::ClearAll() {
    std::lock_guard<std::mutex> queue_lock(queue_mutex_);
    std::lock_guard<std::mutex> cache_lock(cache_mutex_);

    int cleared_count = 0;

    // 清理所有队列
    for (auto& pair : type_queues_) {
        cleared_count += pair.second.size();
        while (!pair.second.empty()) {
            pair.second.pop();
        }
    }
    type_queues_.clear();

    // 清理所有缓存
    latest_responses_.clear();

    return cleared_count;
}

int ResponseQueue::ClearExpired() {
    std::lock_guard<std::mutex> queue_lock(queue_mutex_);
    std::lock_guard<std::mutex> cache_lock(cache_mutex_);

    int cleared_count = 0;

    // 清理过期的队列响应
    for (auto& pair : type_queues_) {
        auto& queue = pair.second;
        std::queue<std::unique_ptr<ResponseContext>> temp_queue;

        while (!queue.empty()) {
            auto response = std::move(queue.front());
            queue.pop();

            if (response && !IsResponseExpired(*response)) {
                temp_queue.push(std::move(response));
            } else {
                cleared_count++;
            }
        }

        queue = std::move(temp_queue);
    }

    // 清理过期的缓存响应
    auto cache_it = latest_responses_.begin();
    while (cache_it != latest_responses_.end()) {
        if (!cache_it->second || IsResponseExpired(*cache_it->second)) {
            cache_it = latest_responses_.erase(cache_it);
            cleared_count++;
        } else {
            ++cache_it;
        }
    }

    expired_responses_.fetch_add(cleared_count);
    return cleared_count;
}

void ResponseQueue::Shutdown() {
    // 停止清理线程
    if (cleanup_running_.load()) {
        cleanup_running_.store(false);
        if (cleanup_thread_.joinable()) {
            cleanup_thread_.join();
        }
    }

    // 清理所有响应
    ClearAll();
}

//+------------------------------------------------------------------+
//| 内部方法                                                          |
//+------------------------------------------------------------------+
void ResponseQueue::CleanupExpiredResponses() {
    if (config_.auto_cleanup) {
        ClearExpired();
    }
}

void ResponseQueue::CleanupThread() {
    while (cleanup_running_.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(config_.cleanup_interval_ms));

        if (cleanup_running_.load()) {
            CleanupExpiredResponses();
        }
    }
}

bool ResponseQueue::IsResponseExpired(const ResponseContext& response) const {
    if (config_.cache_timeout_seconds <= 0) {
        return false; // 永不过期
    }

    auto now = std::chrono::steady_clock::now();
    auto age = std::chrono::duration_cast<std::chrono::seconds>(
        now - response.received_time).count();

    return age > config_.cache_timeout_seconds;
}

void ResponseQueue::UpdateStatistics(const ResponseContext& response) {
    switch (response.type) {
        case REQ_HEARTBEAT:
            heartbeat_responses_.fetch_add(1);
            break;
        case REQ_EVENTS:
            events_responses_.fetch_add(1);
            break;
        case REQ_PARAMS:
            params_responses_.fetch_add(1);
            break;
        default:
            break;
    }
}

//+------------------------------------------------------------------+
//| 响应处理器实现                                                    |
//+------------------------------------------------------------------+
void HeartbeatResponseHandler::HandleResponse(const ResponseContext& response) {
    // 处理心跳响应的特定逻辑
    if (response.status == REQ_STATUS_SUCCESS) {
        // 解析心跳响应，检查服务器指令
        // 这里可以添加具体的心跳响应处理逻辑
    }
}

void EventsResponseHandler::HandleResponse(const ResponseContext& response) {
    // 处理事件上报响应的特定逻辑
    if (response.status == REQ_STATUS_SUCCESS) {
        // 确认事件已成功上报
        // 这里可以添加具体的事件响应处理逻辑
    }
}

void ParamsResponseHandler::HandleResponse(const ResponseContext& response) {
    // 处理参数拉取响应的特定逻辑
    if (response.status == REQ_STATUS_SUCCESS) {
        // 解析并应用新参数
        // 这里可以添加具体的参数响应处理逻辑
    }
}

//+------------------------------------------------------------------+
//| 响应分发器实现                                                    |
//+------------------------------------------------------------------+
ResponseDispatcher::ResponseDispatcher() {
    // 注册默认处理器
    RegisterHandler(std::make_unique<HeartbeatResponseHandler>());
    RegisterHandler(std::make_unique<EventsResponseHandler>());
    RegisterHandler(std::make_unique<ParamsResponseHandler>());
}

ResponseDispatcher::~ResponseDispatcher() {
    std::lock_guard<std::mutex> lock(handlers_mutex_);
    handlers_.clear();
}

void ResponseDispatcher::RegisterHandler(std::unique_ptr<IResponseHandler> handler) {
    if (!handler) return;

    std::lock_guard<std::mutex> lock(handlers_mutex_);
    handlers_.push_back(std::move(handler));
}

void ResponseDispatcher::UnregisterHandler(RequestType type) {
    std::lock_guard<std::mutex> lock(handlers_mutex_);

    handlers_.erase(
        std::remove_if(handlers_.begin(), handlers_.end(),
            [type](const std::unique_ptr<IResponseHandler>& handler) {
                return handler->CanHandle(type);
            }),
        handlers_.end()
    );
}

bool ResponseDispatcher::DispatchResponse(const ResponseContext& response) {
    std::lock_guard<std::mutex> lock(handlers_mutex_);

    bool handled = false;
    for (const auto& handler : handlers_) {
        if (handler->CanHandle(response.type)) {
            handler->HandleResponse(response);
            handled = true;
        }
    }

    return handled;
}

int ResponseDispatcher::DispatchAll(const std::vector<std::unique_ptr<ResponseContext>>& responses) {
    int dispatched_count = 0;

    for (const auto& response : responses) {
        if (response && DispatchResponse(*response)) {
            dispatched_count++;
        }
    }

    return dispatched_count;
}

//+------------------------------------------------------------------+
//| 响应工厂类实现                                                    |
//+------------------------------------------------------------------+
std::unique_ptr<ResponseQueue> ResponseFactory::CreateQueue(const ResponseCacheConfig& config) {
    return std::make_unique<ResponseQueue>(config);
}

std::unique_ptr<ResponseDispatcher> ResponseFactory::CreateDispatcher() {
    return std::make_unique<ResponseDispatcher>();
}

std::unique_ptr<ResponseContext> ResponseFactory::CreateResponse(int request_id, RequestType type,
                                                                RequestStatus status, const std::string& data,
                                                                const std::string& error) {
    return std::make_unique<ResponseContext>(request_id, type, status, data, error);
}