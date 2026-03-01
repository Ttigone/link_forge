# LinkForge - 高性能通用通信框架

## 概述

LinkForge 是一个基于 Qt 和现代 C++ (C++11/14/17) 的高性能通用通信框架，提供了灵活、可扩展的架构来支持多种通信方式和协议。

### 核心特性

- **分层架构**：清晰的传输层、协议层和应用层分离
- **策略模式**：通过抽象接口统一不同实现
- **工厂模式**：动态创建和切换传输/协议类型
- **现代C++**：使用智能指针、std::optional、lambda等特性
- **高性能**：异步IO、零拷贝设计、线程安全
- **易测试**：接口分离，便于 Mock 和单元测试
- **可扩展**：支持自定义传输方式和协议

## 架构设计

```
┌─────────────────────────────────────────────────────┐
│              应用层 (Application Layer)              │
│  ┌──────────────────┐  ┌──────────────────────────┐ │
│  │  MainController  │  │     MessageModel         │ │
│  └──────────────────┘  └──────────────────────────┘ │
└─────────────────────────────────────────────────────┘
                         │
                         ▼
┌─────────────────────────────────────────────────────┐
│            通信管理层 (Communication)                │
│  ┌──────────────────────────────────────────────┐   │
│  │         CommunicationManager                 │   │
│  └──────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────┘
         │                              │
         ▼                              ▼
┌──────────────────┐          ┌──────────────────────┐
│  传输层 (Transport)│         │  协议层 (Protocol)    │
│ ┌──────────────┐ │          │ ┌──────────────────┐ │
│ │  ITransport  │ │          │ │   IProtocol      │ │
│ └──────────────┘ │          │ └──────────────────┘ │
│   ├─ Serial      │          │   ├─ Modbus          │
│   ├─ TCP         │          │   ├─ MAVLink         │
│   ├─ UDP         │          │   ├─ UDS             │
│   └─ CAN         │          │   ├─ DBC             │
│                  │          │   └─ Custom          │
└──────────────────┘          └──────────────────────┘
```

## 目录结构

```
link_forge/
├── core/                       # 核心抽象接口和管理器
│   ├── ITransport.h           # 传输层接口
│   ├── IProtocol.h            # 协议层接口
│   ├── TransportFactory.h/cpp # 传输层工厂
│   ├── ProtocolFactory.h/cpp  # 协议层工厂
│   └── CommunicationManager.h/cpp # 通信管理器
├── transport/                  # 传输层实现
│   ├── SerialTransport.h/cpp  # 串口传输
│   └── SocketTransport.h/cpp  # TCP/UDP Socket
├── protocol/                   # 协议层实现
│   ├── ModbusProtocol.h/cpp   # Modbus协议
│   └── CustomProtocol.h/cpp   # 自定义协议
├── application/                # 应用层
│   ├── MainController.h/cpp   # 主控制器
│   └── MessageModel.h/cpp     # 消息数据模型
└── utils/                      # 工具类
    ├── ConfigManager.h/cpp    # 配置管理
    └── Logger.h/cpp           # 日志系统
```

## 快速开始

### 1. 基本用法

```cpp
#include "application/MainController.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    
    // 创建主控制器
    LinkForge::Application::MainController controller;
    
    // 配置串口参数
    QVariantMap serialConfig;
    serialConfig["port"] = "COM3";
    serialConfig["baudRate"] = 115200;
    serialConfig["dataBits"] = 8;
    serialConfig["parity"] = "None";
    serialConfig["stopBits"] = "1";
    
    // 配置Modbus RTU协议
    QVariantMap modbusConfig;
    modbusConfig["mode"] = "RTU";
    
    // 初始化通信
    if (controller.initializeCommunication("Serial", "Modbus", 
                                          serialConfig, modbusConfig)) {
        // 连接
        if (controller.connect()) {
            qInfo() << "Connected successfully!";
            
            // 发送Modbus读取保持寄存器消息
            QVariantMap message;
            message["slaveId"] = 1;
            message["function"] = "readHoldingRegisters";
            message["address"] = 0;
            message["quantity"] = 10;
            
            controller.sendMessage(message);
        }
    }
    
    return app.exec();
}
```

### 2. TCP/UDP 网络通信

```cpp
// TCP配置
QVariantMap tcpConfig;
tcpConfig["host"] = "192.168.1.100";
tcpConfig["port"] = 502;
tcpConfig["timeout"] = 3000;

// Modbus TCP协议
QVariantMap modbusConfig;
modbusConfig["mode"] = "TCP";

controller.initializeCommunication("TCP", "Modbus", tcpConfig, modbusConfig);
```

### 3. 自定义协议（JSON）

