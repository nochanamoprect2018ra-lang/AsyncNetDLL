//+------------------------------------------------------------------+
//|                                        AsyncNetDLL.cpp          |
//|                  异步网络DLL - 主实现文件                          |
//|              libcurl异步HTTP客户端实现                            |
//+------------------------------------------------------------------+
#include "AsyncNetDLL.h"
#include "AsyncNetworkManager.h"
#include "ConnectionPool.h"
#include "RequestQueue.h"
#include <memory>
#include <string>
#include <mutex>
#include <curl/curl.h>

//+------------------------------------------------------------------+
//| 全局变量                                                          |
//+------------------------------------------------------------------+
static std::unique_ptr<AsyncNetworkManager> g_network_manager = nullptr;
static std::mutex g_init_mutex;
static bool g_initialized = false;
static std::string g_last_error;
static std::mutex g_error_mutex;

//+------------------------------------------------------------------+
//| 内部辅助函数                                                      |
//+------------------------------------------------------------------+
void SetLastError(const std::string& error) {
    std::lock_guard<std::mutex> lock(g_error_mutex);
    g_last_error = error;
}

std::string GetLastErrorInternal() {
    std::lock_guard<std::mutex> lock(g_error_mutex);
    return g_last_error;
}

bool IsInitialized() {
    std::lock_guard<std::mutex> lock(g_init_mutex);
    return g_initialized;
}

//+------------------------------------------------------------------+
//| DLL入口点                                                         |
//+------------------------------------------------------------------+
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
        // 初始化libcurl全局环境
        if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) {
            return FALSE;
        }
        break;
    case DLL_PROCESS_DETACH:
        // 清理libcurl全局环境
        curl_global_cleanup();
        break;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
        break;
    }
    return TRUE;
}

//+------------------------------------------------------------------+
//| 初始化和配置接口                                                  |
//+------------------------------------------------------------------+
ASYNCNET_API int AsyncNet_Initialize(const char* server_host, int timeout_ms) {
    if (!server_host || strlen(server_host) == 0) {
        SetLastError("Invalid server host");
        return ASYNCNET_ERROR_INVALID_PARAM;
    }

    if (timeout_ms <= 0 || timeout_ms > 60000) {
        timeout_ms = ASYNCNET_DEFAULT_TIMEOUT;
    }

    std::lock_guard<std::mutex> lock(g_init_mutex);

    if (g_initialized) {
        SetLastError("Already initialized");
        return ASYNCNET_ERROR_INIT;
    }

    try {
        g_network_manager = std::make_unique<AsyncNetworkManager>();

        if (!g_network_manager->Initialize(server_host, timeout_ms)) {
            g_network_manager.reset();
            SetLastError("Failed to initialize network manager");
            return ASYNCNET_ERROR_INIT;
        }

        g_initialized = true;
        SetLastError("");
        return ASYNCNET_SUCCESS;
    }
    catch (const std::exception& e) {
        g_network_manager.reset();
        SetLastError(std::string("Exception during initialization: ") + e.what());
        return ASYNCNET_ERROR_INIT;
    }
}

ASYNCNET_API int AsyncNet_SetAuth(const char* account, const char* license, const char* secret_key) {
    if (!IsInitialized()) {
        SetLastError("Not initialized");
        return ASYNCNET_ERROR_NOT_INIT;
    }

    if (!account || !license || !secret_key) {
        SetLastError("Invalid authentication parameters");
        return ASYNCNET_ERROR_INVALID_PARAM;
    }

    try {
        g_network_manager->SetAuth(account, license, secret_key);
        SetLastError("");
        return ASYNCNET_SUCCESS;
    }
    catch (const std::exception& e) {
        SetLastError(std::string("Exception setting auth: ") + e.what());
        return ASYNCNET_ERROR_AUTH;
    }
}

