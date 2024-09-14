#include <QMenu>
#include <QIcon>
#include <QAction>
#include <QStyleHints>
#include <QMessageBox>
#include <QApplication>
#include <QWidgetAction>

#include <glog/logging.h>

#include "gui/style/style.h"

#include "tray.h"


using namespace fptn::gui;


inline bool isDarkMode()
{
    const auto scheme = QGuiApplication::styleHints()->colorScheme();
    return scheme == Qt::ColorScheme::Dark;
}


inline bool isUbuntu()
{
    QString osName = QSysInfo::prettyProductName();
    return osName.contains("Ubuntu", Qt::CaseInsensitive);
}

inline bool isWindows()
{
    QString osName = QSysInfo::productType();
    return osName.contains("windows", Qt::CaseInsensitive);
}


TrayApp::TrayApp(QObject *parent)
    : QObject(parent),
        trayIcon_(new QSystemTrayIcon(this)),
        trayMenu_(new QMenu()),
        connectMenu_(new QMenu("Connect to    ", trayMenu_)),
        speedWidget_(new SpeedWidget()),
        updateTimer_(new QTimer(this))
{
#ifdef __linux__
    if (isDarkMode() || isUbuntu()) {
        activeIconPath_ = ":/icons/dark/active.ico";
        inactiveIconPath_ = ":/icons/dark/inactive.ico";
    } else {
        activeIconPath_ = ":/icons/white/active.ico";
        inactiveIconPath_ = ":/icons/white/inactive.ico";
    }
    qApp->setStyleSheet(fptn::gui::ubuntuStyleSheet);
#elif __APPLE__
    if (isDarkMode()) {
        LOG(INFO) << "Set dark mode";
        activeIconPath_ = ":/icons/dark/active.ico";
        inactiveIconPath_ = ":/icons/dark/inactive.ico";
        qApp->setStyleSheet(fptn::gui::darkStyleSheet);
    } else {
        LOG(INFO) << "Set white mode";
        activeIconPath_ = ":/icons/white/active.ico";
        inactiveIconPath_ = ":/icons/white/inactive.ico";
        qApp->setStyleSheet(fptn::gui::whiteStyleSheet);
    }
#elif _WIN32
    if (isDarkMode()) {
        LOG(INFO) << "Set dark mode";
        activeIconPath_ = ":/icons/dark/active.ico";
        inactiveIconPath_ = ":/icons/dark/inactive.ico";
    } else {
        LOG(INFO) << "Set white mode";
        activeIconPath_ = ":/icons/white/active.ico";
        inactiveIconPath_ = ":/icons/white/inactive.ico";
    }
    qApp->setStyleSheet(fptn::gui::windowsStyleSheet);
#else
    #error "Unsupported system!"
#endif


    LOG(INFO) << "activeIconPath: " << activeIconPath_.toStdString();
    LOG(INFO) << "inactiveIconPath: " << inactiveIconPath_.toStdString();

//    if (isUbuntu()) { // Ubuntu
//        activeIconPath_ = ":/icons/white/active.ico";
//        inactiveIconPath_ = ":/icons/white/inactive.ico";
        //qApp->setStyleSheet(fptn::gui::ubuntuStyleSheet);
//    } if (isWindows()) { // Windows
//        activeIconPath_ = ":/icons/dark/active.ico";
//        inactiveIconPath_ = ":/icons/dark/inactive.ico";
//        if (isDarkMode()) {
//            LOG(INFO) << "Set dark mode";
//            activeIconPath_ = ":/icons/dark/active.ico";
//            inactiveIconPath_ = ":/icons/dark/inactive.ico";
//        } else {
//            LOG(INFO) << "Set white mode";
//            activeIconPath_ = ":/icons/white/active.ico";
//            inactiveIconPath_ = ":/icons/white/inactive.ico";
//        }
//        qApp->setStyleSheet(fptn::gui::windowsStyleSheet);
//    } else { // MacOS
//        if (isDarkMode()) {
//            LOG(INFO) << "Set dark mode";
//            activeIconPath_ = ":/icons/dark/active.ico";
//            inactiveIconPath_ = ":/icons/dark/inactive.ico";
//            qApp->setStyleSheet(fptn::gui::darkStyleSheet);
//        } else {
//            LOG(INFO) << "Set white mode";
//            activeIconPath_ = ":/icons/white/active.ico";
//            inactiveIconPath_ = ":/icons/white/inactive.ico";
//            qApp->setStyleSheet(fptn::gui::whiteStyleSheet);
//        }
//    }

    #if defined(_WIN32)
        if (trayIcon_ && trayMenu_) {
            QObject::connect(trayIcon_, &QSystemTrayIcon::activated, [this](QSystemTrayIcon::ActivationReason reason) {
                if (trayMenu_->isVisible()) {
                    trayMenu_->close(); // Hide the menu if it's visible
                } else {
                    trayMenu_->show();
                    trayMenu_->exec(QCursor::pos()); // Show the menu if it's not visible
                }
            });
        }
    #endif

    connect(this, &TrayApp::defaultState, this, &TrayApp::handleDefaultState);
    connect(this, &TrayApp::connecting, this, &TrayApp::handleConnecting);
    connect(this, &TrayApp::connected, this, &TrayApp::handleConnected);
    connect(this, &TrayApp::disconnecting, this, &TrayApp::handleDisconnecting);

    connect(&serverModel_, &SettingsModel::dataChanged, this, &TrayApp::updateTrayMenu);
    connect(updateTimer_, &QTimer::timeout, this, &TrayApp::updateSpeedWidget);
    updateTimer_->start(1000);

    setUpTrayIcon();

    settingsAction_ = new QAction("Settings", this);
    connect(settingsAction_, &QAction::triggered, this, &TrayApp::onShowSettings);

    quitAction_ = new QAction("Quit", this);
    connect(quitAction_, &QAction::triggered, this, &TrayApp::handleQuit);

    trayMenu_->addSeparator();
    trayMenu_->addAction(settingsAction_);
    trayMenu_->addSeparator();
    trayMenu_->addAction(quitAction_);
    trayIcon_->setContextMenu(trayMenu_);

    updateTrayMenu();
}

