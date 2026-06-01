//+------------------------------------------------------------------+
//|                                        ConnectionPool.cpp        |
//|                  连接池管理 - 实现文件                             |
//+------------------------------------------------------------------+
#include "ConnectionPool.h"
#include <algorithm>
#include <chrono>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#endif

//+------------------------------------------------------------------+
//| 构造函数和析构函数                                                |
//+------------------------------------------------------------------+
ConnectionPool::ConnectionPool()
    : timeout_ms_(5000)
    , max_connections_(5)
    , max_requests_per_connection_(100)
    , connection_timeout_seconds_(300)
    , active_count_(0)
    , total_created_(0)
    , total_reused_(0)
    , is_wine_environment_(false)
{
    is_wine_environment_ = DetectWineEnvironment();
}

ConnectionPool::~ConnectionPool() {
    CleanupAll();
}

//+------------------------------------------------------------------+
//| 初始化和配置                                                      |
//+------------------------------------------------------------------+
bool ConnectionPool::Initialize(int max_connections, const std::string& server_host, int timeout_ms) {
    if (server_host.empty() || max_connections <= 0) {
        return false;
    }

    std::lock_guard<std::mutex> lock(pool_mutex_);

    server_host_ = server_host;
    timeout_ms_ = timeout_ms;
    max_connections_ = std::min(max_connections, ASYNCNET_MAX_CONNECTIONS);

    // 预创建一个连接用于测试
    ConnectionInfo* test_conn = CreateNewConnection();
    if (!test_conn) {
        return false;
    }

    // 将测试连接放入可用队列
    available_connections_.push(test_conn);

    return true;
}

void ConnectionPool::Resize(int new_max_connections) {
    std::lock_guard<std::mutex> lock(pool_mutex_);

    new_max_connections = std::min(new_max_connections, ASYNCNET_MAX_CONNECTIONS);

    if (new_max_connections < max_connections_) {
        // 缩小连接池，清理多余连接
        while (connections_.size() > new_max_connections && !available_connections_.empty()) {
            ConnectionInfo* conn = available_connections_.front();
            available_connections_.pop();

            auto it = std::find_if(connections_.begin(), connections_.end(),
                [conn](const std::unique_ptr<ConnectionInfo>& ptr) {
                    return ptr.get() == conn;
                });

            if (it != connections_.end()) {
                if ((*it)->handle) {
                    curl_easy_cleanup((*it)->handle);
                }
                connections_.erase(it);
            }
        }
    }

    max_connections_ = new_max_connections;
}

void ConnectionPool::SetConnectionTimeout(int timeout_seconds) {
    connection_timeout_seconds_ = std::max(60, timeout_seconds);
}

void ConnectionPool::SetMaxRequestsPerConnection(int max_requests) {
    max_requests_per_connection_ = std::max(10, max_requests);
}

//+------------------------------------------------------------------+
//| 连接管理                                                          |
//+------------------------------------------------------------------+
CURL* ConnectionPool::AcquireHandle() {
    std::lock_guard<std::mutex> lock(pool_mutex_);

    // 清理过期连接
    CleanupExpiredConnections();

    ConnectionInfo* conn = nullptr;

    // 尝试获取可用连接
    while (!available_connections_.empty()) {
        conn = available_connections_.front();
        available_connections_.pop();

        if (IsConnectionValid(conn)) {
            break;
        } else {
            // 连接无效，清理并继续
            InvalidateHandle(conn->handle);
            conn = nullptr;
        }
    }

    // 如果没有可用连接，创建新连接
    if (!conn && connections_.size() < max_connections_) {
        conn = CreateNewConnection();
    }

    if (conn) {
        conn->in_use = true;
        conn->last_used = std::chrono::steady_clock::now();
        conn->request_count++;
        active_count_.fetch_add(1);

        if (conn->request_count > 1) {
            total_reused_.fetch_add(1);
        }

        return conn->handle;
    }

    return nullptr;
}

