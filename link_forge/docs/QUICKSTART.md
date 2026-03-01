# LinkForge 快速入门指南

## 安装和构建

### 前提条件

1. **Qt 5.15+ 或 Qt 6.x**
   - 需要模块: Core, Widgets, Network, SerialPort
   
2. **C++17 编译器**
   - MSVC 2019+ (Windows)
   - GCC 7+ (Linux)
   - Clang 5+ (macOS)

### 构建步骤

#### 方法一：使用 Qt Creator

1. 打开 Qt Creator
2. 文件 → 打开文件或项目
3. 选择 `link_forge.sln` 或创建新的 `.pro` 文件
4. 配置构建套件
5. 点击"构建"

#### 方法二：使用 CMake

```bash
cd link_forge
mkdir build
cd build
cmake ..
cmake --build .
```

#### 方法三：使用命令行（Qt）

```bash
cd link_forge
qmake
make  # 或 nmake (Windows)
```

## 5分钟快速上手

### 示例1：串口通信

```cpp
#include "application/MainController.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    
    // 1. 创建控制器
    LinkForge::Application::MainController controller;
    
    // 2. 配置串口
    QVariantMap config;
    config["port"] = "COM3";          // Linux: "/dev/ttyUSB0"
    config["baudRate"] = 115200;
    config["dataBits"] = 8;
    config["parity"] = "None";
    config["stopBits"] = "1";
    
    // 3. 初始化（串口 + JSON协议）
    QVariantMap protocolConfig;
    protocolConfig["subType"] = "json";
    
    controller.initializeCommunication(
        "Serial",     // 传输类型
        "Custom",     // 协议类型
        config,       // 传输配置
        protocolConfig // 协议配置
    );
    
    // 4. 连接
    if (controller.connect()) {
        qInfo() << "连接成功！";
        
        // 5. 发送消息
        QVariantMap message;
        message["type"] = "hello";
        message["data"] = "Hello, LinkForge!";
        controller.sendMessage(message);
    }
    
    return app.exec();
}
```

### 示例2：Modbus RTU 读取寄存器

```cpp
// 配置串口
QVariantMap serialConfig;
serialConfig["port"] = "COM3";
serialConfig["baudRate"] = 9600;

// 配置Modbus RTU
QVariantMap modbusConfig;
modbusConfig["mode"] = "RTU";

// 初始化
controller.initializeCommunication("Serial", "Modbus", 
                                  serialConfig, modbusConfig);

// 连接
if (controller.connect()) {
    // 读取保持寄存器
    QVariantMap readMsg;
    readMsg["slaveId"] = 1;              // 从机地址
    readMsg["function"] = "readHoldingRegisters";
    readMsg["address"] = 0;              // 起始地址
    readMsg["quantity"] = 10;            // 读取数量
    
    controller.sendMessage(readMsg);
}
```

### 示例3：TCP 网络通信

```cpp
// 配置TCP
QVariantMap tcpConfig;
tcpConfig["host"] = "192.168.1.100";
tcpConfig["port"] = 8080;
tcpConfig["timeout"] = 3000;  // 3秒超时

// 配置自定义协议
QVariantMap customConfig;
customConfig["subType"] = "json";

// 初始化
controller.initializeCommunication("TCP", "Custom", 
                                  tcpConfig, customConfig);

// 连接并发送
if (controller.connect()) {
    QVariantMap msg;
    msg["command"] = "getData";
    msg["id"] = 123;
    controller.sendMessage(msg);
}
```

### 示例4：接收消息

```cpp
// 方法1：使用信号槽
QObject::connect(&controller, 
    &LinkForge::Application::MainController::messageReceived,
    [](const QString& messageType) {
        qInfo() << "收到消息类型:" << messageType;
    });

// 方法2：直接访问消息模型
auto* model = controller.messageModel();
int messageCount = model->rowCount();
qInfo() << "总消息数:" << messageCount;

// 方法3：在UI中显示
QTableView* view = new QTableView();
view->setModel(controller.messageModel());
view->show();
```

## 常见场景

### 场景1：PLC通信（Modbus TCP）

```cpp
// 连接到PLC
QVariantMap config;
config["host"] = "192.168.0.10";  // PLC IP
config["port"] = 502;              // Modbus TCP端口

QVariantMap modbusConfig;
modbusConfig["mode"] = "TCP";

controller.initializeCommunication("TCP", "Modbus", config, modbusConfig);
controller.connect();

// 读取输入寄存器
QVariantMap msg;
msg["slaveId"] = 1;
msg["function"] = "readInputRegisters";
msg["address"] = 0;
msg["quantity"] = 20;
controller.sendMessage(msg);

// 写入单个寄存器
QVariantMap writeMsg;
writeMsg["slaveId"] = 1;
writeMsg["function"] = "writeSingleRegister";
writeMsg["address"] = 100;
writeMsg["values"] = QVariantList{5000};
controller.sendMessage(writeMsg);
```

### 场景2：传感器数据采集（串口）

```cpp
// 配置串口
QVariantMap serialConfig;
serialConfig["port"] = "COM5";
serialConfig["baudRate"] = 115200;

QVariantMap customConfig;
customConfig["subType"] = "binary";

controller.initializeCommunication("Serial", "Custom", 
                                  serialConfig, customConfig);

// 连接信号
QObject::connect(&controller,
    &LinkForge::Application::MainController::messageReceived,
    [](const QString& type) {
        // 处理传感器数据
        qInfo() << "传感器数据:" << type;
    });

controller.connect();

// 发送采集命令
QVariantMap cmd;
cmd["data"] = QByteArray::fromHex("01030000000A");
controller.sendMessage(cmd);
```

