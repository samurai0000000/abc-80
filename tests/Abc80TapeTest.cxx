/*
 * Abc80TapeTest.cxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include <abc80/Abc80Tape.hxx>
#include <abc80/Abc80Board.hxx>
#include <CppUTest/TestHarness.h>
#include <rapidcheck.h>
#include <vector>
#include <cstdio>
#include <filesystem>

using namespace abc80;

TEST_GROUP(Abc80Tape)
{
    Abc80Tape tape;
    std::string tempDir{"/tmp/abc80_tapes_test_dir"};

    void setup() override
    {
        std::filesystem::remove_all(tempDir);
        std::filesystem::create_directories(tempDir);
    }

    void teardown() override
    {
        std::remove("/tmp/abc80_tape_test.wav");
        std::remove("/tmp/abc80_tape_test.cas");
        std::filesystem::remove_all(tempDir);
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

TEST(Abc80Tape, FileIdFormattingParsingAnd32CharAnnotationContainer)
{
    STRCMP_EQUAL("0000", Abc80Tape::formatFileId(0x0000).c_str());
    STRCMP_EQUAL("0042", Abc80Tape::formatFileId(0x0042).c_str());
    STRCMP_EQUAL("abcd", Abc80Tape::formatFileId(0xABCD).c_str());
    STRCMP_EQUAL("ffff", Abc80Tape::formatFileId(0xFFFF).c_str());

    uint16_t parsed = 0;
    CHECK_TRUE(Abc80Tape::parseFileId("0001", parsed));
    LONGS_EQUAL(0x0001, parsed);
    CHECK_TRUE(Abc80Tape::parseFileId("CAFE", parsed));
    LONGS_EQUAL(0xCAFE, parsed);
    CHECK_TRUE(Abc80Tape::parseFileId("cafe", parsed));
    LONGS_EQUAL(0xCAFE, parsed);
    CHECK_FALSE(Abc80Tape::parseFileId("123", parsed));
    CHECK_FALSE(Abc80Tape::parseFileId("12345", parsed));
    CHECK_FALSE(Abc80Tape::parseFileId("123G", parsed));

    // 32-character annotation clamping and persistence in .tape file
    const std::string longLabel = "0123456789ABCDEF0123456789ABCDEF_EXTRA_IGNORED";
    const std::string clamped = Abc80Tape::sanitizeAnnotation(longLabel);
    LONGS_EQUAL(32, clamped.size());
    STRCMP_EQUAL("0123456789ABCDEF0123456789ABCDEF", clamped.c_str());

    const uint8_t prog[] = { 0x3E, 0x42, 0x76 };
    std::vector<TapePulseRun> runs;
    LONGS_EQUAL(static_cast<int>(Abc80Status::OK),
                static_cast<int>(Abc80Tape::encodeProgramToRuns(0x00A5, 0x1000, prog, sizeof(prog), runs)));

    const std::string tapeFile = tempDir + "/00a5.tape";
    LONGS_EQUAL(static_cast<int>(Abc80Status::OK),
                static_cast<int>(Abc80Tape::saveTapeFile(tapeFile, 0x00A5, longLabel, runs)));

    uint16_t loadedId = 0;
    std::string loadedLabel;
    std::vector<TapePulseRun> loadedRuns;
    LONGS_EQUAL(static_cast<int>(Abc80Status::OK),
                static_cast<int>(Abc80Tape::loadTapeFile(tapeFile, loadedId, loadedLabel, loadedRuns)));
    LONGS_EQUAL(0x00A5, loadedId);
    STRCMP_EQUAL("0123456789ABCDEF0123456789ABCDEF", loadedLabel.c_str());
    LONGS_EQUAL(runs.size(), loadedRuns.size());
}

TEST(Abc80Tape, RapidCheckRomPulseCodecRoundTripAndCorruptInputSafety)
{
    const bool ok = rc::check("arbitrary ABC-80 program round-trips through ROM pulse runs", []() {
        const uint16_t fileId = *rc::gen::arbitrary<uint16_t>();
        const uint16_t startAddr = *rc::gen::inRange<uint16_t>(0x1000, 0x1700);
        const auto payload = *rc::gen::container<std::vector<uint8_t>>(
            *rc::gen::inRange<size_t>(1, 64),
            rc::gen::arbitrary<uint8_t>());

        std::vector<TapePulseRun> runs;
        RC_ASSERT(Abc80Tape::encodeProgramToRuns(fileId, startAddr, payload.data(), payload.size(), runs) ==
                  Abc80Status::OK);

        uint16_t outId = 0;
        uint16_t outStart = 0;
        uint16_t outEnd = 0;
        std::vector<uint8_t> outData;
        RC_ASSERT(Abc80Tape::decodeProgramFromRuns(runs, outId, outStart, outEnd, outData) ==
                  Abc80Status::OK);
        RC_ASSERT(outId == fileId);
        RC_ASSERT(outStart == startAddr);
        RC_ASSERT(outEnd == static_cast<uint16_t>(startAddr + payload.size() - 1));
        RC_ASSERT(outData == payload);
    });
    CHECK_TRUE(ok);

    // Empty / truncated / noise pulse runs must safely fail without crashing
    uint16_t id = 0, s = 0, e = 0;
    std::vector<uint8_t> data;
    std::vector<TapePulseRun> emptyRuns;
    CHECK_TRUE(Abc80Tape::decodeProgramFromRuns(emptyRuns, id, s, e, data) != Abc80Status::OK);

    std::vector<TapePulseRun> shortRuns = { { 889, 2000 }, { 447, 100 } };
    CHECK_TRUE(Abc80Tape::decodeProgramFromRuns(shortRuns, id, s, e, data) != Abc80Status::OK);
}

TEST(Abc80Tape, AuthenticZ80CpuToTapeAndFromTapeLoopbackVia8255Ports)
{
    Abc80Board board;
    LONGS_EQUAL(static_cast<int>(Abc80Status::OK),
                static_cast<int>(board.powerOnRom(RomId::Monitor, ".")));
    // Configure primary 8255 PPI as Monitor begin (0x0070) does: PA=in, PB=out, PC=out (0x90), PC=0xFF
    board.getPpi().writePort(0x83, 0x90);
    board.getPpi().writePort(0x82, 0xFF);
    board.finishInstruction();

    CassetteDeck& deck = board.getCassetteDeck();
    deck.setTapesDir(tempDir);

    // Write a test machine-code program into RAM at 0x1000..0x1007
    const uint8_t program[] = { 0x3E, 0xAA, 0x32, 0x50, 0x10, 0x76, 0xC9, 0x5A };
    for (size_t i = 0; i < sizeof(program); ++i) {
        board.getBus().writeByte(static_cast<uint16_t>(0x1000 + i), program[i]);
    }

    // Populate stepbf (0x17BF..0x17C4) as the keypad TO TAPE parameter entry does:
    // File ID = 0x04D2 ("04d2.tape"), Start = 0x1000, End = 0x1007
    board.getBus().writeByte(0x17BF, 0xD2);
    board.getBus().writeByte(0x17C0, 0x04);
    board.getBus().writeByte(0x17C1, 0x00);
    board.getBus().writeByte(0x17C2, 0x10);
    board.getBus().writeByte(0x17C3, 0x07);
    board.getBus().writeByte(0x17C4, 0x10);

    // Arm CassetteDeck in RECORD mode with a 32-char annotation, then invoke ROM EWT (0x0193)
    LONGS_EQUAL(static_cast<int>(Abc80Status::OK),
                static_cast<int>(deck.record("Z80 EWT Authentic Test")));
    CHECK_TRUE(deck.state() == TapeDeckState::Recording);

    // Call 0x0193 (ewt) with return address 0x009B (Monitor main loop) pushed onto stack
    board.getCpu().setIX(0x17C6);
    uint16_t sp = 0x17BD;
    board.getBus().writeByte(sp, 0x9B);
    board.getBus().writeByte(static_cast<uint16_t>(sp + 1), 0x00);
    board.getCpu().setSP(sp);
    board.getCpu().setPC(0x0193);
    board.stepInstruction();

    // Step until ewt returns to 0x009B (Monitor main loop) and auto-stop finalizes recording
    uint64_t elapsed = 0;
    while (elapsed < 30000000ULL && board.lastFetchAddress() != 0x009B) {
        elapsed += board.stepInstruction();
    }
    CHECK_TRUE(board.lastFetchAddress() == 0x009B);

    // Step 100,000 T-states in main loop so CassetteDeck detects end-of-trailer silence and auto-saves
    board.stepTStates(100000);
    CHECK_TRUE(deck.state() == TapeDeckState::Stopped);

    // Verify that the file ID entered (04d2) was demodulated from PC7 audio and saved on demand!
    LONGS_EQUAL(0x04D2, deck.selectedFileId());
    STRCMP_EQUAL("04d2", deck.selectedFileIdHex().c_str());
    STRCMP_EQUAL("Z80 EWT Authentic Test", deck.annotation().c_str());
    CHECK_TRUE(std::filesystem::exists(tempDir + "/04d2.tape"));

    // Wipe RAM at 0x1000..0x1007 and corrupt stepbf start/end so only FROM TAPE can restore it
    for (size_t i = 0; i < sizeof(program); ++i) {
        board.getBus().writeByte(static_cast<uint16_t>(0x1000 + i), 0x00);
    }
    board.getBus().writeByte(0x17C1, 0x00);
    board.getBus().writeByte(0x17C2, 0x00);
    board.getBus().writeByte(0x17C3, 0x00);
    board.getBus().writeByte(0x17C4, 0x00);

    // Set requested File ID = 0x04D2 in stepbf (0x17BF..0x17C0), start PLAY, and invoke ROM ERT (0x01CE)
    board.getBus().writeByte(0x17BF, 0xD2);
    board.getBus().writeByte(0x17C0, 0x04);
    LONGS_EQUAL(static_cast<int>(Abc80Status::OK), static_cast<int>(deck.play()));
    CHECK_TRUE(deck.state() == TapeDeckState::Playing);

    board.finishInstruction();
    board.getCpu().setIX(0x17C6);
    sp = 0x17BD;
    board.getBus().writeByte(sp, 0x9B);
    board.getBus().writeByte(static_cast<uint16_t>(sp + 1), 0x00);
    board.getCpu().setSP(sp);
    board.getCpu().setPC(0x01CE);
    board.stepInstruction();

    elapsed = 0;
    while (elapsed < 35000000ULL && board.lastFetchAddress() != 0x009B) {
        elapsed += board.stepInstruction();
    }
    CHECK_TRUE(board.lastFetchAddress() == 0x009B);

    // Verify no error flag was set in test (0x17F6 bit 7 == 0) and adsave (0x17EE) == 0x1007
    BYTES_EQUAL(0x00, board.getBus().readByte(0x17F6) & 0x80);
    LONGS_EQUAL(0x1007, board.getBus().readWord(0x17EE));

    // Verify every single byte in RAM 0x1000..0x1007 was restored via PA7 audio tones!
    for (size_t i = 0; i < sizeof(program); ++i) {
        BYTES_EQUAL(program[i], board.getBus().readByte(static_cast<uint16_t>(0x1000 + i)));
    }
}

TEST(Abc80Tape, AuthenticFromTapeAfterProlongedIdlePlayBeforeGo)
{
    Abc80Board board;
    LONGS_EQUAL(static_cast<int>(Abc80Status::OK),
                static_cast<int>(board.powerOnRom(RomId::Monitor, ".")));
    board.bootTurbo(RomId::Monitor, 400000000ULL);

    CassetteDeck& deck = board.getCassetteDeck();
    deck.setTapesDir(tempDir);
    LONGS_EQUAL(static_cast<int>(Abc80Status::OK),
                static_cast<int>(deck.seedDefaultTapesIfEmpty()));
    LONGS_EQUAL(static_cast<int>(Abc80Status::OK),
                static_cast<int>(deck.selectTape(0x0004)));

    // User presses PLAY on the cassette deck first
    LONGS_EQUAL(static_cast<int>(Abc80Status::OK), static_cast<int>(deck.play()));
    CHECK_TRUE(deck.state() == TapeDeckState::Playing);

    // Emulate 3.5 seconds of the Monitor scanning keypad/display before the user finishes typing
    // FROM TAPE -> 0 -> 0 -> 0 -> 4 -> GO (~210 frames at 60 Hz)
    for (int f = 0; f < 210; ++f) {
        board.stepFrame();
    }

    // Tape deck MUST still be playing the sustained leader tone, NOT prematurely finished!
    CHECK_TRUE(deck.state() == TapeDeckState::Playing);

    // Now execute ROM ERT (0x01CE) for File ID 0x0004
    board.finishInstruction();
    board.getBus().writeByte(0x17BF, 0x04);
    board.getBus().writeByte(0x17C0, 0x00);
    board.getCpu().setIX(0x17C6);
    const uint16_t sp = 0x17BD;
    board.getBus().writeByte(sp, 0x9B);
    board.getBus().writeByte(static_cast<uint16_t>(sp + 1), 0x00);
    board.getCpu().setSP(sp);
    board.getCpu().setPC(0x01CE);
    board.stepInstruction();

    uint64_t elapsed = 0;
    while (elapsed < 60000000ULL && board.lastFetchAddress() != 0x009B) {
        elapsed += board.stepInstruction();
    }
    CHECK_TRUE(board.lastFetchAddress() == 0x009B);
    BYTES_EQUAL(0x00, board.getBus().readByte(0x17F6) & 0x80);
    LONGS_EQUAL(0x1023, board.getBus().readWord(0x17EE));
    BYTES_EQUAL(0x11, board.getBus().readByte(0x1000));
    BYTES_EQUAL(0xE2, board.getBus().readByte(0x1023));

    // Also verify seeded 0003.tape ("Dual-Tone Chime (1000H)") loops cleanly to 0x1000 (JP 1000H)
    uint16_t id3 = 0, start3 = 0, end3 = 0;
    std::string annot3;
    std::vector<TapePulseRun> runs3;
    std::vector<uint8_t> code3;
    LONGS_EQUAL(static_cast<int>(Abc80Status::OK),
                static_cast<int>(Abc80Tape::loadTapeFile(tempDir + "/0003.tape", id3, annot3, runs3)));
    LONGS_EQUAL(static_cast<int>(Abc80Status::OK),
                static_cast<int>(Abc80Tape::decodeProgramFromRuns(runs3, id3, start3, end3, code3)));
    LONGS_EQUAL(0x0003, id3);
    LONGS_EQUAL(0x1000, start3);
    LONGS_EQUAL(0x100E, end3);
    LONGS_EQUAL(15, code3.size());
    BYTES_EQUAL(0xC3, code3[12]);
    BYTES_EQUAL(0x00, code3[13]);
    BYTES_EQUAL(0x10, code3[14]);
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