void TrayApp::setUpTrayIcon()
{
    trayIcon_->show();
}

void TrayApp::updateTrayMenu()
{
    if (connectMenu_) {
        connectMenu_->clear();
    }
    if (trayMenu_ && connectMenu_) {
        trayMenu_->removeAction(connectMenu_->menuAction());
    }
    switch (connectionState_) {
        case ConnectionState::None: {
            trayIcon_->setIcon(QIcon(inactiveIconPath_));
            for (const auto &server : serverModel_.servers()) {
                QAction *serverAction = new QAction(QString("%1:%2").arg(server.address).arg(server.port), this);
                connect(serverAction, &QAction::triggered, [this, server]() {
                    onConnectToServer(server);
                });
                connectMenu_->addAction(serverAction);
            }
            trayMenu_->insertMenu(settingsAction_, connectMenu_);
            if (disconnectAction_) {
                disconnectAction_->setVisible(false);
            }
            if (speedWidgetAction_) {
                speedWidgetAction_->setVisible(false);
            }
            if (settingsAction_) {
                settingsAction_->setEnabled(true);
            }
            if (connectingAction_) {
                connectingAction_->setVisible(false);
            }
            if (speedWidget_) {
                speedWidget_->setVisible(false);
            }
            break;
        }
        case ConnectionState::Connecting: {
            trayIcon_->setIcon(QIcon(inactiveIconPath_));
            if (!connectingAction_) {
                connectingAction_ = new QAction("Connecting...", this);
                trayMenu_->insertAction(settingsAction_, connectingAction_);
            }
            if (disconnectAction_) {
                disconnectAction_->setVisible(false);
            }
            if (speedWidgetAction_) {
                speedWidgetAction_->setVisible(false);
            }
            if (settingsAction_) {
                settingsAction_->setEnabled(false);
            }
            break;
        }
        case ConnectionState::Connected: {
            trayIcon_->setIcon(QIcon(activeIconPath_));
            if (!disconnectAction_) {
                disconnectAction_ = new QAction(this);
                connect(disconnectAction_, &QAction::triggered, this, &TrayApp::onDisconnectFromServer);
                trayMenu_->insertAction(settingsAction_, disconnectAction_);
            }
            if (disconnectAction_) {
                disconnectAction_->setText(
                        QString("Disconnect: %1:%2").arg(selectedServer_.address).arg(selectedServer_.port));
                disconnectAction_->setVisible(true);
            }
            if (connectingAction_) {
                connectingAction_->setVisible(false);
            }
            if (!speedWidgetAction_) {
                speedWidgetAction_ = new QWidgetAction(this);
                speedWidgetAction_->setDefaultWidget(speedWidget_);
                trayMenu_->insertAction(settingsAction_, speedWidgetAction_);
            }
            if (speedWidget_) {
                speedWidget_->setVisible(true);
            }
            if (settingsAction_) {
                settingsAction_->setEnabled(false);
            }
            if (speedWidgetAction_) {
                speedWidgetAction_->setVisible(true);
            }
            break;
        }
        case ConnectionState::Disconnecting: {
            trayIcon_->setIcon(QIcon(inactiveIconPath_));
            if (disconnectAction_) {
                disconnectAction_->setVisible(false);
            }
            if (!connectingAction_) {
                connectingAction_ = new QAction("Disconnecting... ", this);
                trayMenu_->insertAction(settingsAction_, connectingAction_);
            } else {
                connectingAction_->setText("Disconnecting... ");
            }
            connectingAction_->setVisible(true);
            if (speedWidgetAction_) {
                speedWidgetAction_->setVisible(false);
            }
            if (settingsAction_) {
                settingsAction_->setEnabled(false);
            }
            break;
        }
    }
}

