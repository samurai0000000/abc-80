/*
 * WebProtocol.cxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include <abc80/Abc80Types.hxx>
#include <abc80/WebProtocol.hxx>

#include <cctype>
#include <nlohmann/json.hpp>

namespace abc80 {

using json = nlohmann::json;

namespace {

ClientMessage invalid(const std::string &why)
{
    ClientMessage m;
    m.type = ClientMessage::Type::Invalid;
    m.error = why;
    return m;
}

} // namespace

bool keyCodeFromName(const std::string &name, uint8_t &code) noexcept
{
    if (name.size() == 1) {
        const char c = name[0];
        if (c >= '0' && c <= '9') {
            code = static_cast<uint8_t>(c - '0');
            return true;
        }
        if (c >= 'A' && c <= 'F') {
            code = static_cast<uint8_t>(10 + (c - 'A'));
            return true;
        }
        if (c >= 'a' && c <= 'f') {
            code = static_cast<uint8_t>(10 + (c - 'a'));
            return true;
        }
        if (c == '+') {
            code = KEY_PLUS;
            return true;
        }
        if (c == '-') {
            code = KEY_MINUS;
            return true;
        }
        return false;
    }
    if (name == "GO" || name == "RUN") {
        code = KEY_RUN;
        return true;
    }
    if (name == "DATA") {
        code = KEY_DATA;
        return true;
    }
    if (name == "ADRS" || name == "ADDR") {
        code = KEY_ADRS;
        return true;
    }
    if (name == "TO TAPE" || name == "STEP") {
        code = KEY_STEP;
        return true;
    }
    if (name == "FROM TAPE" || name == "BP") {
        code = KEY_BP;
        return true;
    }
    return false;
}

bool validToken(const std::string &token) noexcept
{
    if (token.size() < 8 || token.size() > 64) {
        return false;
    }
    for (char c : token) {
        const bool ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
                        c == '_' || c == '-';
        if (!ok) {
            return false;
        }
    }
    return true;
}

ClientMessage parseClientMessage(const std::string &text)
{
    if (text.size() > kMaxClientMessageBytes) {
        return invalid("message too large");
    }
    const json j = json::parse(text, nullptr, false);
    if (j.is_discarded()) {
        return invalid("not valid JSON");
    }
    if (!j.is_object()) {
        return invalid("message must be a JSON object");
    }
    const auto typeIt = j.find("type");
    if (typeIt == j.end() || !typeIt->is_string()) {
        return invalid("missing string field 'type'");
    }
    const std::string type = typeIt->get<std::string>();

    ClientMessage m;
    if (type == "reset") {
        m.type = ClientMessage::Type::Reset;
        return m;
    }

    if (type == "run") {
        m.type = ClientMessage::Type::Run;
        return m;
    }

    if (type == "pause") {
        m.type = ClientMessage::Type::Pause;
        return m;
    }

    if (type == "step") {
        m.type = ClientMessage::Type::Step;
        return m;
    }

    if (type == "cancelAiPunch") {
        m.type = ClientMessage::Type::CancelAiPunch;
        return m;
    }

    if (type == "hello" || type == "setRom") {
        const auto romIt = j.find("rom");
        if (romIt != j.end()) {
            if (!romIt->is_string() || !parseRomId(romIt->get<std::string>(), m.rom)) {
                return invalid("unknown rom");
            }
            m.hasRom = true;
        }
        if (type == "setRom") {
            if (!m.hasRom) {
                return invalid("setRom needs 'rom'");
            }
            m.type = ClientMessage::Type::SetRom;
            return m;
        }
        const auto tokIt = j.find("token");
        if (tokIt == j.end() || !tokIt->is_string() || !validToken(tokIt->get<std::string>())) {
            return invalid("hello needs a 'token' of 8-64 characters [A-Za-z0-9_-]");
        }
        m.type = ClientMessage::Type::Hello;
        m.token = tokIt->get<std::string>();
        return m;
    }

    if (type == "key") {
        const auto keyIt = j.find("key");
        const auto actIt = j.find("action");
        if (keyIt == j.end() || !keyIt->is_string() || !keyCodeFromName(keyIt->get<std::string>(), m.keyCode)) {
            return invalid("unknown 'key'");
        }
        if (actIt == j.end() || !actIt->is_string()) {
            return invalid("missing 'action'");
        }
        const std::string action = actIt->get<std::string>();
        if (action == "down") {
            m.down = true;
        } else if (action == "up") {
            m.down = false;
        } else {
            return invalid("'action' must be 'down' or 'up'");
        }
        const auto seqIt = j.find("seq");
        if (seqIt != j.end()) {
            if (!seqIt->is_number_unsigned()) {
                return invalid("'seq' must be a non-negative integer");
            }
            m.seq = seqIt->get<uint64_t>();
        }
        m.type = ClientMessage::Type::Key;
        return m;
    }

    if (type == "readMem" || type == "readDisasm") {
        const auto addrIt = j.find("addr");
        if (addrIt == j.end() || !addrIt->is_number_unsigned() || addrIt->get<uint64_t>() > 0xFFFF) {
            return invalid(type + " needs integer 'addr' in range 0..65535");
        }
        const auto countIt = j.find("count");
        if (countIt == j.end() || !countIt->is_number_unsigned() || countIt->get<uint64_t>() == 0) {
            return invalid(type + " needs positive integer 'count'");
        }
        m.type = (type == "readMem") ? ClientMessage::Type::ReadMem : ClientMessage::Type::ReadDisasm;
        m.memAddr = static_cast<uint16_t>(addrIt->get<uint64_t>());
        const uint64_t requestedCount = countIt->get<uint64_t>();
        if (type == "readMem") {
            m.memCount = static_cast<uint16_t>(std::min<uint64_t>(requestedCount, 256));
        } else {
            m.memCount = static_cast<uint16_t>(std::min<uint64_t>(requestedCount, 64));
        }
        return m;
    }

    return invalid("unknown message type");
}

std::string buildTelemetryJson(const TelemetryFrame &f)
{
    json j;
    j["type"] = "telemetry";
    j["seq"] = f.seq;
    j["rom"] = f.rom;
    j["cycles"] = f.cycles;
    j["paused"] = f.paused;
    j["displayMasks"] = json::array();
    for (uint8_t m : f.displayMasks) {
        j["displayMasks"].push_back(m);
    }
    j["displayDigits"] = json::array();
    for (char c : f.displayDigits) {
        j["displayDigits"].push_back(std::string(1, c));
    }
    j["leds"] = { { "ep", f.ep }, { "halt", f.halt }, { "speaker", f.speakerLed } };
    j["speakerLevel"] = f.speakerLevel;
    j["speakerEdges"] = f.speakerEdges;
    j["speakerOverflow"] = f.speakerOverflow;
    j["registers"] = { { "pc", f.pc }, { "sp", f.sp }, { "af", f.af },
                       { "bc", f.bc }, { "de", f.de }, { "hl", f.hl },
                       { "ix", f.ix }, { "iy", f.iy } };
    j["stackBaseAddr"] = f.stackBaseAddr;
    j["stackBytes"] = f.stackBytes;
    return j.dump();
}

std::string buildErrorJson(const std::string &reason)
{
    json j;
    j["type"] = "error";
    j["reason"] = reason;
    return j.dump(-1, ' ', false, json::error_handler_t::replace);
}

std::string buildMemDataJson(uint16_t addr, const std::vector<uint8_t> &data)
{
    json j;
    j["type"] = "memData";
    j["addr"] = addr;
    j["data"] = data;
    return j.dump();
}

std::string buildDisasmJson(uint16_t addr, const std::vector<DisasmLine> &lines)
{
    json j;
    j["type"] = "disasm";
    j["addr"] = addr;
    j["lines"] = json::array();
    for (const auto &line : lines) {
        json item;
        item["addr"] = line.addr;
        item["bytes"] = line.bytes;
        item["mnemonic"] = line.mnemonic;
        j["lines"].push_back(std::move(item));
    }
    return j.dump();
}

std::string buildAiPunchJson(const std::string &key, bool down, size_t remaining)
{
    json j;
    j["type"] = "aiPunch";
    j["key"] = key;
    j["down"] = down;
    j["remaining"] = remaining;
    return j.dump();
}

} // namespace abc80

/*
 * Local variables:
 * mode: C++
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
