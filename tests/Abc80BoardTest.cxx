/*
 * Abc80BoardTest.cxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include <abc80/Abc80Board.hxx>
#include <CppUTest/TestHarness.h>

using namespace abc80;

TEST_GROUP(Abc80Board)
{
    Abc80Board board;

    void setup() override
    {
    }

    void teardown() override
    {
    }
};

TEST(Abc80Board, PowerOnResetVectorsToZeroAndStabilizes)
{
    Abc80Status st = board.powerOn();
    LONGS_EQUAL(static_cast<int>(Abc80Status::OK), static_cast<int>(st));

    // Power on vectors to 0x0000
    LONGS_EQUAL(0x0000, board.getCpu().getPC());

    // Step through the power-on hardware stabilization delay loop (0000h: LD B, 0; 0002h: DJNZ $)
    while (board.getCpu().getPC() == 0x0000 || board.getCpu().getPC() == 0x0002) {
        board.stepInstruction();
    }

    // Exits loop to 0x0004 (JP 0070h)
    LONGS_EQUAL(0x0004, board.getCpu().getPC());

    // Steps JP 0070h to 'begin' entrypoint
    board.stepInstruction();
    LONGS_EQUAL(0x0070, board.getCpu().getPC());
}

TEST(Abc80Board, ColdBootExecutesInitAndWritesMagicFlagAt17F5)
{
    board.powerOn();

    // Step until cold boot completes chime and returns to exactly 0x0083
    bool ok = board.stepUntil([](const Abc80Board& b) {
        return b.getCpu().getPC() == 0x0083;
    }, 50000000);

    CHECK_TRUE(ok);
    BYTES_EQUAL(0x80, board.getBus().readByte(0x17F5));
    LONGS_EQUAL(0x17BF, board.getCpu().getSP());
}

TEST(Abc80Board, ExecutiveInitializationSetsAdsaveAndState)
{
    board.powerOn();

    // Step until initialization routine writes adsave = 0x1000
    bool ok = board.stepUntil([](const Abc80Board& b) {
        return b.getBus().readWord(0x17EE) == 0x1000;
    }, 50000000);

    CHECK_TRUE(ok);
    // Step past state and test initialization (008Ch - 0098h)
    for (int i = 0; i < 6; ++i) {
        board.stepInstruction();
    }
    LONGS_EQUAL(0x1000, board.getBus().readWord(0x17EE));
    BYTES_EQUAL(0x00, board.getBus().readByte(0x17F4)); // state = 0 (Normal monitor mode)
    BYTES_EQUAL(0x00, board.getBus().readByte(0x17F6)); // test = 0
}

TEST(Abc80Board, DisplayBufferFormattedWithInitialDigits)
{
    board.powerOn();

    // Initialize stack pointer and prefetch directly to PC-Link server at 0x0B00
    // (Bypassing power-on tone chime delay loops)
    board.getCpu().setSP(0x17BF);
    board.getCpu().prefetch(0x0B00);

    // Step until PC-Link server is ready at 0x0B04 polling Port C1
    bool ok = board.stepUntil([](const Abc80Board& b) {
        uint16_t pc = b.getCpu().getPC();
        return (pc >= 0x0B04 && pc <= 0x0B08);
    }, 100000);
    CHECK_TRUE(ok);

    // Host sends Command 1 (OutABC: write byte to RAM & format LED display)
    board.getPpi().hostWriteLinkPortB(0x01);

    // Handshake: Sync 1 (0x55)
    board.stepUntil([](const Abc80Board& b) {
        return b.getPpi().hostReadLinkPortA() == 0x55;
    }, 100000);
    // Host sends AddrLow (0x00)
    board.getPpi().hostWriteLinkPortB(0x00);

    // Handshake: Sync 2 (0xAA)
    board.stepUntil([](const Abc80Board& b) {
        return b.getPpi().hostReadLinkPortA() == 0xAA;
    }, 100000);
    // Host sends AddrHigh (0x10)
    board.getPpi().hostWriteLinkPortB(0x10);

    // Handshake: Sync 3 (0x55)
    board.stepUntil([](const Abc80Board& b) {
        return b.getPpi().hostReadLinkPortA() == 0x55;
    }, 100000);
    // Host sends Data (0x42)
    board.getPpi().hostWriteLinkPortB(0x42);

    // Handshake: Sync 4 (0xAA)
    board.stepUntil([](const Abc80Board& b) {
        return b.getPpi().hostReadLinkPortA() == 0xAA;
    }, 100000);

    // Assert data written to RAM address 0x1000
    BYTES_EQUAL(0x42, board.getBus().readByte(0x1000));

    // Assert display buffer at 0x17C6..0x17CB is formatted with 7-segment bitmasks
    bool hasNonZeroDisplay = false;
    for (uint16_t addr = 0x17C6; addr <= 0x17CB; ++addr) {
        if (board.getBus().readByte(addr) != 0x00) {
            hasNonZeroDisplay = true;
            break;
        }
    }
    CHECK_TRUE(hasNonZeroDisplay);
}

TEST(Abc80Board, PpiPort83ConfiguredAndDigitsStrobed)
{
    board.powerOn();

    // Step into the main display scan loop
    board.stepTStates(50000);

    // Primary PPI Port 0x83 configured with Mode 0 (0x90)
    BYTES_EQUAL(0x90, board.getPpi().getPrimaryControl());

    // Step further through display multiplexing
    board.stepTStates(50000);
    // Port 82h (Port C) digit strobes have been active (bits 0-5 low when strobing)
    CHECK_TRUE(board.getPpi().getDigitStrobe() != 0x00);
}

TEST(Abc80Board, SingleStepHardwareTrapTriggersRst38AndRegisterDump)
{
    board.powerOn();

    // Run until stack pointer is initialized at 0x0077 (LD SP, 17BFh at 0x0074)
    while (board.getCpu().getPC() < 0x0077) {
        board.stepInstruction();
    }

    // Set CPU to Interrupt Mode 1
    board.getCpu().setIM(1);
    board.getCpu().setIFF1(true);

    // Arm single-step hardware trap (/M1 -> /INT)
    board.setSingleStep(true);

    // Execute one instruction: on completion, trap arms /INT
    board.stepInstruction();

    // Step into interrupt trap handler (vectors to 0x0038)
    board.stepInstruction();
    LONGS_EQUAL(0x0038, board.getCpu().getPC());

    // Disarm trap so handler can execute register dump without re-trapping itself
    board.setSingleStep(false);

    // Step through register save sequence (0038h: saves HL, PC, SP, IX, IY)
    for (int i = 0; i < 20; ++i) {
        board.stepInstruction();
    }

    // SP save address at 0x13E0 should have been populated with 0x17BF
    LONGS_EQUAL(0x17BF, board.getBus().readWord(0x13E0));
}

TEST(Abc80Board, KeypadInjectionSwitchesStateToAddressMode)
{
    board.powerOn();

    // Initialize board variables to warm-booted monitor state
    board.getCpu().setSP(0x17BF);
    board.getBus().writeByte(0x17F5, 0x80); // pwup = 0x80 (warm boot flag)
    board.getBus().writeWord(0x17EE, 0x1000); // adsave = 0x1000
    board.getBus().writeByte(0x17F4, 0x00); // state = 0 (Monitor idle mode)
    board.getBus().writeByte(0x17F6, 0x00); // test = 0
    board.getPpi().writePort(0x83, 0x90);   // Primary PPI: Mode 0
    board.getPpi().writePort(0x82, 0xFF);   // Strobe lines inactive

    // Start monitor keypad scanning loop at 0x009B
    board.getCpu().prefetch(0x009B);

    // Step until gtpana completes initial idle debounce (no keys pressed) and reaches 0x04C0
    bool reachedWaitLoop = board.stepUntil([](const Abc80Board& b) {
        return b.getCpu().getPC() == 0x04C0;
    }, 200000);
    CHECK_TRUE(reachedWaitLoop);

    // Inject 'ADRS' keypress (key code 0x14)
    board.pressKey(KEY_ADRS);

    // Step until key is detected, dispatched, and state transitions to 1 (Address Entry Mode)
    bool stateChanged = board.stepUntil([](const Abc80Board& b) {
        return b.getBus().readByte(0x17F4) == 0x01;
    }, 500000);

    CHECK_TRUE(stateChanged);
    BYTES_EQUAL(0x01, board.getBus().readByte(0x17F4));

    // Release key
    board.releaseKey(KEY_ADRS);
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