ASYNCNET_API int AsyncNet_SetConfig(int max_connections, int worker_threads, int queue_size) {
    if (!IsInitialized()) {
        SetLastError("Not initialized");
        return ASYNCNET_ERROR_NOT_INIT;
    }

    // 参数验证和默认值
    if (max_connections <= 0 || max_connections > ASYNCNET_MAX_CONNECTIONS) {
        max_connections = 5;
    }
    if (worker_threads <= 0 || worker_threads > ASYNCNET_MAX_WORKERS) {
        worker_threads = 2;
    }
    if (queue_size <= 0 || queue_size > ASYNCNET_MAX_QUEUE_SIZE) {
        queue_size = 100;
    }

    try {
        g_network_manager->SetConfig(max_connections, worker_threads, queue_size);
        SetLastError("");
        return ASYNCNET_SUCCESS;
    }
    catch (const std::exception& e) {
        SetLastError(std::string("Exception setting config: ") + e.what());
        return ASYNCNET_ERROR_INVALID_PARAM;
    }
}

//+------------------------------------------------------------------+
//| 异步请求接口                                                      |
//+------------------------------------------------------------------+
ASYNCNET_API int AsyncNet_SendHeartbeat(const char* data, int data_len) {
    if (!IsInitialized()) {
        SetLastError("Not initialized");
        return ASYNCNET_ERROR_NOT_INIT;
    }

    if (!data || data_len <= 0) {
        SetLastError("Invalid heartbeat data");
        return ASYNCNET_ERROR_INVALID_PARAM;
    }

    try {
        int request_id = g_network_manager->SendRequest(REQ_HEARTBEAT, data, data_len);
        if (request_id > 0) {
            SetLastError("");
            return request_id;
        } else {
            SetLastError("Failed to queue heartbeat request");
            return ASYNCNET_ERROR_QUEUE_FULL;
        }
    }
    catch (const std::exception& e) {
        SetLastError(std::string("Exception sending heartbeat: ") + e.what());
        return ASYNCNET_ERROR_NETWORK;
    }
}

ASYNCNET_API int AsyncNet_SendEvents(const char* data, int data_len) {
    if (!IsInitialized()) {
        SetLastError("Not initialized");
        return ASYNCNET_ERROR_NOT_INIT;
    }

    if (!data || data_len <= 0) {
        SetLastError("Invalid events data");
        return ASYNCNET_ERROR_INVALID_PARAM;
    }

    try {
        int request_id = g_network_manager->SendRequest(REQ_EVENTS, data, data_len);
        if (request_id > 0) {
            SetLastError("");
            return request_id;
        } else {
            SetLastError("Failed to queue events request");
            return ASYNCNET_ERROR_QUEUE_FULL;
        }
    }
    catch (const std::exception& e) {
        SetLastError(std::string("Exception sending events: ") + e.what());
        return ASYNCNET_ERROR_NETWORK;
    }
}

ASYNCNET_API int AsyncNet_FetchParams() {
    if (!IsInitialized()) {
        SetLastError("Not initialized");
        return ASYNCNET_ERROR_NOT_INIT;
    }

    try {
        int request_id = g_network_manager->SendRequest(REQ_PARAMS, nullptr, 0);
        if (request_id > 0) {
            SetLastError("");
            return request_id;
        } else {
            SetLastError("Failed to queue params request");
            return ASYNCNET_ERROR_QUEUE_FULL;
        }
    }
    catch (const std::exception& e) {
        SetLastError(std::string("Exception fetching params: ") + e.what());
        return ASYNCNET_ERROR_NETWORK;
    }
}

ASYNCNET_API int AsyncNet_SendPing() {
    if (!IsInitialized()) {
        SetLastError("Not initialized");
        return ASYNCNET_ERROR_NOT_INIT;
    }

    try {
        int request_id = g_network_manager->SendRequest(REQ_PING, nullptr, 0);
        if (request_id > 0) {
            SetLastError("");
            return request_id;
        } else {
            SetLastError("Failed to queue ping request");
            return ASYNCNET_ERROR_QUEUE_FULL;
        }
    }
    catch (const std::exception& e) {
        SetLastError(std::string("Exception sending ping: ") + e.what());
        return ASYNCNET_ERROR_NETWORK;
    }
}

//+------------------------------------------------------------------+
//| 响应处理接口                                                      |
//+------------------------------------------------------------------+
ASYNCNET_API int AsyncNet_ProcessResponses() {
    if (!IsInitialized()) {
        SetLastError("Not initialized");
        return ASYNCNET_ERROR_NOT_INIT;
    }

    try {
        int processed = g_network_manager->ProcessResponses();
        SetLastError("");
        return processed;
    }
    catch (const std::exception& e) {
        SetLastError(std::string("Exception processing responses: ") + e.what());
        return ASYNCNET_ERROR_NETWORK;
    }
}

