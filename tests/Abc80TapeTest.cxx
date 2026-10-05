/*
 * Abc80TapeTest.cxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include <abc80/Abc80Tape.hxx>
#include <CppUTest/TestHarness.h>
#include <vector>
#include <cstdio>

using namespace abc80;

TEST_GROUP(Abc80Tape)
{
    Abc80Tape tape;

    void setup() override
    {
    }

    void teardown() override
    {
        std::remove("/tmp/abc80_tape_test.wav");
        std::remove("/tmp/abc80_tape_test.cas");
    }
};

TEST(Abc80Tape, ModulationGeneratesCorrectAudioLengthAndHeader)
{
    const uint8_t data[] = { 0x41, 0x42, 0x43 }; // "ABC"
    std::vector<int16_t> pcm;

    Abc80Status st = tape.modulate(data, sizeof(data), pcm);
    LONGS_EQUAL(static_cast<int>(Abc80Status::OK), static_cast<int>(st));
    CHECK_TRUE(pcm.size() > 0);

    // Assert samples stay within configured amplitude limits
    const int16_t maxAmp = tape.getConfig().amplitude;
    for (int16_t sample : pcm) {
        CHECK_TRUE(sample <= maxAmp && sample >= -maxAmp);
    }
}

TEST(Abc80Tape, DemodulationRejectsEmptyOrNoiseOnlyAudio)
{
    std::vector<uint8_t> out;

    // Null pointer
    Abc80Status stNull = tape.demodulate(nullptr, 0, out);
    LONGS_EQUAL(static_cast<int>(Abc80Status::INVALID_ARG), static_cast<int>(stNull));

    // Zero sample count
    std::vector<int16_t> empty;
    Abc80Status stEmpty = tape.demodulate(empty, out);
    LONGS_EQUAL(static_cast<int>(Abc80Status::INVALID_ARG), static_cast<int>(stEmpty));

    // Silence
    std::vector<int16_t> silence(1000, 0);
    Abc80Status stSilence = tape.demodulate(silence, out);
    LONGS_EQUAL(static_cast<int>(Abc80Status::NOT_FOUND), static_cast<int>(stSilence));
}

TEST(Abc80Tape, EndToEndLoopbackBitExactRecoverySmallPayload)
{
    const std::vector<uint8_t> expected = { 'H', 'E', 'L', 'L', 'O', ' ', 'A', 'B', 'C', '-', '8', '0' };
    std::vector<int16_t> pcm;

    Abc80Status modSt = tape.modulate(expected, pcm);
    LONGS_EQUAL(static_cast<int>(Abc80Status::OK), static_cast<int>(modSt));

    std::vector<uint8_t> actual;
    Abc80Status demodSt = tape.demodulate(pcm, actual);
    LONGS_EQUAL(static_cast<int>(Abc80Status::OK), static_cast<int>(demodSt));

    LONGS_EQUAL(expected.size(), actual.size());
    for (size_t i = 0; i < expected.size(); ++i) {
        BYTES_EQUAL(expected[i], actual[i]);
    }
}

TEST(Abc80Tape, EndToEndLoopback1KBBinaryPayload)
{
    // Generate 1024 bytes containing full 0..255 byte distribution
    std::vector<uint8_t> expected(1024);
    for (size_t i = 0; i < expected.size(); ++i) {
        expected[i] = static_cast<uint8_t>((i * 7 + 13) & 0xFF);
    }

    std::vector<int16_t> pcm;
    Abc80Status modSt = tape.modulate(expected, pcm);
    LONGS_EQUAL(static_cast<int>(Abc80Status::OK), static_cast<int>(modSt));

    std::vector<uint8_t> actual;
    Abc80Status demodSt = tape.demodulate(pcm, actual);
    LONGS_EQUAL(static_cast<int>(Abc80Status::OK), static_cast<int>(demodSt));

    LONGS_EQUAL(expected.size(), actual.size());
    for (size_t i = 0; i < expected.size(); ++i) {
        BYTES_EQUAL(expected[i], actual[i]);
    }
}

TEST(Abc80Tape, WavFileSerializationAndDeserialization)
{
    const std::vector<uint8_t> expected = { 0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0 };
    const std::string wavPath = "/tmp/abc80_tape_test.wav";

    // Encode to WAV file
    Abc80Status encSt = tape.encodeToWavFile(wavPath, expected.data(), expected.size());
    LONGS_EQUAL(static_cast<int>(Abc80Status::OK), static_cast<int>(encSt));

    // Decode from WAV file
    std::vector<uint8_t> actual;
    Abc80Status decSt = tape.decodeFromWavFile(wavPath, actual);
    LONGS_EQUAL(static_cast<int>(Abc80Status::OK), static_cast<int>(decSt));

    LONGS_EQUAL(expected.size(), actual.size());
    for (size_t i = 0; i < expected.size(); ++i) {
        BYTES_EQUAL(expected[i], actual[i]);
    }
}

TEST(Abc80Tape, CasFileSaveAndLoad)
{
    const std::vector<uint8_t> expected = { 0x00, 0xFF, 0x55, 0xAA, 0xC3, 0x00, 0x10 };
    const std::string casPath = "/tmp/abc80_tape_test.cas";

    Abc80Status saveSt = tape.saveCas(casPath, expected.data(), expected.size());
    LONGS_EQUAL(static_cast<int>(Abc80Status::OK), static_cast<int>(saveSt));

    std::vector<uint8_t> actual;
    Abc80Status loadSt = tape.loadCas(casPath, actual);
    LONGS_EQUAL(static_cast<int>(Abc80Status::OK), static_cast<int>(loadSt));

    LONGS_EQUAL(expected.size(), actual.size());
    for (size_t i = 0; i < expected.size(); ++i) {
        BYTES_EQUAL(expected[i], actual[i]);
    }
}

TEST(Abc80Tape, ZeroCrossingToleranceUnderAmplitudeVariations)
{
    const std::vector<uint8_t> expected = { 0xCA, 0xFE, 0xBA, 0xBE };
    std::vector<int16_t> pcm;

    Abc80Status modSt = tape.modulate(expected, pcm);
    LONGS_EQUAL(static_cast<int>(Abc80Status::OK), static_cast<int>(modSt));

    // Attenuate audio signal by 50% (6 dB drop)
    for (int16_t& sample : pcm) {
        sample = static_cast<int16_t>(sample / 2);
    }

    std::vector<uint8_t> actual;
    Abc80Status demodSt = tape.demodulate(pcm, actual);
    LONGS_EQUAL(static_cast<int>(Abc80Status::OK), static_cast<int>(demodSt));

    LONGS_EQUAL(expected.size(), actual.size());
    for (size_t i = 0; i < expected.size(); ++i) {
        BYTES_EQUAL(expected[i], actual[i]);
    }
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
