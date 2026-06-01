//+------------------------------------------------------------------+
//|                                   AsyncNetworkManager.cpp       |
//|                  异步网络管理器 - 实现文件                         |
//+------------------------------------------------------------------+
#include "AsyncNetworkManager.h"
#include "ResponseQueue.h"
#include "HMACUtils.h"
#include <curl/curl.h>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cmath>
#include <random>

//+------------------------------------------------------------------+
//| 构造函数和析构函数                                                |
//+------------------------------------------------------------------+
AsyncNetworkManager::AsyncNetworkManager()
    : shutdown_flag_(false)
    , paused_flag_(false)
    , timeout_ms_(5000)
    , max_connections_(5)
    , worker_thread_count_(2)
    , max_queue_size_(100)
    , next_request_id_(1)
{
    // 初始化统计信息
    memset(&stats_, 0, sizeof(stats_));
}

AsyncNetworkManager::~AsyncNetworkManager() {
    Shutdown();
}

//+------------------------------------------------------------------+
//| 初始化和配置                                                      |
//+------------------------------------------------------------------+
bool AsyncNetworkManager::Initialize(const std::string& server_host, int timeout_ms) {
    if (server_host.empty()) {
        return false;
    }

    server_host_ = server_host;
    timeout_ms_ = timeout_ms;

    try {
        // 创建核心组件
        connection_pool_ = std::make_unique<ConnectionPool>();
        request_queue_ = std::make_unique<RequestQueue>();
        response_queue_ = std::make_unique<ResponseQueue>();

        // 初始化连接池
        if (!connection_pool_->Initialize(max_connections_, server_host_, timeout_ms_)) {
            return false;
        }

        // 启动工作线程
        for (int i = 0; i < worker_thread_count_; ++i) {
            worker_threads_.emplace_back(&AsyncNetworkManager::WorkerThread, this);
        }

        return true;
    }
    catch (const std::exception& e) {
        return false;
    }
}

void AsyncNetworkManager::SetAuth(const std::string& account, const std::string& license, const std::string& secret_key) {
    std::lock_guard<std::mutex> lock(auth_mutex_);
    account_ = account;
    license_ = license;
    secret_key_ = secret_key;
}

void AsyncNetworkManager::SetConfig(int max_connections, int worker_threads, int queue_size) {
    max_connections_ = std::max(1, std::min(max_connections, ASYNCNET_MAX_CONNECTIONS));
    worker_thread_count_ = std::max(1, std::min(worker_threads, ASYNCNET_MAX_WORKERS));
    max_queue_size_ = std::max(10, std::min(queue_size, ASYNCNET_MAX_QUEUE_SIZE));

    // 重新配置连接池
    if (connection_pool_) {
        connection_pool_->Resize(max_connections_);
    }

    // 重新配置请求队列
    if (request_queue_) {
        request_queue_->SetMaxSize(max_queue_size_);
    }
}

//+------------------------------------------------------------------+
//| 请求管理                                                          |
//+------------------------------------------------------------------+
int AsyncNetworkManager::SendRequest(RequestType type, const char* data, int data_len) {
    if (shutdown_flag_.load()) {
        return -1;
    }

    // 创建请求上下文
    auto request = std::make_unique<RequestContext>();
    request->id = next_request_id_.fetch_add(1);
    request->type = type;
    request->created_time = std::chrono::steady_clock::now();

    // 设置请求数据
    if (data && data_len > 0) {
        request->data = std::string(data, data_len);
    }

    // 构建请求URL
    request->url = BuildRequestUrl(type, request->data);
    if (request->url.empty()) {
        return -1;
    }

    // 添加到请求队列
    if (!request_queue_->Push(std::move(request))) {
        return -1; // 队列满
    }

    // 通知工作线程
    worker_cv_.notify_one();

    return request->id;
}

int AsyncNetworkManager::ProcessResponses() {
    if (!response_queue_) {
        return 0;
    }

    return response_queue_->ProcessCompleted();
}

bool AsyncNetworkManager::GetResponse(RequestType type, std::string& response) {
    if (!response_queue_) {
        return false;
    }

    return response_queue_->GetResponse(type, response);
}

//+------------------------------------------------------------------+
//| 工作线程实现                                                      |
//+------------------------------------------------------------------+
void AsyncNetworkManager::WorkerThread() {
    while (!shutdown_flag_.load()) {
        std::unique_ptr<RequestContext> request;

        // 等待请求或暂停/关闭信号
        {
            std::unique_lock<std::mutex> lock(worker_mutex_);
            worker_cv_.wait(lock, [this] {
                return shutdown_flag_.load() ||
                       (!paused_flag_.load() && request_queue_->HasRequests());
            });

            if (shutdown_flag_.load()) {
                break;
            }

            if (paused_flag_.load()) {
                continue;
            }

            request = request_queue_->Pop();
        }

        if (request) {
            ExecuteRequest(request.get());

            // 将 RequestContext 转换为 ResponseContext 并添加到响应队列
            auto response = std::make_unique<ResponseContext>();
            response->request_id = request->id;
            response->type = request->type;
            response->status = request->status;
            response->response_data = request->response;
            response->error_message = request->error_message;
            response->http_code = request->response_code;
            response->latency_ms = request->latency_ms;
            response->received_time = std::chrono::steady_clock::now();
            response_queue_->Push(std::move(response));
        }
    }
}

