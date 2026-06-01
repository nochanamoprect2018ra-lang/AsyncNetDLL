# AsyncNetDLL - 异步网络DLL

## 项目概述

AsyncNetDLL是一个专为MT4/MT5交易系统设计的高性能异步网络通信库，用于替换传统的WinINet同步网络实现。该DLL基于libcurl实现，提供非阻塞的HTTP通信能力，显著提升交易系统的响应性能。

## 核心特性

### 🚀 性能优势
- **零阻塞通信**：完全异步的网络请求，不阻塞MT4主线程
- **1秒心跳间隔**：从60秒提升到1秒，实时性提升60倍
- **并发处理**：支持5个并发HTTP连接，吞吐量提升5倍
- **连接复用**：Keep-Alive连接池，减少TCP握手开销

### 📊 数据优化
- **紧凑心跳格式**：从500字节压缩到80字节，节省84%带宽
- **智能批量处理**：事件数据分批上报，避免URL长度限制
- **优先级队列**：心跳请求优先处理，确保实时性

### 🛡️ 稳定性保障
- **Wine环境兼容**：专门适配Linux Wine环境
- **错误重试机制**：智能重试策略，提高网络稳定性
- **资源管理**：自动连接池管理，防止内存泄漏

## 技术架构

```
┌─────────────────────────────────────────────────────────────┐
│                    MQL4/MT5 EA Layer                        │
├─────────────────────────────────────────────────────────────┤
│                AsyncNetAdapter.mqh (适配层)                 │
├─────────────────────────────────────────────────────────────┤
│                   AsyncNetDLL.dll                          │
│  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────┐ │
│  │  Request Queue  │  │ Connection Pool │  │Thread Pool │ │
│  │   (Priority)    │  │  (Keep-Alive)   │  │ (2 workers)│ │
│  └─────────────────┘  └─────────────────┘  └─────────────┘ │
│  ┌─────────────────────────────────────────────────────────┐ │
│  │            libcurl Multi Interface                     │ │
│  └─────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────┘
```

## 快速开始

### 1. 环境要求

- **开发环境**：Visual Studio 2019/2022
- **依赖库**：
  - libcurl 7.80+ (静态链接)
  - OpenSSL 1.1.1+ (静态链接)
- **目标平台**：Windows 7+ (32/64位)
- **MT4/MT5**：Build 600+

### 2. 编译构建

```bash
# 克隆项目
git clone <repository-url>
cd AsyncNetDLL

# 下载依赖库
# 1. 下载libcurl静态库到 lib/libcurl/
# 2. 下载OpenSSL静态库到 lib/openssl/

# 使用Visual Studio编译
# 或使用构建脚本
build.bat
```

### 3. 集成到MT4

```mql4
// 包含适配层
#include "AsyncNetAdapter.mqh"

// 在OnInit中初始化
int OnInit() {
    // 初始化异步网络
    if (!InitAsyncNetwork("http://your-server.com", 5000)) {
        Print("异步网络初始化失败");
        return INIT_FAILED;
    }
    
    // 设置认证信息
    if (!SetAsyncNetAuth("account", "license", "secret_key")) {
        Print("认证设置失败");
        return INIT_FAILED;
    }
    
    // 配置网络参数
    ConfigAsyncNetwork(5, 2, 100); // 5连接，2线程，100队列
    
    return INIT_SUCCEEDED;
}

// 在OnTimer中处理网络通信
void OnTimer() {
    // 处理异步响应
    ProcessAsyncResponses();
    
    // 发送心跳（1秒间隔）
    SendAsyncHeartbeat();
    
    // 发送事件（按需）
    if (HasPendingEvents()) {
        string events_json = BuildEventsJson();
        SendAsyncEvents(events_json);
    }
    
    // 拉取参数（60秒间隔）
    FetchAsyncParams();
}

// 在OnDeinit中清理
void OnDeinit(const int reason) {
    CleanupAsyncNetwork();
}
```

## API参考

### 核心接口

#### 初始化和配置
```cpp
// 初始化异步网络
int AsyncNet_Initialize(const char* server_host, int timeout_ms);

// 设置认证信息
int AsyncNet_SetAuth(const char* account, const char* license, const char* secret_key);

// 配置网络参数
int AsyncNet_SetConfig(int max_connections, int worker_threads, int queue_size);
```

#### 异步请求
```cpp
// 发送心跳请求
int AsyncNet_SendHeartbeat(const char* data, int data_len);

// 发送事件数据
int AsyncNet_SendEvents(const char* data, int data_len);

// 拉取参数
int AsyncNet_FetchParams();
```

#### 响应处理
```cpp
// 处理完成的响应
int AsyncNet_ProcessResponses();

// 获取心跳响应
int AsyncNet_GetHeartbeatResponse(char* buffer, int buffer_size);

// 获取事件响应
int AsyncNet_GetEventsResponse(char* buffer, int buffer_size);

// 获取参数响应
int AsyncNet_GetParamsResponse(char* buffer, int buffer_size);
```

### MQL4适配层

#### 高级接口
```mql4
// 初始化异步网络
bool InitAsyncNetwork(string server_host, int timeout_ms = 5000);

// 设置认证信息
bool SetAsyncNetAuth(string account, string license, string secret_key);

// 发送异步心跳
bool SendAsyncHeartbeat();

// 发送异步事件
bool SendAsyncEvents(string events_json);

// 拉取异步参数
bool FetchAsyncParams();

// 处理异步响应
int ProcessAsyncResponses();
```

