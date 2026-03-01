# LinkForge 框架设计文档

## 1. 设计理念

LinkForge 采用现代化的面向对象设计，遵循 SOLID 原则，提供高性能、可扩展的通信框架。

### 1.1 核心设计原则

- **单一职责原则 (SRP)**: 每个类专注于一个功能
- **开闭原则 (OCP)**: 对扩展开放，对修改关闭
- **里氏替换原则 (LSP)**: 通过抽象接口实现多态
- **接口隔离原则 (ISP)**: 精简的接口设计
- **依赖倒置原则 (DIP)**: 依赖抽象而非具体实现

### 1.2 设计模式

1. **策略模式 (Strategy Pattern)**
   - 用于传输层和协议层的不同实现
   - 运行时可切换策略

2. **工厂模式 (Factory Pattern)**
   - TransportFactory 和 ProtocolFactory
   - 解耦对象创建和使用

3. **观察者模式 (Observer Pattern)**
   - Qt 信号槽机制
   - 事件驱动通信

4. **适配器模式 (Adapter Pattern)**
   - 统一不同协议的接口

5. **单例模式 (Singleton Pattern)**
   - ConfigManager 和 Logger

## 2. 架构层次

### 2.1 传输层 (Transport Layer)

**职责**: 处理底层 IO 操作，管理物理连接

**核心接口**: `ITransport`

```cpp
class ITransport {
    virtual bool connect(const QVariantMap& config) = 0;
    virtual void disconnect() = 0;
    virtual qint64 send(const QByteArray& data) = 0;
    virtual State state() const = 0;
signals:
    void dataReceived(const QByteArray& data);
    void stateChanged(State newState);
    void errorOccurred(const QString& error);
};
```

**实现类**:
- `SerialTransport`: 基于 QSerialPort
- `TcpSocketTransport`: 基于 QTcpSocket
- `UdpSocketTransport`: 基于 QUdpSocket
- `CanTransport`: 基于 QCanBus (待实现)

**特性**:
- 自动重连机制
- 超时处理
- 错误恢复
- 状态机管理

### 2.2 协议层 (Protocol Layer)

**职责**: 数据编码/解码、帧解析、校验

**核心接口**: `IProtocol`

```cpp
class IProtocol {
    virtual std::optional<QByteArray> buildMessage(const QVariantMap& payload) = 0;
    virtual std::vector<Message> parseData(const QByteArray& data) = 0;
    virtual bool validateMessage(const QByteArray& data) const = 0;
signals:
    void messageParsed(const Message& message);
    void parseError(const QString& error);
};
```

**实现类**:
- `ModbusProtocol`: Modbus RTU/TCP
- `CustomProtocol`: 自定义二进制协议
- `JsonProtocol`: JSON 格式协议
- `MavlinkProtocol`: MAVLink (待实现)
- `UdsProtocol`: UDS ISO 14229 (待实现)

**特性**:
- 接收缓冲区管理
- 分包/粘包处理
- CRC/校验和验证
- 超时检测

### 2.3 通信管理层 (Communication Manager)

**职责**: 协调传输层和协议层，提供统一接口

**核心类**: `CommunicationManager`

```cpp
class CommunicationManager {
    bool initialize(transportType, protocolType, configs);
    bool connect();
    void disconnect();
    bool sendMessage(const QVariantMap& payload);
signals:
    void messageReceived(const IProtocol::Message& message);
    void errorOccurred(const QString& error);
};
```

**功能**:
- 生命周期管理
- 数据流协调
- 统计信息收集
- 错误传播

### 2.4 应用层 (Application Layer)

**职责**: 业务逻辑、UI 交互、数据展示

**核心类**:
- `MainController`: 主控制器
- `MessageModel`: 消息数据模型

**特性**:
- MVC 架构
- 数据绑定
- 事件处理
- 统计分析

### 2.5 工具层 (Utility Layer)

**核心类**:
- `ConfigManager`: 配置管理
- `Logger`: 日志系统

## 3. 数据流

### 3.1 发送数据流

```
Application Layer (QVariantMap payload)
        ↓
MainController::sendMessage()
        ↓
CommunicationManager::sendMessage()
        ↓
IProtocol::buildMessage() → QByteArray
        ↓
ITransport::send() → Physical IO
```

### 3.2 接收数据流

```
Physical IO → QByteArray
        ↓
ITransport::dataReceived(data) [signal]
        ↓
CommunicationManager::onTransportDataReceived()
        ↓
IProtocol::parseData() → std::vector<Message>
        ↓
IProtocol::messageParsed(message) [signal]
        ↓
CommunicationManager::onProtocolMessageParsed()
        ↓
MainController::onCommMessageReceived()
        ↓
MessageModel::addReceivedMessage()
        ↓
UI updates automatically
```

## 4. 线程模型

### 4.1 当前实现（单线程）

- 所有操作在主线程（UI线程）
- 使用 Qt 事件循环异步处理
- 非阻塞 IO 操作

### 4.2 多线程扩展（可选）

```cpp
// 将传输层移到工作线程
QThread* ioThread = new QThread;
transport->moveToThread(ioThread);
ioThread->start();
```

**优势**:
- UI 响应更快
- 支持阻塞操作
- 更高吞吐量

**注意事项**:
- 跨线程信号槽使用排队连接
- 数据竞争保护（QMutex）
- 对象生命周期管理

## 5. 内存管理

### 5.1 智能指针策略

```cpp
// 所有权转移：std::unique_ptr
std::unique_ptr<QSerialPort> m_port;

// 共享所有权：std::shared_ptr
std::shared_ptr<ITransport> transport;

// 弱引用：std::weak_ptr
std::weak_ptr<IProtocol> protocolRef;
```

### 5.2 Qt 对象树

