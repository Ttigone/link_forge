#pragma once

#include <QtTest/QtTest>
#include <QSignalSpy>
#include <QVariantMap>
#include <memory>

#include "core/itransport.h"
#include "core/iprotocol.h"
#include "core/communication_manager.h"
#include "core/channel_manager.h"
#include "core/transport_factory.h"
#include "core/protocol_factory.h"
#include "protocol/frame_protocol.h"
#include "protocol/frame_parser.h"

using namespace LinkForge::Core;

// ----------------------------------------------------------------
//  MockTransport — in-process transport, no hardware required
// ----------------------------------------------------------------
class MockTransport : public ITransport
{
    Q_OBJECT
public:
    explicit MockTransport(QObject* parent = nullptr)
        : ITransport(parent) {}

    bool Connect(const QVariantMap& cfg) override
    {
        config_ = cfg;
        state_  = State::Connected;
        emit Connected();
        emit StateChanged(state_);
        return true;
    }

    void Disconnect() override
    {
        state_ = State::Disconnected;
        emit Disconnected();
        emit StateChanged(state_);
    }

    qint64 Send(const QByteArray& data) override
    {
        last_sent_ = data;
        ++sent_count_;
        return data.size();
    }

    State       GetState()  const override { return state_; }
    Type        GetType()   const override { return Type::Unknown; }
    QString     GetName()   const override { return "Mock"; }
    QVariantMap GetConfig() const override { return config_; }

    // ---- test helpers ----
    void SimulateReceive(const QByteArray& data) { emit DataReceived(data); }
    void SimulateError(const QString& msg)       { emit ErrorOccurred(msg); state_ = State::Error; emit StateChanged(state_); }

    QByteArray LastSent()  const { return last_sent_; }
    int        SentCount() const { return sent_count_; }
    void       ResetCounters()   { last_sent_.clear(); sent_count_ = 0; }

private:
    State       state_      {State::Disconnected};
    QVariantMap config_;
    QByteArray  last_sent_;
    int         sent_count_ {0};
};

// ================================================================
//  TestTransportFactory
// ================================================================
class TestTransportFactory : public QObject
{
    Q_OBJECT

private slots:

    // --- create by enum ---

    void TestCreateSerial()
    {
        auto t = TransportFactory::Create(ITransport::Type::Serial);
        QVERIFY(t != nullptr);
        QCOMPARE(t->GetType(), ITransport::Type::Serial);
    }

    void TestCreateTcp()
    {
        auto t = TransportFactory::Create(ITransport::Type::TcpSocket);
        QVERIFY(t != nullptr);
        QCOMPARE(t->GetType(), ITransport::Type::TcpSocket);
    }

    void TestCreateUdp()
    {
        auto t = TransportFactory::Create(ITransport::Type::UdpSocket);
        QVERIFY(t != nullptr);
        QCOMPARE(t->GetType(), ITransport::Type::UdpSocket);
    }

    void TestCreateCan()
    {
        auto t = TransportFactory::Create(ITransport::Type::Can);
        QVERIFY(t != nullptr);
        QCOMPARE(t->GetType(), ITransport::Type::Can);
    }

    // --- create by string ---

    void TestCreateByStringSerial()
    {
        auto t = TransportFactory::Create("Serial");
        QVERIFY(t != nullptr);
    }

    void TestCreateByStringTcp()
    {
        auto t = TransportFactory::Create("TCP");
        QVERIFY(t != nullptr);
    }

    void TestCreateByStringUdp()
    {
        auto t = TransportFactory::Create("UDP");
        QVERIFY(t != nullptr);
    }

    void TestCreateByStringCan()
    {
        auto t = TransportFactory::Create("CAN");
        QVERIFY(t != nullptr);
    }

    // --- unknown name must return nullptr, not crash ---

    void TestCreateUnknownString()
    {
        auto t = TransportFactory::Create("NoSuchTransport_xyz");
        QVERIFY(t == nullptr);
    }

    void TestCreateUnknownEnum()
    {
        auto t = TransportFactory::Create(ITransport::Type::Unknown);
        QVERIFY(t == nullptr);
    }

    // --- string-keyed plugin registration ---

    void TestRegisterAndCreateByStringKey()
    {
        TransportFactory::RegisterTransport("MockBus_test",
            [](QObject* p) -> ITransport::Ptr {
                return std::make_shared<MockTransport>(p);
            });

        auto t = TransportFactory::Create("MockBus_test");
        QVERIFY(t != nullptr);
        QCOMPARE(t->GetName(), QString("Mock"));
    }

