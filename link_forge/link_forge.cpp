#include "link_forge.h"
#include "utils/Logger.h"
#include "utils/ConfigManager.h"
#include <QMessageBox>

link_forge::link_forge(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::link_forgeClass())
    , controller_(std::make_unique<LinkForge::Application::MainController>(this))
{
    ui->setupUi(this);
    SetupUi();
    SetupConnections();

    // Initialise logging.
    LinkForge::Utils::Logger::Install();
    LinkForge::Utils::Logger::Instance().SetLevel(LinkForge::Utils::Logger::Level::Info);

    LOG_INFO("LinkForge application started");
}

link_forge::~link_forge()
{
    if (controller_) {
        controller_->Disconnect();
    }
    delete ui;
}

void link_forge::SetupUi()
{
    setWindowTitle("LinkForge - Universal Communication Framework");

    // TODO: Add widgets in the UI designer:
    //   - Transport type selector (Serial / TCP / UDP)
    //   - Protocol type selector (Modbus / Custom / JSON)
    //   - Configuration parameter inputs
    //   - Connect / Disconnect buttons
    //   - Send input + Send button
    //   - Message table (backed by MessageModel)
    //   - Status bar
}

void link_forge::SetupConnections()
{
    connect(controller_.get(), &LinkForge::Application::MainController::ConnectionStateChanged,
            this, &link_forge::OnConnectionStateChanged);
    connect(controller_.get(), &LinkForge::Application::MainController::MessageReceived,
            this, &link_forge::OnMessageReceived);
    connect(controller_.get(), &LinkForge::Application::MainController::ErrorOccurred,
            this, &link_forge::OnErrorOccurred);

    // TODO: connect UI button signals:
    // connect(ui->connectButton,    &QPushButton::clicked, this, &link_forge::OnConnectClicked);
    // connect(ui->disconnectButton, &QPushButton::clicked, this, &link_forge::OnDisconnectClicked);
    // connect(ui->sendButton,       &QPushButton::clicked, this, &link_forge::OnSendClicked);
}

void link_forge::OnConnectClicked()
{
    // TODO: read actual values from the UI widgets.
    const QString transport_type = QStringLiteral("Serial");
    const QString protocol_type  = QStringLiteral("Modbus");

    QVariantMap transport_config;
    transport_config["port"]     = QStringLiteral("COM3");
    transport_config["baudRate"] = 9600;

    QVariantMap protocol_config;
    protocol_config["mode"] = QStringLiteral("RTU");

    if (controller_->InitializeCommunication(transport_type, protocol_type,
                                              transport_config, protocol_config)) {
        controller_->Connect();  // non-blocking -- result via OnConnectionStateChanged
        LOG_INFO("Connection attempt started");
    } else {
        QMessageBox::warning(this, "Initialisation Error",
                             "Failed to initialise communication");
    }
}

void link_forge::OnDisconnectClicked()
{
    controller_->Disconnect();
    LOG_INFO("Disconnect requested");
}

void link_forge::OnSendClicked()
{
    // TODO: build payload from UI inputs.
    QVariantMap message;
    message["slaveId"]  = 1;
    message["function"] = QStringLiteral("readHoldingRegisters");
    message["address"]  = 0;
    message["quantity"] = 10;

        QMessageBox::warning(this, "Send Error", "Failed to queue message");
    }
}

void link_forge::OnConnectionStateChanged(bool connected)
{
    UpdateConnectionState(connected);
    LOG_INFO(connected ? "Connection established" : "Connection closed");
}

void link_forge::OnMessageReceived(const QString& message)
{
    LOG_INFO(QString("Message received: %1").arg(message));
    // Message is already appended to MessageModel; the view updates automatically.
}

void link_forge::OnErrorOccurred(const QString& error)
{
    LOG_ERROR(error);
    QMessageBox::warning(this, "Error", error);
}

void link_forge::UpdateConnectionState(bool connected)
{
    // TODO: toggle UI controls.
    // ui->disconnectButton->setEnabled(connected);
    // ui->sendButton->setEnabled(connected);
    Q_UNUSED(connected)
}