void TrayApp::onConnectToServer(const ServerConnectionInformation &server)
{
    selectedServer_ = server;
    connectionState_ = ConnectionState::Connecting;
    updateTrayMenu();
    emit connecting();
}

void TrayApp::onDisconnectFromServer()
{
    if (vpnClient_) {
        vpnClient_->stop();
        vpnClient_.reset();
    }
    if (ipTables_) {
        ipTables_->clean();
        ipTables_.reset();
    }
    connectionState_ = ConnectionState::None;
    updateTrayMenu();
}

void TrayApp::onShowSettings()
{
    if (!settingsWidget_) {
        settingsWidget_ = new SettingsWidget(&serverModel_, nullptr);
    }
    if (!settingsWidget_->isVisible()) {
        settingsWidget_->show();
    } else {
        settingsWidget_->raise();
        settingsWidget_->activateWindow();
    }
}

void TrayApp::handleDefaultState()
{
    if (vpnClient_) {
        vpnClient_->stop();
        vpnClient_.reset();
    }
    if (ipTables_) {
        ipTables_->clean();
        ipTables_.reset();
    }

    updateTrayMenu();
}

void TrayApp::handleConnecting()
{
    LOG(INFO) << "Handling connecting state";
    updateTrayMenu();
    trayIcon_->setIcon(QIcon(inactiveIconPath_));

    const std::string tunInterfaceAddress = "10.0.1.1";
    const std::string tunInterfaceName = "tun0";

    const std::string gatewayIP = (
            serverModel_.gatewayIp() == "auto"
            ? ""
            : serverModel_.gatewayIp().toStdString()
    );
    const std::string networkInterface = (
            serverModel_.networkInterface() == "auto"
            ? ""
            : serverModel_.networkInterface().toStdString()
    );

    auto webSocketClient = std::make_unique<fptn::http::WebSocketClient>(
            selectedServer_.address.toStdString(),
            selectedServer_.port,
            tunInterfaceAddress,
            true
    );

    bool loginStatus = webSocketClient->login(
            selectedServer_.username.toStdString(),
            selectedServer_.password.toStdString()
    );

    if (!loginStatus) {
        QWidget tempWidget;
        QMessageBox::critical(
                &tempWidget,
                "Connection Error",
                "Failed to connect to the server. Please check your credentials and try again."
        );
        connectionState_ = ConnectionState::None;
        updateTrayMenu();
        return;
    }

    ipTables_ = std::make_unique<fptn::system::IPTables>(
            networkInterface,
            tunInterfaceName,
            selectedServer_.address.toStdString(),
            gatewayIP,
            tunInterfaceAddress // FIX IT
    );
    auto virtualNetworkInterface = std::make_unique<fptn::common::network::TunInterface>(
            tunInterfaceName, pcpp::IPv4Address(tunInterfaceAddress), 30, nullptr
    );
    vpnClient_ = std::make_unique<fptn::vpn::VpnClient>(
            std::move(webSocketClient),
            std::move(virtualNetworkInterface)
    );
    vpnClient_->start();
    std::this_thread::sleep_for(std::chrono::seconds(2)); // FIX IT!
    ipTables_->apply();

    connectionState_ = ConnectionState::Connected;
    updateTrayMenu();
    emit connected();
}

void TrayApp::handleConnected()
{
    connectionState_ = ConnectionState::Connected;
    updateTrayMenu();
}

void TrayApp::handleDisconnecting()
{
    if (vpnClient_) {
        vpnClient_->stop();
        vpnClient_.reset();
    }
    if (ipTables_) {
        ipTables_->clean();
        ipTables_.reset();
    }
    connectionState_ = ConnectionState::None;
    updateTrayMenu();
    emit defaultState();
}

void TrayApp::updateSpeedWidget()
{
    if (vpnClient_ && speedWidget_ && connectionState_ == ConnectionState::Connected) {
        speedWidget_->updateSpeed(vpnClient_->getReceiveRate(), vpnClient_->getSendRate());
    }
}

void TrayApp::handleQuit()
{
    QApplication::quit();
}