    // Registering the same key twice is safe (second call wins or is ignored)
    void TestRegisterDuplicateKey()
    {
        int call_count = 0;
        TransportFactory::RegisterTransport("DupKey_transport",
            [&call_count](QObject* p) -> ITransport::Ptr {
                ++call_count;
                return std::make_shared<MockTransport>(p);
            });
        TransportFactory::RegisterTransport("DupKey_transport",
            [&call_count](QObject* p) -> ITransport::Ptr {
                ++call_count;
                return std::make_shared<MockTransport>(p);
            });

        auto t = TransportFactory::Create("DupKey_transport");
        QVERIFY(t != nullptr);
    }

    // --- AvailableTypes lists all built-ins ---

    void TestAvailableTypesContainsBuiltIns()
    {
        QStringList types = TransportFactory::AvailableTypes();
        QVERIFY(types.contains("Serial"));
        QVERIFY(types.contains("TCP"));
        QVERIFY(types.contains("UDP"));
        QVERIFY(types.contains("CAN"));
    }

    void TestAvailableTypesContainsRegisteredPlugin()
    {
        TransportFactory::RegisterTransport("AvailableTypesTest",
            [](QObject* p) -> ITransport::Ptr {
                return std::make_shared<MockTransport>(p);
            });
        QStringList types = TransportFactory::AvailableTypes();
        QVERIFY(types.contains("AvailableTypesTest"));
    }
};

// ================================================================
//  TestProtocolFactory
// ================================================================
class TestProtocolFactory : public QObject
{
    Q_OBJECT

private slots:

    void TestCreateModbusRTU()
    {
        auto p = ProtocolFactory::Create("Modbus", {{"mode","RTU"}});
        QVERIFY(p != nullptr);
        QCOMPARE(p->GetName(), QString("Modbus"));
    }

    void TestCreateModbusTCP()
    {
        auto p = ProtocolFactory::Create("Modbus", {{"mode","TCP"}});
        QVERIFY(p != nullptr);
    }

    void TestCreateCustomBinary()
    {
        auto p = ProtocolFactory::Create("Custom", {{"subType","binary"}});
        QVERIFY(p != nullptr);
    }

    void TestCreateCustomJson()
    {
        auto p = ProtocolFactory::Create("Custom", {{"subType","json"}});
        QVERIFY(p != nullptr);
    }

    void TestCreateFrame()
    {
        QVariantMap cfg;
        cfg["headerMagic"]       = "AA55";
        cfg["lengthFieldOffset"] = 4;
        cfg["lengthFieldSize"]   = 2;
        cfg["dataOffset"]        = 6;
        cfg["checksumAlgorithm"] = "crc16ibm";
        auto p = ProtocolFactory::Create("Frame", cfg);
        QVERIFY(p != nullptr);
        QCOMPARE(p->GetName(), QString("Frame"));
    }

    void TestCreateUnknown()
    {
        auto p = ProtocolFactory::Create("AlienProto_xyz");
        QVERIFY(p == nullptr);
    }

    void TestAvailableTypesBuiltIns()
    {
        QStringList types = ProtocolFactory::AvailableTypes();
        QVERIFY(types.contains("Modbus"));
        QVERIFY(types.contains("Custom"));
        QVERIFY(types.contains("Frame"));
    }

    // String-keyed registration
    void TestRegisterCustomProtocol()
    {
        ProtocolFactory::RegisterProtocol("EchoFrame_test",
            [](const QVariantMap& cfg, QObject* p) -> IProtocol::Ptr {
                QVariantMap m = cfg;
                if (!m.contains("headerMagic"))  m["headerMagic"]       = "AA55";
                if (!m.contains("dataOffset"))   m["dataOffset"]        = 2;
                if (!m.contains("checksumAlgorithm")) m["checksumAlgorithm"] = "none";
                return FrameProtocol::FromConfig(m, p);
            });

        auto p = ProtocolFactory::Create("EchoFrame_test");
        QVERIFY(p != nullptr);
    }

    void TestAvailableTypesAfterRegistration()
    {
        ProtocolFactory::RegisterProtocol("ProtoForAvailTest",
            [](const QVariantMap&, QObject* p) -> IProtocol::Ptr {
                return ProtocolFactory::Create("Custom", {}, p);
            });
        QVERIFY(ProtocolFactory::AvailableTypes().contains("ProtoForAvailTest"));
    }
};