void ConnectionPool::ReleaseHandle(CURL* handle) {
    if (!handle) return;

    std::lock_guard<std::mutex> lock(pool_mutex_);

    // 找到对应的连接信息
    auto it = std::find_if(connections_.begin(), connections_.end(),
        [handle](const std::unique_ptr<ConnectionInfo>& conn) {
            return conn->handle == handle;
        });

    if (it != connections_.end()) {
        ConnectionInfo* conn = it->get();
        conn->in_use = false;
        conn->last_used = std::chrono::steady_clock::now();
        active_count_.fetch_sub(1);

        // 检查连接是否还能继续使用
        if (IsConnectionValid(conn) &&
            conn->request_count < max_requests_per_connection_) {
            available_connections_.push(conn);
        } else {
            // 连接已达到最大使用次数或无效，清理
            InvalidateHandle(handle);
        }
    }
}

void ConnectionPool::InvalidateHandle(CURL* handle) {
    if (!handle) return;

    // 注意：此函数假设已经持有pool_mutex_锁

    auto it = std::find_if(connections_.begin(), connections_.end(),
        [handle](const std::unique_ptr<ConnectionInfo>& conn) {
            return conn->handle == handle;
        });

    if (it != connections_.end()) {
        if ((*it)->in_use) {
            active_count_.fetch_sub(1);
        }

        curl_easy_cleanup(handle);
        connections_.erase(it);
    }
}

//+------------------------------------------------------------------+
//| 内部方法                                                          |
//+------------------------------------------------------------------+
ConnectionInfo* ConnectionPool::CreateNewConnection() {
    // 注意：此函数假设已经持有pool_mutex_锁

    CURL* handle = curl_easy_init();
    if (!handle) {
        return nullptr;
    }

    try {
        ConfigureHandle(handle);

        auto conn = std::make_unique<ConnectionInfo>();
        conn->handle = handle;
        conn->server_host = server_host_;
        conn->in_use = false;
        conn->created = std::chrono::steady_clock::now();
        conn->last_used = conn->created;
        conn->request_count = 0;
        conn->keep_alive = true;

        ConnectionInfo* conn_ptr = conn.get();
        connections_.push_back(std::move(conn));
        total_created_.fetch_add(1);

        return conn_ptr;
    }
    catch (const std::exception& e) {
        curl_easy_cleanup(handle);
        return nullptr;
    }
}

