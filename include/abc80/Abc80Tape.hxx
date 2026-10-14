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

struct TapePulseRun {
    uint32_t halfPeriodTStates{0};
    uint32_t halfCycles{0};
};

struct TapeFileInfo {
    uint16_t fileId{0};
    std::string annotation;
    uint16_t startAddr{0};
    uint16_t endAddr{0};
    uint8_t checksum{0};
    bool hasValidProgram{false};
    uint64_t totalTStates{0};
    double durationSec{0.0};
    size_t payloadBytes{0};
};

enum class TapeDeckState {
    Stopped = 0,
    Playing = 1,
    Recording = 2
};

class Abc80Ppi;

class Abc80Tape {
public:
    static constexpr uint32_t kRom1kHzHalfPeriodTStates = 889;
    static constexpr uint32_t kRom2kHzHalfPeriodTStates = 447;
    static constexpr uint32_t kRomFullCycleThresholdTStates = 1386;
    static constexpr uint32_t kRomLeaderCycles = 4000;
    static constexpr uint32_t kRomInterBlockCycles = 4000;
    static constexpr uint32_t kRomTrailerCycles = 4000;
    static constexpr size_t kAnnotationMaxLen = 32;

    explicit Abc80Tape(const TapeConfig& config = TapeConfig());
    ~Abc80Tape() = default;

    const TapeConfig& getConfig() const noexcept { return _config; }
    void setConfig(const TapeConfig& config) noexcept { _config = config; }

    // Authentic ABC-80 Monitor ROM (ewt 0193H / ert 01CEH) Pulse-Run Codec
    static std::string formatFileId(uint16_t fileId);
    static bool parseFileId(const std::string& hexStr, uint16_t& fileIdOut);
    static std::string sanitizeAnnotation(const std::string& raw);
    static uint8_t computeChecksum(const uint8_t* data, size_t length);

    static Abc80Status encodeProgramToRuns(
        uint16_t fileId,
        uint16_t startAddr,
        const uint8_t* data,
        size_t length,
        std::vector<TapePulseRun>& runsOut);

    static Abc80Status decodeProgramFromRuns(
        const std::vector<TapePulseRun>& runs,
        uint16_t& fileIdOut,
        uint16_t& startAddrOut,
        uint16_t& endAddrOut,
        std::vector<uint8_t>& dataOut);

    // Binary .tape Container (~/.config/abc-80/tapes/xxxx.tape)
    static Abc80Status saveTapeFile(
        const std::string& filename,
        uint16_t fileId,
        const std::string& annotation,
        const std::vector<TapePulseRun>& runs);

    static Abc80Status loadTapeFile(
        const std::string& filename,
        uint16_t& fileIdOut,
        std::string& annotationOut,
        std::vector<TapePulseRun>& runsOut);

    static Abc80Status inspectTapeFile(
        const std::string& filename,
        TapeFileInfo& infoOut);

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

class CassetteDeck {
public:
    explicit CassetteDeck(Abc80Ppi* ppi = nullptr);
    ~CassetteDeck() = default;

    void attachPpi(Abc80Ppi* ppi) noexcept { _ppi = ppi; }
    void setTapesDir(const std::string& dir);
    const std::string& tapesDir() const noexcept { return _tapesDir; }
    std::string tapePathForId(uint16_t fileId) const;

    // Seed built-in demo programs (0001.tape .. 0004.tape)
    Abc80Status seedDefaultTapes(bool overwrite = false) const;
    Abc80Status seedDefaultTapesIfEmpty() const;
    std::vector<TapeFileInfo> listTapes() const;

    // Slot selection and 32-char annotation management
    Abc80Status selectTape(uint16_t fileId);
    Abc80Status setAnnotation(uint16_t fileId, const std::string& annotation);
    Abc80Status loadRunsDirect(uint16_t fileId, const std::string& annotation, const std::vector<TapePulseRun>& runs);

    // Transport controls
    Abc80Status play();
    Abc80Status record(const std::string& annotationOverride = "");
    void rewind();
    void stop();

    // Hardware hooks invoked by Abc80Board / Abc80Ppi
    void step(uint64_t nowTStates);
    void onSpeakerEdge(uint64_t nowTStates, bool newLevel);
    void onTapeInputSampled(uint64_t nowTStates);

    // Live telemetry accessors
    TapeDeckState state() const noexcept { return _state; }
    bool isActive() const noexcept { return _state != TapeDeckState::Stopped; }
    uint16_t selectedFileId() const noexcept { return _selectedFileId; }
    std::string selectedFileIdHex() const { return Abc80Tape::formatFileId(_selectedFileId); }
    const std::string& annotation() const noexcept { return _annotation; }
    uint32_t counter() const noexcept;
    double progress() const noexcept;
    uint32_t currentToneHz() const noexcept { return _currentToneHz; }
    const std::vector<TapePulseRun>& runs() const noexcept { return _runs; }

private:
    void finalizeRecording();
    void recomputeTotalTStates() noexcept;

    Abc80Ppi* _ppi{nullptr};
    std::string _tapesDir;
    TapeDeckState _state{TapeDeckState::Stopped};
    uint16_t _selectedFileId{0x0001};
    std::string _annotation;
    std::string _pendingRecordAnnotation;

    // Playback state
    std::vector<TapePulseRun> _runs;
    uint64_t _totalTStates{0};
    uint64_t _elapsedTStates{0};
    size_t _runIndex{0};
    uint32_t _halfCycleIndex{0};
    uint32_t _halfCycleRemaining{0};
    bool _currentLevel{false};
    uint64_t _lastStepTStates{0};
    bool _stepInitialized{false};
    uint64_t _lastPortAReadTStates{0};
    bool _cpuPollingTape{false};
    uint32_t _currentToneHz{0};

    // Recording state
    std::vector<uint32_t> _rawHalfPeriods;
    uint64_t _lastEdgeTStates{0};
    bool _hasLastEdge{false};
    bool _recStartLevel{false};
    uint32_t _recLeaderStreak{0};
    bool _recLeaderLocked{false};
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