void AsyncNetworkManager::ExecuteRequest(RequestContext* request) {
    if (!request || !connection_pool_) {
        return;
    }

    request->sent_time = std::chrono::steady_clock::now();
    request->status = static_cast<RequestStatus>(REQ_STATUS_PENDING);

    // 获取连接
    CURL* handle = connection_pool_->AcquireHandle();
    if (!handle) {
        HandleRequestError(request, "Failed to acquire connection");
        return;
    }

    try {
        // 配置请求
        curl_easy_setopt(handle, CURLOPT_URL, request->url.c_str());
        curl_easy_setopt(handle, CURLOPT_TIMEOUT, timeout_ms_ / 1000);
        curl_easy_setopt(handle, CURLOPT_CONNECTTIMEOUT, 2L);

        // 设置响应回调
        std::string response_data;
        curl_easy_setopt(handle, CURLOPT_WRITEDATA, &response_data);
        curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, [](void* contents, size_t size, size_t nmemb, std::string* response) -> size_t {
            size_t total_size = size * nmemb;
            response->append((char*)contents, total_size);
            return total_size;
        });

        // 设置请求头
        struct curl_slist* headers = nullptr;
        if (!request->headers.empty()) {
            headers = curl_slist_append(headers, request->headers.c_str());
            curl_easy_setopt(handle, CURLOPT_HTTPHEADER, headers);
        }

        // 执行请求
        CURLcode res = curl_easy_perform(handle);

        // 获取响应信息
        long response_code = 0;
        curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE, &response_code);
        request->response_code = response_code;

        // 计算延迟
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - request->sent_time);
        request->latency_ms = duration.count();

        // 清理请求头
        if (headers) {
            curl_slist_free_all(headers);
        }

        // 处理结果
        if (res == CURLE_OK && response_code >= 200 && response_code < 300) {
            request->status = REQ_STATUS_SUCCESS;
            request->response = response_data;
        } else if (res == CURLE_OPERATION_TIMEDOUT) {
            request->status = REQ_STATUS_TIMEOUT;
            request->error_message = "Request timeout";
        } else {
            request->status = REQ_STATUS_ERROR;
            request->error_message = curl_easy_strerror(res);
        }

        // 更新统计信息
        UpdateStatistics(request);

    } catch (const std::exception& e) {
        HandleRequestError(request, std::string("Exception: ") + e.what());
    }

    // 释放连接
    connection_pool_->ReleaseHandle(handle);

    // 处理重试逻辑
    if (request->status != REQ_STATUS_SUCCESS && ShouldRetry(request)) {
        request->retry_count++;
        request->status = static_cast<RequestStatus>(REQ_STATUS_PENDING);

        // 重新加入队列
        auto retry_request = std::make_unique<RequestContext>(*request);
        request_queue_->Push(std::move(retry_request));
    }
}

//+------------------------------------------------------------------+
//| 辅助方法                                                          |
//+------------------------------------------------------------------+
std::string AsyncNetworkManager::BuildRequestUrl(RequestType type, const std::string& data) {
    std::string path;
    switch (type) {
        case REQ_HEARTBEAT:
            path = UrlPaths::HEARTBEAT;
            break;
        case REQ_EVENTS:
            path = UrlPaths::EVENTS;
            break;
        case REQ_PARAMS:
            path = UrlPaths::PARAMS;
            break;
        case REQ_PING:
            path = UrlPaths::PING;
            break;
        default:
            return "";
    }

    // 构建基础URL
    std::ostringstream url;
    url << server_host_ << path;

    // 添加认证参数
    {
        std::lock_guard<std::mutex> lock(auth_mutex_);
        if (account_.empty() || license_.empty() || secret_key_.empty()) {
            return "";
        }

        // 生成时间戳和nonce
        auto now = std::chrono::system_clock::now();
        auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
        std::string nonce = GenerateNonce();

        // 生成签名
        std::string signature = GenerateSignature(data);

        // 构建完整URL
        url << "?account=" << account_
            << "&license=" << license_
            << "&timestamp=" << timestamp
            << "&nonce=" << nonce
            << "&sign=" << signature;

        // 添加数据参数
        if (!data.empty()) {
            url << "&data=" << UrlEncode(data);
        }
    }

    return url.str();
}

std::string AsyncNetworkManager::GenerateSignature(const std::string& data) {
    std::lock_guard<std::mutex> lock(auth_mutex_);

    // 使用HMAC-SHA256生成签名
    std::string sign_data = secret_key_ + data;
    return HMACUtils::SHA256(sign_data);
}

