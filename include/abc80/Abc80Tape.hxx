/*
 * Abc80Tape.hxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#ifndef ABC80_TAPE_HXX
#define ABC80_TAPE_HXX

#include <abc80/Abc80Types.hxx>
#include <cstdint>
#include <vector>
#include <string>

namespace abc80 {

struct TapeConfig {
    uint32_t sampleRate = 44100;    // Audio sample rate (Hz)
    uint32_t markFreq = 2400;       // Frequency for logic '1' (Hz)
    uint32_t spaceFreq = 1200;      // Frequency for logic '0' (Hz)
    uint32_t baudRate = 1200;       // Modulation baud rate (bps)
    int16_t  amplitude = 24000;     // PCM peak amplitude (0..32767)
    uint32_t leaderCycles = 120;    // Number of leader mark cycles
    uint32_t trailerCycles = 40;    // Number of trailer mark cycles
};

class Abc80Tape {
public:
    explicit Abc80Tape(const TapeConfig& config = TapeConfig());
    ~Abc80Tape() = default;

    const TapeConfig& getConfig() const noexcept { return _config; }
    void setConfig(const TapeConfig& config) noexcept { _config = config; }

    // FSK Modulation: Convert binary payload into 16-bit mono PCM sample stream
    Abc80Status modulate(const uint8_t* data, size_t length, std::vector<int16_t>& pcmOut) const;
    Abc80Status modulate(const std::vector<uint8_t>& data, std::vector<int16_t>& pcmOut) const {
        return modulate(data.data(), data.size(), pcmOut);
    }

    // FSK Demodulation: Decode 16-bit mono PCM samples back into binary data bytes via zero-crossing analysis
    Abc80Status demodulate(const int16_t* pcm, size_t sampleCount, std::vector<uint8_t>& dataOut) const;
    Abc80Status demodulate(const std::vector<int16_t>& pcm, std::vector<uint8_t>& dataOut) const {
        return demodulate(pcm.data(), pcm.size(), dataOut);
    }

    // Standard WAV File Container (RIFF WAVE 16-bit Mono PCM)
    Abc80Status saveWav(const std::string& filename, const std::vector<int16_t>& pcm) const;
    Abc80Status loadWav(const std::string& filename, std::vector<int16_t>& pcmOut, uint32_t* sampleRateOut = nullptr) const;

    // End-to-end WAV file encoding & decoding
    Abc80Status encodeToWavFile(const std::string& filename, const uint8_t* data, size_t length) const;
    Abc80Status decodeFromWavFile(const std::string& filename, std::vector<uint8_t>& dataOut) const;

    // CAS / Raw Binary Container
    Abc80Status saveCas(const std::string& filename, const uint8_t* data, size_t length) const;
    Abc80Status loadCas(const std::string& filename, std::vector<uint8_t>& dataOut) const;

private:
    TapeConfig _config;

    void appendTone(std::vector<int16_t>& pcm, double& phase, uint32_t freq, uint32_t sampleCount) const;
    void appendBit(std::vector<int16_t>& pcm, double& phase, bool bit) const;
    void appendByte(std::vector<int16_t>& pcm, double& phase, uint8_t byte) const;
};

} // namespace abc80

#endif // ABC80_TAPE_HXX

/*
 * Local variables:
 * mode: C++
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
