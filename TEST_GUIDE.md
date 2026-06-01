# AsyncNetDLL 测试验证指南

## 测试环境准备

### 前置条件
1. Windows 7+ 系统（32位或64位）
2. MT4/MT5 已安装
3. 模拟账户已创建
4. AsyncNetDLL.dll 已编译并部署到 MT4/Libraries 目录
5. AsyncNetAdapter.mqh 已复制到 MT4/Include 目录

### 部署检查清单
- [ ] DLL 文件存在于 `C:\Users\[User]\AppData\Roaming\MetaQuotes\Terminal\[ID]\MQL4\Libraries\AsyncNetDLL.dll`
- [ ] MQL4 适配层存在于 `C:\Users\[User]\AppData\Roaming\MetaQuotes\Terminal\[ID]\MQL4\Include\AsyncNetAdapter.mqh`
- [ ] MT4 已启用"允许DLL导入"选项
- [ ] 修改后的 GoldKylin_v1.mq4 已编译通过

## 测试场景

### 测试1：DLL 加载验证

**目标**：验证 AsyncNetDLL 能否正确加载

**步骤**：
1. 打开 MT4
2. 在 EA 中添加以下代码到 OnInit：
```mql4
int OnInit() {
    Print("[测试] 开始初始化异步网络");
    
    if (!InitAsyncNetwork("http://your-server.com", 5000)) {
        Print("[测试] 异步网络初始化失败");
        return INIT_FAILED;
    }
    
    Print("[测试] 异步网络初始化成功");
    Print("[测试] DLL版本: ", GetAsyncNetworkVersion());
    
    return INIT_SUCCEEDED;
}
```

**预期结果**：
```
[测试] 开始初始化异步网络
[测试] 异步网络初始化成功
[测试] DLL版本: 1.0.0 (Build 1001)
```

**失败排查**：
- 如果看到 "cannot load library" 错误，检查 DLL 是否在正确位置
- 如果看到初始化失败，检查服务器地址是否正确

### 测试2：1秒心跳验证

**目标**：验证心跳间隔是否为1秒

**步骤**：
1. 在 OnTimer 中添加以下代码：
```mql4
void OnTimer() {
    static datetime last_heartbeat = 0;
    static int heartbeat_count = 0;
    
    datetime current_time = TimeLocal();
    
    // 发送心跳
    if (SendAsyncHeartbeat()) {
        heartbeat_count++;
        
        // 每10个心跳输出一次统计
        if (heartbeat_count % 10 == 0) {
            int interval = current_time - last_heartbeat;
            Print("[心跳测试] 已发送 ", heartbeat_count, " 个心跳");
            Print("[心跳测试] 最后10个心跳耗时: ", interval, " 秒");
            Print("[心跳测试] 平均间隔: ", (double)interval / 10, " 秒");
            last_heartbeat = current_time;
        }
    }
}
```

2. 运行 EA 并观察日志输出

**预期结果**：
```
[心跳测试] 已发送 10 个心跳
[心跳测试] 最后10个心跳耗时: 10 秒
[心跳测试] 平均间隔: 1.0 秒

[心跳测试] 已发送 20 个心跳
[心跳测试] 最后10个心跳耗时: 10 秒
[心跳测试] 平均间隔: 1.0 秒
```

**失败排查**：
- 如果间隔不是1秒，检查 SendAsyncHeartbeat() 的实现
- 如果心跳发送失败，检查网络连接

### 测试3：网络状态监控

**目标**：验证异步网络的运行状态

**步骤**：
1. 在 OnTimer 中添加监控代码：
```mql4
void OnTimer() {
    static datetime last_status_check = 0;
    datetime current_time = TimeLocal();
    
    // 每30秒输出一次网络状态
    if (current_time - last_status_check >= 30) {
        Print("[网络状态] ", GetAsyncNetworkStatus());
        Print("[网络统计] ", GetAsyncNetworkStats());
        last_status_check = current_time;
    }
}
```

2. 运行 EA 并观察输出

**预期结果**：
```
[网络状态] 连接数: 3, 队列: H0/E0/P0
[网络统计] 心跳: 30/30, 事件: 0/0, 参数: 0/0

[网络状态] 连接数: 5, 队列: H0/E0/P0
[网络统计] 心跳: 60/60, 事件: 0/0, 参数: 0/0
```

### 测试4：事件上报验证

**目标**：验证事件上报功能

**步骤**：
1. 手动开仓一个订单
2. 在 OnTimer 中添加事件监控：
```mql4
void OnTimer() {
    ProcessAsyncResponses();
    
    // 检查是否有待上报的事件
    int queue_length = AsyncNet_GetQueueLength(REQ_EVENTS);
    if (queue_length > 0) {
        Print("[事件监控] 待上报事件数: ", queue_length);
    }
    
    // 定期上报事件
    static datetime last_flush = 0;
    if (TimeLocal() - last_flush >= 30) {
        CloudFlushEvents();
        last_flush = TimeLocal();
    }
}
```

