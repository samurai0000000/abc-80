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
    f.af = 0x1234;
    f.bc = 0x5678;
    f.de = 0x9ABC;
    f.hl = 0xDEF0;
    const json j = json::parse(buildTelemetryJson(f));
    STRCMP_EQUAL("telemetry", j.at("type").get<std::string>().c_str());
    LONGS_EQUAL(42, j.at("seq").get<long>());
    STRCMP_EQUAL("monitor", j.at("rom").get<std::string>().c_str());
    CHECK(j.at("cycles").get<uint64_t>() == 123456789ULL);
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
