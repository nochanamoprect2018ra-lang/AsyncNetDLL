# AsyncNetDLL 部署和使用指南

## 快速部署指南

### 第一步：编译DLL

1. **准备开发环境**
```bash
# 确保已安装Visual Studio 2019/2022
# 下载依赖库到指定目录
AsyncNetDLL/
├── lib/
│   ├── libcurl/     # 下载libcurl静态库
│   └── openssl/     # 下载OpenSSL静态库
```

2. **编译DLL**
```bash
cd AsyncNetDLL
build.bat
```

编译成功后会生成：
- `bin/AsyncNetDLL.dll` - Release版本
- `bin/AsyncNetDLL_Debug.dll` - Debug版本（可选）

### 第二步：部署到MT4

1. **复制DLL文件**
```bash
# 复制到MT4的Libraries目录
copy bin\AsyncNetDLL.dll "C:\Users\%USERNAME%\AppData\Roaming\MetaQuotes\Terminal\[TERMINAL_ID]\MQL4\Libraries\"

# 或复制到MT4安装目录
copy bin\AsyncNetDLL.dll "C:\Program Files\MetaTrader 4\MQL4\Libraries\"
```

2. **复制适配层文件**
```bash
# 复制AsyncNetAdapter.mqh到Include目录
copy client\ceshi\new61\AsyncNetAdapter.mqh "C:\Users\%USERNAME%\AppData\Roaming\MetaQuotes\Terminal\[TERMINAL_ID]\MQL4\Include\"
```

3. **启用DLL导入**
- 在MT4中：工具 → 选项 → 专家顾问
- 勾选"允许DLL导入"
- 勾选"允许导入外部专家"

### 第三步：更新EA代码

修改现有的EA文件以使用异步网络：

```mql4
// 替换原有的网络库包含
// #include "pu_WinINet.mqh"  // 删除这行
#include "AsyncNetAdapter.mqh"  // 添加这行

// 在OnInit中初始化异步网络
int OnInit() {
    // 初始化异步网络
    if (!InitAsyncNetwork("http://your-server.com", 5000)) {
        Print("异步网络初始化失败");
        return INIT_FAILED;
    }
    
    // 设置认证信息
    if (!SetAsyncNetAuth(g_account, g_license, g_secret_key)) {
        Print("认证设置失败");
        return INIT_FAILED;
    }
    
    // 配置网络参数（可选）
    ConfigAsyncNetwork(5, 2, 100);
    
    return INIT_SUCCEEDED;
}

// 修改OnTimer以使用异步网络
void OnTimer() {
    // 处理异步响应（必须调用）
    ProcessAsyncResponses();
    
    // 发送1秒心跳
    SendAsyncHeartbeat();
    
    // 其他逻辑保持不变...
}

// 在OnDeinit中清理
void OnDeinit(const int reason) {
    CleanupAsyncNetwork();
}
```

## 性能验证

### 验证异步网络是否工作

1. **检查初始化状态**
```mql4
void OnTimer() {
    static datetime last_check = 0;
    if (TimeLocal() - last_check > 10) {
        Print("网络状态: ", GetAsyncNetworkStatus());
        Print("网络统计: ", GetAsyncNetworkStats());
        Print("DLL版本: ", GetAsyncNetworkVersion());
        last_check = TimeLocal();
    }
}
```

2. **预期输出示例**
```
网络状态: 连接数: 3, 队列: H0/E0/P0
网络统计: 心跳: 120/118, 事件: 45/45, 参数: 2/2
DLL版本: 1.0.0 (Build 1001)
```

### 性能对比测试

**测试方法**：
1. 运行原版EA（使用WinINet）记录性能
2. 运行异步版EA（使用AsyncNetDLL）记录性能
3. 对比关键指标

**关键指标**：
- 心跳间隔：应从60秒降到1秒
- 主线程阻塞：应从5-10秒降到0秒
- 网络吞吐量：应提升3-5倍
- 系统响应性：UI应更流畅

## 故障排除

### 常见问题及解决方案

**问题1：DLL加载失败**
```
错误信息：cannot load library 'AsyncNetDLL.dll'
解决方案：
1. 检查DLL是否在正确的Libraries目录
2. 确认MT4已启用"允许DLL导入"
3. 检查DLL依赖库是否完整
4. 确认DLL与MT4位数匹配（32位/64位）
```

**问题2：网络连接失败**
```
错误信息：异步网络初始化失败
解决方案：
1. 检查服务器地址是否正确
2. 确认防火墙未阻止连接
3. 验证网络连接是否正常
4. 检查代理设置
```

**问题3：认证失败**
```
错误信息：认证设置失败
解决方案：
1. 验证License和SecretKey是否正确
2. 检查账户是否已绑定
3. 确认服务器端认证配置
```