**预期结果**：
```
[事件监控] 待上报事件数: 1
[异步事件] 已发送 1 个事件
```

### 测试5：性能对比测试

**目标**：对比异步网络与原有 WinINet 的性能

**测试指标**：
1. **主线程阻塞时间**
   - 原有方式：5-10秒
   - 异步方式：< 10ms

2. **心跳间隔**
   - 原有方式：60秒
   - 异步方式：1秒

3. **系统响应性**
   - 原有方式：网络请求时 UI 卡顿
   - 异步方式：UI 始终流畅

**测试方法**：
```mql4
void OnTimer() {
    datetime start_time = TimeLocal();
    
    // 处理异步响应
    ProcessAsyncResponses();
    
    datetime end_time = TimeLocal();
    int elapsed_ms = (end_time - start_time) * 1000;
    
    if (elapsed_ms > 10) {
        Print("[性能警告] 响应处理耗时: ", elapsed_ms, " ms");
    }
}
```

## 监控指标

### 关键性能指标（KPI）

| 指标 | 目标值 | 警告值 | 严重值 |
|------|--------|--------|--------|
| 心跳成功率 | > 95% | < 90% | < 80% |
| 事件上报成功率 | > 98% | < 95% | < 90% |
| 平均响应延迟 | < 500ms | > 1000ms | > 2000ms |
| 主线程阻塞时间 | < 10ms | > 50ms | > 100ms |
| 内存增长速率 | < 1MB/h | > 5MB/h | > 10MB/h |

### 监控脚本

在 EA 中添加以下监控代码：

```mql4
struct PerformanceMetrics {
    int heartbeat_sent;
    int heartbeat_success;
    int events_sent;
    int events_success;
    double avg_latency;
    int max_blocking_time;
};

PerformanceMetrics g_metrics;

void UpdateMetrics() {
    string stats = GetAsyncNetworkStats();
    
    // 解析统计信息
    // 格式: "心跳: 120/118, 事件: 45/45, 参数: 2/2"
    
    // 计算成功率
    if (g_metrics.heartbeat_sent > 0) {
        double success_rate = (double)g_metrics.heartbeat_success / g_metrics.heartbeat_sent * 100;
        if (success_rate < 95) {
            Print("[警告] 心跳成功率低: ", success_rate, "%");
        }
    }
}

void OnTimer() {
    // 更新指标
    UpdateMetrics();
    
    // 每分钟输出一次报告
    static datetime last_report = 0;
    if (TimeLocal() - last_report >= 60) {
        Print("[性能报告] 心跳成功率: ", 
              (double)g_metrics.heartbeat_success / g_metrics.heartbeat_sent * 100, "%");
        Print("[性能报告] 事件成功率: ", 
              (double)g_metrics.events_success / g_metrics.events_sent * 100, "%");
        last_report = TimeLocal();
    }
}
```

## 故障排查

### 常见问题

**问题1：DLL 加载失败**
```
cannot load library 'AsyncNetDLL.dll'
```
解决方案：
1. 检查 DLL 是否在正确的 Libraries 目录
2. 确认 MT4 已启用"允许DLL导入"
3. 检查 DLL 依赖库是否完整（libcurl, OpenSSL）

**问题2：心跳发送失败**
```
[异步心跳] 发送失败，错误码: -5
```
解决方案：
1. 检查网络连接
2. 检查服务器地址是否正确
3. 检查防火墙设置

**问题3：内存泄漏**
```
内存使用持续增长
```
解决方案：
1. 检查是否正确调用了 CleanupAsyncNetwork()
2. 检查响应队列是否被正确清理
3. 监控连接池的连接数

## 测试报告模板

```
AsyncNetDLL 测试报告
====================

测试日期: [日期]
测试环境: [MT4版本], [Windows版本]
测试账户: [模拟/真实]

测试结果:
---------
✓ DLL 加载成功
✓ 1秒心跳验证通过
✓ 网络状态监控正常
✓ 事件上报功能正常
✓ 性能指标达标

性能数据:
---------
心跳成功率: 99.5%
事件成功率: 100%
平均响应延迟: 250ms
主线程阻塞时间: < 5ms
内存增长速率: 0.5MB/h

结论:
-----
AsyncNetDLL 已准备好用于生产环境。
```

## 下一步

1. 完成所有测试场景
2. 生成测试报告
3. 逐步部署到生产账户
4. 持续监控性能指标
5. 根据实际情况调整参数