// ================================================================
//  TestCommunicationManager
// ================================================================
class TestCommunicationManager : public QObject
{
    Q_OBJECT

private slots:

    // --- Initialize ---

    void TestInitializeSerial()
    {
        CommunicationManager mgr;
        bool ok = mgr.Initialize("Serial", "Custom",
                                 {{"port","COM1"}},
                                 {{"subType","binary"}});
        QVERIFY(ok);
        QCOMPARE(mgr.GetState(), CommunicationManager::State::Idle);
    }

    void TestInitializeTcp()
    {
        CommunicationManager mgr;
        bool ok = mgr.Initialize("TCP", "Modbus",
                                 {{"host","127.0.0.1"},{"port",502}},
                                 {{"mode","TCP"}});
        QVERIFY(ok);
    }

    void TestInitializeFrame()
    {
        CommunicationManager mgr;
        QVariantMap proto_cfg;
        proto_cfg["headerMagic"]       = "AA55";
        proto_cfg["lengthFieldOffset"] = 4;
        proto_cfg["lengthFieldSize"]   = 2;
        proto_cfg["dataOffset"]        = 6;
        proto_cfg["checksumAlgorithm"] = "crc16ibm";
        bool ok = mgr.Initialize("UDP", "Frame",
                                 {{"remoteHost","127.0.0.1"},{"remotePort",4000}},
                                 proto_cfg);
        QVERIFY(ok);
    }

    void TestInitializeBadTransportType()
    {
        CommunicationManager mgr;
        bool ok = mgr.Initialize("GhostBus_xyz", "Custom", {});
        QVERIFY(!ok);
    }

    void TestInitializeBadProtocolType()
    {
        CommunicationManager mgr;
        bool ok = mgr.Initialize("Serial", "AlienProto_xyz", {{"port","COM1"}});
        QVERIFY(!ok);
    }

    // Re-initialize tears down previous session and succeeds
    void TestReInitialize()
    {
        CommunicationManager mgr;
        QVERIFY(mgr.Initialize("Serial", "Custom",
                               {{"port","COM1"}}, {}));
        QVERIFY(mgr.Initialize("TCP", "Modbus",
                               {{"host","127.0.0.1"},{"port",502}},
                               {{"mode","TCP"}}));
        QCOMPARE(mgr.GetState(), CommunicationManager::State::Idle);
    }

    // --- State guards ---

    void TestIsConnectedInitiallyFalse()
    {
        CommunicationManager mgr;
        mgr.Initialize("Serial", "Custom", {{"port","COM1"}});
        QVERIFY(!mgr.IsConnected());
    }

    void TestGetStateInitiallyIdle()
    {
        CommunicationManager mgr;
        mgr.Initialize("Serial", "Custom", {{"port","COM1"}});
        QCOMPARE(mgr.GetState(), CommunicationManager::State::Idle);
    }

    // --- Send guards (must fail gracefully before Connect) ---

    void TestSendMessageWhenNotConnected()
    {
        CommunicationManager mgr;
        mgr.Initialize("Serial", "Custom", {{"port","COM1"}});
        bool ok = mgr.SendMessage({{"data","test"}});
        QVERIFY(!ok);
    }

    void TestSendRawDataWhenNotConnected()
    {
        CommunicationManager mgr;
        mgr.Initialize("Serial", "Custom", {{"port","COM1"}});
        bool ok = mgr.SendRawData(QByteArray("raw"));
        QVERIFY(!ok);
    }

    // SendMessage before Initialize must also fail cleanly
    void TestSendMessageBeforeInitialize()
    {
        CommunicationManager mgr;
        bool ok = mgr.SendMessage({{"data","test"}});
        QVERIFY(!ok);
    }

    // --- Reset ---

    void TestResetAfterInitialize()
    {
        CommunicationManager mgr;
        mgr.Initialize("Serial", "Custom", {{"port","COM1"}});
        mgr.Reset();
        QCOMPARE(mgr.GetState(), CommunicationManager::State::Idle);
    }

    void TestResetWithoutInitializeIsHarmless()
    {
        CommunicationManager mgr;
        mgr.Reset();  // must not crash
        QCOMPARE(mgr.GetState(), CommunicationManager::State::Idle);
    }

    // --- Pointer accessors ---