void ConnectionPool::ConfigureHandle(CURL* handle) {
    if (!handle) return;

    // 基础配置
    curl_easy_setopt(handle, CURLOPT_TIMEOUT, timeout_ms_ / 1000);
    curl_easy_setopt(handle, CURLOPT_CONNECTTIMEOUT, 2L);
    curl_easy_setopt(handle, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(handle, CURLOPT_MAXREDIRS, 3L);

    // Keep-Alive配置
    curl_easy_setopt(handle, CURLOPT_TCP_KEEPALIVE, 1L);
    curl_easy_setopt(handle, CURLOPT_TCP_KEEPIDLE, 60L);
    curl_easy_setopt(handle, CURLOPT_TCP_KEEPINTVL, 30L);

    // HTTP版本配置
    curl_easy_setopt(handle, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2_0);

    // SSL配置
    curl_easy_setopt(handle, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(handle, CURLOPT_SSL_VERIFYHOST, 2L);
    curl_easy_setopt(handle, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_2);

    // 用户代理
    curl_easy_setopt(handle, CURLOPT_USERAGENT, "AsyncNetDLL/1.0 (MT4/MT5 Trading Client)");

    // 压缩支持
    curl_easy_setopt(handle, CURLOPT_ACCEPT_ENCODING, "gzip, deflate");

    // 缓冲区配置
    curl_easy_setopt(handle, CURLOPT_BUFFERSIZE, 16384L);

    // Wine环境特殊配置
    if (is_wine_environment_) {
        ConfigureForWine(handle);
    }

    // 回调函数配置
    curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, CurlCallbacks::WriteCallback);
    curl_easy_setopt(handle, CURLOPT_HEADERFUNCTION, CurlCallbacks::HeaderCallback);
    curl_easy_setopt(handle, CURLOPT_NOPROGRESS, 1L);
}

void ConnectionPool::ConfigureForWine(CURL* handle) {
    if (!handle) return;

    // Wine环境下的特殊配置
    curl_easy_setopt(handle, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(handle, CURLOPT_DNS_CACHE_TIMEOUT, 300L);
    curl_easy_setopt(handle, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1);
    curl_easy_setopt(handle, CURLOPT_TCP_NODELAY, 1L);

    // 降低超时时间以避免Wine下的阻塞问题
    curl_easy_setopt(handle, CURLOPT_TIMEOUT, std::min(timeout_ms_ / 1000L, 10L));
    curl_easy_setopt(handle, CURLOPT_CONNECTTIMEOUT, 3L);
}

bool ConnectionPool::IsConnectionValid(ConnectionInfo* conn) {
    if (!conn || !conn->handle) {
        return false;
    }

    // 检查连接是否超时
    auto now = std::chrono::steady_clock::now();
    auto age = std::chrono::duration_cast<std::chrono::seconds>(now - conn->last_used).count();

    if (age > connection_timeout_seconds_) {
        return false;
    }

    // 检查请求次数是否超限
    if (conn->request_count >= max_requests_per_connection_) {
        return false;
    }

    return true;
}

void ConnectionPool::CleanupExpiredConnections() {
    // 注意：此函数假设已经持有pool_mutex_锁

    auto now = std::chrono::steady_clock::now();

    // 清理可用队列中的过期连接
    std::queue<ConnectionInfo*> valid_connections;
    while (!available_connections_.empty()) {
        ConnectionInfo* conn = available_connections_.front();
        available_connections_.pop();

        if (IsConnectionValid(conn)) {
            valid_connections.push(conn);
        } else {
            InvalidateHandle(conn->handle);
        }
    }
    available_connections_ = valid_connections;

    // 清理连接池中的过期连接（仅限未使用的）
    connections_.erase(
        std::remove_if(connections_.begin(), connections_.end(),
            [this, now](const std::unique_ptr<ConnectionInfo>& conn) {
                if (!conn->in_use && !IsConnectionValid(conn.get())) {
                    curl_easy_cleanup(conn->handle);
                    return true;
                }
                return false;
            }),
        connections_.end()
    );
}

bool ConnectionPool::DetectWineEnvironment() {
#ifdef _WIN32
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    if (ntdll) {
        return GetProcAddress(ntdll, "wine_get_version") != nullptr;
    }
#endif
    return false;
}

//+------------------------------------------------------------------+
//| 状态查询                                                          |
//+------------------------------------------------------------------+
int ConnectionPool::GetAvailableCount() const {
    std::lock_guard<std::mutex> lock(pool_mutex_);
    return available_connections_.size();
}

void ConnectionPool::GetStatistics(int& total_created, int& total_reused, int& active_count) const {
    total_created = total_created_.load();
    total_reused = total_reused_.load();
    active_count = active_count_.load();
}

//+------------------------------------------------------------------+
//| 维护操作                                                          |
//+------------------------------------------------------------------+
void ConnectionPool::CleanupAll() {
    std::lock_guard<std::mutex> lock(pool_mutex_);

    // 清空可用队列
    while (!available_connections_.empty()) {
        available_connections_.pop();
    }

    // 清理所有连接
    for (auto& conn : connections_) {
        if (conn->handle) {
            curl_easy_cleanup(conn->handle);
        }
    }
    connections_.clear();

    active_count_.store(0);
}

void ConnectionPool::ForceCleanup() {
    CleanupAll();
}

//+------------------------------------------------------------------+
//| libcurl回调函数实现                                               |
//+------------------------------------------------------------------+
namespace CurlCallbacks {

size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* response) {
    if (!response || !contents) {
        return 0;
    }

    size_t total_size = size * nmemb;
    try {
        response->append(static_cast<char*>(contents), total_size);
        return total_size;
    }
    catch (const std::exception& e) {
        return 0; // 返回0表示错误
    }
}

size_t HeaderCallback(void* contents, size_t size, size_t nmemb, std::string* headers) {
    if (!headers || !contents) {
        return 0;
    }

    size_t total_size = size * nmemb;
    try {
        headers->append(static_cast<char*>(contents), total_size);
        return total_size;
    }
    catch (const std::exception& e) {
        return 0;
    }
}

int ProgressCallback(void* clientp, curl_off_t dltotal, curl_off_t dlnow,
                    curl_off_t ultotal, curl_off_t ulnow) {
    // 简单的进度回调，可以用于超时控制
    // 返回0继续，非0中止传输
    return 0;
}

} // namespace CurlCallbacks