/*
 * Abc80WebProtocolTest.cxx
 *
 * The client/server wire protocol (plan Section 2.4.6): parsing of untrusted client messages,
 * telemetry JSON, and the latest-frame-wins outbox. RapidCheck properties cover arbitrary input.
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include <abc80/Abc80Types.hxx>
#include <abc80/FrameOutbox.hxx>
#include <abc80/WebProtocol.hxx>
#include <cstdint>
#include <string>
#include <thread>
#include <vector>
#include <nlohmann/json.hpp>
#include <rapidcheck.h>
#include <CppUTest/TestHarness.h>

using namespace abc80;
using json = nlohmann::json;

TEST_GROUP(Abc80WebProtocol)
{
};

TEST(Abc80WebProtocol, KeyNamesMapToMonitorKeyCodes)
{
    const struct { const char *name; uint8_t code; } table[] = {
        { "0", 0x00 }, { "9", 0x09 }, { "A", 0x0A }, { "F", 0x0F }, { "a", 0x0A }, { "f", 0x0F },
        { "+", KEY_PLUS }, { "-", KEY_MINUS }, { "GO", KEY_RUN }, { "DATA", KEY_DATA },
        { "ADRS", KEY_ADRS }, { "TO TAPE", KEY_STEP }, { "FROM TAPE", KEY_BP },
    };
    for (const auto &e : table) {
        uint8_t code = 0xEE;
        CHECK_TEXT(keyCodeFromName(e.name, code), e.name);
        BYTES_EQUAL(e.code, code);
    }
    // Every hex digit maps to its value, and the characters either side of each range are refused.
    for (int v = 0; v < 16; ++v) {
        const char upper = "0123456789ABCDEF"[v];
        const char lower = "0123456789abcdef"[v];
        uint8_t c1 = 0xEE, c2 = 0xEE;
        CHECK(keyCodeFromName(std::string(1, upper), c1));
        CHECK(keyCodeFromName(std::string(1, lower), c2));
        BYTES_EQUAL(v, c1);
        BYTES_EQUAL(v, c2);
    }
    for (char bad : { '/', ':', '@', 'G', '`', 'g', '*', ',', '.', ' ', '!' }) {
        uint8_t c = 0xEE;
        CHECK_TEXT(!keyCodeFromName(std::string(1, bad), c), std::string(1, bad).c_str());
        BYTES_EQUAL(0xEE, c);
    }
    uint8_t code = 0xEE;
    CHECK(!keyCodeFromName("RST", code));   // reset is not a matrix key
    CHECK(!keyCodeFromName("G", code));
    CHECK(!keyCodeFromName("", code));
    CHECK(!keyCodeFromName("10", code));
    BYTES_EQUAL(0xEE, code);                // untouched on failure
}

TEST(Abc80WebProtocol, TokenLengthAndCharacterSetBoundaries)
{
    CHECK(!validToken(std::string(7, 'a')));
    CHECK(validToken(std::string(8, 'a')));
    CHECK(validToken(std::string(64, 'a')));
    CHECK(!validToken(std::string(65, 'a')));
    CHECK(validToken("AZaz09_-"));  // the first and last of every allowed range
    for (char bad : { '@', '[', '`', '{', '/', ':', ' ', '.', '"', '\\' }) {
        std::string t = "abcdefg";
        t.push_back(bad);
        CHECK_TEXT(!validToken(t), std::string(1, bad).c_str());
    }
}

TEST(Abc80WebProtocol, ParsesEachValidMessageType)
{
    ClientMessage m = parseClientMessage(R"({"type":"hello","token":"abcDEF12-_x","rom":"1993"})");
    CHECK(m.type == ClientMessage::Type::Hello);
    STRCMP_EQUAL("abcDEF12-_x", m.token.c_str());
    CHECK(m.hasRom && m.rom == RomId::Vintage1993);

    m = parseClientMessage(R"({"type":"hello","token":"abcdef12"})");
    CHECK(m.type == ClientMessage::Type::Hello);
    CHECK(!m.hasRom);

    m = parseClientMessage(R"({"type":"setRom","rom":"monitor"})");
    CHECK(m.type == ClientMessage::Type::SetRom);
    CHECK(m.rom == RomId::Monitor);

    m = parseClientMessage(R"({"type":"key","key":"ADRS","action":"down","seq":7})");
    CHECK(m.type == ClientMessage::Type::Key);
    BYTES_EQUAL(KEY_ADRS, m.keyCode);
    CHECK(m.down);
    LONGS_EQUAL(7, static_cast<long>(m.seq));

    m = parseClientMessage(R"({"type":"key","key":"5","action":"up"})");
    CHECK(m.type == ClientMessage::Type::Key);
    CHECK(!m.down);

    m = parseClientMessage(R"({"type":"reset"})");
    CHECK(m.type == ClientMessage::Type::Reset);

    m = parseClientMessage(R"({"type":"run"})");
    CHECK(m.type == ClientMessage::Type::Run);

    m = parseClientMessage(R"({"type":"pause"})");
    CHECK(m.type == ClientMessage::Type::Pause);

    m = parseClientMessage(R"({"type":"step"})");
    CHECK(m.type == ClientMessage::Type::Step);

    m = parseClientMessage(R"({"type":"readMem","addr":4096,"count":128})");
    CHECK(m.type == ClientMessage::Type::ReadMem);
    LONGS_EQUAL(4096, m.memAddr);
    LONGS_EQUAL(128, m.memCount);

    m = parseClientMessage(R"({"type":"readDisasm","addr":112,"count":16})");
    CHECK(m.type == ClientMessage::Type::ReadDisasm);
    LONGS_EQUAL(112, m.memAddr);
    LONGS_EQUAL(16, m.memCount);

    // Clamping
    m = parseClientMessage(R"({"type":"readMem","addr":0,"count":1000})");
    CHECK(m.type == ClientMessage::Type::ReadMem);
    LONGS_EQUAL(256, m.memCount);

    m = parseClientMessage(R"({"type":"readDisasm","addr":0,"count":1000})");
    CHECK(m.type == ClientMessage::Type::ReadDisasm);
    LONGS_EQUAL(64, m.memCount);
}

TEST(Abc80WebProtocol, RejectsEverythingElseWithAReason)
{
    const char *bad[] = {
        "", "not json", "[]", "42", "null", "{}", R"({"type":7})", R"({"type":"fly"})",
        R"({"type":"hello"})", R"({"type":"hello","token":"short"})", R"({"type":"hello","token":"has space 12"})",
        R"({"type":"hello","token":"abcdef12","rom":"2600"})", R"({"type":"hello","token":12345678})",
        R"({"type":"setRom"})", R"({"type":"setRom","rom":"nope"})",
        R"({"type":"key","key":"RST","action":"down"})", R"({"type":"key","key":"1","action":"sideways"})",
        R"({"type":"key","action":"down"})", R"({"type":"key","key":"1"})", R"({"type":"key","key":1,"action":"down"})",
        R"({"type":"key","key":"1","action":"down","seq":-1})", R"({"type":"key","key":"1","action":"down","seq":"x"})",
        R"({"type":"readMem"})", R"({"type":"readMem","addr":"bad","count":16})",
        R"({"type":"readMem","addr":70000,"count":16})", R"({"type":"readMem","addr":0,"count":0})",
        R"({"type":"readDisasm"})", R"({"type":"readDisasm","addr":0,"count":-1})",
    };
    for (const char *text : bad) {
        const ClientMessage m = parseClientMessage(text);
        CHECK_TEXT(m.type == ClientMessage::Type::Invalid, text);
        CHECK_TEXT(!m.error.empty(), text);
    }
}

TEST(Abc80WebProtocol, RejectsAMessageOverTheSizeLimitEvenIfItIsValidJson)
{
    std::string big = R"({"type":"key","key":"1","action":"down","pad":")";
    big.append(kMaxClientMessageBytes, 'x');
    big += "\"}";
    CHECK(big.size() > kMaxClientMessageBytes);
    const ClientMessage m = parseClientMessage(big);
    CHECK(m.type == ClientMessage::Type::Invalid);
    CHECK(m.error.find("too large") != std::string::npos);

    std::string exact = R"({"type":"reset","pad":")";
    exact.append(kMaxClientMessageBytes - exact.size() - 2, 'x');
    exact += "\"}";
    LONGS_EQUAL(static_cast<long>(kMaxClientMessageBytes), static_cast<long>(exact.size()));
    CHECK(parseClientMessage(exact).type == ClientMessage::Type::Reset);  // at the limit is fine
}

TEST(Abc80WebProtocol, TelemetryJsonCarriesEveryField)
{
    TelemetryFrame f;
    f.seq = 42;
    f.rom = "monitor";
    f.cycles = 123456789ULL;
    f.paused = true;
    f.displayMasks = { 0x3F, 0x06, 0x5B, 0x4F, 0x66, 0xFF };
    f.displayDigits = { '0', '1', '2', '3', '4', '?' };
    f.ep = true;
    f.halt = false;
    f.speakerLed = 0.25;
    f.speakerLevel = true;
    f.speakerEdges = { 10, 20, 30 };
    f.speakerOverflow = false;
    f.pc = 0x04CD;
    f.sp = 0x17BF;
    f.stackBaseAddr = 0x17BD;
    f.stackBytes = { 0x78, 0x00, 0x34, 0x12 };
    f.af = 0x1234;
    f.bc = 0x5678;
    f.de = 0x9ABC;
    f.hl = 0xDEF0;
    const json j = json::parse(buildTelemetryJson(f));
    STRCMP_EQUAL("telemetry", j.at("type").get<std::string>().c_str());
    LONGS_EQUAL(42, j.at("seq").get<long>());
    STRCMP_EQUAL("monitor", j.at("rom").get<std::string>().c_str());
    CHECK(j.at("cycles").get<uint64_t>() == 123456789ULL);
    CHECK(j.at("paused").get<bool>());
    LONGS_EQUAL(6, static_cast<long>(j.at("displayMasks").size()));
    LONGS_EQUAL(0x3F, j.at("displayMasks")[0].get<int>());
    LONGS_EQUAL(0xFF, j.at("displayMasks")[5].get<int>());
    LONGS_EQUAL(6, static_cast<long>(j.at("displayDigits").size()));
    STRCMP_EQUAL("0", j.at("displayDigits")[0].get<std::string>().c_str());
    STRCMP_EQUAL("?", j.at("displayDigits")[5].get<std::string>().c_str());
    CHECK(j.at("leds").at("ep").get<bool>());
    CHECK(!j.at("leds").at("halt").get<bool>());
    DOUBLES_EQUAL(0.25, j.at("leds").at("speaker").get<double>(), 1e-12);
    CHECK(j.at("speakerLevel").get<bool>());
    LONGS_EQUAL(3, static_cast<long>(j.at("speakerEdges").size()));
    LONGS_EQUAL(30, j.at("speakerEdges")[2].get<int>());
    CHECK(!j.at("speakerOverflow").get<bool>());
    LONGS_EQUAL(0x04CD, j.at("registers").at("pc").get<int>());
    LONGS_EQUAL(0x17BF, j.at("registers").at("sp").get<int>());
    LONGS_EQUAL(0xDEF0, j.at("registers").at("hl").get<int>());
    LONGS_EQUAL(0x17BD, j.at("stackBaseAddr").get<long>());
    LONGS_EQUAL(4, static_cast<long>(j.at("stackBytes").size()));
    BYTES_EQUAL(0x78, j.at("stackBytes")[0].get<int>());
}

TEST(Abc80WebProtocol, BuildMemDataAndDisasmJson)
{
    // memData
    std::vector<uint8_t> data = { 0x06, 0x00, 0x10, 0xFE };
    std::string memJson = buildMemDataJson(0x1000, data);
    json jm = json::parse(memJson);
    STRCMP_EQUAL("memData", jm.at("type").get<std::string>().c_str());
    LONGS_EQUAL(0x1000, jm.at("addr").get<long>());
    LONGS_EQUAL(4, static_cast<long>(jm.at("data").size()));
    BYTES_EQUAL(0x06, jm.at("data")[0].get<int>());

    // disasm
    std::vector<DisasmLine> lines = {
        { 0x0000, { 0x06, 0x00 }, "LD B,00H" },
        { 0x0002, { 0x10, 0xFE }, "DJNZ 0002H" },
    };
    std::string disasmJson = buildDisasmJson(0x0000, lines);
    json jd = json::parse(disasmJson);
    STRCMP_EQUAL("disasm", jd.at("type").get<std::string>().c_str());
    LONGS_EQUAL(0x0000, jd.at("addr").get<long>());
    LONGS_EQUAL(2, static_cast<long>(jd.at("lines").size()));
    LONGS_EQUAL(0x0000, jd.at("lines")[0].at("addr").get<long>());
    STRCMP_EQUAL("LD B,00H", jd.at("lines")[0].at("mnemonic").get<std::string>().c_str());
    LONGS_EQUAL(2, static_cast<long>(jd.at("lines")[0].at("bytes").size()));
}

TEST(Abc80WebProtocol, ErrorJsonEscapesTheReason)
{
    const json j = json::parse(buildErrorJson("bad \"quote\" and \\ slash\n"));
    STRCMP_EQUAL("error", j.at("type").get<std::string>().c_str());
    STRCMP_EQUAL("bad \"quote\" and \\ slash\n", j.at("reason").get<std::string>().c_str());
}

// ---- Properties ----

// Arbitrary bytes never crash the parser, and anything accepted has in-range fields.
TEST(Abc80WebProtocol, PropertyArbitraryInputIsRejectedOrInRange)
{
    const bool ok = rc::check("arbitrary bytes", [](const std::string &text) {
        const ClientMessage m = parseClientMessage(text);
        if (m.type == ClientMessage::Type::Invalid) {
            RC_ASSERT(!m.error.empty());
        } else {
            RC_ASSERT(text.size() <= kMaxClientMessageBytes);
            if (m.type == ClientMessage::Type::Key) RC_ASSERT(m.keyCode <= KEY_BP);
            if (m.type == ClientMessage::Type::Hello) {
                RC_ASSERT(m.token.size() >= 8 && m.token.size() <= 64);
            }
        }
    });
    CHECK(ok);
}

// Every prefix and every single-byte corruption of a valid message is handled without a crash.
TEST(Abc80WebProtocol, PropertyTruncationsAndCorruptionsAreHandled)
{
    const std::string valid = R"({"type":"key","key":"ADRS","action":"down","seq":12})";
    const bool ok = rc::check("truncate and corrupt", [&](uint8_t cut, uint8_t pos, uint8_t byte) {
        const std::string t = valid.substr(0, cut % (valid.size() + 1));
        const ClientMessage a = parseClientMessage(t);
        RC_ASSERT(a.type == ClientMessage::Type::Invalid || t == valid);
        std::string c = valid;
        c[pos % c.size()] = static_cast<char>(byte);
        const ClientMessage b = parseClientMessage(c);
        if (b.type == ClientMessage::Type::Key) RC_ASSERT(b.keyCode <= KEY_BP);
    });
    CHECK(ok);
}

TEST(Abc80WebProtocol, TelemetryJsonCarriesIxAndIyAndKeepsTheOldRegisters)
{
    TelemetryFrame f;
    f.pc = 0x04CD;
    f.sp = 0x17BF;
    f.af = 0x1234;
    f.bc = 0x5678;
    f.de = 0x9ABC;
    f.hl = 0xDEF0;
    f.ix = 0x2468;
    f.iy = 0x1357;
    const json j = json::parse(buildTelemetryJson(f));
    const json &r = j.at("registers");
    LONGS_EQUAL(0x04CD, r.at("pc").get<int>());
    LONGS_EQUAL(0x17BF, r.at("sp").get<int>());
    LONGS_EQUAL(0x1234, r.at("af").get<int>());
    LONGS_EQUAL(0x5678, r.at("bc").get<int>());
    LONGS_EQUAL(0x9ABC, r.at("de").get<int>());
    LONGS_EQUAL(0xDEF0, r.at("hl").get<int>());
    LONGS_EQUAL(0x2468, r.at("ix").get<int>());
    LONGS_EQUAL(0x1357, r.at("iy").get<int>());
    LONGS_EQUAL(8, static_cast<long>(r.size()));
}

TEST(Abc80WebProtocol, TapeClientMessagesAndTelemetrySerialization)
{
    const ClientMessage listMsg = parseClientMessage(R"({"type":"tape_list"})");
    CHECK(listMsg.type == ClientMessage::Type::TapeList);

    const ClientMessage selMsg = parseClientMessage(R"({"type":"tape_select","fileId":"04aF"})");
    CHECK(selMsg.type == ClientMessage::Type::TapeSelect);
    CHECK(selMsg.hasTapeFileId);
    LONGS_EQUAL(0x04AF, selMsg.tapeFileId);

    CHECK(parseClientMessage(R"({"type":"tape_select","fileId":"123"})").type == ClientMessage::Type::Invalid);
    CHECK(parseClientMessage(R"({"type":"tape_select","fileId":"ZZZZ"})").type == ClientMessage::Type::Invalid);

    const ClientMessage annMsg = parseClientMessage(
        R"({"type":"tape_annotate","fileId":"0002","annotation":"0123456789ABCDEF0123456789ABCDEF_OVERFLOW"})");
    CHECK(annMsg.type == ClientMessage::Type::TapeAnnotate);
    LONGS_EQUAL(0x0002, annMsg.tapeFileId);
    STRCMP_EQUAL("0123456789ABCDEF0123456789ABCDEF", annMsg.tapeAnnotation.c_str());

    const ClientMessage playMsg = parseClientMessage(R"({"type":"tape_transport","action":"play","fileId":"0001"})");
    CHECK(playMsg.type == ClientMessage::Type::TapeTransport);
    STRCMP_EQUAL("play", playMsg.tapeAction.c_str());
    CHECK(playMsg.hasTapeFileId);
    LONGS_EQUAL(0x0001, playMsg.tapeFileId);

    const ClientMessage recMsg = parseClientMessage(
        R"({"type":"tape_transport","action":"record","annotation":"My Cassette Tape"})");
    CHECK(recMsg.type == ClientMessage::Type::TapeTransport);
    STRCMP_EQUAL("record", recMsg.tapeAction.c_str());
    CHECK(recMsg.hasTapeAnnotation);
    STRCMP_EQUAL("My Cassette Tape", recMsg.tapeAnnotation.c_str());

    CHECK(parseClientMessage(R"({"type":"tape_transport","action":"eject"})").type == ClientMessage::Type::Invalid);

    TelemetryFrame tf;
    tf.rom = "monitor";
    tf.tapeState = "playing";
    tf.tapeFileId = "0003";
    tf.tapeAnnotation = "Dual-Tone Chime (1000H)";
    tf.tapeCounter = 123;
    tf.tapeProgress = 0.5;
    tf.tapeToneHz = 1000;
    const json tj = json::parse(buildTelemetryJson(tf));
    CHECK(tj.contains("tape"));
    STRCMP_EQUAL("playing", tj["tape"].value("state", "").c_str());
    STRCMP_EQUAL("0003", tj["tape"].value("fileId", "").c_str());
    STRCMP_EQUAL("Dual-Tone Chime (1000H)", tj["tape"].value("annotation", "").c_str());
    LONGS_EQUAL(123, tj["tape"].value("counter", 0));
    LONGS_EQUAL(1000, tj["tape"].value("toneHz", 0));

    TapeFileInfo info;
    info.fileId = 0x0001;
    info.annotation = "BCD Up-Counter (1000H)";
    info.startAddr = 0x1000;
    info.endAddr = 0x1018;
    info.payloadBytes = 25;
    info.durationSec = 11.5;
    info.hasValidProgram = true;
    const json lj = json::parse(buildTapeListJson({ info }));
    STRCMP_EQUAL("tape_list", lj.value("type", "").c_str());
    LONGS_EQUAL(1, static_cast<long>(lj.at("tapes").size()));
    STRCMP_EQUAL("0001", lj["tapes"][0].value("fileId", "").c_str());
    STRCMP_EQUAL("BCD Up-Counter (1000H)", lj["tapes"][0].value("annotation", "").c_str());
}

TEST(Abc80WebProtocol, QuickBasicClientMessagesAndQbStateSerialization)
{
    const ClientMessage connMsg = parseClientMessage(R"({"type":"qb_connect"})");
    CHECK(connMsg.type == ClientMessage::Type::QbConnect);

    const ClientMessage statusMsg = parseClientMessage(R"({"type":"qb_status"})");
    CHECK(statusMsg.type == ClientMessage::Type::QbStatus);

    const ClientMessage keyMsg = parseClientMessage(R"({"type":"qb_key","key":"HOME"})");
    CHECK(keyMsg.type == ClientMessage::Type::QbKey);
    STRCMP_EQUAL("HOME", keyMsg.qbKey.c_str());

    CHECK(parseClientMessage(R"({"type":"qb_key"})").type == ClientMessage::Type::Invalid);
    CHECK(parseClientMessage(R"({"type":"qb_key","key":""})").type == ClientMessage::Type::Invalid);

    const ClientMessage saveMsg = parseClientMessage(
        R"({"type":"qb_save","filename":"demo.abc","startAddr":4096,"endAddr":4127})");
    CHECK(saveMsg.type == ClientMessage::Type::QbSave);
    STRCMP_EQUAL("demo.abc", saveMsg.qbFilename.c_str());
    LONGS_EQUAL(4096, saveMsg.qbStartAddr);
    LONGS_EQUAL(4127, saveMsg.qbEndAddr);

    CHECK(parseClientMessage(R"({"type":"qb_save","filename":"../bad.abc","startAddr":4096,"endAddr":4100})").type ==
          ClientMessage::Type::Invalid);
    CHECK(parseClientMessage(R"({"type":"qb_save","filename":"demo.abc","startAddr":5000,"endAddr":4096})").type ==
          ClientMessage::Type::Invalid);

    const ClientMessage loadMsg = parseClientMessage(R"({"type":"qb_load","filename":"demo.abc"})");
    CHECK(loadMsg.type == ClientMessage::Type::QbLoad);
    STRCMP_EQUAL("demo.abc", loadMsg.qbFilename.c_str());
    CHECK(parseClientMessage(R"({"type":"qb_load","filename":"../bad.abc"})").type ==
          ClientMessage::Type::Invalid);

    Abc80Board board;
    board.powerOn("vintage/rom.abc");
    Abc80QbHost qbHost(board);
    CHECK(qbHost.initialAbc());
    const json qj = json::parse(buildQbStateJson(qbHost));
    STRCMP_EQUAL("qb_state", qj.value("type", "").c_str());
    CHECK(qj.value("connected", false));
    CHECK(qj.value("linkReady", false));
    STRCMP_EQUAL("neutral", qj.value("mode", "").c_str());
    LONGS_EQUAL(0x1000, qj.value("currentAddress", 0));
    LONGS_EQUAL(25, static_cast<long>(qj.at("rows").size()));
    LONGS_EQUAL(25, static_cast<long>(qj.at("attrs").size()));
    LONGS_EQUAL(80, static_cast<long>(qj["rows"][0].get<std::string>().size()));
    LONGS_EQUAL(160, static_cast<long>(qj["attrs"][0].get<std::string>().size()));
    CHECK(!qj.at("audioEvents").empty());
    STRCMP_EQUAL("play", qj["audioEvents"][0].value("kind", "").c_str());
}

TEST(Abc80WebProtocol, ExtCircuitClientMessageParsingAndValidation)
{
    const struct {
        const char *name;
        ExtCircuitId id;
    } circuits[] = {
        { "traffic_light", ExtCircuitId::TrafficLight },
        { "led_chaser", ExtCircuitId::LedChaser },
        { "oscilloscope", ExtCircuitId::Oscilloscope },
        { "school_bell", ExtCircuitId::SchoolBell },
        { "appliance_control", ExtCircuitId::ApplianceControl },
        { "robotic_arm", ExtCircuitId::RoboticArm },
    };

    for (const auto &c : circuits) {
        const std::string attachExplicit =
            std::string(R"({"type":"ext_circuit","action":"attach","circuit":")") + c.name + "\"}";
        const ClientMessage m1 = parseClientMessage(attachExplicit);
        CHECK_TEXT(m1.type == ClientMessage::Type::ExtCircuit, c.name);
        STRCMP_EQUAL("attach", m1.extCircuitAction.c_str());
        CHECK(m1.extCircuitId == c.id);

        // Omitted action with non-none circuit defaults to "attach"
        const std::string attachImplicit =
            std::string(R"({"type":"ext_circuit","circuit":")") + c.name + "\"}";
        const ClientMessage m2 = parseClientMessage(attachImplicit);
        CHECK_TEXT(m2.type == ClientMessage::Type::ExtCircuit, c.name);
        STRCMP_EQUAL("attach", m2.extCircuitAction.c_str());
        CHECK(m2.extCircuitId == c.id);
    }

    // Explicit detach
    const ClientMessage detachMsg = parseClientMessage(R"({"type":"ext_circuit","action":"detach"})");
    CHECK(detachMsg.type == ClientMessage::Type::ExtCircuit);
    STRCMP_EQUAL("detach", detachMsg.extCircuitAction.c_str());
    CHECK(detachMsg.extCircuitId == ExtCircuitId::None);

    // Implicit detach via circuit="none" or attach circuit="none"
    const ClientMessage detachImplicit = parseClientMessage(R"({"type":"ext_circuit","circuit":"none"})");
    CHECK(detachImplicit.type == ClientMessage::Type::ExtCircuit);
    STRCMP_EQUAL("detach", detachImplicit.extCircuitAction.c_str());
    CHECK(detachImplicit.extCircuitId == ExtCircuitId::None);

    const ClientMessage attachNone = parseClientMessage(R"({"type":"ext_circuit","action":"attach","circuit":"none"})");
    CHECK(attachNone.type == ClientMessage::Type::ExtCircuit);
    STRCMP_EQUAL("detach", attachNone.extCircuitAction.c_str());
    CHECK(attachNone.extCircuitId == ExtCircuitId::None);

    // Reset & status
    const ClientMessage resetMsg = parseClientMessage(R"({"type":"ext_circuit","action":"reset"})");
    CHECK(resetMsg.type == ClientMessage::Type::ExtCircuit);
    STRCMP_EQUAL("reset", resetMsg.extCircuitAction.c_str());

    const ClientMessage statusMsg = parseClientMessage(R"({"type":"ext_circuit","action":"status"})");
    CHECK(statusMsg.type == ClientMessage::Type::ExtCircuit);
    STRCMP_EQUAL("status", statusMsg.extCircuitAction.c_str());

    // set_clock with BCD bytes 0x07, 0x59, 0x58
    const ClientMessage clockMsg = parseClientMessage(
        R"({"type":"ext_circuit","action":"set_clock","hh":7,"mm":89,"ss":88})");
    CHECK(clockMsg.type == ClientMessage::Type::ExtCircuit);
    STRCMP_EQUAL("set_clock", clockMsg.extCircuitAction.c_str());
    BYTES_EQUAL(0x07, clockMsg.clockHh);
    BYTES_EQUAL(0x59, clockMsg.clockMm);
    BYTES_EQUAL(0x58, clockMsg.clockSs);

    // Invalid ext_circuit messages
    const char *badExt[] = {
        R"({"type":"ext_circuit"})",
        R"({"type":"ext_circuit","action":123})",
        R"({"type":"ext_circuit","action":"explode"})",
        R"({"type":"ext_circuit","action":"attach"})",
        R"({"type":"ext_circuit","action":"attach","circuit":42})",
        R"({"type":"ext_circuit","action":"attach","circuit":"unknown_board"})",
        R"({"type":"ext_circuit","circuit":"unknown_board"})",
        R"({"type":"ext_circuit","action":"set_clock"})",
        R"({"type":"ext_circuit","action":"set_clock","hh":7,"mm":89})",
        R"({"type":"ext_circuit","action":"set_clock","hh":160,"mm":0,"ss":0})",
        R"({"type":"ext_circuit","action":"set_clock","hh":0,"mm":200,"ss":0})",
        R"({"type":"ext_circuit","action":"set_clock","hh":0,"mm":0,"ss":255})",
        R"({"type":"ext_circuit","action":"set_clock","hh":-1,"mm":0,"ss":0})",
        R"({"type":"ext_circuit","action":"set_clock","hh":"07","mm":0,"ss":0})",
    };
    for (const char *bad : badExt) {
        const ClientMessage m = parseClientMessage(bad);
        CHECK_TEXT(m.type == ClientMessage::Type::Invalid, bad);
        CHECK_TEXT(!m.error.empty(), bad);
    }
}

TEST(Abc80WebProtocol, CaptureBoardExtTelemetryAndBuildTelemetryJsonForAllSixCircuits)
{
    Abc80Board board;
    CHECK(board.powerOn("vintage/rom.abc") == Abc80Status::OK);

    // 0. Detached (None)
    {
        TelemetryFrame tf;
        captureBoardExtTelemetry(board, tf);
        const json j = json::parse(buildTelemetryJson(tf));
        CHECK(j.contains("ppi"));
        CHECK(j["ppi"].contains("pa"));
        CHECK(j["ppi"].contains("pb"));
        CHECK(j["ppi"].contains("pc"));
        CHECK(j["ppi"].contains("ctrl"));
        CHECK(j["ppi"].contains("auxPa"));
        CHECK(j["ppi"].contains("auxPb"));
        CHECK(j["ppi"].contains("auxPc"));
        CHECK(j["ppi"].contains("auxCtrl"));
        CHECK(j["ppi"].contains("expPa"));
        CHECK(j["ppi"].contains("expPb"));
        CHECK(j["ppi"].contains("expPc"));
        CHECK(j["ppi"].contains("expCtrl"));

        CHECK(j.contains("extCircuit"));
        STRCMP_EQUAL("none", j["extCircuit"].at("id").get<std::string>().c_str());
        CHECK(!j["extCircuit"].at("attached").get<bool>());
    }

    // 1. Exercise 24: Traffic Light (Port 0x81)
    {
        board.attachCircuit(ExtCircuitId::TrafficLight);
        board.getPpi().writePort(0x83, 0x80);
        board.getPpi().writePort(0x81, 0x21); // Road B Green (PB5) + Road A Red (PB0)
        board.stepTStates(1000);

        TelemetryFrame tf;
        captureBoardExtTelemetry(board, tf);
        const json j = json::parse(buildTelemetryJson(tf));
        const json &ec = j.at("extCircuit");
        STRCMP_EQUAL("traffic_light", ec.at("id").get<std::string>().c_str());
        CHECK(ec.at("attached").get<bool>());
        LONGS_EQUAL(24, ec.at("exercise").get<int>());
        LONGS_EQUAL(0x21, ec.at("portB").get<int>());
        CHECK(ec.at("roadA").at("red").get<bool>());
        CHECK(!ec.at("roadA").at("yellow").get<bool>());
        CHECK(!ec.at("roadA").at("green").get<bool>());
        LONGS_EQUAL(3, static_cast<long>(ec.at("roadA").at("duty").size()));
        CHECK(!ec.at("roadB").at("red").get<bool>());
        CHECK(!ec.at("roadB").at("yellow").get<bool>());
        CHECK(ec.at("roadB").at("green").get<bool>());
        LONGS_EQUAL(3, static_cast<long>(ec.at("roadB").at("duty").size()));
        LONGS_EQUAL(1, ec.at("transitions").get<long>());
    }

    // 2. Exercise 25: LED Chaser (Port 0x81 active-low via inverter)
    {
        board.attachCircuit(ExtCircuitId::LedChaser);
        board.getPpi().writePort(0x83, 0x80);
        board.getPpi().writePort(0x81, 0xFB); // ~0xFB = 0x04 -> LED 2 lit
        board.stepTStates(1000);

        TelemetryFrame tf;
        captureBoardExtTelemetry(board, tf);
        const json j = json::parse(buildTelemetryJson(tf));
        const json &ec = j.at("extCircuit");
        STRCMP_EQUAL("led_chaser", ec.at("id").get<std::string>().c_str());
        CHECK(ec.at("attached").get<bool>());
        LONGS_EQUAL(25, ec.at("exercise").get<int>());
        LONGS_EQUAL(0xFB, ec.at("rawPortB").get<int>());
        LONGS_EQUAL(0x04, ec.at("activeMask").get<int>());
        LONGS_EQUAL(2, ec.at("activeLed").get<int>());
        LONGS_EQUAL(8, static_cast<long>(ec.at("leds").size()));
        CHECK(ec.at("leds")[2].get<bool>());
        CHECK(!ec.at("leds")[0].get<bool>());
        LONGS_EQUAL(8, static_cast<long>(ec.at("duty").size()));
        LONGS_EQUAL(1, ec.at("steps").get<long>());
    }

    // 3. Exercise 26: Oscilloscope (Port 0x81 X/Y DAC)
    {
        board.attachCircuit(ExtCircuitId::Oscilloscope);
        board.getPpi().writePort(0x83, 0x80);
        board.getPpi().writePort(0x81, 0xAF); // X = 15 (0x0F), Y = 10 (0x0A)
        board.stepTStates(500);

        TelemetryFrame tf;
        captureBoardExtTelemetry(board, tf);
        const json j = json::parse(buildTelemetryJson(tf));
        const json &ec = j.at("extCircuit");
        STRCMP_EQUAL("oscilloscope", ec.at("id").get<std::string>().c_str());
        CHECK(ec.at("attached").get<bool>());
        LONGS_EQUAL(26, ec.at("exercise").get<int>());
        LONGS_EQUAL(0xAF, ec.at("rawPortB").get<int>());
        CHECK(ec.at("beamValid").get<bool>());
        LONGS_EQUAL(15, ec.at("beamX").get<int>());
        LONGS_EQUAL(10, ec.at("beamY").get<int>());
        LONGS_EQUAL(1, ec.at("samples").get<long>());
        LONGS_EQUAL(1, static_cast<long>(ec.at("points").size()));
        LONGS_EQUAL(15, ec.at("points")[0].at("x").get<int>());
        LONGS_EQUAL(10, ec.at("points")[0].at("y").get<int>());
        CHECK(ec.at("points")[0].at("dwell").get<uint32_t>() >= 500u);
    }

    // 4. Exercise 27: School Bell (Port 0x82 PC6 + BCD clock 0x1200..0x1202)
    {
        board.attachCircuit(ExtCircuitId::SchoolBell);
        auto *bell = board.attachedCircuitAs<Abc80SchoolBell>();
        CHECK(bell != nullptr);
        bell->setBcdTime(0x08, 0x00, 0x05);
        board.getPpi().writePort(0x83, 0x80);
        board.getPpi().writePort(0x82, 0x3F); // PC6 = 0 -> Bell Relay ON
        bell->step(bell->currentTStates() + 1200);

        TelemetryFrame tf;
        captureBoardExtTelemetry(board, tf);
        const json j = json::parse(buildTelemetryJson(tf));
        const json &ec = j.at("extCircuit");
        STRCMP_EQUAL("school_bell", ec.at("id").get<std::string>().c_str());
        CHECK(ec.at("attached").get<bool>());
        LONGS_EQUAL(27, ec.at("exercise").get<int>());
        LONGS_EQUAL(0x3F, ec.at("portC").get<int>());
        CHECK(ec.at("bellRinging").get<bool>());
        LONGS_EQUAL(1, ec.at("ringCount").get<long>());
        CHECK(ec.at("totalRingTStates").get<uint64_t>() >= 1200ULL);
        LONGS_EQUAL(0x08, ec.at("bcdHour").get<int>());
        LONGS_EQUAL(0x00, ec.at("bcdMinute").get<int>());
        LONGS_EQUAL(0x05, ec.at("bcdSecond").get<int>());
    }

    // 5. Exercise 28: Appliance Control (Aux Port 0x42 + BCD clock + 24-hour schedule)
    {
        board.attachCircuit(ExtCircuitId::ApplianceControl);
        auto *app = board.attachedCircuitAs<Abc80ApplianceControl>();
        CHECK(app != nullptr);
        app->setBcdTime(0x18, 0x30, 0x00);
        board.memory().writeByte(0x1118, 0xA5);
        board.getPpi().writePort(0x43, 0x80);
        board.getPpi().writePort(0x42, 0xA5);
        app->step(app->currentTStates() + 900);

        TelemetryFrame tf;
        captureBoardExtTelemetry(board, tf);
        const json j = json::parse(buildTelemetryJson(tf));
        const json &ec = j.at("extCircuit");
        STRCMP_EQUAL("appliance_control", ec.at("id").get<std::string>().c_str());
        CHECK(ec.at("attached").get<bool>());
        LONGS_EQUAL(28, ec.at("exercise").get<int>());
        LONGS_EQUAL(0x80, ec.at("auxControl").get<int>());
        LONGS_EQUAL(0xA5, ec.at("relayMask").get<int>());
        LONGS_EQUAL(8, static_cast<long>(ec.at("relays").size()));
        CHECK(ec.at("relays")[0].get<bool>());
        CHECK(!ec.at("relays")[1].get<bool>());
        CHECK(ec.at("relays")[2].get<bool>());
        CHECK(ec.at("relays")[7].get<bool>());
        LONGS_EQUAL(8, static_cast<long>(ec.at("duty").size()));
        LONGS_EQUAL(8, static_cast<long>(ec.at("onTStates").size()));
        LONGS_EQUAL(1, ec.at("updates").get<long>());
        LONGS_EQUAL(0x18, ec.at("bcdHour").get<int>());
        LONGS_EQUAL(0x30, ec.at("bcdMinute").get<int>());
        LONGS_EQUAL(0x00, ec.at("bcdSecond").get<int>());
        LONGS_EQUAL(18, ec.at("activeHourIndex").get<int>());
        LONGS_EQUAL(24, static_cast<long>(ec.at("scheduleMasks").size()));
        LONGS_EQUAL(0xA5, ec.at("scheduleMasks")[18].get<int>());
    }

    // 6. Exercise 29: Robotic Arm (Expansion Ports 0x00..0x03)
    {
        board.attachCircuit(ExtCircuitId::RoboticArm);
        auto *arm = board.attachedCircuitAs<Abc80RoboticArm>();
        CHECK(arm != nullptr);
        board.getPpi().writePort(0x03, 0x80);
        board.getPpi().writePort(0x00, 0x00);
        board.getPpi().writePort(0x00, 0x01); // Motor 1 +1 step
        board.getPpi().writePort(0x01, 0x00);
        board.getPpi().writePort(0x01, 0x22); // Motor 2 -1 step (0->2), Motor 3 -1 step (0->2)
        board.getPpi().writePort(0x02, 0x01); // Electromagnet ON
        arm->step(arm->currentTStates() + 750);

        TelemetryFrame tf;
        captureBoardExtTelemetry(board, tf);
        const json j = json::parse(buildTelemetryJson(tf));
        const json &ec = j.at("extCircuit");
        STRCMP_EQUAL("robotic_arm", ec.at("id").get<std::string>().c_str());
        CHECK(ec.at("attached").get<bool>());
        LONGS_EQUAL(29, ec.at("exercise").get<int>());
        LONGS_EQUAL(0x80, ec.at("expControl").get<int>());
        LONGS_EQUAL(0x01, ec.at("portA").get<int>());
        LONGS_EQUAL(0x22, ec.at("portB").get<int>());
        LONGS_EQUAL(0x01, ec.at("portC").get<int>());
        CHECK(ec.at("electromagnet").get<bool>());
        CHECK(ec.at("magnetOnTStates").get<uint64_t>() >= 750ULL);
        LONGS_EQUAL(3, static_cast<long>(ec.at("motors").size()));
        LONGS_EQUAL(1, ec.at("motors")[0].at("steps").get<int>());
        LONGS_EQUAL(1, ec.at("motors")[0].at("totalSteps").get<int>());
        LONGS_EQUAL(-1, ec.at("motors")[1].at("steps").get<int>());
        LONGS_EQUAL(-1, ec.at("motors")[2].at("steps").get<int>());
    }
}

TEST(Abc80WebProtocol, PropertyExtCircuitFuzzAndRoundTrip)
{
    const bool ok = rc::check("ext_circuit fuzz and round-trip", [](uint8_t idx, uint8_t hh, uint8_t mm, uint8_t ss, const std::string &noise) {
        const ExtCircuitId ids[] = {
            ExtCircuitId::None,
            ExtCircuitId::TrafficLight,
            ExtCircuitId::LedChaser,
            ExtCircuitId::Oscilloscope,
            ExtCircuitId::SchoolBell,
            ExtCircuitId::ApplianceControl,
            ExtCircuitId::RoboticArm
        };
        const ExtCircuitId cid = ids[idx % 7];
        const std::string cname = extCircuitIdName(cid);
        const json jAttach = {
            { "type", "ext_circuit" },
            { "action", "attach" },
            { "circuit", cname }
        };
        const ClientMessage mAttach = parseClientMessage(jAttach.dump());
        RC_ASSERT(mAttach.type == ClientMessage::Type::ExtCircuit);
        if (cid == ExtCircuitId::None) {
            RC_ASSERT(mAttach.extCircuitAction == "detach");
            RC_ASSERT(mAttach.extCircuitId == ExtCircuitId::None);
        } else {
            RC_ASSERT(mAttach.extCircuitAction == "attach");
            RC_ASSERT(mAttach.extCircuitId == cid);
        }

        const uint8_t validHh = static_cast<uint8_t>(hh % 0x9A);
        const uint8_t validMm = static_cast<uint8_t>(mm % 0x9A);
        const uint8_t validSs = static_cast<uint8_t>(ss % 0x9A);
        const json jClock = {
            { "type", "ext_circuit" },
            { "action", "set_clock" },
            { "hh", validHh },
            { "mm", validMm },
            { "ss", validSs }
        };
        const ClientMessage mClock = parseClientMessage(jClock.dump());
        RC_ASSERT(mClock.type == ClientMessage::Type::ExtCircuit);
        RC_ASSERT(mClock.extCircuitAction == "set_clock");
        RC_ASSERT(mClock.clockHh == validHh);
        RC_ASSERT(mClock.clockMm == validMm);
        RC_ASSERT(mClock.clockSs == validSs);

        const json jNoise = {
            { "type", "ext_circuit" },
            { "action", noise },
            { "circuit", noise }
        };
        const ClientMessage mNoise = parseClientMessage(
            jNoise.dump(-1, ' ', false, json::error_handler_t::replace));
        if (mNoise.type != ClientMessage::Type::Invalid) {
            RC_ASSERT(mNoise.type == ClientMessage::Type::ExtCircuit);
        } else {
            RC_ASSERT(!mNoise.error.empty());
        }
    });
    CHECK(ok);
}

// ---- FrameOutbox: latest frame wins; a slow consumer never blocks the producer ----

TEST_GROUP(Abc80FrameOutbox)
{
};

TEST(Abc80FrameOutbox, ConsumerGetsOnlyTheLatestFrame)
{
    FrameOutbox box;
    box.post("one");
    box.post("two");
    box.post("three");
    std::string out;
    CHECK(box.wait(out, 100));
    STRCMP_EQUAL("three", out.c_str());
    CHECK(!box.wait(out, 20));  // nothing newer
}

TEST(Abc80FrameOutbox, CloseWakesTheConsumerAndRefusesPosts)
{
    FrameOutbox box;
    std::string out;
    bool woke = false;
    std::thread consumer([&] { woke = !box.wait(out, 5000) && box.closed(); });
    box.close();
    consumer.join();
    CHECK(woke);
    box.post("late");
    CHECK(!box.wait(out, 10));
}

TEST(Abc80FrameOutbox, ProducerNeverBlocksWhenNobodyConsumes)
{
    FrameOutbox box;
    for (int i = 0; i < 100000; ++i) box.post("frame");  // would hang or grow without bound if it queued
    std::string out;
    CHECK(box.wait(out, 10));
}

/*
 * Local variables:
 * mode: C++
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