**问题4：心跳发送失败**
```
错误信息：心跳发送失败，错误码: -5
解决方案：
1. 检查网络队列是否已满
2. 降低心跳频率或增加队列大小
3. 检查网络连接稳定性
```

### 调试方法

1. **启用详细日志**
```mql4
// 在EA中添加调试输出
void OnTimer() {
    ProcessAsyncResponses();
    
    // 每10秒输出一次状态
    static datetime last_debug = 0;
    if (TimeLocal() - last_debug > 10) {
        Print("[调试] ", GetAsyncNetworkStatus());
        Print("[统计] ", GetAsyncNetworkStats());
        last_debug = TimeLocal();
    }
}
```

2. **监控网络队列**
```mql4
// 检查队列状态
int heartbeat_queue = AsyncNet_GetQueueLength(REQ_HEARTBEAT);
int events_queue = AsyncNet_GetQueueLength(REQ_EVENTS);
int params_queue = AsyncNet_GetQueueLength(REQ_PARAMS);

if (heartbeat_queue > 5) {
    Print("警告: 心跳队列积压 ", heartbeat_queue);
}
```

3. **性能监控**
```mql4
// 监控主线程阻塞时间
datetime start_time = TimeLocal();
ProcessAsyncResponses();
datetime end_time = TimeLocal();

if (end_time - start_time > 1) {
    Print("警告: 响应处理耗时 ", end_time - start_time, " 秒");
}
```

## 高级配置

### 网络参数调优

```mql4
// 根据网络环境调整参数
if (IsHighLatencyNetwork()) {
    // 高延迟网络：增加超时时间，减少并发
    ConfigAsyncNetwork(3, 1, 50);
} else if (IsHighFrequencyTrading()) {
    // 高频交易：增加并发，扩大队列
    ConfigAsyncNetwork(8, 4, 200);
} else {
    // 默认配置
    ConfigAsyncNetwork(5, 2, 100);
}
```

### Wine环境优化

```mql4
// Wine环境下的特殊配置
#ifdef __WINE__
    // 降低超时时间避免阻塞
    InitAsyncNetwork(SERVER_HOST, 3000);
    // 使用较少的连接数
    ConfigAsyncNetwork(3, 1, 50);
#endif
```

### 生产环境部署

1. **服务器端配置**
```nginx
# Nginx配置示例
upstream trading_backend {
    server 127.0.0.1:8000;
    keepalive 32;
}

server {
    listen 80;
    server_name your-trading-server.com;
    
    location /v1/ {
        proxy_pass http://trading_backend;
        proxy_http_version 1.1;
        proxy_set_header Connection "";
        proxy_set_header Host $host;
        proxy_connect_timeout 2s;
        proxy_send_timeout 5s;
        proxy_read_timeout 5s;
    }
}
```

2. **负载均衡配置**
```yaml
# Docker Compose示例
version: '3.8'
services:
  trading-api:
    image: trading-system:latest
    replicas: 3
    ports:
      - "8000-8002:8000"
    environment:
      - ASYNC_WORKERS=4
      - MAX_CONNECTIONS=100
```

## 迁移检查清单

### 迁移前准备
- [ ] 备份现有EA代码
- [ ] 测试AsyncNetDLL在测试环境
- [ ] 验证服务器端兼容性
- [ ] 准备回滚方案

### 迁移步骤
- [ ] 编译并部署AsyncNetDLL
- [ ] 修改EA代码集成异步网络
- [ ] 在模拟账户测试
- [ ] 监控性能指标
- [ ] 逐步部署到生产环境

### 迁移后验证
- [ ] 心跳间隔确实为1秒
- [ ] 主线程无阻塞现象
- [ ] 事件上报正常工作
- [ ] 参数拉取功能正常
- [ ] 系统稳定运行24小时以上

## 监控和维护

### 关键监控指标

1. **网络性能指标**
   - 心跳成功率 > 95%
   - 事件上报成功率 > 98%
   - 参数拉取成功率 > 99%
   - 平均响应延迟 < 500ms

2. **系统性能指标**
   - 主线程阻塞时间 < 10ms
   - 内存使用增长 < 1MB/小时
   - CPU使用率增加 < 5%

3. **业务指标**
   - 交易执行延迟改善
   - 系统响应性提升
   - 用户体验改善

### 定期维护任务

1. **每日检查**
   - 监控网络连接状态
   - 检查错误日志
   - 验证关键功能正常

2. **每周检查**
   - 分析性能趋势
   - 检查内存泄漏
   - 更新监控报告

3. **每月检查**
   - 评估性能改善效果
   - 优化网络参数
   - 计划功能升级

---

通过以上部署和使用指南，您可以成功将现有的同步网络系统迁移到高性能的异步网络架构，实现1秒心跳间隔和零阻塞网络通信的目标。