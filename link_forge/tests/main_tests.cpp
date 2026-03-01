/**
 * LinkForge Unit Test Runner
 *
 * Includes all test suites and executes them in sequence.
 * A non-zero exit code means at least one test failed.
 *
 * Build:
 *   qmake tests.pro && make
 *
 * Run all suites:
 *   ./link_forge_tests -v2
 *
 * Run one suite by class name:
 *   ./link_forge_tests TestFrameParser -v2
 *
 * Run one method:
 *   ./link_forge_tests TestFrameParser::TestSingleFrame -v2
 */

#include <QtTest/QtTest>
#include <QCoreApplication>

// ------ test suites ------
#include "tst_frame_parser.h"
#include "tst_frame_protocol.h"
#include "tst_factories.h"

// Helper: run a suite and accumulate the failure count
template<typename T>
static int RunSuite(int argc, char* argv[])
{
    T obj;
    return QTest::qExec(&obj, argc, argv);
}

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    int failures = 0;

    // ---- FrameSpec ----
    failures += RunSuite<TestFrameSpec>(argc, argv);

    // ---- FrameParser: checksums ----
    failures += RunSuite<TestFrameParserChecksums>(argc, argv);

    // ---- FrameParser: streaming ----
    failures += RunSuite<TestFrameParserStreaming>(argc, argv);

    // ---- FrameParser: named fields ----
    failures += RunSuite<TestFrameParserNamedFields>(argc, argv);

    // ---- FrameParser: invalid / edge cases ----
    failures += RunSuite<TestFrameParserInvalidFrames>(argc, argv);

    // ---- FrameProtocol: round-trip ----
    failures += RunSuite<TestFrameProtocolRoundTrip>(argc, argv);

    // ---- FrameProtocol: FromConfig ----
    failures += RunSuite<TestFrameProtocolFromConfig>(argc, argv);

    // ---- FrameProtocol: signals ----
    failures += RunSuite<TestFrameProtocolSignals>(argc, argv);

    // ---- FrameProtocol: validation ----
    failures += RunSuite<TestFrameProtocolValidation>(argc, argv);

    // ---- Factories ----
    failures += RunSuite<TestTransportFactory>(argc, argv);
    failures += RunSuite<TestProtocolFactory>(argc, argv);

    // ---- CommunicationManager ----
    failures += RunSuite<TestCommunicationManager>(argc, argv);

    // ---- ChannelManager ----
    failures += RunSuite<TestChannelManager>(argc, argv);

    // ---- IProtocol::Message ----
    failures += RunSuite<TestIProtocolMessage>(argc, argv);

    return failures;
}

// Include moc output from headers that define Q_OBJECT
#include "tst_frame_parser.moc"
#include "tst_frame_protocol.moc"
#include "tst_factories.moc"
