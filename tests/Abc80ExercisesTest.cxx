/*
 * Abc80ExercisesTest.cxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include <abc80/Abc80Board.hxx>
#include <abc80/Abc80ExtCircuit.hxx>
#include <abc80/Abc80Tape.hxx>
#include <abc80/WebProtocol.hxx>
#include <CppUTest/TestHarness.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

using namespace abc80;

namespace {

struct TapeCatalogEntry {
    uint16_t fileId;
    const char *tapePath;
    const char *binPath;
    uint16_t expectedStart;
    uint16_t expectedEnd;
};

constexpr TapeCatalogEntry kTapeCatalog[] = {
    { 0x0001, "tapes/0001.tape", "build/tapes/0001.bin", 0x1000, 0x1055 },
    { 0x0002, "tapes/0002.tape", "build/tapes/0002.bin", 0x1000, 0x1052 },
    { 0x0003, "tapes/0003.tape", "build/tapes/0003.bin", 0x1000, 0x1053 },
    { 0x0004, "tapes/0004.tape", "build/tapes/0004.bin", 0x1000, 0x1062 },
    { 0x0005, "tapes/0005.tape", "build/tapes/0005.bin", 0x1000, 0x1062 },
    { 0x0006, "tapes/0006.tape", "build/tapes/0006.bin", 0x1000, 0x1011 },
    { 0x0007, "tapes/0007.tape", "build/tapes/0007.bin", 0x1000, 0x1056 },
    { 0x0008, "tapes/0008.tape", "build/tapes/0008.bin", 0x1000, 0x1052 },
    { 0x0009, "tapes/0009.tape", "build/tapes/0009.bin", 0x1000, 0x104B },
    { 0x0010, "tapes/0010.tape", "build/tapes/0010.bin", 0x1000, 0x1052 },
    { 0x0011, "tapes/0011.tape", "build/tapes/0011.bin", 0x1000, 0x104B },
    { 0x0012, "tapes/0012.tape", "build/tapes/0012.bin", 0x1000, 0x102B },
    { 0x0013, "tapes/0013.tape", "build/tapes/0013.bin", 0x1000, 0x1065 },
    { 0x0014, "tapes/0014.tape", "build/tapes/0014.bin", 0x1000, 0x1035 },
    { 0x0015, "tapes/0015.tape", "build/tapes/0015.bin", 0x1000, 0x1011 },
    { 0x0016, "tapes/0016.tape", "build/tapes/0016.bin", 0x1000, 0x1060 },
    { 0x0017, "tapes/0017.tape", "build/tapes/0017.bin", 0x1000, 0x108E },
    { 0x0018, "tapes/0018.tape", "build/tapes/0018.bin", 0x1000, 0x103C },
    { 0x0019, "tapes/0019.tape", "build/tapes/0019.bin", 0x1000, 0x1032 },
    { 0x0020, "tapes/0020.tape", "build/tapes/0020.bin", 0x1000, 0x1027 },
    { 0x0021, "tapes/0021.tape", "build/tapes/0021.bin", 0x1000, 0x1108 },
    { 0x0022, "tapes/0022.tape", "build/tapes/0022.bin", 0x1000, 0x1017 },
    { 0x0023, "tapes/0023.tape", "build/tapes/0023.bin", 0x1000, 0x110A },
    { 0x0024, "tapes/0024.tape", "build/tapes/0024.bin", 0x1000, 0x1056 },
    { 0x0025, "tapes/0025.tape", "build/tapes/0025.bin", 0x1000, 0x1018 },
    { 0x0026, "tapes/0026.tape", "build/tapes/0026.bin", 0x1000, 0x1065 },
    { 0x0027, "tapes/0027.tape", "build/tapes/0027.bin", 0x1000, 0x10D6 },
    { 0x0028, "tapes/0028.tape", "build/tapes/0028.bin", 0x1000, 0x1124 },
    { 0x0029, "tapes/0029.tape", "build/tapes/0029.bin", 0x1000, 0x1173 },
    { 0x0100, "tapes/0100.tape", "build/tapes/0100.bin", 0x1000, 0x1018 },
    { 0x0101, "tapes/0101.tape", "build/tapes/0101.bin", 0x1000, 0x1015 },
    { 0x0102, "tapes/0102.tape", "build/tapes/0102.bin", 0x1000, 0x100E },
    { 0x0103, "tapes/0103.tape", "build/tapes/0103.bin", 0x1000, 0x1023 },
};

std::vector<uint8_t> readBinaryFile(const std::string &path)
{
    std::ifstream in(path, std::ios::binary);
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(in)),
                                std::istreambuf_iterator<char>());
}

void initBoardAndLoadTape(Abc80Board &board, const char *tapePath)
{
    LONGS_EQUAL(static_cast<int>(Abc80Status::OK),
                static_cast<int>(board.powerOnRom(RomId::Monitor, ".")));
    board.getPpi().writePort(0x83, 0x90);
    board.getPpi().writePort(0x82, 0xFF);
    board.finishInstruction();

    uint16_t fileId = 0;
    uint16_t startAddr = 0;
    uint16_t endAddr = 0;
    std::string annot;
    std::vector<TapePulseRun> runs;
    std::vector<uint8_t> code;
    LONGS_EQUAL(static_cast<int>(Abc80Status::OK),
                static_cast<int>(Abc80Tape::loadTapeFile(tapePath, fileId, annot, runs)));
    LONGS_EQUAL(static_cast<int>(Abc80Status::OK),
                static_cast<int>(Abc80Tape::decodeProgramFromRuns(runs, fileId, startAddr, endAddr, code)));

    for (size_t i = 0; i < code.size(); ++i) {
        board.getBus().writeByte(static_cast<uint16_t>(startAddr + i), code[i]);
    }
    board.getCpu().setSP(0x17BD);
    board.getCpu().setIX(0x13C6);
    board.getCpu().setPC(startAddr);
    board.stepInstruction();
}

void stepUntilFetch(Abc80Board &board, uint16_t targetPc, uint64_t maxSteps = 500000)
{
    uint64_t steps = 0;
    while (board.lastFetchAddress() != targetPc) {
        if (++steps > maxSteps) {
            char msg[128];
            std::snprintf(msg, sizeof(msg),
                          "Timeout waiting for fetch 0x%04X (stuck at 0x%04X after %llu steps)",
                          targetPc, board.lastFetchAddress(),
                          static_cast<unsigned long long>(maxSteps));
            FAIL(msg);
        }
        board.stepInstruction();
    }
}

} // namespace

TEST_GROUP(Abc80Exercises)
{
};

TEST(Abc80Exercises, All33TapesLoadAndDecodeExactPayloads)
{
    for (const auto &entry : kTapeCatalog) {
        uint16_t loadedId = 0;
        std::string annot;
        std::vector<TapePulseRun> runs;
        LONGS_EQUAL(static_cast<int>(Abc80Status::OK),
                    static_cast<int>(Abc80Tape::loadTapeFile(entry.tapePath, loadedId, annot, runs)));
        LONGS_EQUAL(entry.fileId, loadedId);
        CHECK_FALSE(annot.empty());

        uint16_t decodedId = 0;
        uint16_t startAddr = 0;
        uint16_t endAddr = 0;
        std::vector<uint8_t> payload;
        LONGS_EQUAL(static_cast<int>(Abc80Status::OK),
                    static_cast<int>(Abc80Tape::decodeProgramFromRuns(
                        runs, decodedId, startAddr, endAddr, payload)));
        LONGS_EQUAL(entry.fileId, decodedId);
        LONGS_EQUAL(entry.expectedStart, startAddr);
        LONGS_EQUAL(entry.expectedEnd, endAddr);

        const std::vector<uint8_t> expectedBin = readBinaryFile(entry.binPath);
        LONGS_EQUAL(expectedBin.size(), payload.size());
        CHECK_TRUE(expectedBin == payload);
    }
}

TEST(Abc80Exercises, Exercises01To08ComputeTextbookResultsAndDriveDisplayAndSpeaker)
{
    // Lab 01: Data Transfer (8-bit 1050H..1054H = 01..05, 16-bit 1060H..1065H = 1234H, 5678H, 1050H)
    {
        Abc80Board board;
        initBoardAndLoadTape(board, "tapes/0001.tape");
        board.getPpi().beginFrame();
        board.stepTStates(500000);
        board.getPpi().endFrame();

        for (uint8_t i = 0; i < 5; ++i) {
            BYTES_EQUAL(static_cast<uint8_t>(i + 1), board.getBus().readByte(static_cast<uint16_t>(0x1050 + i)));
        }
        LONGS_EQUAL(0x1234, board.getBus().readWord(0x1060));
        LONGS_EQUAL(0x5678, board.getBus().readWord(0x1062));
        LONGS_EQUAL(0x1050, board.getBus().readWord(0x1064));
        CHECK_TRUE(board.getPpi().frameAudio().edgeCount > 0);
    }

    // Lab 02: Bitwise AND (590760) & OR (FF4FF3) into 1050H..1052H
    {
        Abc80Board board;
        initBoardAndLoadTape(board, "tapes/0002.tape");
        stepUntilFetch(board, 0x1018);
        BYTES_EQUAL(0x60, board.getBus().readByte(0x1050));
        BYTES_EQUAL(0x07, board.getBus().readByte(0x1051));
        BYTES_EQUAL(0x59, board.getBus().readByte(0x1052));

        stepUntilFetch(board, 0x1033);
        BYTES_EQUAL(0xF3, board.getBus().readByte(0x1050));
        BYTES_EQUAL(0x4F, board.getBus().readByte(0x1051));
        BYTES_EQUAL(0xFF, board.getBus().readByte(0x1052));
    }

    // Lab 03: XOR (0FH -> 1050H), CPL (A4H -> 1051H), NEG (A5H -> 1052H)
    {
        Abc80Board board;
        initBoardAndLoadTape(board, "tapes/0003.tape");
        board.getPpi().beginFrame();
        board.stepTStates(300000);
        board.getPpi().endFrame();

        BYTES_EQUAL(0x0F, board.getBus().readByte(0x1050));
        BYTES_EQUAL(0xA4, board.getBus().readByte(0x1051));
        BYTES_EQUAL(0xA5, board.getBus().readByte(0x1052));
        CHECK_TRUE(board.getPpi().frameAudio().edgeCount > 0);
    }

    // Lab 04: 3-Byte BCD Addition (473256 + 422344 = 895600 -> 1050H..1052H = 00, 56, 89)
    {
        Abc80Board board;
        initBoardAndLoadTape(board, "tapes/0004.tape");
        board.getPpi().beginFrame();
        board.stepTStates(300000);
        board.getPpi().endFrame();

        BYTES_EQUAL(0x00, board.getBus().readByte(0x1050));
        BYTES_EQUAL(0x56, board.getBus().readByte(0x1051));
        BYTES_EQUAL(0x89, board.getBus().readByte(0x1052));
        CHECK_TRUE(board.getPpi().frameAudio().edgeCount > 0);
    }

    // Lab 05: 3-Byte BCD Subtraction (796825 - 335836 = 460989 -> 1050H..1052H = 89, 09, 46)
    {
        Abc80Board board;
        initBoardAndLoadTape(board, "tapes/0005.tape");
        board.getPpi().beginFrame();
        board.stepTStates(300000);
        board.getPpi().endFrame();

        BYTES_EQUAL(0x89, board.getBus().readByte(0x1050));
        BYTES_EQUAL(0x09, board.getBus().readByte(0x1051));
        BYTES_EQUAL(0x46, board.getBus().readByte(0x1052));
        CHECK_TRUE(board.getPpi().frameAudio().edgeCount > 0);
    }

    // Lab 06: Loop & Branch Delay Square-Wave Generator (SQWAV on Port C bit 7, ~1.04M T-states/half-period)
    {
        Abc80Board board;
        initBoardAndLoadTape(board, "tapes/0006.tape");
        board.getPpi().beginFrame();
        board.stepTStates(2500000);
        board.getPpi().endFrame();
        CHECK_TRUE(board.getPpi().frameAudio().edgeCount >= 2);
    }

    // Lab 07: Maximum Search (max of {69H, 39H, 96H, 9AH, 47H} = 9AH -> 1050H)
    {
        Abc80Board board;
        initBoardAndLoadTape(board, "tapes/0007.tape");
        board.getPpi().beginFrame();
        board.stepTStates(300000);
        board.getPpi().endFrame();

        BYTES_EQUAL(0x9A, board.getBus().readByte(0x1050));
        CHECK_TRUE(board.getPpi().frameAudio().edgeCount > 0);
    }

    // Lab 08: Shift & Rotate (SLA 6FH -> DEH at 1050H, RRCA 3FH -> 9FH at 1052H)
    {
        Abc80Board board;
        initBoardAndLoadTape(board, "tapes/0008.tape");
        board.getPpi().beginFrame();
        board.stepTStates(300000);
        board.getPpi().endFrame();

        BYTES_EQUAL(0xDE, board.getBus().readByte(0x1050));
        BYTES_EQUAL(0x9F, board.getBus().readByte(0x1052));
        CHECK_TRUE(board.getPpi().frameAudio().edgeCount > 0);
    }
}

TEST(Abc80Exercises, Exercises09To17DisplayAnimationsAndSoundEffects)
{
    // Lab 09: Monitor Subroutines (" HELLO" at 1040H..1045H, '-' blanks, '+' scrolls)
    {
        Abc80Board board;
        initBoardAndLoadTape(board, "tapes/0009.tape");
        stepUntilFetch(board, 0x04C0);
        LONGS_EQUAL(0x1040, board.getCpu().getIX());
        const uint8_t hello[6] = { 0xBD, 0x85, 0x85, 0x8F, 0x37, 0x00 };
        for (int i = 0; i < 6; ++i) {
            BYTES_EQUAL(hello[i], board.getBus().readByte(static_cast<uint16_t>(0x1040 + i)));
        }
        // Press '-' (0x11) to transition to blank buffer (1046H)
        board.getPpi().pressKey(0x11);
        stepUntilFetch(board, 0x100F);
        LONGS_EQUAL(0x1046, board.getCpu().getIX());
    }

    // Lab 10: Hex/BCD Display Decoding (3AH, CFH, 6BH -> ADC+DAA -> 40H, 35H (C=1), 72H -> "723540")
    {
        Abc80Board board;
        initBoardAndLoadTape(board, "tapes/0010.tape");
        stepUntilFetch(board, 0x1021);
        BYTES_EQUAL(0x40, board.getBus().readByte(0x1050));
        BYTES_EQUAL(0x35, board.getBus().readByte(0x1051));
        BYTES_EQUAL(0x72, board.getBus().readByte(0x1052));
        const uint8_t expectedSegs[6] = { 0xBD, 0x36, 0xAE, 0xBA, 0x9B, 0x38 };
        for (int i = 0; i < 6; ++i) {
            BYTES_EQUAL(expectedSegs[i], board.getBus().readByte(static_cast<uint16_t>(0x13C6 + i)));
        }
    }

    // Lab 11: Subroutine Practice ("FEDCBA" at 1040H..1045H scrolled via IX from 1046H to 1040H)
    {
        Abc80Board board;
        initBoardAndLoadTape(board, "tapes/0011.tape");
        board.getBus().writeByte(0x1007, 0x01); // 1 scan pass per scroll step
        stepUntilFetch(board, 0x1012);
        LONGS_EQUAL(0x1040, board.getCpu().getIX());
        const uint8_t fedcba[6] = { 0x0F, 0x8F, 0xB3, 0x8D, 0xA7, 0x3F };
        for (int i = 0; i < 6; ++i) {
            BYTES_EQUAL(fedcba[i], board.getBus().readByte(static_cast<uint16_t>(0x1040 + i)));
        }
    }

    // Lab 12: Alternating Display (" -GO- " at 1026H and "-StOP-" at 1020H)
    {
        Abc80Board board;
        initBoardAndLoadTape(board, "tapes/0012.tape");
        board.getBus().writeByte(0x1005, 0x01);
        stepUntilFetch(board, 0x1006);
        LONGS_EQUAL(0x1026, board.getCpu().getIX());
        stepUntilFetch(board, 0x1011);
        LONGS_EQUAL(0x1020, board.getCpu().getIX());
    }

    // Lab 13: 26-Letter Scrolling Marquee ("A..Z" at 1046H..105FH scrolled via IX)
    {
        Abc80Board board;
        initBoardAndLoadTape(board, "tapes/0013.tape");
        board.getBus().writeByte(0x1007, 0x01);
        stepUntilFetch(board, 0x100F);
        LONGS_EQUAL(0x105E, board.getCpu().getIX());
        BYTES_EQUAL(0x3F, board.getBus().readByte(0x105F)); // 'A'
        BYTES_EQUAL(0xA7, board.getBus().readByte(0x105E)); // 'B'
    }

    // Lab 14: Telephone Ringing Tone Generator (SOUND 047CH bursts)
    {
        Abc80Board board;
        initBoardAndLoadTape(board, "tapes/0014.tape");
        board.getPpi().beginFrame();
        board.stepTStates(500000);
        board.getPpi().endFrame();
        CHECK_TRUE(board.getPpi().frameAudio().edgeCount > 50);
    }

    // Lab 15: Two-Tone Police Siren (265 Hz & 352 Hz via SOUND 047CH)
    {
        Abc80Board board;
        initBoardAndLoadTape(board, "tapes/0015.tape");
        board.getPpi().beginFrame();
        board.stepTStates(2000000);
        board.getPpi().endFrame();
        CHECK_TRUE(board.getPpi().frameAudio().edgeCount >= 400);
    }

    // Lab 16: Roulette Wheel Game (perimeter segments in 1100H..1105H, '+' pos 03H stops wheel)
    {
        Abc80Board board;
        initBoardAndLoadTape(board, "tapes/0016.tape");
        stepUntilFetch(board, 0x101E);
        // First table entry: digit 05H, segment 08H (top bar 'a' on leftmost digit)
        BYTES_EQUAL(0x08, board.getBus().readByte(0x1105));

        // Hold '+' key (keyCode 0x10 -> matrix position 0x03) and verify HL stays on first pair (1040H)
        board.getPpi().pressKey(0x10);
        stepUntilFetch(board, 0x1003);
        LONGS_EQUAL(0x1040, board.getCpu().getHL());
    }

    // Lab 17: Pac-Man Sprite Animation & Audio Chirps
    {
        Abc80Board board;
        initBoardAndLoadTape(board, "tapes/0017.tape");
        board.getPpi().beginFrame();
        board.stepTStates(1500000);
        board.getPpi().endFrame();
        CHECK_TRUE(board.getPpi().frameAudio().edgeCount > 0);
    }
}

TEST(Abc80Exercises, Exercises18To23MusicTimersAndCounters)
{
    // Lab 18: Computer Music Score Player (MUSIC 0411H with IY -> 1008H)
    {
        Abc80Board board;
        initBoardAndLoadTape(board, "tapes/0018.tape");
        board.getPpi().beginFrame();
        board.stepTStates(1000000);
        board.getPpi().endFrame();
        CHECK_TRUE(board.getCpu().getIY() >= 0x1008 && board.getCpu().getIY() <= 0x103C);
        CHECK_TRUE(board.getPpi().frameAudio().edgeCount > 0);
    }

    // Lab 19: 16-Key Electronic Organ (press key '0' -> pitch constant B2H -> toggles Port C bit 7)
    {
        Abc80Board board;
        initBoardAndLoadTape(board, "tapes/0019.tape");
        // Wait until SCD&K (04A6H) finishes its 4-pass key-release debounce and enters scloop (04C0H)
        stepUntilFetch(board, 0x04C0);
        board.getPpi().pressKey(0x00); // Key '0'
        board.getPpi().beginFrame();
        board.stepTStates(200000);
        board.getPpi().endFrame();
        CHECK_TRUE(board.getPpi().frameAudio().edgeCount > 0);
    }

    // Lab 20: 1/100th-Second Digital Stopwatch (DE increments BCD centiseconds)
    {
        Abc80Board board;
        initBoardAndLoadTape(board, "tapes/0020.tape");
        stepUntilFetch(board, 0x100A);
        // Step one centisecond pass through 100AH..1025H
        board.stepInstruction();
        stepUntilFetch(board, 0x100A);
        LONGS_EQUAL(0x0001, board.getCpu().getDE());
    }

    // Lab 21: 24-Hour Digital Clock with Hourly Chime (seed 07:59:59 -> 08:00:00)
    {
        Abc80Board board;
        initBoardAndLoadTape(board, "tapes/0021.tape");
        board.getBus().writeByte(0x1200, 0x59); // SS = 59
        board.getBus().writeByte(0x1201, 0x59); // MM = 59
        board.getBus().writeByte(0x1202, 0x07); // HH = 07
        // Call TMUPD (0x1022) and BFUPD (0x103A) via main loop by clamping B=1 at 1007H
        stepUntilFetch(board, 0x1007);
        board.getCpu().setBC(static_cast<uint16_t>((0x01 << 8) | (board.getCpu().getBC() & 0xFF)));
        stepUntilFetch(board, 0x1012);
        BYTES_EQUAL(0x00, board.getBus().readByte(0x1200));
        BYTES_EQUAL(0x00, board.getBus().readByte(0x1201));
        BYTES_EQUAL(0x08, board.getBus().readByte(0x1202));
        // Decimal point bit 6 set on minutes low digit (1205H) and hours low digit (1207H)
        CHECK_TRUE((board.getBus().readByte(0x1205) & 0x40) != 0);
        CHECK_TRUE((board.getBus().readByte(0x1207) & 0x40) != 0);
    }

    // Lab 22: 1 kHz Square-Wave Signal Generator
    {
        Abc80Board board;
        initBoardAndLoadTape(board, "tapes/0022.tape");
        board.getPpi().beginFrame();
        board.stepTStates(500000);
        board.getPpi().endFrame();
        CHECK_TRUE(board.getPpi().frameAudio().edgeCount >= 100);
    }

    // Lab 23: 6-Digit BCD Production / Pulse Counter (drive PA7 EAR high across INPUT debounce)
    {
        Abc80Board board;
        initBoardAndLoadTape(board, "tapes/0023.tape");
        board.getPpi().setTapeInput(true);
        stepUntilFetch(board, 0x1050);
        board.stepInstruction();
        stepUntilFetch(board, 0x100E);
        BYTES_EQUAL(0x01, board.getBus().readByte(0x13CC));
        BYTES_EQUAL(0x00, board.getBus().readByte(0x13CD));
        BYTES_EQUAL(0x00, board.getBus().readByte(0x13CE));
    }
}

TEST(Abc80Exercises, Exercises24To29ExternalHardwarePeripherals)
{
    // Lab 24: Traffic Light Controller on Port B (0x81)
    {
        Abc80Board board;
        auto *tl = dynamic_cast<Abc80TrafficLight *>(board.attachCircuit(ExtCircuitId::TrafficLight));
        CHECK_TRUE(tl != nullptr);
        initBoardAndLoadTape(board, "tapes/0024.tape");
        // Patch DELAY at 1049H (LD DE, 8000H -> LD DE, 0001H) for fast full-cycle verification
        board.getBus().writeByte(0x104A, 0x01);
        board.getBus().writeByte(0x104B, 0x00);

        std::vector<uint8_t> observedPortB;
        uint8_t lastVal = 0xFF;
        uint64_t steps = 0;
        bool verified0C = false;
        bool verified0A = false;
        bool verified21 = false;
        bool verified11 = false;
        while (steps < 200000 && observedPortB.size() < 14) {
            board.stepInstruction();
            steps++;
            const uint8_t pb = board.getPpi().getSegmentOutput();
            if (pb != 0x00 && pb != lastVal) {
                observedPortB.push_back(pb);
                lastVal = pb;
                if (pb == 0x0C) {
                    CHECK_TRUE(tl->roadAGreen() && tl->roadBRed());
                    verified0C = true;
                } else if (pb == 0x0A) {
                    CHECK_TRUE(tl->roadAYellow() && tl->roadBRed());
                    verified0A = true;
                } else if (pb == 0x21) {
                    CHECK_TRUE(tl->roadARed() && tl->roadBGreen());
                    verified21 = true;
                } else if (pb == 0x11) {
                    CHECK_TRUE(tl->roadARed() && tl->roadBYellow());
                    verified11 = true;
                }
            }
        }
        // Must visit 0x0C (A Green/B Red), 0x0A/0x08 (A Yellow blink), 0x21 (A Red/B Green), 0x11/0x01 (B Yellow blink)
        CHECK_TRUE(std::find(observedPortB.begin(), observedPortB.end(), 0x0C) != observedPortB.end());
        CHECK_TRUE(std::find(observedPortB.begin(), observedPortB.end(), 0x0A) != observedPortB.end());
        CHECK_TRUE(std::find(observedPortB.begin(), observedPortB.end(), 0x08) != observedPortB.end());
        CHECK_TRUE(std::find(observedPortB.begin(), observedPortB.end(), 0x21) != observedPortB.end());
        CHECK_TRUE(std::find(observedPortB.begin(), observedPortB.end(), 0x11) != observedPortB.end());
        CHECK_TRUE(std::find(observedPortB.begin(), observedPortB.end(), 0x01) != observedPortB.end());
        CHECK_TRUE(verified0C);
        CHECK_TRUE(verified0A);
        CHECK_TRUE(verified21);
        CHECK_TRUE(verified11);
        CHECK_TRUE(tl->transitionCount() >= 14);

        TelemetryFrame frame;
        captureBoardExtTelemetry(board, frame);
        const std::string json = buildTelemetryJson(frame);
        CHECK_TRUE(json.find("\"id\":\"traffic_light\"") != std::string::npos);
        CHECK_TRUE(json.find("\"exercise\":24") != std::string::npos);
    }

    // Lab 25: 8-LED Sequential Chaser on Port B (0x81)
    {
        Abc80Board board;
        auto *chaser = dynamic_cast<Abc80LedChaser *>(board.attachCircuit(ExtCircuitId::LedChaser));
        CHECK_TRUE(chaser != nullptr);
        initBoardAndLoadTape(board, "tapes/0025.tape");
        // Clamp initial HL delay seed at 1009H (LD HL, FFFEH) so each step takes 2 iterations
        board.getBus().writeByte(0x100A, 0xFE);
        board.getBus().writeByte(0x100B, 0xFF);

        const uint8_t expectedChaser[8] = { 0xFE, 0xFD, 0xFB, 0xF7, 0xEF, 0xDF, 0xBF, 0x7F };
        for (int i = 0; i < 8; ++i) {
            stepUntilFetch(board, 0x1008);
            BYTES_EQUAL(expectedChaser[i], board.getPpi().getSegmentOutput());
            BYTES_EQUAL(expectedChaser[i], chaser->rawPortB());
            BYTES_EQUAL(static_cast<uint8_t>(1u << i), chaser->activeMask());
            LONGS_EQUAL(i, chaser->activeLedIndex());
            for (int bit = 0; bit < 8; ++bit) {
                if (bit == i) {
                    CHECK_TRUE(chaser->isLedOn(static_cast<uint8_t>(bit)));
                } else {
                    CHECK_FALSE(chaser->isLedOn(static_cast<uint8_t>(bit)));
                }
            }
            board.stepInstruction();
        }
        CHECK_TRUE(chaser->stepCount() >= 8);

        TelemetryFrame frame;
        captureBoardExtTelemetry(board, frame);
        const std::string json = buildTelemetryJson(frame);
        CHECK_TRUE(json.find("\"id\":\"led_chaser\"") != std::string::npos);
        CHECK_TRUE(json.find("\"exercise\":25") != std::string::npos);
    }

    // Lab 26: Oscilloscope X-Y Pattern Generator on Port B (0x81)
    {
        Abc80Board board;
        auto *scope = dynamic_cast<Abc80Oscilloscope *>(board.attachCircuit(ExtCircuitId::Oscilloscope));
        CHECK_TRUE(scope != nullptr);
        initBoardAndLoadTape(board, "tapes/0026.tape");
        // Clamp DELAY C counter at 1016H (LD C, 01H) to capture all 37 coordinates rapidly
        board.getBus().writeByte(0x1017, 0x01);

        std::vector<uint8_t> coords;
        uint64_t steps = 0;
        while (coords.size() < 37) {
            if (++steps > 100000) {
                FAIL("Timeout in Lab 26 coordinate capture");
            }
            if (board.lastFetchAddress() == 0x100F) {
                coords.push_back(board.getPpi().getSegmentOutput());
                CHECK_TRUE(scope->isBeamValid());
                BYTES_EQUAL(coords.back(), scope->rawPortB());
                BYTES_EQUAL(static_cast<uint8_t>(coords.back() & 0x0F), scope->beamX());
                BYTES_EQUAL(static_cast<uint8_t>((coords.back() >> 4) & 0x0F), scope->beamY());
            }
            board.stepInstruction();
        }
        BYTES_EQUAL(0x92, coords.front());
        BYTES_EQUAL(0x6D, coords.back());
        CHECK_TRUE(scope->isBeamValid());
        BYTES_EQUAL(coords.back(), scope->rawPortB());
        BYTES_EQUAL(static_cast<uint8_t>(coords.back() & 0x0F), scope->beamX());
        BYTES_EQUAL(static_cast<uint8_t>((coords.back() >> 4) & 0x0F), scope->beamY());
        CHECK_TRUE(scope->sampleCount() >= 37);
        CHECK_TRUE(scope->tracePoints().size() > 0);
        CHECK_TRUE(scope->gridDwell(2, 9) > 0);
        CHECK_TRUE(scope->gridDwell(13, 6) > 0);

        TelemetryFrame frame;
        captureBoardExtTelemetry(board, frame);
        const std::string json = buildTelemetryJson(frame);
        CHECK_TRUE(json.find("\"id\":\"oscilloscope\"") != std::string::npos);
        CHECK_TRUE(json.find("\"exercise\":26") != std::string::npos);
        CHECK_TRUE(json.find("\"points\":[") != std::string::npos);
    }

    // Lab 27: Automated School Bell Clock (seed 07:59:59 -> triggers BALL at 08:00:00, then 08:09:59 -> 08:10:00)
    {
        Abc80Board board;
        auto *bell = dynamic_cast<Abc80SchoolBell *>(board.attachCircuit(ExtCircuitId::SchoolBell));
        CHECK_TRUE(bell != nullptr);
        initBoardAndLoadTape(board, "tapes/0027.tape");

        bell->setBcdTime(0x07, 0x59, 0x59);
        BYTES_EQUAL(0x07, bell->bcdHour());
        BYTES_EQUAL(0x59, bell->bcdMinute());
        BYTES_EQUAL(0x59, bell->bcdSecond());
        CHECK_FALSE(bell->isBellRinging());

        stepUntilFetch(board, 0x1007);
        board.getCpu().setBC(static_cast<uint16_t>((0x01 << 8) | (board.getCpu().getBC() & 0xFF)));
        stepUntilFetch(board, 0x1012);
        // Normal SCD&K1 display strobes (0xFE..0xDF) must not trigger the active-low PC6 bell relay
        CHECK_FALSE(bell->isBellRinging());
        BYTES_EQUAL(0x08, bell->bcdHour());
        BYTES_EQUAL(0x00, bell->bcdMinute());
        BYTES_EQUAL(0x00, bell->bcdSecond());
        board.getCpu().setBC(static_cast<uint16_t>((0x01 << 8) | (board.getCpu().getBC() & 0xFF)));

        // Step into BALL (0x107B -> OUT (82H), 3FH) and verify Port C bit 6 is pulled low (0x3F)
        stepUntilFetch(board, 0x107F);
        BYTES_EQUAL(0x3F, board.getPpi().getDigitStrobe());
        BYTES_EQUAL(0x3F, bell->portC());
        CHECK_TRUE(bell->isBellRinging());
        LONGS_EQUAL(1, bell->ringCount());

        // Clamp first chime note HL cycle count at 1082H to 0002H and jump from 1088H to 10C9H (end of BALL)
        board.getBus().writeByte(0x1083, 0x02);
        board.getBus().writeByte(0x1084, 0x00);
        board.getBus().writeByte(0x1088, 0xC3);
        board.getBus().writeByte(0x1089, 0xC9);
        board.getBus().writeByte(0x108A, 0x10);

        // Step into Monitor ROM SOUND (047CH) and through to 10CEH (after OUT (82H), FFH at 10CAH)
        stepUntilFetch(board, 0x047C);
        stepUntilFetch(board, 0x10CE);
        CHECK_TRUE(bell->totalRingTStates() > 0);
        CHECK_TRUE(bell->speakerEdges() > 0);
        BYTES_EQUAL(0xFF, bell->portC());
        CHECK_FALSE(bell->isBellRinging());

        // Step back to CLOCK loop (1005H), seed 08:09:59, and verify 08:10:00 triggers BALL a second time
        stepUntilFetch(board, 0x1005);
        bell->setBcdTime(0x08, 0x09, 0x59);
        stepUntilFetch(board, 0x1007);
        board.getCpu().setBC(static_cast<uint16_t>((0x01 << 8) | (board.getCpu().getBC() & 0xFF)));
        stepUntilFetch(board, 0x1012);
        BYTES_EQUAL(0x08, bell->bcdHour());
        BYTES_EQUAL(0x10, bell->bcdMinute());
        BYTES_EQUAL(0x00, bell->bcdSecond());
        board.getCpu().setBC(static_cast<uint16_t>((0x01 << 8) | (board.getCpu().getBC() & 0xFF)));

        stepUntilFetch(board, 0x107F);
        BYTES_EQUAL(0x3F, bell->portC());
        CHECK_TRUE(bell->isBellRinging());
        LONGS_EQUAL(2, bell->ringCount());

        TelemetryFrame frame;
        captureBoardExtTelemetry(board, frame);
        const std::string json = buildTelemetryJson(frame);
        CHECK_TRUE(json.find("\"id\":\"school_bell\"") != std::string::npos);
        CHECK_TRUE(json.find("\"exercise\":27") != std::string::npos);
        CHECK_TRUE(json.find("\"ringCount\":2") != std::string::npos);
    }

    // Lab 28: Home Appliance Computerized Control on Auxiliary 8255 Port C (0x42)
    {
        Abc80Board board;
        auto *app = dynamic_cast<Abc80ApplianceControl *>(board.attachCircuit(ExtCircuitId::ApplianceControl));
        CHECK_TRUE(app != nullptr);
        initBoardAndLoadTape(board, "tapes/0028.tape");

        // Seed 05:59:59 so next tick rolls over to 06:00:00 (schedule table 1106H = 0xE1)
        app->setBcdTime(0x05, 0x59, 0x59);
        BYTES_EQUAL(0x05, app->bcdHour());
        BYTES_EQUAL(0x59, app->bcdMinute());
        BYTES_EQUAL(0x59, app->bcdSecond());

        stepUntilFetch(board, 0x100E);
        BYTES_EQUAL(0x80, app->auxControl());
        board.getCpu().setBC(static_cast<uint16_t>((0x01 << 8) | (board.getCpu().getBC() & 0xFF)));
        stepUntilFetch(board, 0x1019);
        board.getCpu().setBC(static_cast<uint16_t>((0x01 << 8) | (board.getCpu().getBC() & 0xFF)));
        stepUntilFetch(board, 0x101F);
        BYTES_EQUAL(0x06, board.getBus().readByte(0x1202));
        BYTES_EQUAL(0x06, app->bcdHour());
        BYTES_EQUAL(0x00, app->bcdMinute());
        BYTES_EQUAL(0x00, app->bcdSecond());
        BYTES_EQUAL(6, app->activeHourIndex());
        BYTES_EQUAL(0xE1, board.getPpi().auxPortCOutput());
        BYTES_EQUAL(0xE1, app->relayMask());
        CHECK_TRUE(app->isRelayOn(0));
        CHECK_FALSE(app->isRelayOn(1));
        CHECK_FALSE(app->isRelayOn(2));
        CHECK_FALSE(app->isRelayOn(3));
        CHECK_FALSE(app->isRelayOn(4));
        CHECK_TRUE(app->isRelayOn(5));
        CHECK_TRUE(app->isRelayOn(6));
        CHECK_TRUE(app->isRelayOn(7));

        // Seed 07:59:59 -> rolls over to 08:00:00 (schedule table 1108H = 0xC4)
        app->setBcdTime(0x07, 0x59, 0x59);
        stepUntilFetch(board, 0x100E);
        board.getCpu().setBC(static_cast<uint16_t>((0x01 << 8) | (board.getCpu().getBC() & 0xFF)));
        stepUntilFetch(board, 0x1019);
        board.getCpu().setBC(static_cast<uint16_t>((0x01 << 8) | (board.getCpu().getBC() & 0xFF)));
        stepUntilFetch(board, 0x101F);
        BYTES_EQUAL(8, app->activeHourIndex());
        BYTES_EQUAL(0xC4, app->relayMask());

        // Seed 18:59:59 -> rolls over to 19:00:00 (schedule table 1119H = 0xF2)
        app->setBcdTime(0x18, 0x59, 0x59);
        stepUntilFetch(board, 0x100E);
        board.getCpu().setBC(static_cast<uint16_t>((0x01 << 8) | (board.getCpu().getBC() & 0xFF)));
        stepUntilFetch(board, 0x1019);
        board.getCpu().setBC(static_cast<uint16_t>((0x01 << 8) | (board.getCpu().getBC() & 0xFF)));
        stepUntilFetch(board, 0x101F);
        BYTES_EQUAL(19, app->activeHourIndex());
        BYTES_EQUAL(0xF2, app->relayMask());
        CHECK_TRUE(app->updateCount() >= 3);
        BYTES_EQUAL(0xE1, app->scheduleMasks()[6]);

        TelemetryFrame frame;
        captureBoardExtTelemetry(board, frame);
        const std::string json = buildTelemetryJson(frame);
        CHECK_TRUE(json.find("\"id\":\"appliance_control\"") != std::string::npos);
        CHECK_TRUE(json.find("\"exercise\":28") != std::string::npos);
        CHECK_TRUE(json.find("\"relayMask\":242") != std::string::npos);
    }

    // Lab 29: 3-Axis Robotic Arm Stepper Controller on Expansion 8255 (0x00..0x03)
    {
        Abc80Board board;
        auto *arm = dynamic_cast<Abc80RoboticArm *>(board.attachCircuit(ExtCircuitId::RoboticArm));
        CHECK_TRUE(arm != nullptr);
        initBoardAndLoadTape(board, "tapes/0029.tape");
        // Patch DELAY at 115EH with RET (0xC9) so all trajectory commands complete rapidly
        board.getBus().writeByte(0x115E, 0xC9);

        bool seenMotor1 = false;
        bool seenMotor2 = false;
        bool seenMotor3 = false;
        bool seenSolenoidUp = false;

        uint64_t steps = 0;
        while (steps < 100000 && board.lastFetchAddress() != 0x1047) {
            board.stepInstruction();
            steps++;
            if (board.getPpi().expPortAOutput() == 0x03) {
                seenMotor1 = true;
            }
            if ((board.getPpi().expPortBOutput() & 0x03) == 0x03) {
                seenMotor2 = true;
            }
            if ((board.getPpi().expPortBOutput() & 0x30) == 0x30) {
                seenMotor3 = true;
            }
            if (board.getPpi().expPortCOutput() == 0x01) {
                seenSolenoidUp = true;
                CHECK_TRUE(arm->isElectromagnetOn());
            }
        }
        LONGS_EQUAL(0x1047, board.lastFetchAddress()); // Reached STOP: HALT at 1047H
        BYTES_EQUAL(0x80, board.getPpi().getExpControl());
        BYTES_EQUAL(0x80, arm->expControl());
        CHECK_TRUE(seenMotor1);
        CHECK_TRUE(seenMotor2);
        CHECK_TRUE(seenMotor3);
        CHECK_TRUE(seenSolenoidUp);
        BYTES_EQUAL(0x00, board.getPpi().expPortCOutput()); // Released at end (DOWN = 00H)
        CHECK_FALSE(arm->isElectromagnetOn());
        CHECK_TRUE(arm->magnetOnTStates() > 0);
        CHECK_TRUE(arm->lastStepTStates() > 0);

        LONGS_EQUAL(-4, arm->motor(0).steps);
        LONGS_EQUAL(24, arm->motor(0).totalSteps);
        LONGS_EQUAL(16, arm->motor(1).steps);
        LONGS_EQUAL(16, arm->motor(1).totalSteps);
        LONGS_EQUAL(3, arm->motor(2).steps);
        LONGS_EQUAL(19, arm->motor(2).totalSteps);

        TelemetryFrame frame;
        captureBoardExtTelemetry(board, frame);
        const std::string json = buildTelemetryJson(frame);
        CHECK_TRUE(json.find("\"id\":\"robotic_arm\"") != std::string::npos);
        CHECK_TRUE(json.find("\"exercise\":29") != std::string::npos);
        CHECK_TRUE(json.find("\"steps\":-4") != std::string::npos);
        CHECK_TRUE(json.find("\"steps\":16") != std::string::npos);
        CHECK_TRUE(json.find("\"steps\":3") != std::string::npos);
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