```cpp
// UDP配置
QVariantMap udpConfig;
udpConfig["localHost"] = "0.0.0.0";
udpConfig["localPort"] = 5000;
udpConfig["remoteHost"] = "192.168.1.100";
udpConfig["remotePort"] = 5001;

// JSON协议配置
QVariantMap jsonConfig;
jsonConfig["subType"] = "json";

controller.initializeCommunication("UDP", "Custom", udpConfig, jsonConfig);

// 发送JSON消息
QVariantMap message;
message["type"] = "command";
message["action"] = "start";
message["value"] = 100;
controller.sendMessage(message);
```

### 4. 接收消息

```cpp
// 连接消息接收信号
QObject::connect(&controller, 
    &LinkForge::Application::MainController::messageReceived,
    [](const QString& messageType) {
        qInfo() << "Received message type:" << messageType;
    });

// 访问消息模型（可用于UI显示）
auto* model = controller.messageModel();
// model 可以直接用于 QTableView
```

## 扩展框架

### 添加自定义传输类型

```cpp
#include "core/TransportFactory.h"

// 1. 实现自定义传输类
class MyCustomTransport : public LinkForge::Core::ITransport {
    // ... 实现接口方法
};

// 2. 注册到工厂
LinkForge::Core::TransportFactory::registerTransport(
    LinkForge::Core::ITransport::Type::Can,
    [](QObject* parent) -> ITransport::Ptr {
        return std::make_shared<MyCustomTransport>(parent);
    }
);
```

### 添加自定义协议

```cpp
#include "core/ProtocolFactory.h"

// 1. 实现自定义协议类
class MyProtocol : public LinkForge::Core::IProtocol {
    // ... 实现接口方法
};

// 2. 注册到工厂
LinkForge::Core::ProtocolFactory::registerProtocol(
    LinkForge::Core::IProtocol::Type::MAVLink,
    [](const QVariantMap& config, QObject* parent) -> IProtocol::Ptr {
        return std::make_shared<MyProtocol>(parent);
    }
);
```

## 配置管理

```cpp
#include "utils/ConfigManager.h"

auto& config = LinkForge::Utils::ConfigManager::instance();

// 加载配置
config.loadConfig("my_config.json");

// 获取传输配置
QVariantMap serialConfig = config.getTransportConfig("serial");

// 修改配置
serialConfig["baudRate"] = 115200;
config.setTransportConfig("serial", serialConfig);

// 保存配置
config.saveConfig();
```

## 日志系统

```cpp
#include "utils/Logger.h"

// 启用日志
LinkForge::Utils::Logger::install();

// 启用文件日志
LinkForge::Utils::Logger::instance().enableFileLogging("app.log");

// 使用日志
LOG_INFO("Application started");
LOG_WARNING("Connection timeout");
LOG_ERROR("Failed to open port");
```

## 单元测试

框架的分层设计使得测试非常容易：

```cpp
// Mock传输层
class MockTransport : public LinkForge::Core::ITransport {
public:
    qint64 send(const QByteArray& data) override {
        sentData = data;  // 记录发送的数据
        return data.size();
    }
    
    void simulateReceive(const QByteArray& data) {
        emit dataReceived(data);  // 模拟接收
    }
    
    QByteArray sentData;
};

// 测试协议
TEST(ModbusProtocol, BuildReadRequest) {
    ModbusProtocol protocol(ModbusProtocol::Mode::RTU);
    
    QVariantMap payload;
    payload["slaveId"] = 1;
    payload["function"] = "readHoldingRegisters";
    payload["address"] = 100;
    payload["quantity"] = 5;
    
    auto result = protocol.buildMessage(payload);
    ASSERT_TRUE(result.has_value());
    
    // 验证生成的数据
    QByteArray expected = // ... 预期的Modbus RTU数据
    EXPECT_EQ(result.value(), expected);
}
```

## 性能优化

- **零拷贝**：使用 QByteArray 引用计数机制
- **智能指针**：自动内存管理，避免泄漏
- **异步IO**：Qt的事件循环，非阻塞通信
- **缓冲管理**：协议层维护接收缓冲区，支持分包处理
- **线程安全**：关键部分使用 QMutex 保护

## 支持的协议

### 已实现
- ✅ Modbus RTU/TCP
- ✅ 自定义二进制协议
- ✅ JSON协议

### 计划支持
- 🔲 MAVLink
- 🔲 UDS (ISO 14229)
- 🔲 DBC (CAN Database)
- 🔲 J1939

## 依赖

- Qt 5.15+ 或 Qt 6.x
- C++17 编译器
- Qt SerialPort模块
- Qt Network模块

## 构建

使用 Qt Creator 打开 `.sln` 或 `.pro` 文件，或使用 CMake：

```bash
mkdir build
cd build
cmake ..
cmake --build .
```

## 许可证

[根据你的项目添加许可证信息]

## 贡献

欢迎提交 Issue 和 Pull Request！

## 联系方式

[添加你的联系方式]