    void TestGetTransportNullBeforeInit()
    {
        CommunicationManager mgr;
        QVERIFY(mgr.GetTransport() == nullptr);
    }

    void TestGetProtocolNullBeforeInit()
    {
        CommunicationManager mgr;
        QVERIFY(mgr.GetProtocol() == nullptr);
    }

    void TestGetTransportNonNullAfterInit()
    {
        CommunicationManager mgr;
        mgr.Initialize("Serial", "Custom", {{"port","COM1"}});
        QVERIFY(mgr.GetTransport() != nullptr);
    }

    void TestGetProtocolNonNullAfterInit()
    {
        CommunicationManager mgr;
        mgr.Initialize("Serial", "Custom", {{"port","COM1"}});
        QVERIFY(mgr.GetProtocol() != nullptr);
    }
};

// ================================================================
//  TestChannelManager
// ================================================================
class TestChannelManager : public QObject
{
    Q_OBJECT

private slots:

    // --- AddChannel ---

    void TestAddChannelReturnsTrue()
    {
        ChannelManager ch;
        bool ok = ch.AddChannel("ch1", "Serial", "Custom", {{"port","COM1"}});
        QVERIFY(ok);
    }

    void TestAddChannelAppearsInIds()
    {
        ChannelManager ch;
        ch.AddChannel("ch1", "Serial", "Custom", {{"port","COM1"}});
        QVERIFY(ch.ChannelIds().contains("ch1"));
    }

    void TestAddChannelHasChannel()
    {
        ChannelManager ch;
        ch.AddChannel("ch1", "Serial", "Custom", {{"port","COM1"}});
        QVERIFY(ch.HasChannel("ch1"));
    }

    void TestAddChannelIncrementsCount()
    {
        ChannelManager ch;
        ch.AddChannel("a", "Serial", "Custom", {{"port","COM1"}});
        ch.AddChannel("b", "TCP",    "Custom", {{"host","127.0.0.1"},{"port",502}});
        QCOMPARE(ch.ChannelCount(), 2);
    }

    void TestAddChannelWithBadTransportFails()
    {
        ChannelManager ch;
        bool ok = ch.AddChannel("bad", "GhostBus_xyz", "Custom", {});
        QVERIFY(!ok);
        QCOMPARE(ch.ChannelCount(), 0);
    }

    void TestAddChannelWithBadProtocolFails()
    {
        ChannelManager ch;
        bool ok = ch.AddChannel("bad", "Serial", "AlienProto_xyz", {{"port","COM1"}});
        QVERIFY(!ok);
    }

    void TestAddDuplicateChannelReturnsFalse()
    {
        ChannelManager ch;
        ch.AddChannel("ch1", "Serial", "Custom", {{"port","COM1"}});
        bool ok = ch.AddChannel("ch1", "TCP",    "Custom",
                                {{"host","127.0.0.1"},{"port",502}});
        QVERIFY(!ok);
        QCOMPARE(ch.ChannelCount(), 1);
    }

    // --- RemoveChannel ---

    void TestRemoveChannelDecreasesCount()
    {
        ChannelManager ch;
        ch.AddChannel("ch1", "Serial", "Custom", {{"port","COM1"}});
        ch.RemoveChannel("ch1");
        QCOMPARE(ch.ChannelCount(), 0);
    }

    void TestRemoveChannelHasChannelReturnsFalse()
    {
        ChannelManager ch;
        ch.AddChannel("ch1", "Serial", "Custom", {{"port","COM1"}});
        ch.RemoveChannel("ch1");
        QVERIFY(!ch.HasChannel("ch1"));
    }

    void TestRemoveChannelGetChannelReturnsNull()
    {
        ChannelManager ch;
        ch.AddChannel("ch1", "Serial", "Custom", {{"port","COM1"}});
        ch.RemoveChannel("ch1");
        QVERIFY(ch.GetChannel("ch1") == nullptr);
    }

    void TestRemoveUnknownChannelIsHarmless()
    {
        ChannelManager ch;
        ch.RemoveChannel("does_not_exist");  // must not crash
        QCOMPARE(ch.ChannelCount(), 0);
    }

    // --- RemoveAllChannels ---