```cpp
// 利用 Qt 父子关系自动管理
new QSerialPort(this);  // this 作为 parent
```

### 5.3 RAII 原则

```cpp
// 析构函数自动清理资源
~CommunicationManager() {
    disconnect();  // 关闭连接
    // 智能指针自动释放
}
```

## 6. 错误处理

### 6.1 错误传播链

```
Low Level Error (QSerialPort::errorOccurred)
        ↓
ITransport::errorOccurred [signal]
        ↓
CommunicationManager::onTransportErrorOccurred
        ↓
CommunicationManager::errorOccurred [signal]
        ↓
MainController::onCommErrorOccurred
        ↓
MainController::errorOccurred [signal]
        ↓
UI displays error
```

### 6.2 错误类型

- **连接错误**: 端口打开失败、网络连接超时
- **传输错误**: 写入失败、读取中断
- **协议错误**: 校验失败、格式错误
- **业务错误**: 无效参数、逻辑错误

### 6.3 恢复策略

- 自动重连（传输层）
- 重发机制（协议层）
- 降级处理（应用层）
- 用户通知（UI层）

## 7. 性能优化

### 7.1 零拷贝设计

```cpp
// QByteArray 使用隐式共享
QByteArray data = receiveData();
emit dataReceived(data);  // 引用计数，不拷贝
```

### 7.2 缓冲区管理

```cpp
// 预分配缓冲区
m_receiveBuffer.reserve(4096);

// 批量处理
while (hasCompleteFrame()) {
    processFrame();
}
```

### 7.3 异步操作

```cpp
// 非阻塞 IO
socket->connectToHost(host, port);
// 不等待，通过信号槽处理结果
```

### 7.4 编译优化

```cpp
// 使用 constexpr
static constexpr quint16 HEADER = 0xAA55;

// 使用 inline
inline bool isValid() const { return m_isValid; }

// 移动语义
ITransport(ITransport&&) = default;
```

## 8. 扩展指南

### 8.1 添加新传输方式

1. 创建类继承 `ITransport`
2. 实现所有纯虚函数
3. 注册到 `TransportFactory`

```cpp
class BluetoothTransport : public ITransport {
    // 实现接口...
};

// 注册
TransportFactory::registerTransport(
    ITransport::Type::Bluetooth,
    [](QObject* parent) {
        return std::make_shared<BluetoothTransport>(parent);
    }
);
```

### 8.2 添加新协议

1. 创建类继承 `IProtocol`
2. 实现编码/解码逻辑
3. 注册到 `ProtocolFactory`

```cpp
class J1939Protocol : public IProtocol {
    // 实现接口...
};

// 注册
ProtocolFactory::registerProtocol(
    IProtocol::Type::J1939,
    [](const QVariantMap& config, QObject* parent) {
        return std::make_shared<J1939Protocol>(parent);
    }
);
```

## 9. 测试策略

### 9.1 单元测试

```cpp
// Mock 对象
class MockTransport : public ITransport {
    MOCK_METHOD(bool, connect, (const QVariantMap&), (override));
    MOCK_METHOD(qint64, send, (const QByteArray&), (override));
};

// 测试
TEST(ProtocolTest, ParseValidMessage) {
    ModbusProtocol protocol(Mode::RTU);
    QByteArray testData = createTestData();
    auto messages = protocol.parseData(testData);
    EXPECT_EQ(messages.size(), 1);
    EXPECT_TRUE(messages[0].isValid);
}
```

### 9.2 集成测试

```cpp
// 端到端测试
TEST(IntegrationTest, SerialModbusCommunication) {
    MainController controller;
    controller.initializeCommunication("Serial", "Modbus", config);
    EXPECT_TRUE(controller.connect());
    EXPECT_TRUE(controller.sendMessage(testMessage));
}
```

### 9.3 压力测试

```cpp
// 大量消息发送
for (int i = 0; i < 10000; ++i) {
    controller.sendMessage(message);
}
// 验证无内存泄漏、性能稳定
```

## 10. 最佳实践

1. **使用配置文件**: 避免硬编码
2. **日志记录**: 记录关键操作和错误
3. **异常安全**: 使用 RAII 和智能指针
4. **接口稳定**: 不轻易修改公共接口
5. **文档完整**: 注释清晰、示例丰富
6. **版本控制**: 语义化版本号
7. **持续集成**: 自动化测试
8. **代码审查**: 保证质量

## 11. 未来规划

- [ ] CAN 总线支持（QCanBus）
- [ ] MAVLink 协议集成
- [ ] UDS 诊断协议
- [ ] DBC 文件解析
- [ ] J1939 协议
- [ ] WebSocket 支持
- [ ] 加密传输（TLS/SSL）
- [ ] 数据录制/回放
- [ ] 插件系统
- [ ] Python 绑定

## 12. 性能指标

### 12.1 目标性能

- 串口吞吐量: > 1 MB/s
- TCP吞吐量: > 10 MB/s
- 消息延迟: < 10 ms
- 内存占用: < 50 MB
- CPU占用: < 5% (空闲)

### 12.2 测试环境

- CPU: Intel i5 或更高
- RAM: 8 GB
- OS: Windows 10/11, Linux
- Qt: 5.15+ 或 6.x

## 13. 依赖管理

### 13.1 必需依赖

- Qt Core (5.15+)
- Qt SerialPort
- Qt Network
- C++17 编译器

### 13.2 可选依赖

- Qt SerialBus (CAN/Modbus)
- OpenSSL (加密)
- Google Test (测试)

### 13.3 第三方库

- MAVLink (C 库)
- cantools (Python, DBC解析)

---

**文档版本**: 1.0  
**最后更新**: 2026-02-24  
**维护者**: LinkForge Team
