#pragma once

#include <QtWidgets/QWidget>
#include "ui_link_forge.h"
#include "application/main_controller.h"
#include <memory>

QT_BEGIN_NAMESPACE
namespace Ui { class link_forgeClass; }
QT_END_NAMESPACE

class link_forge : public QWidget {
    Q_OBJECT

public:
    explicit link_forge(QWidget* parent = nullptr);
    ~link_forge() override;

private slots:
    void OnConnectClicked();
    void OnDisconnectClicked();
    void OnSendClicked();
    void OnConnectionStateChanged(bool connected);
    void OnMessageReceived(const QString& message);
    void OnErrorOccurred(const QString& error);

private:
    void SetupUi();
    void SetupConnections();
    void UpdateConnectionState(bool connected);

    Ui::link_forgeClass*                                    ui;
    std::unique_ptr<LinkForge::Application::MainController> controller_;
};
