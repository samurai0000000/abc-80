/*
 * Abc80Tape.cxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include <abc80/Abc80Tape.hxx>
#include <cmath>
#include <fstream>
#include <cstring>
#include <algorithm>

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