    void TestRemoveAllChannels()
    {
        ChannelManager ch;
        ch.AddChannel("a", "Serial", "Custom", {{"port","COM1"}});
        ch.AddChannel("b", "TCP",    "Custom", {{"host","127.0.0.1"},{"port",502}});
        ch.AddChannel("c", "UDP",    "Custom", {{"remoteHost","127.0.0.1"},{"remotePort",5000}});
        ch.RemoveAllChannels();
        QCOMPARE(ch.ChannelCount(), 0);
        QVERIFY(ch.ChannelIds().isEmpty());
    }

    // --- GetChannel ---

    void TestGetChannelReturnsNonNull()
    {
        ChannelManager ch;
        ch.AddChannel("ch1", "Serial", "Custom", {{"port","COM1"}});
        QVERIFY(ch.GetChannel("ch1") != nullptr);
    }

    void TestGetChannelUnknownReturnsNull()
    {
        ChannelManager ch;
        QVERIFY(ch.GetChannel("nobody") == nullptr);
    }

    // --- UpdateChannel ---

    void TestUpdateChannelReturnsTrueForExistingId()
    {
        ChannelManager ch;
        ch.AddChannel("ch1", "Serial", "Custom", {{"port","COM1"}});
        bool ok = ch.UpdateChannel("ch1", "TCP", "Custom",
                                   {{"host","127.0.0.1"},{"port",502}});
        QVERIFY(ok);
        QCOMPARE(ch.ChannelCount(), 1);
    }

    void TestUpdateChannelReturnsFalseForUnknownId()
    {
        ChannelManager ch;
        bool ok = ch.UpdateChannel("nobody", "TCP", "Custom",
                                   {{"host","127.0.0.1"},{"port",502}});
        QVERIFY(!ok);
    }

    void TestUpdateChannelPreservesChannelId()
    {
        ChannelManager ch;
        ch.AddChannel("keep_id", "Serial", "Custom", {{"port","COM1"}});
        ch.UpdateChannel("keep_id", "TCP", "Custom",
                         {{"host","127.0.0.1"},{"port",502}});
        QVERIFY(ch.HasChannel("keep_id"));
    }

    // --- Signals ---

    void TestChannelAddedSignal()
    {
        ChannelManager ch;
        QSignalSpy spy(&ch, &ChannelManager::ChannelAdded);
        ch.AddChannel("sig1", "Serial", "Custom", {{"port","COM1"}});
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toString(), QString("sig1"));
    }

    void TestChannelRemovedSignal()
    {
        ChannelManager ch;
        ch.AddChannel("sig1", "Serial", "Custom", {{"port","COM1"}});
        QSignalSpy spy(&ch, &ChannelManager::ChannelRemoved);
        ch.RemoveChannel("sig1");
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toString(), QString("sig1"));
    }

    void TestChannelAddedSignalNotFiredOnDuplicate()
    {
        ChannelManager ch;
        ch.AddChannel("dup", "Serial", "Custom", {{"port","COM1"}});
        QSignalSpy spy(&ch, &ChannelManager::ChannelAdded);
        ch.AddChannel("dup", "TCP", "Custom",
                      {{"host","127.0.0.1"},{"port",502}});
        QCOMPARE(spy.count(), 0);
    }
};

// ================================================================
//  TestIProtocolMessage
// ================================================================
class TestIProtocolMessage : public QObject
{
    Q_OBJECT

private slots:

    void TestDefaultValues()
    {
        IProtocol::Message m;
        QVERIFY(!m.is_valid);
        QCOMPARE(m.timestamp_ms, qint64(0));
        QVERIFY(m.payload.isEmpty());
        QVERIFY(m.raw_data.isEmpty());
        QVERIFY(m.message_type.isEmpty());
    }

    void TestCopyIsValueSemantics()
    {
        IProtocol::Message a;
        a.is_valid     = true;
        a.timestamp_ms = 99999;
        a.message_type = "T";
        a.raw_data     = QByteArray("raw");
        a.payload["k"] = "v";

        IProtocol::Message b = a;
        QCOMPARE(b.is_valid,     true);
        QCOMPARE(b.timestamp_ms, qint64(99999));
        QCOMPARE(b.message_type, QString("T"));
        QCOMPARE(b.raw_data,     QByteArray("raw"));
        QCOMPARE(b.payload,      a.payload);

        // Mutating b does not affect a
        b.is_valid = false;
        QVERIFY(a.is_valid);
    }

    void TestMoveSemantics()
    {
        IProtocol::Message a;
        a.raw_data = QByteArray(1024, 0x42);

        IProtocol::Message b = std::move(a);
        QCOMPARE(b.raw_data.size(), 1024);
    }
};