void AsyncNetworkManager::UpdateStatistics(RequestContext* request) {
    std::lock_guard<std::mutex> lock(stats_mutex_);

    switch (request->type) {
        case REQ_HEARTBEAT:
            stats_.heartbeat_sent++;
            if (request->status == REQ_STATUS_SUCCESS) {
                stats_.heartbeat_success++;
                stats_.avg_heartbeat_latency =
                    (stats_.avg_heartbeat_latency * (stats_.heartbeat_success - 1) + request->latency_ms) / stats_.heartbeat_success;
            } else {
                stats_.heartbeat_failed++;
            }
            break;

        case REQ_EVENTS:
            stats_.events_sent++;
            if (request->status == REQ_STATUS_SUCCESS) {
                stats_.events_success++;
                stats_.avg_events_latency =
                    (stats_.avg_events_latency * (stats_.events_success - 1) + request->latency_ms) / stats_.events_success;
            } else {
                stats_.events_failed++;
            }
            break;

        case REQ_PARAMS:
            stats_.params_fetched++;
            if (request->status == REQ_STATUS_SUCCESS) {
                stats_.params_success++;
                stats_.avg_params_latency =
                    (stats_.avg_params_latency * (stats_.params_success - 1) + request->latency_ms) / stats_.params_success;
            } else {
                stats_.params_failed++;
            }
            break;

        default:
            break;
    }

    // 更新字节统计
    stats_.bytes_sent += request->data.length();
    stats_.bytes_received += request->response.length();

    // 更新连接统计
    if (connection_pool_) {
        stats_.active_connections = connection_pool_->GetActiveCount();
        stats_.pool_size = connection_pool_->GetPoolSize();
    }
}

bool AsyncNetworkManager::ShouldRetry(RequestContext* request) {
    if (!request) return false;

    RetryConfig config;
    switch (request->type) {
        case REQ_HEARTBEAT:
            config = RetryConfig::ForHeartbeat();
            break;
        case REQ_EVENTS:
            config = RetryConfig::ForEvents();
            break;
        case REQ_PARAMS:
            config = RetryConfig::ForParams();
            break;
        case REQ_PING:
            config = RetryConfig::ForPing();
            break;
        default:
            return false;
    }

    // 检查重试次数
    if (request->retry_count >= config.max_retries) {
        return false;
    }

    // 检查错误类型
    if (request->status == REQ_STATUS_AUTH_ERROR) {
        return false; // 认证错误不重试
    }

    // 计算重试延迟
    int delay_ms = config.base_delay_ms * std::pow(config.backoff_multiplier, request->retry_count);
    std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));

    return true;
}

void AsyncNetworkManager::HandleRequestError(RequestContext* request, const std::string& error) {
    if (!request) return;

    request->status = REQ_STATUS_ERROR;
    request->error_message = error;

    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - request->sent_time);
    request->latency_ms = duration.count();

    UpdateStatistics(request);
}

//+------------------------------------------------------------------+
//| 状态查询                                                          |
//+------------------------------------------------------------------+
int AsyncNetworkManager::GetConnectionStatus() const {
    if (!connection_pool_) {
        return 0;
    }
    return connection_pool_->GetActiveCount();
}

int AsyncNetworkManager::GetQueueLength(int request_type) const {
    if (!request_queue_) {
        return 0;
    }
    return request_queue_->GetLength(request_type);
}

void AsyncNetworkManager::GetStatistics(NetworkStats& stats) const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    stats = stats_;
}

//+------------------------------------------------------------------+
//| 控制接口                                                          |
//+------------------------------------------------------------------+
void AsyncNetworkManager::Pause() {
    paused_flag_.store(true);
}

void AsyncNetworkManager::Resume() {
    paused_flag_.store(false);
    worker_cv_.notify_all();
}

int AsyncNetworkManager::ClearQueue(int request_type) {
    if (!request_queue_) {
        return 0;
    }
    return request_queue_->Clear(request_type);
}

void AsyncNetworkManager::Shutdown() {
    // 设置关闭标志
    shutdown_flag_.store(true);
    worker_cv_.notify_all();

    // 等待工作线程结束
    for (auto& thread : worker_threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    worker_threads_.clear();

    // 清理资源
    connection_pool_.reset();
    request_queue_.reset();
    response_queue_.reset();
}

//+------------------------------------------------------------------+
//| 工具函数                                                          |
//+------------------------------------------------------------------+
std::string AsyncNetworkManager::GenerateNonce() {
    static thread_local std::mt19937 rng(std::random_device{}());
    static const char charset[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    std::uniform_int_distribution<int> dist(0, sizeof(charset) - 2);

    std::string nonce;
    nonce.reserve(16);
    for (int i = 0; i < 16; ++i) {
        nonce += charset[dist(rng)];
    }
    return nonce;
}

std::string AsyncNetworkManager::UrlEncode(const std::string& data) {
    // 使用 libcurl 的 curl_easy_escape 进行 URL 编码
    CURL* curl = curl_easy_init();
    if (!curl) {
        return data; // 回退，返回原始数据
    }

    char* encoded = curl_easy_escape(curl, data.c_str(), static_cast<int>(data.length()));
    std::string result;
    if (encoded) {
        result = encoded;
        curl_free(encoded);
    }
    curl_easy_cleanup(curl);
    return result;
}