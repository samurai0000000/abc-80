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

    if (type == "tape_list") {
        m.type = ClientMessage::Type::TapeList;
        return m;
    }

    if (type == "tape_select") {
        const auto fidIt = j.find("fileId");
        if (fidIt == j.end() || !fidIt->is_string() ||
            !Abc80Tape::parseFileId(fidIt->get<std::string>(), m.tapeFileId)) {
            return invalid("tape_select needs 4-digit hex 'fileId' [0-F]{4}");
        }
        m.hasTapeFileId = true;
        m.type = ClientMessage::Type::TapeSelect;
        return m;
    }

    if (type == "tape_annotate") {
        const auto fidIt = j.find("fileId");
        const auto annIt = j.find("annotation");
        if (fidIt == j.end() || !fidIt->is_string() ||
            !Abc80Tape::parseFileId(fidIt->get<std::string>(), m.tapeFileId)) {
            return invalid("tape_annotate needs 4-digit hex 'fileId' [0-F]{4}");
        }
        if (annIt == j.end() || !annIt->is_string()) {
            return invalid("tape_annotate needs string 'annotation'");
        }
        m.hasTapeFileId = true;
        m.hasTapeAnnotation = true;
        m.tapeAnnotation = Abc80Tape::sanitizeAnnotation(annIt->get<std::string>());
        m.type = ClientMessage::Type::TapeAnnotate;
        return m;
    }

    if (type == "tape_transport") {
        const auto actIt = j.find("action");
        if (actIt == j.end() || !actIt->is_string()) {
            return invalid("tape_transport needs string 'action'");
        }
        const std::string action = actIt->get<std::string>();
        if (action != "play" && action != "record" && action != "rewind" && action != "stop") {
            return invalid("tape_transport 'action' must be play, record, rewind, or stop");
        }
        const auto fidIt = j.find("fileId");
        if (fidIt != j.end()) {
            if (!fidIt->is_string() || !Abc80Tape::parseFileId(fidIt->get<std::string>(), m.tapeFileId)) {
                return invalid("tape_transport optional 'fileId' must be 4-digit hex [0-F]{4}");
            }
            m.hasTapeFileId = true;
        }
        const auto annIt = j.find("annotation");
        if (annIt != j.end()) {
            if (!annIt->is_string()) {
                return invalid("tape_transport optional 'annotation' must be string");
            }
            m.hasTapeAnnotation = true;
            m.tapeAnnotation = Abc80Tape::sanitizeAnnotation(annIt->get<std::string>());
        }
        m.type = ClientMessage::Type::TapeTransport;
        m.tapeAction = action;
        return m;
    }

    if (type == "qb_connect") {
        m.type = ClientMessage::Type::QbConnect;
        return m;
    }

    if (type == "qb_status") {
        m.type = ClientMessage::Type::QbStatus;
        return m;
    }

    if (type == "qb_key") {
        const auto keyIt = j.find("key");
        if (keyIt == j.end() || !keyIt->is_string() || keyIt->get<std::string>().empty() ||
            keyIt->get<std::string>().size() > 32) {
            return invalid("qb_key needs non-empty string 'key' (1..32 chars)");
        }
        m.qbKey = keyIt->get<std::string>();
        m.type = ClientMessage::Type::QbKey;
        return m;
    }

    if (type == "qb_save") {
        const auto fnIt = j.find("filename");
        const auto startIt = j.find("startAddr");
        const auto endIt = j.find("endAddr");
        std::string cleanName;
        if (fnIt == j.end() || !fnIt->is_string() ||
            !Abc80QbHost::sanitizeAbcFilename(fnIt->get<std::string>(), cleanName)) {
            return invalid("qb_save needs valid 'filename'");
        }
        if (startIt == j.end() || !startIt->is_number_unsigned() || startIt->get<uint64_t>() > 0xFFFF ||
            endIt == j.end() || !endIt->is_number_unsigned() || endIt->get<uint64_t>() > 0xFFFF) {
            return invalid("qb_save needs 'startAddr' and 'endAddr' in range 0..65535");
        }
        uint16_t sAddr = static_cast<uint16_t>(startIt->get<uint64_t>());
        uint16_t eAddr = static_cast<uint16_t>(endIt->get<uint64_t>());
        if (sAddr > eAddr) {
            return invalid("qb_save requires startAddr <= endAddr");
        }
        m.qbFilename = cleanName;
        m.qbStartAddr = sAddr;
        m.qbEndAddr = eAddr;
        m.type = ClientMessage::Type::QbSave;
        return m;
    }

    if (type == "qb_load") {
        const auto fnIt = j.find("filename");
        std::string cleanName;
        if (fnIt == j.end() || !fnIt->is_string() ||
            !Abc80QbHost::sanitizeAbcFilename(fnIt->get<std::string>(), cleanName)) {
            return invalid("qb_load needs valid 'filename'");
        }
        m.qbFilename = cleanName;
        m.type = ClientMessage::Type::QbLoad;
        return m;
    }

    if (type == "ext_circuit") {
        const auto actIt = j.find("action");
        const auto cirIt = j.find("circuit");
        std::string action;
        if (actIt != j.end()) {
            if (!actIt->is_string()) {
                return invalid("ext_circuit 'action' must be string");
            }
            action = actIt->get<std::string>();
        }

        ExtCircuitId cid = ExtCircuitId::None;
        bool hasCircuit = false;
        if (cirIt != j.end()) {
            if (!cirIt->is_string() || !parseExtCircuitId(cirIt->get<std::string>(), cid)) {
                return invalid("ext_circuit needs valid 'circuit' name");
            }
            hasCircuit = true;
        }

        if (action.empty()) {
            if (!hasCircuit) {
                return invalid("ext_circuit needs 'action' or 'circuit'");
            }
            action = (cid == ExtCircuitId::None) ? "detach" : "attach";
        }

        if (action == "attach") {
            if (!hasCircuit) {
                return invalid("ext_circuit 'attach' needs 'circuit'");
            }
            if (cid == ExtCircuitId::None) {
                m.extCircuitAction = "detach";
                m.extCircuitId = ExtCircuitId::None;
            } else {
                m.extCircuitAction = "attach";
                m.extCircuitId = cid;
            }
            m.type = ClientMessage::Type::ExtCircuit;
            return m;
        }

        if (action == "detach" || action == "reset" || action == "status") {
            m.extCircuitAction = action;
            m.extCircuitId = ExtCircuitId::None;
            m.type = ClientMessage::Type::ExtCircuit;
            return m;
        }

        if (action == "set_clock") {
            const auto hhIt = j.find("hh");
            const auto mmIt = j.find("mm");
            const auto ssIt = j.find("ss");
            if (hhIt == j.end() || !hhIt->is_number_unsigned() || hhIt->get<uint64_t>() > 0x99 ||
                mmIt == j.end() || !mmIt->is_number_unsigned() || mmIt->get<uint64_t>() > 0x99 ||
                ssIt == j.end() || !ssIt->is_number_unsigned() || ssIt->get<uint64_t>() > 0x99) {
                return invalid("ext_circuit 'set_clock' needs 'hh', 'mm', 'ss' in 0x00..0x99");
            }
            m.extCircuitAction = "set_clock";
            m.clockHh = static_cast<uint8_t>(hhIt->get<uint64_t>());
            m.clockMm = static_cast<uint8_t>(mmIt->get<uint64_t>());
            m.clockSs = static_cast<uint8_t>(ssIt->get<uint64_t>());
            m.type = ClientMessage::Type::ExtCircuit;
            return m;
        }

        return invalid("ext_circuit 'action' must be attach, detach, reset, status, or set_clock");
    }

    return invalid("unknown message type");
}

