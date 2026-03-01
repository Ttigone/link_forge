# CMakeLists.txt for LinkForge Framework
cmake_minimum_required(VERSION 3.16)

project(LinkForge VERSION 1.0.0 LANGUAGES CXX)

# C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Qt 设置
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# 查找 Qt
find_package(Qt6 COMPONENTS Core Widgets Network SerialPort QUIET)
if (NOT Qt6_FOUND)
    find_package(Qt5 5.15 REQUIRED COMPONENTS Core Widgets Network SerialPort)
endif()

# 包含目录
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

# 源文件
set(CORE_SOURCES
    core/ITransport.h
    core/IProtocol.h
    core/TransportFactory.h
    core/TransportFactory.cpp
    core/ProtocolFactory.h
    core/ProtocolFactory.cpp
    core/CommunicationManager.h
    core/CommunicationManager.cpp
)

set(TRANSPORT_SOURCES
    transport/SerialTransport.h
    transport/SerialTransport.cpp
    transport/SocketTransport.h
    transport/SocketTransport.cpp
)

set(PROTOCOL_SOURCES
    protocol/ModbusProtocol.h
    protocol/ModbusProtocol.cpp
    protocol/CustomProtocol.h
    protocol/CustomProtocol.cpp
)

set(APPLICATION_SOURCES
    application/MainController.h
    application/MainController.cpp
    application/MessageModel.h
    application/MessageModel.cpp
)

set(UTILS_SOURCES
    utils/ConfigManager.h
    utils/ConfigManager.cpp
    utils/Logger.h
    utils/Logger.cpp
)

set(UI_SOURCES
    link_forge.h
    link_forge.cpp
    link_forge.ui
    main.cpp
)

set(RESOURCES
    link_forge.qrc
)

# 主可执行文件
add_executable(LinkForge
    ${CORE_SOURCES}
    ${TRANSPORT_SOURCES}
    ${PROTOCOL_SOURCES}
    ${APPLICATION_SOURCES}
    ${UTILS_SOURCES}
    ${UI_SOURCES}
    ${RESOURCES}
)

# 链接库
if (Qt6_FOUND)
    target_link_libraries(LinkForge PRIVATE
        Qt6::Core
        Qt6::Widgets
        Qt6::Network
        Qt6::SerialPort
    )
else()
    target_link_libraries(LinkForge PRIVATE
        Qt5::Core
        Qt5::Widgets
        Qt5::Network
        Qt5::SerialPort
    )
endif()

# 编译选项
if(MSVC)
    target_compile_options(LinkForge PRIVATE /W4 /WX-)
    target_compile_definitions(LinkForge PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(LinkForge PRIVATE -Wall -Wextra -Wpedantic)
endif()

# 安装规则
install(TARGETS LinkForge
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

# 可选：示例程序
option(BUILD_EXAMPLES "Build example programs" OFF)
if(BUILD_EXAMPLES)
    add_executable(ExampleUsage examples/ExampleUsage.cpp)
    target_link_libraries(ExampleUsage PRIVATE LinkForge)
endif()

# 可选：单元测试
option(BUILD_TESTS "Build unit tests" OFF)
if(BUILD_TESTS)
    enable_testing()
    find_package(GTest REQUIRED)
    
    add_executable(LinkForgeTests
        tests/TransportTests.cpp
        tests/ProtocolTests.cpp
        ${CORE_SOURCES}
        ${TRANSPORT_SOURCES}
        ${PROTOCOL_SOURCES}
    )
    
    target_link_libraries(LinkForgeTests PRIVATE
        GTest::GTest
        GTest::Main
        Qt6::Core
        Qt6::Network
        Qt6::SerialPort
    )
    
    add_test(NAME LinkForgeTests COMMAND LinkForgeTests)
endif()

# 输出信息
message(STATUS "LinkForge Configuration:")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Qt Version: ${Qt6_VERSION}${Qt5_VERSION}")
message(STATUS "  Build Examples: ${BUILD_EXAMPLES}")
message(STATUS "  Build Tests: ${BUILD_TESTS}")
