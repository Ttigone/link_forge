/**
 * @file ExampleUsage.cpp
 * @brief LinkForge framework usage examples.
 *
 * Demonstrates various communication scenarios using the refactored API.
 * All public methods follow PascalCase; member variables end with _.
 */

#include "application/main_controller.h"
#include "utils/ConfigManager.h"
#include "utils/Logger.h"
#include <QApplication>
#include <QDebug>

using namespace LinkForge;

// ============================================================================
// Example 1: Serial Modbus RTU
// ============================================================================
void Example1_SerialModbus()
{
    qInfo() << "
=== Example 1: Serial Modbus RTU ===";

    Application::MainController controller;

    QVariantMap serial_config;
    serial_config["port"]        = "COM3";
    serial_config["baudRate"]    = 9600;
    serial_config["dataBits"]    = 8;
    serial_config["parity"]      = "None";
    serial_config["stopBits"]    = "1";
    serial_config["flowControl"] = "None";

    QVariantMap modbus_config;
    modbus_config["mode"] = "RTU";

                                             serial_config, modbus_config)) {
        qWarning() << "Failed to initialise communication";
        return;
    }

    QObject::connect(&controller, &Application::MainController::MessageReceived,
        [](const QString& msg_type) {
            qInfo() << "Received message type:" << msg_type;
        });

    QObject::connect(&controller, &Application::MainController::ErrorOccurred,
        [](const QString& error) {
            qWarning() << "Error:" << error;
        });

    // Non-blocking connect -- result arrives via ConnectionStateChanged.
    controller.Connect();

    // (In a real application you would wait for the ConnectionStateChanged signal
    //  before sending.  Here we send immediately as an illustration.)
    QVariantMap read_msg;
    read_msg["slaveId"]  = 1;
    read_msg["function"] = "readHoldingRegisters";
    read_msg["address"]  = 0;
    read_msg["quantity"] = 10;
    controller.SendMessage(read_msg);

    QVariantMap write_msg;
    write_msg["slaveId"]  = 1;
    write_msg["function"] = "writeSingleRegister";
    write_msg["address"]  = 100;
    write_msg["values"]   = QVariantList{1234};
    controller.SendMessage(write_msg);
}

// ============================================================================
// Example 2: TCP Modbus
// ============================================================================
void Example2_TcpModbus()
{
    qInfo() << "
=== Example 2: TCP Modbus ===";

    Application::MainController controller;

    QVariantMap tcp_config;
    tcp_config["host"]    = "192.168.1.100";
    tcp_config["port"]    = 502;
    tcp_config["timeout"] = 3000;

    QVariantMap modbus_config;
    modbus_config["mode"] = "TCP";

    if (controller.InitializeCommunication("TCP", "Modbus", tcp_config, modbus_config)) {
        controller.Connect();

        QVariantMap msg;
        msg["slaveId"]  = 1;
        msg["function"] = "readInputRegisters";
        msg["address"]  = 0;
        msg["quantity"] = 20;
        controller.SendMessage(msg);
    }
}

// ============================================================================
// Example 3: UDP JSON protocol
// ============================================================================
void Example3_UdpJson()
{
    qInfo() << "
=== Example 3: UDP JSON Protocol ===";

    Application::MainController controller;

    QVariantMap udp_config;
    udp_config["localHost"]  = "0.0.0.0";
    udp_config["localPort"]  = 5000;
    udp_config["remoteHost"] = "127.0.0.1";
    udp_config["remotePort"] = 5001;

    QVariantMap json_config;
    json_config["subType"] = "json";

    if (controller.InitializeCommunication("UDP", "Custom", udp_config, json_config)) {
        controller.Connect();

        QVariantMap msg;
        msg["type"]   = "command";
        msg["action"] = "start";
        msg["parameters"] = QVariantMap{
            {"speed", 100},
            {"mode",  "auto"}
        };
        controller.SendMessage(msg);
    }
}

// ============================================================================
// Example 4: Custom binary protocol
// ============================================================================
void Example4_CustomBinary()
{
    qInfo() << "
=== Example 4: Custom Binary Protocol ===";

    Application::MainController controller;

    QVariantMap serial_config;
    serial_config["port"]     = "COM4";
    serial_config["baudRate"] = 115200;

    QVariantMap custom_config;
    custom_config["subType"] = "binary";

    if (controller.InitializeCommunication("Serial", "Custom",
                                            serial_config, custom_config)) {
        controller.Connect();

        QVariantMap msg;
        msg["data"] = QByteArray::fromHex("0102030405");
        controller.SendMessage(msg);
    }
}