void captureBoardExtTelemetry(const Abc80Board &board, TelemetryFrame &frame) noexcept
{
    const Abc80Ppi &ppi = board.getPpi();
    frame.ppi.pa = 0x7F;
    frame.ppi.pb = ppi.getSegmentOutput();
    frame.ppi.pc = ppi.getDigitStrobe();
    frame.ppi.ctrl = ppi.getPrimaryControl();
    frame.ppi.auxPa = ppi.auxPortAOutput();
    frame.ppi.auxPb = ppi.auxPortBOutput();
    frame.ppi.auxPc = ppi.auxPortCOutput();
    frame.ppi.auxCtrl = ppi.getAuxControl();
    frame.ppi.expPa = ppi.expPortAOutput();
    frame.ppi.expPb = ppi.expPortBOutput();
    frame.ppi.expPc = ppi.expPortCOutput();
    frame.ppi.expCtrl = ppi.getExpControl();

    ExtCircuitTelemetry &ec = frame.extCircuit;
    ec = ExtCircuitTelemetry{};
    const Abc80ExtCircuit *circuit = board.attachedCircuit();
    if (circuit == nullptr || circuit->id() == ExtCircuitId::None) {
        ec.id = ExtCircuitId::None;
        ec.attached = false;
        return;
    }

    ec.id = circuit->id();
    ec.attached = true;
    ec.exercise = circuit->exerciseNumber();
    ec.clockTStates = circuit->currentTStates();

    switch (circuit->id()) {
    case ExtCircuitId::None:
        ec.attached = false;
        break;
    case ExtCircuitId::TrafficLight:
        if (const auto *tl = board.attachedCircuitAs<Abc80TrafficLight>()) {
            ec.trafficPortB = tl->portB();
            ec.trafficLampDuty = tl->lampDuty();
            ec.trafficTransitions = tl->transitionCount();
        }
        break;
    case ExtCircuitId::LedChaser:
        if (const auto *lc = board.attachedCircuitAs<Abc80LedChaser>()) {
            ec.chaserRawPortB = lc->rawPortB();
            ec.chaserActiveMask = lc->activeMask();
            ec.chaserActiveLed = lc->activeLedIndex();
            ec.chaserLedDuty = lc->ledDuty();
            ec.chaserStepCount = lc->stepCount();
        }
        break;
    case ExtCircuitId::Oscilloscope:
        if (const auto *osc = board.attachedCircuitAs<Abc80Oscilloscope>()) {
            ec.scopeRawPortB = osc->rawPortB();
            ec.scopeBeamValid = osc->isBeamValid();
            ec.scopeBeamX = osc->beamX();
            ec.scopeBeamY = osc->beamY();
            ec.scopeSampleCount = osc->sampleCount();
            ec.scopeTrace = osc->trace();
        }
        break;
    case ExtCircuitId::SchoolBell:
        if (const auto *bell = board.attachedCircuitAs<Abc80SchoolBell>()) {
            ec.bellPortC = bell->portC();
            ec.bellRinging = bell->isBellRinging();
            ec.bellDuty = bell->bellDuty();
            ec.bellRingCount = bell->ringCount();
            ec.bellTotalRingTStates = bell->totalRingTStates();
            ec.bellSpeakerEdges = bell->speakerEdges();
            ec.bcdHour = bell->bcdHour();
            ec.bcdMinute = bell->bcdMinute();
            ec.bcdSecond = bell->bcdSecond();
        }
        break;
    case ExtCircuitId::ApplianceControl:
        if (const auto *app = board.attachedCircuitAs<Abc80ApplianceControl>()) {
            ec.applianceAuxControl = app->auxControl();
            ec.applianceRelayMask = app->relayMask();
            ec.applianceRelayDuty = app->relayDuty();
            ec.applianceTotalOnTStates = app->totalOnTStates();
            ec.applianceUpdateCount = app->updateCount();
            ec.bcdHour = app->bcdHour();
            ec.bcdMinute = app->bcdMinute();
            ec.bcdSecond = app->bcdSecond();
            ec.applianceActiveHourIndex = app->activeHourIndex();
            ec.applianceScheduleMasks = app->scheduleMasks();
        }
        break;
    case ExtCircuitId::RoboticArm:
        if (const auto *arm = board.attachedCircuitAs<Abc80RoboticArm>()) {
            ec.armExpControl = arm->expControl();
            ec.armPortA = arm->portA();
            ec.armPortB = arm->portB();
            ec.armPortC = arm->portC();
            ec.armElectromagnet = arm->isElectromagnetOn();
            ec.armMagnetOnTStates = arm->magnetOnTStates();
            ec.armLastStepTStates = arm->lastStepTStates();
            for (size_t i = 0; i < 3; ++i) {
                ec.armMotors[i] = arm->motor(i);
            }
        }
        break;
    }
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
    j["tape"] = {
        { "state", f.tapeState },
        { "fileId", f.tapeFileId },
        { "annotation", f.tapeAnnotation },
        { "counter", f.tapeCounter },
        { "progress", f.tapeProgress },
        { "toneHz", f.tapeToneHz }
    };
    j["ppi"] = {
        { "pa", f.ppi.pa },
        { "pb", f.ppi.pb },
        { "pc", f.ppi.pc },
        { "ctrl", f.ppi.ctrl },
        { "auxPa", f.ppi.auxPa },
        { "auxPb", f.ppi.auxPb },
        { "auxPc", f.ppi.auxPc },
        { "auxCtrl", f.ppi.auxCtrl },
        { "expPa", f.ppi.expPa },
        { "expPb", f.ppi.expPb },
        { "expPc", f.ppi.expPc },
        { "expCtrl", f.ppi.expCtrl }
    };

    const ExtCircuitTelemetry &ec = f.extCircuit;
    json jec;
    jec["id"] = extCircuitIdName(ec.id);
    jec["attached"] = ec.attached;
    if (ec.attached && ec.id != ExtCircuitId::None) {
        jec["exercise"] = ec.exercise;
        jec["clockTStates"] = ec.clockTStates;
        switch (ec.id) {
        case ExtCircuitId::None:
            break;
        case ExtCircuitId::TrafficLight:
            jec["portB"] = ec.trafficPortB;
            jec["roadA"] = {
                { "red", (ec.trafficPortB & 0x01) != 0 },
                { "yellow", (ec.trafficPortB & 0x02) != 0 },
                { "green", (ec.trafficPortB & 0x04) != 0 },
                { "duty", { ec.trafficLampDuty[0], ec.trafficLampDuty[1], ec.trafficLampDuty[2] } }
            };
            jec["roadB"] = {
                { "red", (ec.trafficPortB & 0x08) != 0 },
                { "yellow", (ec.trafficPortB & 0x10) != 0 },
                { "green", (ec.trafficPortB & 0x20) != 0 },
                { "duty", { ec.trafficLampDuty[3], ec.trafficLampDuty[4], ec.trafficLampDuty[5] } }
            };
            jec["transitions"] = ec.trafficTransitions;
            break;
        case ExtCircuitId::LedChaser: {
            jec["rawPortB"] = ec.chaserRawPortB;
            jec["activeMask"] = ec.chaserActiveMask;
            jec["activeLed"] = ec.chaserActiveLed;
            json leds = json::array();
            json duty = json::array();
            for (size_t i = 0; i < 8; ++i) {
                leds.push_back((ec.chaserActiveMask & (1u << i)) != 0);
                duty.push_back(ec.chaserLedDuty[i]);
            }
            jec["leds"] = std::move(leds);
            jec["duty"] = std::move(duty);
            jec["steps"] = ec.chaserStepCount;
            break;
        }
        case ExtCircuitId::Oscilloscope: {
            jec["rawPortB"] = ec.scopeRawPortB;
            jec["beamValid"] = ec.scopeBeamValid;
            jec["beamX"] = ec.scopeBeamX;
            jec["beamY"] = ec.scopeBeamY;
            jec["samples"] = ec.scopeSampleCount;
            json pts = json::array();
            for (const auto &pt : ec.scopeTrace) {
                pts.push_back({ { "x", pt.x }, { "y", pt.y }, { "dwell", pt.dwellTStates } });
            }
            jec["points"] = std::move(pts);
            break;
        }
        case ExtCircuitId::SchoolBell:
            jec["portC"] = ec.bellPortC;
            jec["bellRinging"] = ec.bellRinging;
            jec["bellDuty"] = ec.bellDuty;
            jec["ringCount"] = ec.bellRingCount;
            jec["totalRingTStates"] = ec.bellTotalRingTStates;
            jec["speakerEdges"] = ec.bellSpeakerEdges;
            jec["bcdHour"] = ec.bcdHour;
            jec["bcdMinute"] = ec.bcdMinute;
            jec["bcdSecond"] = ec.bcdSecond;
            break;
        case ExtCircuitId::ApplianceControl: {
            jec["auxControl"] = ec.applianceAuxControl;
            jec["relayMask"] = ec.applianceRelayMask;
            json relays = json::array();
            json duty = json::array();
            json onT = json::array();
            for (size_t i = 0; i < 8; ++i) {
                relays.push_back((ec.applianceRelayMask & (1u << i)) != 0);
                duty.push_back(ec.applianceRelayDuty[i]);
                onT.push_back(ec.applianceTotalOnTStates[i]);
            }
            jec["relays"] = std::move(relays);
            jec["duty"] = std::move(duty);
            jec["onTStates"] = std::move(onT);
            jec["updates"] = ec.applianceUpdateCount;
            jec["bcdHour"] = ec.bcdHour;
            jec["bcdMinute"] = ec.bcdMinute;
            jec["bcdSecond"] = ec.bcdSecond;
            jec["activeHourIndex"] = ec.applianceActiveHourIndex;
            jec["scheduleMasks"] = ec.applianceScheduleMasks;
            break;
        }
        case ExtCircuitId::RoboticArm: {
            jec["expControl"] = ec.armExpControl;
            jec["portA"] = ec.armPortA;
            jec["portB"] = ec.armPortB;
            jec["portC"] = ec.armPortC;
            jec["electromagnet"] = ec.armElectromagnet;
            jec["magnetOnTStates"] = ec.armMagnetOnTStates;
            jec["lastStepTStates"] = ec.armLastStepTStates;
            json motors = json::array();
            for (size_t i = 0; i < 3; ++i) {
                motors.push_back({
                    { "phase", ec.armMotors[i].phase },
                    { "coils", ec.armMotors[i].coils },
                    { "steps", ec.armMotors[i].steps },
                    { "totalSteps", ec.armMotors[i].totalSteps }
                });
            }
            jec["motors"] = std::move(motors);
            break;
        }
        }
    }
    j["extCircuit"] = std::move(jec);
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

std::string buildTapeListJson(const std::vector<TapeFileInfo> &tapes)
{
    json j;
    j["type"] = "tape_list";
    j["tapes"] = json::array();
    for (const auto &t : tapes) {
        json item;
        item["fileId"] = Abc80Tape::formatFileId(t.fileId);
        item["annotation"] = t.annotation;
        item["startAddr"] = t.startAddr;
        item["endAddr"] = t.endAddr;
        item["bytes"] = t.payloadBytes;
        item["durationSec"] = t.durationSec;
        item["valid"] = t.hasValidProgram;
        j["tapes"].push_back(std::move(item));
    }
    return j.dump();
}

std::string buildQbStateJson(Abc80QbHost &qbHost)
{
    json j;
    j["type"] = "qb_state";
    j["connected"] = qbHost.isConnected();
    j["linkReady"] = qbHost.isLinkServerPolling();
    j["mode"] = qbHost.getModeName();
    j["currentAddress"] = qbHost.getCurrentAddress();
    j["highlightedKey"] = qbHost.getHighlightedKey();
    j["statusMessage"] = qbHost.getStatusMessage();

    const Cp437Screen &scr = qbHost.getScreen();
    j["rows"] = json::array();
    j["attrs"] = json::array();
    for (int r = 1; r <= Cp437Screen::HEIGHT; ++r) {
        j["rows"].push_back(scr.getRowText(r));
        j["attrs"].push_back(scr.getRowAttrHex(r));
    }

    j["audioEvents"] = json::array();
    for (const auto &ev : qbHost.consumeAudioEvents()) {
        json item;
        item["kind"] = ev.kind;
        item["freqHz"] = ev.freqHz;
        item["durationSec"] = ev.durationSec;
        item["mml"] = ev.mml;
        j["audioEvents"].push_back(std::move(item));
    }

    j["programs"] = qbHost.listPrograms();
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
