/*
 * Abc80Tape.cxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include <abc80/Abc80Tape.hxx>
#include <abc80/Abc80Ppi.hxx>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <cstring>
#include <algorithm>
#include <filesystem>

namespace abc80 {

static constexpr double TWO_PI = 6.28318530717958647692;

Abc80Tape::Abc80Tape(const TapeConfig& config)
    : _config(config)
{
}

void Abc80Tape::appendTone(std::vector<int16_t>& pcm, double& phase, uint32_t freq, uint32_t sampleCount) const
{
    const double phaseInc = TWO_PI * static_cast<double>(freq) / static_cast<double>(_config.sampleRate);
    for (uint32_t i = 0; i < sampleCount; ++i) {
        double val = std::sin(phase) * _config.amplitude;
        int32_t clamped = std::clamp(static_cast<int32_t>(std::round(val)), -32768, 32767);
        pcm.push_back(static_cast<int16_t>(clamped));
        phase += phaseInc;
        if (phase >= TWO_PI) {
            phase -= TWO_PI;
        }
    }
}

void Abc80Tape::appendBit(std::vector<int16_t>& pcm, double& phase, bool bit) const
{
    const uint32_t freq = bit ? _config.markFreq : _config.spaceFreq;
    const uint32_t samplesPerBit = _config.sampleRate / _config.baudRate;
    appendTone(pcm, phase, freq, samplesPerBit);
}

void Abc80Tape::appendByte(std::vector<int16_t>& pcm, double& phase, uint8_t byte) const
{
    // Start bit: Space (0)
    appendBit(pcm, phase, false);

    // 8 Data bits: LSB first
    for (int i = 0; i < 8; ++i) {
        bool bit = ((byte >> i) & 1) != 0;
        appendBit(pcm, phase, bit);
    }

    // 2 Stop bits: Mark (1)
    appendBit(pcm, phase, true);
    appendBit(pcm, phase, true);
}

Abc80Status Abc80Tape::modulate(const uint8_t* data, size_t length, std::vector<int16_t>& pcmOut) const
{
    if (!data && length > 0) {
        return Abc80Status::INVALID_ARG;
    }

    pcmOut.clear();
    double phase = 0.0;
    const double spb = static_cast<double>(_config.sampleRate) / static_cast<double>(_config.baudRate);

    // Pre-allocate
    const size_t leaderSamples = static_cast<size_t>(_config.leaderCycles) * _config.sampleRate / _config.markFreq;
    const size_t trailerSamples = static_cast<size_t>(_config.trailerCycles) * _config.sampleRate / _config.markFreq;
    const size_t dataSamples = static_cast<size_t>(length * 11 * spb);
    pcmOut.reserve(leaderSamples + dataSamples + trailerSamples + 1024);

    // Leader Tone: continuous mark frequency
    appendTone(pcmOut, phase, _config.markFreq, static_cast<uint32_t>(leaderSamples));

    // Data bytes: fractional sample accumulator prevents clock drift
    double targetSamples = static_cast<double>(pcmOut.size());

    auto appendBitExact = [&](bool bit) {
        targetSamples += spb;
        uint32_t count = static_cast<uint32_t>(std::round(targetSamples)) - static_cast<uint32_t>(pcmOut.size());
        uint32_t freq = bit ? _config.markFreq : _config.spaceFreq;
        appendTone(pcmOut, phase, freq, count);
    };

    for (size_t i = 0; i < length; ++i) {
        uint8_t byte = data[i];
        // Start bit: Space (0)
        appendBitExact(false);
        // 8 Data bits: LSB first
        for (int b = 0; b < 8; ++b) {
            appendBitExact(((byte >> b) & 1) != 0);
        }
        // 2 Stop bits: Mark (1)
        appendBitExact(true);
        appendBitExact(true);
    }

    // Trailer Tone: mark frequency
    appendTone(pcmOut, phase, _config.markFreq, static_cast<uint32_t>(trailerSamples));

    return Abc80Status::OK;
}

Abc80Status Abc80Tape::demodulate(const int16_t* pcm, size_t sampleCount, std::vector<uint8_t>& dataOut) const
{
    if (!pcm || sampleCount == 0) {
        return Abc80Status::INVALID_ARG;
    }

    dataOut.clear();

    const double samplesPerBit = static_cast<double>(_config.sampleRate) / static_cast<double>(_config.baudRate);

    // Adaptive peak & hysteresis detection
    int16_t peak = 0;
    for (size_t i = 0; i < sampleCount; ++i) {
        int16_t absVal = (pcm[i] < 0) ? static_cast<int16_t>(-pcm[i]) : pcm[i];
        if (absVal > peak) {
            peak = absVal;
        }
    }
    const int16_t hysteresis = std::max<int16_t>(200, static_cast<int16_t>(peak / 8));

    // Step 1: Detect zero-crossings with Schmitt-trigger hysteresis
    std::vector<size_t> crossings;
    crossings.reserve(sampleCount / 10);

    int lastSign = 0; // -1 = negative, +1 = positive
    for (size_t i = 0; i < sampleCount; ++i) {
        if (pcm[i] >= hysteresis && lastSign != 1) {
            crossings.push_back(i);
            lastSign = 1;
        } else if (pcm[i] <= -hysteresis && lastSign != -1) {
            crossings.push_back(i);
            lastSign = -1;
        }
    }

    if (crossings.size() < 10) {
        return Abc80Status::NOT_FOUND;
    }

    // Step 2: Convert zero-crossing half-cycle durations into a binary baseband signal
    // Threshold between mark (2400 Hz) and space (1200 Hz) half-cycles:
    const double halfCycleThreshold = (static_cast<double>(_config.sampleRate) / _config.markFreq +
                                       static_cast<double>(_config.sampleRate) / _config.spaceFreq) * 0.25;

    std::vector<uint8_t> demodSignal(sampleCount, 1);
    for (size_t k = 1; k < crossings.size(); ++k) {
        size_t dt = crossings[k] - crossings[k - 1];
        uint8_t val = (static_cast<double>(dt) < halfCycleThreshold) ? 1 : 0;
        for (size_t s = crossings[k - 1]; s < crossings[k]; ++s) {
            demodSignal[s] = val;
        }
    }

    // Majority-vote bit sampler across central 50% window
    auto sampleBit = [&](double bitCenter) -> bool {
        size_t s0 = static_cast<size_t>(std::max(0.0, std::round(bitCenter - samplesPerBit * 0.25)));
        size_t s1 = static_cast<size_t>(std::min(static_cast<double>(sampleCount), std::round(bitCenter + samplesPerBit * 0.25)));
        size_t ones = 0;
        for (size_t s = s0; s < s1; ++s) {
            if (demodSignal[s] == 1) {
                ones++;
            }
        }
        return (ones * 2 >= (s1 - s0));
    };

    // Step 3: UART Framer with DPLL bit-clock recovery
    size_t i = 0;
    const size_t minFrameSamples = static_cast<size_t>(11.0 * samplesPerBit);

    while (i + minFrameSamples < sampleCount) {
        if (demodSignal[i] == 1 && demodSignal[i + 1] == 0) {
            // Potential start bit transition at i + 1: verify center of start bit
            if (!sampleBit(static_cast<double>(i + 1) + samplesPerBit * 0.5)) {
                // Validated start bit: sample 8 data bits (LSB first)
                uint8_t byteVal = 0;
                for (int b = 0; b < 8; ++b) {
                    if (sampleBit(static_cast<double>(i + 1) + (static_cast<double>(b) + 1.5) * samplesPerBit)) {
                        byteVal |= static_cast<uint8_t>(1 << b);
                    }
                }

                // Verify stop bit 1
                if (sampleBit(static_cast<double>(i + 1) + 9.5 * samplesPerBit)) {
                    dataOut.push_back(byteVal);
                    // Advance cursor past stop bit to avoid false edge triggers
                    i = static_cast<size_t>(std::round(static_cast<double>(i + 1) + 10.0 * samplesPerBit));
                    continue;
                }
            }
        }
        i++;
    }

    return dataOut.empty() ? Abc80Status::NOT_FOUND : Abc80Status::OK;
}

Abc80Status Abc80Tape::saveWav(const std::string& filename, const std::vector<int16_t>& pcm) const
{
    std::ofstream out(filename, std::ios::binary);
    if (!out.is_open()) {
        return Abc80Status::BUS_ERROR;
    }

    const uint32_t numSamples = static_cast<uint32_t>(pcm.size());
    const uint32_t subchunk2Size = numSamples * 2; // 16-bit mono = 2 bytes per sample
    const uint32_t chunkSize = 36 + subchunk2Size;
    const uint16_t numChannels = 1;
    const uint16_t bitsPerSample = 16;
    const uint32_t byteRate = _config.sampleRate * numChannels * (bitsPerSample / 8);
    const uint16_t blockAlign = numChannels * (bitsPerSample / 8);
    const uint32_t subchunk1Size = 16;
    const uint16_t audioFormat = 1; // PCM

    // RIFF header
    out.write("RIFF", 4);
    out.write(reinterpret_cast<const char*>(&chunkSize), 4);
    out.write("WAVE", 4);

    // fmt subchunk
    out.write("fmt ", 4);
    out.write(reinterpret_cast<const char*>(&subchunk1Size), 4);
    out.write(reinterpret_cast<const char*>(&audioFormat), 2);
    out.write(reinterpret_cast<const char*>(&numChannels), 2);
    out.write(reinterpret_cast<const char*>(&_config.sampleRate), 4);
    out.write(reinterpret_cast<const char*>(&byteRate), 4);
    out.write(reinterpret_cast<const char*>(&blockAlign), 2);
    out.write(reinterpret_cast<const char*>(&bitsPerSample), 2);

    // data subchunk
    out.write("data", 4);
    out.write(reinterpret_cast<const char*>(&subchunk2Size), 4);
    out.write(reinterpret_cast<const char*>(pcm.data()), subchunk2Size);

    return out.good() ? Abc80Status::OK : Abc80Status::BUS_ERROR;
}

Abc80Status Abc80Tape::loadWav(const std::string& filename, std::vector<int16_t>& pcmOut, uint32_t* sampleRateOut) const
{
    std::ifstream in(filename, std::ios::binary);
    if (!in.is_open()) {
        return Abc80Status::NOT_FOUND;
    }

    char riffTag[4];
    in.read(riffTag, 4);
    if (std::memcmp(riffTag, "RIFF", 4) != 0) {
        return Abc80Status::INVALID_ARG;
    }

    uint32_t chunkSize = 0;
    in.read(reinterpret_cast<char*>(&chunkSize), 4);

    char waveTag[4];
    in.read(waveTag, 4);
    if (std::memcmp(waveTag, "WAVE", 4) != 0) {
        return Abc80Status::INVALID_ARG;
    }

    uint32_t sampleRate = 44100;
    uint16_t numChannels = 1;
    uint16_t bitsPerSample = 16;
    bool dataFound = false;

    // Scan chunks
    while (in.good() && !dataFound) {
        char chunkId[4];
        uint32_t chunkLen = 0;
        in.read(chunkId, 4);
        in.read(reinterpret_cast<char*>(&chunkLen), 4);

        if (std::memcmp(chunkId, "fmt ", 4) == 0) {
            uint16_t formatTag = 0;
            in.read(reinterpret_cast<char*>(&formatTag), 2);
            in.read(reinterpret_cast<char*>(&numChannels), 2);
            in.read(reinterpret_cast<char*>(&sampleRate), 4);
            uint32_t byteRate = 0;
            in.read(reinterpret_cast<char*>(&byteRate), 4);
            uint16_t blockAlign = 0;
            in.read(reinterpret_cast<char*>(&blockAlign), 2);
            in.read(reinterpret_cast<char*>(&bitsPerSample), 2);

            // Skip remaining bytes in fmt chunk if any
            if (chunkLen > 16) {
                in.seekg(chunkLen - 16, std::ios::cur);
            }
        } else if (std::memcmp(chunkId, "data", 4) == 0) {
            dataFound = true;
            const size_t numSamples = chunkLen / (bitsPerSample / 8);
            pcmOut.resize(numSamples / numChannels);

            if (numChannels == 1 && bitsPerSample == 16) {
                in.read(reinterpret_cast<char*>(pcmOut.data()), chunkLen);
            } else {
                // Read and downmix/convert
                for (size_t s = 0; s < pcmOut.size(); ++s) {
                    int16_t sample = 0;
                    in.read(reinterpret_cast<char*>(&sample), 2);
                    pcmOut[s] = sample;
                    // Discard extra channels
                    for (uint16_t ch = 1; ch < numChannels; ++ch) {
                        int16_t dummy = 0;
                        in.read(reinterpret_cast<char*>(&dummy), 2);
                    }
                }
            }
        } else {
            // Unknown chunk: skip
            in.seekg(chunkLen, std::ios::cur);
        }
    }

    if (!dataFound) {
        return Abc80Status::INVALID_ARG;
    }

    if (sampleRateOut) {
        *sampleRateOut = sampleRate;
    }

    return Abc80Status::OK;
}

Abc80Status Abc80Tape::encodeToWavFile(const std::string& filename, const uint8_t* data, size_t length) const
{
    std::vector<int16_t> pcm;
    Abc80Status st = modulate(data, length, pcm);
    if (st != Abc80Status::OK) {
        return st;
    }
    return saveWav(filename, pcm);
}

Abc80Status Abc80Tape::decodeFromWavFile(const std::string& filename, std::vector<uint8_t>& dataOut) const
{
    std::vector<int16_t> pcm;
    Abc80Status st = loadWav(filename, pcm);
    if (st != Abc80Status::OK) {
        return st;
    }
    return demodulate(pcm, dataOut);
}

Abc80Status Abc80Tape::saveCas(const std::string& filename, const uint8_t* data, size_t length) const
{
    std::ofstream out(filename, std::ios::binary);
    if (!out.is_open()) {
        return Abc80Status::BUS_ERROR;
    }
    if (data && length > 0) {
        out.write(reinterpret_cast<const char*>(data), length);
    }
    return out.good() ? Abc80Status::OK : Abc80Status::BUS_ERROR;
}

Abc80Status Abc80Tape::loadCas(const std::string& filename, std::vector<uint8_t>& dataOut) const
{
    std::ifstream in(filename, std::ios::binary | std::ios::ate);
    if (!in.is_open()) {
        return Abc80Status::NOT_FOUND;
    }
    std::streamsize size = in.tellg();
    in.seekg(0, std::ios::beg);
    dataOut.resize(static_cast<size_t>(size));
    if (size > 0) {
        in.read(reinterpret_cast<char*>(dataOut.data()), size);
    }
    return in.good() ? Abc80Status::OK : Abc80Status::BUS_ERROR;
}

std::string Abc80Tape::formatFileId(uint16_t fileId)
{
    char buf[8];
    std::snprintf(buf, sizeof(buf), "%04x", static_cast<unsigned>(fileId));
    return std::string(buf);
}

bool Abc80Tape::parseFileId(const std::string& hexStr, uint16_t& fileIdOut)
{
    if (hexStr.size() != 4) {
        return false;
    }
    uint16_t val = 0;
    for (char c : hexStr) {
        uint8_t nibble = 0;
        if (c >= '0' && c <= '9') {
            nibble = static_cast<uint8_t>(c - '0');
        } else if (c >= 'a' && c <= 'f') {
            nibble = static_cast<uint8_t>(10 + (c - 'a'));
        } else if (c >= 'A' && c <= 'F') {
            nibble = static_cast<uint8_t>(10 + (c - 'A'));
        } else {
            return false;
        }
        val = static_cast<uint16_t>((val << 4) | nibble);
    }
    fileIdOut = val;
    return true;
}

std::string Abc80Tape::sanitizeAnnotation(const std::string& raw)
{
    std::string out;
    out.reserve(kAnnotationMaxLen);
    for (char c : raw) {
        if (c == '\0' || c == '\r' || c == '\n') {
            break;
        }
        const unsigned char uc = static_cast<unsigned char>(c);
        if (uc >= 0x20 && uc < 0x7F) {
            out.push_back(c);
        } else {
            out.push_back(' ');
        }
        if (out.size() >= kAnnotationMaxLen) {
            break;
        }
    }
    return out;
}

uint8_t Abc80Tape::computeChecksum(const uint8_t* data, size_t length)
{
    uint8_t sum = 0;
    if (data != nullptr) {
        for (size_t i = 0; i < length; ++i) {
            sum = static_cast<uint8_t>(sum + data[i]);
        }
    }
    return sum;
}

namespace {

void appendPulseRun(std::vector<TapePulseRun>& runs, uint32_t halfPeriodTStates, uint32_t halfCycles)
{
    if (halfCycles == 0) {
        return;
    }
    if (!runs.empty() && runs.back().halfPeriodTStates == halfPeriodTStates) {
        runs.back().halfCycles += halfCycles;
    } else {
        runs.push_back(TapePulseRun{halfPeriodTStates, halfCycles});
    }
}

void appendRomBitRuns(std::vector<TapePulseRun>& runs, bool bit)
{
    // ROM outbit (0x034F):
    //   Bit 0 (out0): 12 cycles (24 half-cycles) of 2 kHz + 3 cycles (6 half-cycles) of 1 kHz
    //   Bit 1 (out1):  6 cycles (12 half-cycles) of 2 kHz + 6 cycles (12 half-cycles) of 1 kHz
    if (!bit) {
        appendPulseRun(runs, Abc80Tape::kRom2kHzHalfPeriodTStates, 24);
        appendPulseRun(runs, Abc80Tape::kRom1kHzHalfPeriodTStates, 6);
    } else {
        appendPulseRun(runs, Abc80Tape::kRom2kHzHalfPeriodTStates, 12);
        appendPulseRun(runs, Abc80Tape::kRom1kHzHalfPeriodTStates, 12);
    }
}

void appendRomByteRuns(std::vector<TapePulseRun>& runs, uint8_t byte)
{
    // ROM otbyte (0x033B): Start bit (0), 8 data bits (LSB first), Stop bit (1)
    appendRomBitRuns(runs, false);
    for (int b = 0; b < 8; ++b) {
        appendRomBitRuns(runs, ((byte >> b) & 1u) != 0);
    }
    appendRomBitRuns(runs, true);
}

struct RomCycleReader {
    const std::vector<TapePulseRun>& runs;
    size_t runIdx{0};
    uint32_t hcIdx{0};

    explicit RomCycleReader(const std::vector<TapePulseRun>& r) : runs(r) {}

    bool nextHalfPeriod(uint32_t& hpOut)
    {
        while (runIdx < runs.size() && hcIdx >= runs[runIdx].halfCycles) {
            runIdx++;
            hcIdx = 0;
        }
        if (runIdx >= runs.size()) {
            return false;
        }
        hpOut = runs[runIdx].halfPeriodTStates;
        hcIdx++;
        return true;
    }

    // Mirrors ROM period (0x0314): measures 1 HIGH half-period + 1 LOW half-period,
    // compares against midpd (42 * 33 = 1386 T-states), and rejects timeout (D != 0 -> > 8448 T-states).
    bool nextPeriod(bool& isShort2kHzOut)
    {
        uint32_t hp0 = 0;
        uint32_t hp1 = 0;
        if (!nextHalfPeriod(hp0) || !nextHalfPeriod(hp1)) {
            return false;
        }
        const uint32_t fullCycle = hp0 + hp1;
        if (fullCycle == 0 || fullCycle > 8448) {
            return false;
        }
        isShort2kHzOut = (fullCycle < Abc80Tape::kRomFullCycleThresholdTStates);
        return true;
    }
};

constexpr char kTapeMagic[8] = { 'A', 'B', 'C', '8', '0', 'T', 'A', 'P' };
constexpr uint16_t kTapeFormatVersion = 1;
constexpr uint32_t kMaxTapeRuns = 2000000;

void writeLe16(std::ostream& os, uint16_t val)
{
    const uint8_t b[2] = {
        static_cast<uint8_t>(val & 0xFF),
        static_cast<uint8_t>((val >> 8) & 0xFF)
    };
    os.write(reinterpret_cast<const char*>(b), 2);
}

void writeLe32(std::ostream& os, uint32_t val)
{
    const uint8_t b[4] = {
        static_cast<uint8_t>(val & 0xFF),
        static_cast<uint8_t>((val >> 8) & 0xFF),
        static_cast<uint8_t>((val >> 16) & 0xFF),
        static_cast<uint8_t>((val >> 24) & 0xFF)
    };
    os.write(reinterpret_cast<const char*>(b), 4);
}

bool readLe16(std::istream& is, uint16_t& valOut)
{
    uint8_t b[2]{};
    if (!is.read(reinterpret_cast<char*>(b), 2)) {
        return false;
    }
    valOut = static_cast<uint16_t>(b[0] | (static_cast<uint16_t>(b[1]) << 8));
    return true;
}

bool readLe32(std::istream& is, uint32_t& valOut)
{
    uint8_t b[4]{};
    if (!is.read(reinterpret_cast<char*>(b), 4)) {
        return false;
    }
    valOut = static_cast<uint32_t>(b[0]) |
             (static_cast<uint32_t>(b[1]) << 8) |
             (static_cast<uint32_t>(b[2]) << 16) |
             (static_cast<uint32_t>(b[3]) << 24);
    return true;
}

} // namespace

Abc80Status Abc80Tape::encodeProgramToRuns(
    uint16_t fileId,
    uint16_t startAddr,
    const uint8_t* data,
    size_t length,
    std::vector<TapePulseRun>& runsOut)
{
    runsOut.clear();
    if (data == nullptr || length == 0 || length > 0x10000) {
        return Abc80Status::INVALID_ARG;
    }
    const uint32_t endAddr32 = static_cast<uint32_t>(startAddr) + static_cast<uint32_t>(length) - 1u;
    if (endAddr32 > 0xFFFFu) {
        return Abc80Status::INVALID_ARG;
    }
    const uint16_t endAddr = static_cast<uint16_t>(endAddr32);
    const uint8_t checksum = computeChecksum(data, length);

    // 7-byte stepbf header (0x17BF..0x17C5): fileId (LE), startAddr (LE), endAddr (LE), checksum
    const uint8_t header[7] = {
        static_cast<uint8_t>(fileId & 0xFF),
        static_cast<uint8_t>((fileId >> 8) & 0xFF),
        static_cast<uint8_t>(startAddr & 0xFF),
        static_cast<uint8_t>((startAddr >> 8) & 0xFF),
        static_cast<uint8_t>(endAddr & 0xFF),
        static_cast<uint8_t>((endAddr >> 8) & 0xFF),
        checksum
    };

    runsOut.reserve(64 + length * 20);

    // 1. 4000 cycles (8000 half-cycles) of 1 kHz leader tone (ms1k)
    appendPulseRun(runsOut, kRom1kHzHalfPeriodTStates, kRomLeaderCycles * 2);

    // 2. 7 header bytes (tapout)
    for (int i = 0; i < 7; ++i) {
        appendRomByteRuns(runsOut, header[i]);
    }

    // 3. 4000 cycles (8000 half-cycles) of 2 kHz inter-block tone (ms2k)
    appendPulseRun(runsOut, kRom2kHzHalfPeriodTStates, kRomInterBlockCycles * 2);

    // 4. Payload bytes (tapout)
    for (size_t i = 0; i < length; ++i) {
        appendRomByteRuns(runsOut, data[i]);
    }

    // 5. 4000 cycles (8000 half-cycles) of 2 kHz trailer tone (ms2k)
    appendPulseRun(runsOut, kRom2kHzHalfPeriodTStates, kRomTrailerCycles * 2);

    return Abc80Status::OK;
}

Abc80Status Abc80Tape::decodeProgramFromRuns(
    const std::vector<TapePulseRun>& runs,
    uint16_t& fileIdOut,
    uint16_t& startAddrOut,
    uint16_t& endAddrOut,
    std::vector<uint8_t>& dataOut)
{
    dataOut.clear();
    if (runs.empty()) {
        return Abc80Status::INVALID_ARG;
    }

    RomCycleReader reader(runs);

    // Step 1 (lead1 at 0x01DB): wait for 1000 consecutive 1 kHz cycles
    uint32_t leaderCycles = 0;
    bool isShort = false;
    while (leaderCycles < 1000) {
        if (!reader.nextPeriod(isShort)) {
            return Abc80Status::NOT_FOUND;
        }
        if (isShort) {
            leaderCycles = 0;
        } else {
            leaderCycles++;
        }
    }

    // Step 2 (lead2 at 0x01E5): wait until 1 kHz carrier finishes (first 2 kHz cycle of header start bit)
    do {
        if (!reader.nextPeriod(isShort)) {
            return Abc80Status::NOT_FOUND;
        }
    } while (!isShort);

    // Exact ROM getbit (0x02EF) & gtbyte (0x02DD) state machines
    auto getBit = [&](bool& bitOut) -> bool {
        int8_t l = 0;
        bool seen1kHz = false;
        while (true) {
            bool shortCycle = false;
            if (!reader.nextPeriod(shortCycle)) {
                return false;
            }
            if (!shortCycle) {
                l = static_cast<int8_t>(l - 2);
                seen1kHz = true;
            } else {
                l = static_cast<int8_t>(l + 1);
                if (seen1kHz) {
                    bitOut = (l < 0);
                    return true;
                }
            }
        }
    };

    auto getByte = [&](uint8_t& byteOut) -> bool {
        bool startBit = false;
        if (!getBit(startBit)) {
            return false;
        }
        uint8_t val = 0;
        for (int b = 0; b < 8; ++b) {
            bool dataBit = false;
            if (!getBit(dataBit)) {
                return false;
            }
            if (dataBit) {
                val = static_cast<uint8_t>(val | (1u << b));
            }
        }
        bool stopBit = false;
        if (!getBit(stopBit)) {
            return false;
        }
        byteOut = val;
        return true;
    };

    // Step 3 (0x01EA): read 7 header bytes
    uint8_t hdr[7]{};
    for (int i = 0; i < 7; ++i) {
        if (!getByte(hdr[i])) {
            return Abc80Status::NOT_FOUND;
        }
    }

    const uint16_t fileId = static_cast<uint16_t>(hdr[0] | (static_cast<uint16_t>(hdr[1]) << 8));
    const uint16_t startAddr = static_cast<uint16_t>(hdr[2] | (static_cast<uint16_t>(hdr[3]) << 8));
    const uint16_t endAddr = static_cast<uint16_t>(hdr[4] | (static_cast<uint16_t>(hdr[5]) << 8));
    const uint8_t expectedSum = hdr[6];

    if (endAddr < startAddr) {
        return Abc80Status::INVALID_ARG;
    }
    const size_t payloadLen = static_cast<size_t>(endAddr - startAddr) + 1u;

    // Step 4 (0x0214): read payload block (getBit naturally rides through the 2 kHz inter-block carrier)
    std::vector<uint8_t> payload(payloadLen, 0);
    for (size_t i = 0; i < payloadLen; ++i) {
        if (!getByte(payload[i])) {
            return Abc80Status::NOT_FOUND;
        }
    }

    // Step 5 (0x0219): verify 8-bit sum checksum
    if (computeChecksum(payload.data(), payload.size()) != expectedSum) {
        return Abc80Status::BUS_ERROR;
    }

    fileIdOut = fileId;
    startAddrOut = startAddr;
    endAddrOut = endAddr;
    dataOut = std::move(payload);
    return Abc80Status::OK;
}

Abc80Status Abc80Tape::saveTapeFile(
    const std::string& filename,
    uint16_t fileId,
    const std::string& annotation,
    const std::vector<TapePulseRun>& runs)
{
    if (filename.empty()) {
        return Abc80Status::INVALID_ARG;
    }
    std::error_code ec;
    const std::filesystem::path p(filename);
    if (p.has_parent_path()) {
        std::filesystem::create_directories(p.parent_path(), ec);
    }

    std::ofstream out(filename, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
        return Abc80Status::BUS_ERROR;
    }

    out.write(kTapeMagic, 8);
    writeLe16(out, kTapeFormatVersion);
    writeLe16(out, fileId);

    char annotBuf[kAnnotationMaxLen];
    std::memset(annotBuf, 0, sizeof(annotBuf));
    const std::string cleanAnnot = sanitizeAnnotation(annotation);
    std::memcpy(annotBuf, cleanAnnot.data(), std::min(cleanAnnot.size(), kAnnotationMaxLen));
    out.write(annotBuf, kAnnotationMaxLen);

    const uint32_t runCount = static_cast<uint32_t>(runs.size());
    writeLe32(out, runCount);
    for (const TapePulseRun& r : runs) {
        writeLe32(out, r.halfPeriodTStates);
        writeLe32(out, r.halfCycles);
    }

    return out.good() ? Abc80Status::OK : Abc80Status::BUS_ERROR;
}

Abc80Status Abc80Tape::loadTapeFile(
    const std::string& filename,
    uint16_t& fileIdOut,
    std::string& annotationOut,
    std::vector<TapePulseRun>& runsOut)
{
    runsOut.clear();
    annotationOut.clear();

    std::ifstream in(filename, std::ios::binary);
    if (!in.is_open()) {
        return Abc80Status::NOT_FOUND;
    }

    char magic[8]{};
    if (!in.read(magic, 8) || std::memcmp(magic, kTapeMagic, 8) != 0) {
        return Abc80Status::INVALID_ARG;
    }

    uint16_t version = 0;
    uint16_t fileId = 0;
    if (!readLe16(in, version) || version != kTapeFormatVersion || !readLe16(in, fileId)) {
        return Abc80Status::INVALID_ARG;
    }

    char annotBuf[kAnnotationMaxLen + 1];
    std::memset(annotBuf, 0, sizeof(annotBuf));
    if (!in.read(annotBuf, kAnnotationMaxLen)) {
        return Abc80Status::INVALID_ARG;
    }

    uint32_t runCount = 0;
    if (!readLe32(in, runCount) || runCount > kMaxTapeRuns) {
        return Abc80Status::INVALID_ARG;
    }

    std::vector<TapePulseRun> runs(runCount);
    for (uint32_t i = 0; i < runCount; ++i) {
        if (!readLe32(in, runs[i].halfPeriodTStates) || !readLe32(in, runs[i].halfCycles)) {
            return Abc80Status::INVALID_ARG;
        }
    }

    fileIdOut = fileId;
    annotationOut = sanitizeAnnotation(std::string(annotBuf));
    runsOut = std::move(runs);
    return Abc80Status::OK;
}

Abc80Status Abc80Tape::inspectTapeFile(
    const std::string& filename,
    TapeFileInfo& infoOut)
{
    uint16_t fileId = 0;
    std::string annot;
    std::vector<TapePulseRun> runs;
    Abc80Status st = loadTapeFile(filename, fileId, annot, runs);
    if (st != Abc80Status::OK) {
        return st;
    }

    infoOut = TapeFileInfo{};
    infoOut.fileId = fileId;
    infoOut.annotation = annot;
    uint64_t totalT = 0;
    for (const TapePulseRun& r : runs) {
        totalT += static_cast<uint64_t>(r.halfPeriodTStates) * static_cast<uint64_t>(r.halfCycles);
    }
    infoOut.totalTStates = totalT;
    infoOut.durationSec = static_cast<double>(totalT) / static_cast<double>(ABC80_CPU_CLOCK_HZ);

    uint16_t decodedId = 0;
    uint16_t startAddr = 0;
    uint16_t endAddr = 0;
    std::vector<uint8_t> payload;
    if (decodeProgramFromRuns(runs, decodedId, startAddr, endAddr, payload) == Abc80Status::OK) {
        infoOut.hasValidProgram = true;
        infoOut.startAddr = startAddr;
        infoOut.endAddr = endAddr;
        infoOut.payloadBytes = payload.size();
        infoOut.checksum = computeChecksum(payload.data(), payload.size());
    }
    return Abc80Status::OK;
}

CassetteDeck::CassetteDeck(Abc80Ppi* ppi)
    : _ppi(ppi)
{
}

void CassetteDeck::setTapesDir(const std::string& dir)
{
    _tapesDir = dir;
    if (!_tapesDir.empty()) {
        std::error_code ec;
        std::filesystem::create_directories(_tapesDir, ec);
    }
}

std::string CassetteDeck::tapePathForId(uint16_t fileId) const
{
    if (_tapesDir.empty()) {
        return "";
    }
    return _tapesDir + "/" + Abc80Tape::formatFileId(fileId) + ".tape";
}

Abc80Status CassetteDeck::seedDefaultTapesIfEmpty() const
{
    return seedDefaultTapes(false);
}

Abc80Status CassetteDeck::seedDefaultTapes(bool overwrite) const
{
    if (_tapesDir.empty()) {
        return Abc80Status::INVALID_ARG;
    }
    std::error_code ec;
    std::filesystem::create_directories(_tapesDir, ec);
    if (ec) {
        return Abc80Status::BUS_ERROR;
    }

    if (!overwrite) {
        // Check if any xxxx.tape file already exists
        for (const auto& entry : std::filesystem::directory_iterator(_tapesDir, ec)) {
            if (entry.is_regular_file() && entry.path().extension() == ".tape") {
                return Abc80Status::OK;
            }
        }
    }

    struct SeedProgram {
        uint16_t fileId;
        uint16_t startAddr;
        const char* annotation;
        std::vector<uint8_t> code;
    };

    const SeedProgram seeds[] = {
        {
            0x0001,
            0x1000,
            "BCD Up-Counter (1000H)",
            {
                0xAF,             // 1000: XOR A
                0x32, 0x30, 0x10, // 1001: LD (1030H),A
                0x3A, 0x30, 0x10, // 1004: LD A,(1030H)
                0xCD, 0x11, 0x05, // 1007: CALL DADP (0511H)
                0x06, 0x20,       // 100A: LD B,20H
                0xCD, 0xCD, 0x04, // 100C: CALL SCD_K1 (04CDH)
                0x10, 0xFB,       // 100F: DJNZ 100CH
                0x3A, 0x30, 0x10, // 1011: LD A,(1030H)
                0xC6, 0x01,       // 1014: ADD A,01H
                0x27,             // 1016: DAA
                0x18, 0xE8        // 1017: JR 1001H
            }
        },
        {
            0x0002,
            0x1000,
            "Segment Chaser (1000H)",
            {
                0x3E, 0xFE,       // 1000: LD A,0FEH
                0xD3, 0x81,       // 1002: OUT (81H),A
                0x47,             // 1004: LD B,A
                0x3E, 0xC0,       // 1005: LD A,0C0H
                0xD3, 0x82,       // 1007: OUT (82H),A
                0x21, 0x00, 0x30, // 1009: LD HL,3000H
                0x2B,             // 100C: DEC HL
                0x7C,             // 100D: LD A,H
                0xB5,             // 100E: OR L
                0x20, 0xFB,       // 100F: JR NZ,100CH
                0x78,             // 1011: LD A,B
                0xCB, 0x07,       // 1012: RLC A
                0x18, 0xEC        // 1014: JR 1002H
            }
        },
        {
            0x0003,
            0x1000,
            "Dual-Tone Chime (1000H)",
            {
                0x21, 0xFA, 0x00, // 1000: LD HL,00FAH
                0xCD, 0x70, 0x04, // 1003: CALL MS1K (0470H)
                0x21, 0xF4, 0x01, // 1006: LD HL,01F4H
                0xCD, 0x75, 0x04, // 1009: CALL MS2K (0475H)
                0xC3, 0x00, 0x10  // 100C: JP 1000H
            }
        },
        {
            0x0004,
            0x1000,
            "Fibonacci Display (1000H)",
            {
                0x11, 0x40, 0x10, // 1000: LD DE,1040H
                0x01, 0x01, 0x00, // 1003: LD BC,0001H (B=prev=00, C=curr=01)
                0x79,             // 1006: LD A,C
                0x12,             // 1007: LD (DE),A
                0xCD, 0x11, 0x05, // 1008: CALL DADP (0511H)
                0xCD, 0x04, 0x05, // 100B: CALL ADRSDP (0504H)
                0xCD, 0x64, 0x04, // 100E: CALL MS3K17 (0464H)
                0xC5,             // 1011: PUSH BC
                0x06, 0x28,       // 1012: LD B,28H
                0xCD, 0xCD, 0x04, // 1014: CALL SCD_K1 (04CDH)
                0x10, 0xFB,       // 1017: DJNZ 1014H
                0xC1,             // 1019: POP BC
                0x79,             // 101A: LD A,C
                0x80,             // 101B: ADD A,B
                0x27,             // 101C: DAA
                0x38, 0xE1,       // 101D: JR C,1000H
                0x41,             // 101F: LD B,C
                0x4F,             // 1020: LD C,A
                0x13,             // 1021: INC DE
                0x18, 0xE2        // 1022: JR 1006H
            }
        }
    };

    for (const SeedProgram& s : seeds) {
        std::vector<TapePulseRun> runs;
        Abc80Status st = Abc80Tape::encodeProgramToRuns(s.fileId, s.startAddr, s.code.data(), s.code.size(), runs);
        if (st != Abc80Status::OK) {
            return st;
        }
        st = Abc80Tape::saveTapeFile(tapePathForId(s.fileId), s.fileId, s.annotation, runs);
        if (st != Abc80Status::OK) {
            return st;
        }
    }
    return Abc80Status::OK;
}

std::vector<TapeFileInfo> CassetteDeck::listTapes() const
{
    std::vector<TapeFileInfo> list;
    if (_tapesDir.empty()) {
        return list;
    }
    std::error_code ec;
    if (!std::filesystem::exists(_tapesDir, ec)) {
        return list;
    }

    for (const auto& entry : std::filesystem::directory_iterator(_tapesDir, ec)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".tape") {
            continue;
        }
        const std::string stem = entry.path().stem().string();
        uint16_t parsedId = 0;
        if (!Abc80Tape::parseFileId(stem, parsedId)) {
            continue;
        }
        TapeFileInfo info;
        if (Abc80Tape::inspectTapeFile(entry.path().string(), info) == Abc80Status::OK) {
            info.fileId = parsedId;
            list.push_back(std::move(info));
        }
    }

    std::sort(list.begin(), list.end(), [](const TapeFileInfo& a, const TapeFileInfo& b) {
        return a.fileId < b.fileId;
    });
    return list;
}

void CassetteDeck::recomputeTotalTStates() noexcept
{
    uint64_t total = 0;
    for (const TapePulseRun& r : _runs) {
        total += static_cast<uint64_t>(r.halfPeriodTStates) * static_cast<uint64_t>(r.halfCycles);
    }
    _totalTStates = total;
}

Abc80Status CassetteDeck::selectTape(uint16_t fileId)
{
    stop();
    _selectedFileId = fileId;
    _runs.clear();
    _annotation.clear();

    const std::string path = tapePathForId(fileId);
    if (!path.empty()) {
        std::error_code ec;
        if (std::filesystem::exists(path, ec)) {
            uint16_t diskId = 0;
            std::string diskAnnot;
            std::vector<TapePulseRun> diskRuns;
            Abc80Status st = Abc80Tape::loadTapeFile(path, diskId, diskAnnot, diskRuns);
            if (st == Abc80Status::OK) {
                _annotation = diskAnnot;
                _runs = std::move(diskRuns);
            }
        }
    }

    recomputeTotalTStates();
    rewind();
    return Abc80Status::OK;
}

Abc80Status CassetteDeck::setAnnotation(uint16_t fileId, const std::string& annotation)
{
    const std::string clean = Abc80Tape::sanitizeAnnotation(annotation);
    if (fileId == _selectedFileId) {
        _annotation = clean;
    }

    const std::string path = tapePathForId(fileId);
    if (path.empty()) {
        return Abc80Status::OK;
    }

    std::vector<TapePulseRun> runsToSave;
    std::error_code ec;
    if (std::filesystem::exists(path, ec)) {
        uint16_t diskId = 0;
        std::string oldAnnot;
        Abc80Tape::loadTapeFile(path, diskId, oldAnnot, runsToSave);
    } else if (fileId == _selectedFileId) {
        runsToSave = _runs;
    }

    return Abc80Tape::saveTapeFile(path, fileId, clean, runsToSave);
}

Abc80Status CassetteDeck::loadRunsDirect(
    uint16_t fileId,
    const std::string& annotation,
    const std::vector<TapePulseRun>& runs)
{
    stop();
    _selectedFileId = fileId;
    _annotation = Abc80Tape::sanitizeAnnotation(annotation);
    _runs = runs;
    recomputeTotalTStates();
    rewind();
    return Abc80Status::OK;
}

Abc80Status CassetteDeck::play()
{
    if (_state == TapeDeckState::Recording) {
        stop();
    }

    if (_runs.empty() && !_tapesDir.empty()) {
        const std::string path = tapePathForId(_selectedFileId);
        std::error_code ec;
        if (std::filesystem::exists(path, ec)) {
            uint16_t diskId = 0;
            std::string diskAnnot;
            std::vector<TapePulseRun> diskRuns;
            if (Abc80Tape::loadTapeFile(path, diskId, diskAnnot, diskRuns) == Abc80Status::OK) {
                _annotation = diskAnnot;
                _runs = std::move(diskRuns);
                recomputeTotalTStates();
            }
        }
    }

    if (_runs.empty()) {
        return Abc80Status::NOT_FOUND;
    }

    if (_runIndex >= _runs.size()) {
        rewind();
    }

    _state = TapeDeckState::Playing;
    _stepInitialized = false;
    _cpuPollingTape = false;
    _lastPortAReadTStates = 0;
    if (_halfCycleRemaining == 0) {
        _halfCycleRemaining = _runs[_runIndex].halfPeriodTStates;
        _currentLevel = true;
    }
    _currentToneHz = (_runs[_runIndex].halfPeriodTStates >= 650) ? 1000 : 2000;
    if (_ppi != nullptr) {
        _ppi->setTapeInput(_currentLevel);
    }
    return Abc80Status::OK;
}

Abc80Status CassetteDeck::record(const std::string& annotationOverride)
{
    stop();
    _state = TapeDeckState::Recording;
    if (!annotationOverride.empty()) {
        _pendingRecordAnnotation = Abc80Tape::sanitizeAnnotation(annotationOverride);
        _annotation = _pendingRecordAnnotation;
    } else {
        _pendingRecordAnnotation = _annotation;
    }
    _rawHalfPeriods.clear();
    _lastEdgeTStates = 0;
    _hasLastEdge = false;
    _recStartLevel = false;
    _recLeaderStreak = 0;
    _recLeaderLocked = false;
    _elapsedTStates = 0;
    _totalTStates = 0;
    _currentToneHz = 0;
    _stepInitialized = false;
    return Abc80Status::OK;
}

void CassetteDeck::rewind()
{
    if (_state != TapeDeckState::Stopped) {
        stop();
    }
    _runIndex = 0;
    _halfCycleIndex = 0;
    _halfCycleRemaining = _runs.empty() ? 0 : _runs[0].halfPeriodTStates;
    _currentLevel = false;
    _elapsedTStates = 0;
    _cpuPollingTape = false;
    _lastPortAReadTStates = 0;
    _currentToneHz = 0;
    if (_ppi != nullptr) {
        _ppi->setTapeInput(false);
    }
}

void CassetteDeck::stop()
{
    if (_state == TapeDeckState::Recording) {
        finalizeRecording();
    }
    _state = TapeDeckState::Stopped;
    _currentToneHz = 0;
    _stepInitialized = false;
    if (_ppi != nullptr) {
        _ppi->setTapeInput(false);
    }
}

void CassetteDeck::onTapeInputSampled(uint64_t nowTStates)
{
    if (_state == TapeDeckState::Playing) {
        // In ROM period (0x0314), IN A,(80H) is polled every 33 T-states.
        // In ROM scd_k1 (0x04CD), IN A,(80H) is polled once per digit column (~2928 T-states).
        if (_lastPortAReadTStates != 0 && nowTStates > _lastPortAReadTStates &&
            (nowTStates - _lastPortAReadTStates) <= 120) {
            _cpuPollingTape = true;
        }
        _lastPortAReadTStates = nowTStates;
    }
}

void CassetteDeck::onSpeakerEdge(uint64_t nowTStates, bool newLevel)
{
    if (_state != TapeDeckState::Recording) {
        return;
    }

    if (!_hasLastEdge) {
        _lastEdgeTStates = nowTStates;
        _hasLastEdge = true;
        _recStartLevel = newLevel;
        return;
    }

    if (nowTStates <= _lastEdgeTStates) {
        _lastEdgeTStates = nowTStates;
        return;
    }

    const uint64_t delta64 = nowTStates - _lastEdgeTStates;
    _lastEdgeTStates = nowTStates;

    if (!_recLeaderLocked) {
        // Wait for authentic 1 kHz leader tone (ms1k half-period = 889 T-states).
        // Ignore 3 kHz key-click beeps (226 T-states) and 80-cycle error beeps (160 half-periods).
        if (delta64 >= 650 && delta64 <= 1200) {
            if (_rawHalfPeriods.empty()) {
                // The half-period of duration delta64 that just ended had level !newLevel.
                // If !newLevel == false (LOW), the initial HIGH half-period of sqwave had no rising
                // edge because PC7 was already HIGH in scd_k1; prepend 889 T-states so (HIGH, LOW) aligns.
                if (newLevel) {
                    _rawHalfPeriods.push_back(Abc80Tape::kRom1kHzHalfPeriodTStates);
                    _elapsedTStates += Abc80Tape::kRom1kHzHalfPeriodTStates;
                }
            }
            _rawHalfPeriods.push_back(static_cast<uint32_t>(delta64));
            _elapsedTStates += delta64;
            _recLeaderStreak++;
            _currentToneHz = 1000;
            if (_recLeaderStreak >= 400) {
                _recLeaderLocked = true;
            }
        } else {
            _rawHalfPeriods.clear();
            _elapsedTStates = 0;
            _recLeaderStreak = 0;
            _currentToneHz = 0;
        }
        return;
    }

    if (delta64 <= 4000) {
        _rawHalfPeriods.push_back(static_cast<uint32_t>(delta64));
        _elapsedTStates += delta64;
        _currentToneHz = (delta64 >= 650) ? 1000 : 2000;
    }
}

void CassetteDeck::step(uint64_t nowTStates)
{
    if (_state == TapeDeckState::Stopped) {
        return;
    }

    if (!_stepInitialized || nowTStates < _lastStepTStates) {
        _lastStepTStates = nowTStates;
        _stepInitialized = true;
        return;
    }

    uint64_t dt = nowTStates - _lastStepTStates;
    _lastStepTStates = nowTStates;
    if (dt == 0) {
        return;
    }

    if (_state == TapeDeckState::Recording) {
        // Auto-stop recording when PC7 stays silent for > 50,000 T-states (~28 ms) after leader lock
        if (_recLeaderLocked && _hasLastEdge && nowTStates > _lastEdgeTStates &&
            (nowTStates - _lastEdgeTStates) > 50000) {
            stop();
        }
        return;
    }

    // Playing state: advance through _runs and toggle PA7 (_ppi->setTapeInput)
    while (dt > 0 && _state == TapeDeckState::Playing) {
        if (_runIndex >= _runs.size()) {
            stop();
            break;
        }

        if (dt < _halfCycleRemaining) {
            _halfCycleRemaining -= static_cast<uint32_t>(dt);
            _elapsedTStates += dt;
            dt = 0;
        } else {
            dt -= _halfCycleRemaining;
            _elapsedTStates += _halfCycleRemaining;
            _currentLevel = !_currentLevel;
            if (_ppi != nullptr) {
                _ppi->setTapeInput(_currentLevel);
            }
            _halfCycleIndex++;

            // While in the initial 1 kHz leader run and the CPU has not started tight-polling PA7
            // in ROM period (0x0314), sustain the leader tone so the user has unlimited time to type
            // FROM TAPE xxxx GO on the keypad after pressing PLAY.
            if (_runIndex == 0 && !_cpuPollingTape &&
                _runs[0].halfPeriodTStates >= 650 &&
                _runs[0].halfCycles >= 6000 &&
                _halfCycleIndex >= 2000 &&
                (_halfCycleIndex % 2 == 0)) {
                _halfCycleIndex -= 2;
            }

            if (_halfCycleIndex >= _runs[_runIndex].halfCycles) {
                _runIndex++;
                _halfCycleIndex = 0;
                if (_runIndex >= _runs.size()) {
                    stop();
                    break;
                }
            }
            _halfCycleRemaining = _runs[_runIndex].halfPeriodTStates;
            _currentToneHz = (_runs[_runIndex].halfPeriodTStates >= 650) ? 1000 : 2000;
        }
    }
}

void CassetteDeck::finalizeRecording()
{
    if (!_recLeaderLocked || _rawHalfPeriods.size() < 2000) {
        _rawHalfPeriods.clear();
        _recLeaderLocked = false;
        _recLeaderStreak = 0;
        return;
    }

    // Convert measured (HIGH, LOW) half-period pairs into canonical pulse runs
    // using the exact ROM period threshold (1386 T-states per full cycle).
    std::vector<TapePulseRun> recordedRuns;
    recordedRuns.reserve(_rawHalfPeriods.size() / 8);
    for (size_t i = 0; i + 1 < _rawHalfPeriods.size(); i += 2) {
        const uint32_t fullCycle = _rawHalfPeriods[i] + _rawHalfPeriods[i + 1];
        if (fullCycle == 0 || fullCycle > 8448) {
            break;
        }
        const uint32_t canonicalHalf =
            (fullCycle < Abc80Tape::kRomFullCycleThresholdTStates)
                ? Abc80Tape::kRom2kHzHalfPeriodTStates
                : Abc80Tape::kRom1kHzHalfPeriodTStates;
        appendPulseRun(recordedRuns, canonicalHalf, 2);
    }

    _rawHalfPeriods.clear();
    _recLeaderLocked = false;
    _recLeaderStreak = 0;

    if (recordedRuns.empty()) {
        return;
    }

    // Demodulate the 7-byte ROM header from the recorded audio stream so we save
    // to the exact 4-digit hex File ID (xxxx.tape) the user entered in TO TAPE!
    uint16_t decodedFileId = _selectedFileId;
    uint16_t startAddr = 0;
    uint16_t endAddr = 0;
    std::vector<uint8_t> payload;
    if (Abc80Tape::decodeProgramFromRuns(recordedRuns, decodedFileId, startAddr, endAddr, payload) ==
        Abc80Status::OK) {
        _selectedFileId = decodedFileId;
    }

    std::string finalAnnot = _pendingRecordAnnotation;
    if (finalAnnot.empty() && !_tapesDir.empty()) {
        const std::string existingPath = tapePathForId(_selectedFileId);
        std::error_code ec;
        if (std::filesystem::exists(existingPath, ec)) {
            uint16_t diskId = 0;
            std::string diskAnnot;
            std::vector<TapePulseRun> dummyRuns;
            if (Abc80Tape::loadTapeFile(existingPath, diskId, diskAnnot, dummyRuns) == Abc80Status::OK) {
                finalAnnot = diskAnnot;
            }
        }
    }
    if (finalAnnot.empty() && endAddr >= startAddr && !payload.empty()) {
        char buf[33];
        std::snprintf(buf, sizeof(buf), "PROG %04X (%04X-%04X)",
                      static_cast<unsigned>(_selectedFileId),
                      static_cast<unsigned>(startAddr),
                      static_cast<unsigned>(endAddr));
        finalAnnot = Abc80Tape::sanitizeAnnotation(buf);
    }

    _annotation = finalAnnot;
    _runs = std::move(recordedRuns);
    recomputeTotalTStates();

    if (!_tapesDir.empty()) {
        Abc80Tape::saveTapeFile(tapePathForId(_selectedFileId), _selectedFileId, _annotation, _runs);
    }

    // Reset playback cursor to start of newly recorded tape
    _runIndex = 0;
    _halfCycleIndex = 0;
    _halfCycleRemaining = _runs.empty() ? 0 : _runs[0].halfPeriodTStates;
    _currentLevel = false;
    _elapsedTStates = 0;
    _cpuPollingTape = false;
    _lastPortAReadTStates = 0;
}

uint32_t CassetteDeck::counter() const noexcept
{
    // 1 mechanical counter unit per ~0.25s of tape travel (447,500 T-states), wrapping at 1000 (000..999)
    return static_cast<uint32_t>((_elapsedTStates / 447500ULL) % 1000ULL);
}

double CassetteDeck::progress() const noexcept
{
    if (_state == TapeDeckState::Recording) {
        // Visual progress during recording (wraps smoothly over ~30s window)
        const double sec = static_cast<double>(_elapsedTStates) / static_cast<double>(ABC80_CPU_CLOCK_HZ);
        const double p = std::fmod(sec / 30.0, 1.0);
        return std::clamp(p, 0.0, 1.0);
    }
    if (_totalTStates == 0) {
        return 0.0;
    }
    const double p = static_cast<double>(_elapsedTStates) / static_cast<double>(_totalTStates);
    return std::clamp(p, 0.0, 1.0);
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