// ============================================================================
// Example 5: Config manager
// ============================================================================
void Example5_ConfigManager()
{
    qInfo() << "
=== Example 5: Config Manager ===";

    auto& config = Utils::ConfigManager::Instance();

    QVariantMap serial_config;
    serial_config["port"]     = "COM5";
    serial_config["baudRate"] = 115200;
    serial_config["dataBits"] = 8;
    config.SetTransportConfig("serial", serial_config);

    config.SaveConfig("my_config.json");
    config.LoadConfig("my_config.json");

    const QVariantMap loaded = config.GetTransportConfig("serial");
    qInfo() << "Loaded config:" << loaded;

    Application::MainController controller;
    controller.InitializeCommunication("Serial", "Modbus",
                                        loaded,
                                        config.GetProtocolConfig("modbus"));
}

// ============================================================================
// Example 6: Message model + view
// ============================================================================
void Example6_MessageModel()
{
    qInfo() << "
=== Example 6: Message Model ===";

    Application::MainController controller;

    auto* model = controller.GetMessageModel();
    model->SetMaxMessages(500);

    // Attach to a QTableView:
    // QTableView* view = new QTableView();
    // view->setModel(model);

    QVariantMap serial_config;
    serial_config["port"]     = "COM3";
    serial_config["baudRate"] = 9600;

    if (controller.InitializeCommunication("Serial", "Modbus", serial_config)) {
        controller.Connect();

        QVariantMap msg;
        msg["slaveId"]  = 1;
        msg["function"] = "readCoils";
        msg["address"]  = 0;
        msg["quantity"] = 16;
        controller.SendMessage(msg);
    }
}

// ============================================================================
// Example 7: Logger
// ============================================================================
void Example7_Logger()
{
    qInfo() << "
=== Example 7: Logger ===";

    Utils::Logger::Install();
    Utils::Logger::Instance().SetLevel(Utils::Logger::Level::Debug);
    Utils::Logger::Instance().EnableFileLogging("linkforge.log");

    LOG_INFO("Application started");
    LOG_DEBUG("Debug information");
    LOG_WARNING("This is a warning");
    LOG_ERROR("An error occurred");

    qInfo()    << "Qt info message";
    qWarning() << "Qt warning message";
}

// ============================================================================
// Example 8: Statistics
// ============================================================================
void Example8_Statistics()
{
    qInfo() << "
=== Example 8: Statistics ===";

    Application::MainController controller;

    QObject::connect(&controller, &Application::MainController::StatisticsUpdated,
        [](const QVariantMap& stats) {
            qInfo() << "Bytes sent:"         << stats["bytesSent"];
            qInfo() << "Bytes received:"     << stats["bytesReceived"];
            qInfo() << "Messages sent:"      << stats["messagesSent"];
            qInfo() << "Messages received:"  << stats["messagesReceived"];
        });

    const QVariantMap stats = controller.GetStatistics();
    qInfo() << "Current statistics:" << stats;
}

// ============================================================================
// Example 9: Error handling + reconnect pattern
// ============================================================================
void Example9_ErrorHandling()
{
    qInfo() << "
=== Example 9: Error Handling ===";

    Application::MainController controller;

    QObject::connect(&controller, &Application::MainController::ErrorOccurred,
        [](const QString& error) {
            qWarning() << "Error occurred:" << error;
        });

    QObject::connect(&controller, &Application::MainController::ConnectionStateChanged,
        [&controller](bool connected) {
            if (connected) {
                qInfo() << "Connection established";
            } else {
                qInfo() << "Connection lost -- consider reconnecting";
            }
        });

    QVariantMap config;
    config["port"] = "INVALID_PORT";

    if (controller.InitializeCommunication("Serial", "Modbus", config)) {
        controller.Connect(); // will fail, triggering ErrorOccurred
    }
}

// ============================================================================
// main
// ============================================================================
int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    Utils::Logger::Install();
    Utils::Logger::Instance().SetLevel(Utils::Logger::Level::Info);

    qInfo() << "LinkForge Framework - Usage Examples";
    qInfo() << "=====================================";

    // Uncomment the examples you want to run:
    // Example1_SerialModbus();
    // Example2_TcpModbus();
    // Example3_UdpJson();
    // Example4_CustomBinary();
    // Example5_ConfigManager();
    // Example6_MessageModel();
    // Example7_Logger();
    // Example8_Statistics();
    // Example9_ErrorHandling();

    return 0;
}