ASYNCNET_API int AsyncNet_GetHeartbeatResponse(char* buffer, int buffer_size) {
    if (!IsInitialized()) {
        SetLastError("Not initialized");
        return ASYNCNET_ERROR_NOT_INIT;
    }

    if (!buffer || buffer_size <= 0) {
        SetLastError("Invalid buffer");
        return ASYNCNET_ERROR_INVALID_PARAM;
    }

    try {
        std::string response;
        if (g_network_manager->GetResponse(REQ_HEARTBEAT, response)) {
            int copy_len = std::min((int)response.length(), buffer_size - 1);
            memcpy(buffer, response.c_str(), copy_len);
            buffer[copy_len] = '\0';
            SetLastError("");
            return copy_len;
        } else {
            SetLastError("");
            return 0; // 没有响应数据
        }
    }
    catch (const std::exception& e) {
        SetLastError(std::string("Exception getting heartbeat response: ") + e.what());
        return ASYNCNET_ERROR_NETWORK;
    }
}

ASYNCNET_API int AsyncNet_GetEventsResponse(char* buffer, int buffer_size) {
    if (!IsInitialized()) {
        SetLastError("Not initialized");
        return ASYNCNET_ERROR_NOT_INIT;
    }

    if (!buffer || buffer_size <= 0) {
        SetLastError("Invalid buffer");
        return ASYNCNET_ERROR_INVALID_PARAM;
    }

    try {
        std::string response;
        if (g_network_manager->GetResponse(REQ_EVENTS, response)) {
            int copy_len = std::min((int)response.length(), buffer_size - 1);
            memcpy(buffer, response.c_str(), copy_len);
            buffer[copy_len] = '\0';
            SetLastError("");
            return copy_len;
        } else {
            SetLastError("");
            return 0;
        }
    }
    catch (const std::exception& e) {
        SetLastError(std::string("Exception getting events response: ") + e.what());
        return ASYNCNET_ERROR_NETWORK;
    }
}

ASYNCNET_API int AsyncNet_GetParamsResponse(char* buffer, int buffer_size) {
    if (!IsInitialized()) {
        SetLastError("Not initialized");
        return ASYNCNET_ERROR_NOT_INIT;
    }

    if (!buffer || buffer_size <= 0) {
        SetLastError("Invalid buffer");
        return ASYNCNET_ERROR_INVALID_PARAM;
    }

    try {
        std::string response;
        if (g_network_manager->GetResponse(REQ_PARAMS, response)) {
            int copy_len = std::min((int)response.length(), buffer_size - 1);
            memcpy(buffer, response.c_str(), copy_len);
            buffer[copy_len] = '\0';
            SetLastError("");
            return copy_len;
        } else {
            SetLastError("");
            return 0;
        }
    }
    catch (const std::exception& e) {
        SetLastError(std::string("Exception getting params response: ") + e.what());
        return ASYNCNET_ERROR_NETWORK;
    }
}

ASYNCNET_API int AsyncNet_GetPingResponse(char* buffer, int buffer_size) {
    if (!IsInitialized()) {
        SetLastError("Not initialized");
        return ASYNCNET_ERROR_NOT_INIT;
    }

    if (!buffer || buffer_size <= 0) {
        SetLastError("Invalid buffer");
        return ASYNCNET_ERROR_INVALID_PARAM;
    }

    try {
        std::string response;
        if (g_network_manager->GetResponse(REQ_PING, response)) {
            int copy_len = std::min((int)response.length(), buffer_size - 1);
            memcpy(buffer, response.c_str(), copy_len);
            buffer[copy_len] = '\0';
            SetLastError("");
            return copy_len;
        } else {
            SetLastError("");
            return 0;
        }
    }
    catch (const std::exception& e) {
        SetLastError(std::string("Exception getting ping response: ") + e.what());
        return ASYNCNET_ERROR_NETWORK;
    }
}

//+------------------------------------------------------------------+
//| 状态查询接口                                                      |
//+------------------------------------------------------------------+
ASYNCNET_API int AsyncNet_GetConnectionStatus() {
    if (!IsInitialized()) {
        return 0; // 未初始化
    }

    try {
        return g_network_manager->GetConnectionStatus();
    }
    catch (const std::exception& e) {
        SetLastError(std::string("Exception getting connection status: ") + e.what());
        return 0;
    }
}

