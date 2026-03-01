QT += core testlib serialport serialbus network
CONFIG += console testcase c++17
CONFIG -= app_bundle

TARGET   = link_forge_tests
TEMPLATE = app

SOURCES += \
    test_link_forge.cpp \
    ../core/communication_manager.cpp \
    ../core/transport_factory.cpp \
    ../core/protocol_factory.cpp \
    ../core/channel_manager.cpp \
    ../core/plugin_manager.cpp \
    ../transport/serial_transport.cpp \
    ../transport/socket_transport.cpp \
    ../transport/can_transport.cpp \
    ../protocol/modbus_protocol.cpp \
    ../protocol/custom_protocol.cpp \
    ../protocol/frame_parser.cpp \
    ../protocol/frame_protocol.cpp \
    ../utils/Logger.cpp \
    ../utils/ConfigManager.cpp

HEADERS += \
    ../core/itransport.h \
    ../core/iprotocol.h \
    ../core/itransport_plugin.h \
    ../core/iprotocol_plugin.h \
    ../core/communication_manager.h \
    ../core/transport_factory.h \
    ../core/protocol_factory.h \
    ../core/channel_manager.h \
    ../core/plugin_manager.h \
    ../core/type_names.h \
    ../transport/serial_transport.h \
    ../transport/socket_transport.h \
    ../transport/can_transport.h \
    ../protocol/modbus_protocol.h \
    ../protocol/custom_protocol.h \
    ../protocol/frame_parser.h \
    ../protocol/frame_protocol.h \
    ../utils/Logger.h \
    ../utils/ConfigManager.h

INCLUDEPATH += ..
