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

TEST(Abc80Board, StepInstructionUpdatesLastFetchAddress)
{
    board.powerOn();
    // Step first instruction (0x0000: LD B, 0). Next opcode fetch is at 0x0002.
    board.stepInstruction();
    LONGS_EQUAL(0x0002, board.lastFetchAddress());
}

TEST(Abc80Board, FinishInstructionBehavior)
{
    board.powerOn();
    CHECK_TRUE(board.getCpu().isOpDone());
    // At boundary: finishInstruction returns 0
    LONGS_EQUAL(0, board.finishInstruction());
    CHECK_TRUE(board.getCpu().isOpDone());

    // Single clock tick steps into the instruction (LD B, 0 is 7 T-states)
    board.getCpu().tick();
    CHECK_FALSE(board.getCpu().isOpDone());

    // finishInstruction completes the remaining 6 T-states
    uint32_t finished = board.finishInstruction();
    LONGS_EQUAL(6, finished);
    CHECK_TRUE(board.getCpu().isOpDone());
}

TEST(Abc80Board, StepInstructionHaltsCleanly)
{
    board.powerOn();
    // Inject HALT (0x76) in RAM at 0x1000
    board.getBus().writeByte(0x1000, 0x76);
    board.getCpu().prefetch(0x1000);

    // Step the HALT instruction
    uint32_t t = board.stepInstruction();
    CHECK_TRUE(t >= 4);
    CHECK_TRUE(board.getCpu().isHalted());

    // Stepping while halted returns without infinite loop
    uint32_t t2 = board.stepInstruction();
    CHECK_TRUE(t2 >= 4);
    CHECK_TRUE(board.getCpu().isHalted());
}

TEST(Abc80Board, DisassembleAndReadMemoryBlock)
{
    board.powerOn();

    // Read initial 4 bytes of Monitor ROM (0000h: LD B, 0; 0002h: DJNZ $)
    std::vector<uint8_t> mem = board.readMemoryBlock(0x0000, 4);
    LONGS_EQUAL(4, static_cast<long>(mem.size()));
    BYTES_EQUAL(0x06, mem[0]);
    BYTES_EQUAL(0x00, mem[1]);
    BYTES_EQUAL(0x10, mem[2]);
    BYTES_EQUAL(0xFE, mem[3]);

    // Disassemble starting at 0x0000 for 2 instructions
    std::vector<DisasmLine> lines = board.disassemble(0x0000, 2);
    LONGS_EQUAL(2, static_cast<long>(lines.size()));
    LONGS_EQUAL(0x0000, lines[0].addr);
    LONGS_EQUAL(2, static_cast<long>(lines[0].bytes.size()));
    BYTES_EQUAL(0x06, lines[0].bytes[0]);
    BYTES_EQUAL(0x00, lines[0].bytes[1]);

    LONGS_EQUAL(0x0002, lines[1].addr);
    LONGS_EQUAL(2, static_cast<long>(lines[1].bytes.size()));
    BYTES_EQUAL(0x10, lines[1].bytes[0]);
    BYTES_EQUAL(0xFE, lines[1].bytes[1]);
}

TEST(Abc80Board, DisassemblePrefixesAndAddressWrap)
{
    board.powerOn();

    // Inject in RAM at 0x1100:
    // 3E 90       LD A, 90H
    // D3 83       OUT (83H), A
    // DD 21 00 10 LD IX, 1000H
    // FD 21 00 20 LD IY, 2000H
    // ED 4B 00 10 LD BC, (1000H)
    // CB C0       SET 0, B
    const std::vector<uint8_t> code = {
        0x3E, 0x90,
        0xD3, 0x83,
        0xDD, 0x21, 0x00, 0x10,
        0xFD, 0x21, 0x00, 0x20,
        0xED, 0x4B, 0x00, 0x10,
        0xCB, 0xC0
    };
    for (size_t i = 0; i < code.size(); ++i) {
        board.getBus().writeByte(static_cast<uint16_t>(0x1100 + i), code[i]);
    }

    std::vector<DisasmLine> lines = board.disassemble(0x1100, 6);
    LONGS_EQUAL(6, static_cast<long>(lines.size()));
    STRCMP_EQUAL("LD A,90h", lines[0].mnemonic.c_str());
    STRCMP_EQUAL("OUT (83h),A", lines[1].mnemonic.c_str());
    STRCMP_EQUAL("LD IX,1000h", lines[2].mnemonic.c_str());
    STRCMP_EQUAL("LD IY,2000h", lines[3].mnemonic.c_str());
    STRCMP_EQUAL("LD BC,(1000h)", lines[4].mnemonic.c_str());
    STRCMP_EQUAL("SET 0,B", lines[5].mnemonic.c_str());

    // Test address wrap at 0xFFFF
    board.getBus().writeByte(0xFFFF, 0x00); // NOP at 0xFFFF
    std::vector<DisasmLine> wrapLines = board.disassemble(0xFFFF, 2);
    LONGS_EQUAL(2, static_cast<long>(wrapLines.size()));
    LONGS_EQUAL(0xFFFF, wrapLines[0].addr);
    STRCMP_EQUAL("NOP", wrapLines[0].mnemonic.c_str());
    LONGS_EQUAL(0x0000, wrapLines[1].addr);

    // Test readMemoryBlock wrapping across 0xFFFF
    std::vector<uint8_t> wrapMem = board.readMemoryBlock(0xFFFE, 4);
    LONGS_EQUAL(4, static_cast<long>(wrapMem.size()));
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