### 场景3：多设备管理

```cpp
// 创建多个控制器管理不同设备
LinkForge::Application::MainController controller1;  // 设备1
LinkForge::Application::MainController controller2;  // 设备2

// 设备1：串口传感器
QVariantMap serial1;
serial1["port"] = "COM3";
serial1["baudRate"] = 9600;
controller1.initializeCommunication("Serial", "Modbus", serial1);

// 设备2：网络PLC
QVariantMap tcp2;
tcp2["host"] = "192.168.1.100";
tcp2["port"] = 502;
controller2.initializeCommunication("TCP", "Modbus", tcp2);

// 同时连接
controller1.connect();
controller2.connect();
```

## 配置文件使用

### 保存配置

```cpp
#include "utils/ConfigManager.h"

auto& config = LinkForge::Utils::ConfigManager::instance();

// 设置串口配置
QVariantMap serialConfig;
serialConfig["port"] = "COM3";
serialConfig["baudRate"] = 115200;
config.setTransportConfig("serial", serialConfig);

// 设置Modbus配置
QVariantMap modbusConfig;
modbusConfig["mode"] = "RTU";
modbusConfig["timeout"] = 1000;
config.setProtocolConfig("modbus", modbusConfig);

// 保存到文件
config.saveConfig("my_app_config.json");
```

### 加载配置

```cpp
auto& config = LinkForge::Utils::ConfigManager::instance();

// 从文件加载
if (config.loadConfig("my_app_config.json")) {
    // 使用已保存的配置
    QVariantMap serialConfig = config.getTransportConfig("serial");
    QVariantMap modbusConfig = config.getProtocolConfig("modbus");
    
    controller.initializeCommunication("Serial", "Modbus",
                                      serialConfig, modbusConfig);
}
```

## 日志记录

```cpp
#include "utils/Logger.h"

// 启用日志系统
LinkForge::Utils::Logger::install();

// 设置日志级别
auto& logger = LinkForge::Utils::Logger::instance();
logger.setLevel(LinkForge::Utils::Logger::Level::Debug);

// 启用文件日志
logger.enableFileLogging("linkforge.log");

// 使用日志
LOG_INFO("应用程序启动");
LOG_DEBUG("调试信息");
LOG_WARNING("警告信息");
LOG_ERROR("错误信息");
```

## 错误处理

```cpp
// 监听错误
QObject::connect(&controller,
    &LinkForge::Application::MainController::errorOccurred,
    [](const QString& error) {
        qWarning() << "错误发生:" << error;
        
        // 根据错误类型处理
        if (error.contains("Connection")) {
            // 处理连接错误
        } else if (error.contains("Protocol")) {
            // 处理协议错误
        }
    });

// 检查连接状态
if (!controller.isConnected()) {
    qWarning() << "未连接";
}

// 获取统计信息
QVariantMap stats = controller.statistics();
qInfo() << "发送字节数:" << stats["bytesSent"];
qInfo() << "接收字节数:" << stats["bytesReceived"];
```

## UI集成示例

```cpp
class MyWindow : public QWidget {
    Q_OBJECT
    
public:
    MyWindow() {
        // 创建UI控件
        connectBtn = new QPushButton("连接");
        sendBtn = new QPushButton("发送");
        messageView = new QTableView();
        
        // 设置消息模型
        messageView->setModel(controller.messageModel());
        
        // 连接信号
        connect(connectBtn, &QPushButton::clicked, [this]() {
            QVariantMap config = getConfigFromUI();
            controller.initializeCommunication("Serial", "Modbus", config);
            if (controller.connect()) {
                connectBtn->setEnabled(false);
                sendBtn->setEnabled(true);
            }
        });
        
        connect(sendBtn, &QPushButton::clicked, [this]() {
            QVariantMap msg = getMessageFromUI();
            controller.sendMessage(msg);
        });
        
        connect(&controller, 
            &LinkForge::Application::MainController::connectionStateChanged,
            [this](bool connected) {
                connectBtn->setEnabled(!connected);
                sendBtn->setEnabled(connected);
            });
    }
    
private:
    LinkForge::Application::MainController controller;
    QPushButton* connectBtn;
    QPushButton* sendBtn;
    QTableView* messageView;
};
```

## 下一步

- 查看 [README.md](README.md) 了解完整功能
- 阅读 [ARCHITECTURE.md](ARCHITECTURE.md) 理解框架设计
- 查看 [examples/ExampleUsage.cpp](examples/ExampleUsage.cpp) 获取更多示例
- 参考 API 文档（待生成）

## 获取帮助

- 查看日志文件了解详细错误信息
- 使用 Qt Creator 调试器
- 在 Issues 中提问

## 常见问题

**Q: 为什么连接失败？**
- 检查端口名称是否正确（Windows: COM3, Linux: /dev/ttyUSB0）
- 检查端口权限（Linux需要添加到dialout组）
- 检查波特率等参数是否匹配

**Q: 收不到数据？**
- 检查协议配置是否正确
- 使用日志查看原始数据
- 验证设备是否正常工作

**Q: 如何切换传输方式？**
- 调用 `disconnect()`
- 重新调用 `initializeCommunication()` 使用新参数
- 调用 `connect()`

开始你的LinkForge之旅吧！🚀