ASYNCNET_API int AsyncNet_GetQueueLength(int request_type) {
    if (!IsInitialized()) {
        SetLastError("Not initialized");
        return ASYNCNET_ERROR_NOT_INIT;
    }

    try {
        int length = g_network_manager->GetQueueLength(request_type);
        SetLastError("");
        return length;
    }
    catch (const std::exception& e) {
        SetLastError(std::string("Exception getting queue length: ") + e.what());
        return ASYNCNET_ERROR_NETWORK;
    }
}

ASYNCNET_API int AsyncNet_GetStatistics(NetworkStats* stats) {
    if (!IsInitialized()) {
        SetLastError("Not initialized");
        return ASYNCNET_ERROR_NOT_INIT;
    }

    if (!stats) {
        SetLastError("Invalid stats pointer");
        return ASYNCNET_ERROR_INVALID_PARAM;
    }

    try {
        g_network_manager->GetStatistics(*stats);
        SetLastError("");
        return ASYNCNET_SUCCESS;
    }
    catch (const std::exception& e) {
        SetLastError(std::string("Exception getting statistics: ") + e.what());
        return ASYNCNET_ERROR_NETWORK;
    }
}

ASYNCNET_API int AsyncNet_GetLastError(char* error_buffer, int buffer_size) {
    if (!error_buffer || buffer_size <= 0) {
        return ASYNCNET_ERROR_INVALID_PARAM;
    }

    std::string error = GetLastErrorInternal();
    int copy_len = std::min((int)error.length(), buffer_size - 1);
    memcpy(error_buffer, error.c_str(), copy_len);
    error_buffer[copy_len] = '\0';

    return copy_len;
}

//+------------------------------------------------------------------+
//| 控制接口                                                          |
//+------------------------------------------------------------------+
ASYNCNET_API int AsyncNet_Pause() {
    if (!IsInitialized()) {
        SetLastError("Not initialized");
        return ASYNCNET_ERROR_NOT_INIT;
    }

    try {
        g_network_manager->Pause();
        SetLastError("");
        return ASYNCNET_SUCCESS;
    }
    catch (const std::exception& e) {
        SetLastError(std::string("Exception pausing: ") + e.what());
        return ASYNCNET_ERROR_NETWORK;
    }
}

ASYNCNET_API int AsyncNet_Resume() {
    if (!IsInitialized()) {
        SetLastError("Not initialized");
        return ASYNCNET_ERROR_NOT_INIT;
    }

    try {
        g_network_manager->Resume();
        SetLastError("");
        return ASYNCNET_SUCCESS;
    }
    catch (const std::exception& e) {
        SetLastError(std::string("Exception resuming: ") + e.what());
        return ASYNCNET_ERROR_NETWORK;
    }
}

ASYNCNET_API int AsyncNet_ClearQueue(int request_type) {
    if (!IsInitialized()) {
        SetLastError("Not initialized");
        return ASYNCNET_ERROR_NOT_INIT;
    }

    try {
        int cleared = g_network_manager->ClearQueue(request_type);
        SetLastError("");
        return cleared;
    }
    catch (const std::exception& e) {
        SetLastError(std::string("Exception clearing queue: ") + e.what());
        return ASYNCNET_ERROR_NETWORK;
    }
}

//+------------------------------------------------------------------+
//| 清理接口                                                          |
//+------------------------------------------------------------------+
ASYNCNET_API void AsyncNet_Cleanup() {
    std::lock_guard<std::mutex> lock(g_init_mutex);

    if (g_initialized && g_network_manager) {
        try {
            g_network_manager->Shutdown();
            g_network_manager.reset();
        }
        catch (const std::exception& e) {
            // 清理时忽略异常，但记录错误
            SetLastError(std::string("Exception during cleanup: ") + e.what());
        }
    }

    g_initialized = false;
    SetLastError("");
}

//+------------------------------------------------------------------+
//| 版本信息接口                                                      |
//+------------------------------------------------------------------+
ASYNCNET_API const char* AsyncNet_GetVersion() {
    return ASYNCNET_VERSION;
}

ASYNCNET_API int AsyncNet_GetBuildNumber() {
    return ASYNCNET_BUILD_NUMBER;
}