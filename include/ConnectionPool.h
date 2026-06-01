//+------------------------------------------------------------------+
//|                                        ConnectionPool.h          |
//|                  连接池管理 - libcurl连接复用                      |
//|              管理HTTP连接的创建、复用和释放                         |
//+------------------------------------------------------------------+
#pragma once

#include <curl/curl.h>
#include <vector>
#include <queue>
#include <mutex>
#include <string>
#include <memory>
#include <atomic>

//+------------------------------------------------------------------+
//| 连接信息结构                                                      |
//+------------------------------------------------------------------+
struct ConnectionInfo {
    CURL* handle;                    // curl句柄
    std::string server_host;         // 服务器地址
    bool in_use;                     // 是否正在使用
    std::chrono::steady_clock::time_point last_used;  // 最后使用时间
    std::chrono::steady_clock::time_point created;    // 创建时间
    int request_count;               // 请求计数
    bool keep_alive;                 // 是否支持Keep-Alive

    ConnectionInfo() : handle(nullptr), in_use(false), request_count(0), keep_alive(true) {}
};

//+------------------------------------------------------------------+
//| 连接池类                                                          |
//+------------------------------------------------------------------+
class ConnectionPool {
private:
    std::vector<std::unique_ptr<ConnectionInfo>> connections_;
    std::queue<ConnectionInfo*> available_connections_;
    std::mutex pool_mutex_;

    // 配置参数
    std::string server_host_;
    int timeout_ms_;
    int max_connections_;
    int max_requests_per_connection_;
    int connection_timeout_seconds_;

    // 统计信息
    std::atomic<int> active_count_;
    std::atomic<int> total_created_;
    std::atomic<int> total_reused_;

    // Wine环境检测
    bool is_wine_environment_;

    // 内部方法
    ConnectionInfo* CreateNewConnection();
    void ConfigureHandle(CURL* handle);
    void ConfigureForWine(CURL* handle);
    bool IsConnectionValid(ConnectionInfo* conn);
    void CleanupExpiredConnections();
    bool DetectWineEnvironment();

public:
    ConnectionPool();
    ~ConnectionPool();

    // 初始化和配置
    bool Initialize(int max_connections, const std::string& server_host, int timeout_ms);
    void Resize(int new_max_connections);
    void SetConnectionTimeout(int timeout_seconds);
    void SetMaxRequestsPerConnection(int max_requests);

    // 连接管理
    CURL* AcquireHandle();
    void ReleaseHandle(CURL* handle);
    void InvalidateHandle(CURL* handle);

    // 状态查询
    int GetActiveCount() const { return active_count_.load(); }
    int GetPoolSize() const { return connections_.size(); }
    int GetAvailableCount() const;
    void GetStatistics(int& total_created, int& total_reused, int& active_count) const;

    // 维护操作
    void CleanupAll();
    void ForceCleanup();
};

//+------------------------------------------------------------------+
//| libcurl回调函数                                                   |
//+------------------------------------------------------------------+
namespace CurlCallbacks {
    // 写入回调函数
    size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* response);

    // 头部回调函数
    size_t HeaderCallback(void* contents, size_t size, size_t nmemb, std::string* headers);

    // 进度回调函数
    int ProgressCallback(void* clientp, curl_off_t dltotal, curl_off_t dlnow,
                        curl_off_t ultotal, curl_off_t ulnow);
}