#### 状态查询
```mql4
// 获取网络状态
string GetAsyncNetworkStatus();

// 获取统计信息
string GetAsyncNetworkStats();

// 获取版本信息
string GetAsyncNetworkVersion();
```

## 性能对比

| 指标 | WinINet同步 | AsyncNetDLL异步 | 提升倍数 |
|------|-------------|-----------------|----------|
| 心跳间隔 | 60秒 | 1秒 | 60x |
| 主线程阻塞 | 5-10秒 | 0秒 | ∞ |
| 心跳数据大小 | 500字节 | 80字节 | 6.25x |
| 并发连接数 | 1个 | 5个 | 5x |
| 网络吞吐量 | 基准 | 3-5倍 | 3-5x |

## 配置说明

### 连接池配置
```cpp
// 最大连接数：1-10，推荐5
// 工作线程数：1-4，推荐2
// 队列大小：10-1000，推荐100
AsyncNet_SetConfig(5, 2, 100);
```

### 重试策略
- **心跳请求**：最多重试2次，基础延迟200ms，指数退避1.5倍
- **事件上报**：最多重试3次，基础延迟500ms，指数退避2.0倍
- **参数拉取**：最多重试5次，基础延迟1000ms，指数退避2.0倍

### Wine环境适配
- 自动检测Wine环境
- 禁用信号处理（CURLOPT_NOSIGNAL）
- 使用HTTP/1.1协议
- 降低超时时间避免阻塞

## 故障排除

### 常见问题

**Q: DLL加载失败**
```
A: 检查以下项目：
1. AsyncNetDLL.dll是否在MT4的Libraries目录
2. 依赖的运行时库是否已安装（VC++ Redistributable）
3. DLL是否与MT4位数匹配（32位/64位）
```

**Q: 网络连接失败**
```
A: 检查以下项目：
1. 服务器地址是否正确
2. 防火墙是否阻止连接
3. 代理设置是否正确
4. SSL证书是否有效
```

**Q: 心跳发送失败**
```
A: 检查以下项目：
1. 认证信息是否正确设置
2. 网络连接是否正常
3. 服务器是否响应
4. 查看GetAsyncNetworkStats()的错误信息
```

### 调试方法

1. **启用详细日志**
```mql4
// 在EA中添加调试输出
void OnTimer() {
    ProcessAsyncResponses();
    
    // 输出网络状态
    static datetime last_debug = 0;
    if (TimeLocal() - last_debug > 10) {
        Print("[调试] ", GetAsyncNetworkStatus());
        Print("[统计] ", GetAsyncNetworkStats());
        last_debug = TimeLocal();
    }
}
```

2. **检查队列状态**
```cpp
// 在C++代码中添加调试输出
int heartbeat_queue = AsyncNet_GetQueueLength(REQ_HEARTBEAT);
int events_queue = AsyncNet_GetQueueLength(REQ_EVENTS);
int params_queue = AsyncNet_GetQueueLength(REQ_PARAMS);
```

## CI/CD 自动构建

项目配置了 GitHub Actions，推送到 main/master 分支时自动编译 Windows DLL 并发布 Release。

### 触发条件
- push 到 main/master/develop 分支（AsyncNetDLL 目录变更）
- 手动触发（workflow_dispatch）

### 构建环境
- Ubuntu + MinGW-w64 交叉编译
- vcpkg 管理 libcurl + OpenSSL 依赖
- 静态链接，生成无外部依赖的 DLL

### 手动触发构建
1. 进入 GitHub 仓库 -> Actions -> Build AsyncNetDLL
2. 点击 "Run workflow"
3. 选择分支后确认

### 下载构建产物
- 构建完成后，DLL 在 Actions -> Artifacts 中下载
- main/master 分支会自动创建 GitHub Release

## 开发指南

### 扩展功能

1. **添加新的请求类型**
```cpp
// 在AsyncNetDLL.h中添加新类型
enum RequestType {
    REQ_HEARTBEAT = 1,
    REQ_EVENTS = 2,
    REQ_PARAMS = 3,
    REQ_PING = 4,
    REQ_CUSTOM = 5  // 新增类型
};
```

2. **自定义重试策略**
```cpp
// 在RequestQueue.cpp中修改重试配置
static RetryConfig ForCustom() {
    return {3, 1000, 2.0}; // 3次重试，1秒基础延迟，2倍退避
}
```

### 性能调优

1. **连接池大小**：根据并发需求调整，过大会浪费资源
2. **队列大小**：根据请求频率调整，过小会导致丢弃请求
3. **超时时间**：根据网络环境调整，过短会导致频繁重试

## 版本历史

### v1.0.0 (2024-06-01)
- 初始版本发布
- 实现基础异步网络功能
- 支持心跳、事件、参数三种请求类型
- 提供MQL4适配层
- Wine环境兼容性支持

## 许可证

本项目采用MIT许可证，详见LICENSE文件。

## 技术支持

如有问题或建议，请通过以下方式联系：

- 项目Issues：<repository-url>/issues
- 邮箱：support@trading-system.com
- 文档：<documentation-url>

---

**注意**：本DLL仅用于合法的交易系统开发，请遵守相关法律法规和交易所规定。