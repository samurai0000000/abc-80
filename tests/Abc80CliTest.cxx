/*
 * Abc80CliTest.cxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include <abc80/Abc80Board.hxx>
#include <abc80/Abc80Cli.hxx>
#include <CppUTest/TestHarness.h>
#include <sstream>

using namespace abc80;

TEST_GROUP(Abc80Cli)
{
    Abc80Board board;
    Abc80Cli* cli;

    void setup() override
    {
        board.powerOn("vintage/rom.abc");
        cli = new Abc80Cli(board);
    }

    void teardown() override
    {
        delete cli;
    }
};

TEST(Abc80Cli, SegmentBitmaskDecodingMatchesAsciiGlyphs)
{
    // Test all 16 hexadecimal 7-segment font bitmasks from segtab (0x07F0)
    BYTES_EQUAL('0', Abc80Cli::decodeSegmentGlyph(0xBD));
    BYTES_EQUAL('1', Abc80Cli::decodeSegmentGlyph(0x30));
    BYTES_EQUAL('2', Abc80Cli::decodeSegmentGlyph(0x9B));
    BYTES_EQUAL('3', Abc80Cli::decodeSegmentGlyph(0xBA));
    BYTES_EQUAL('4', Abc80Cli::decodeSegmentGlyph(0x36));
    BYTES_EQUAL('5', Abc80Cli::decodeSegmentGlyph(0xAE));
    BYTES_EQUAL('6', Abc80Cli::decodeSegmentGlyph(0xAF));
    BYTES_EQUAL('7', Abc80Cli::decodeSegmentGlyph(0x38));
    BYTES_EQUAL('8', Abc80Cli::decodeSegmentGlyph(0xBF));
    BYTES_EQUAL('9', Abc80Cli::decodeSegmentGlyph(0xBE));
    BYTES_EQUAL('A', Abc80Cli::decodeSegmentGlyph(0x3F));
    BYTES_EQUAL('B', Abc80Cli::decodeSegmentGlyph(0xA7));
    BYTES_EQUAL('C', Abc80Cli::decodeSegmentGlyph(0x8D));
    BYTES_EQUAL('D', Abc80Cli::decodeSegmentGlyph(0xB3));
    BYTES_EQUAL('E', Abc80Cli::decodeSegmentGlyph(0x8F));
    BYTES_EQUAL('F', Abc80Cli::decodeSegmentGlyph(0x0F));
    BYTES_EQUAL('-', Abc80Cli::decodeSegmentGlyph(0x02));
    BYTES_EQUAL(' ', Abc80Cli::decodeSegmentGlyph(0x00));
}

TEST(Abc80Cli, DisplayFormattingRendersSevenSegmentGlyphs)
{
    // Populate display buffer in RAM at 0x17C6..0x17CB with "1000 3E"
    // Digits: 5='1' (0x30), 4='0' (0xBD), 3='0' (0xBD), 2='0' (0xBD), 1='3' (0xBA), 0='E' (0x8F)
    board.getBus().writeByte(0x17CB, 0x30); // digit 5
    board.getBus().writeByte(0x17CA, 0xBD); // digit 4
    board.getBus().writeByte(0x17C9, 0xBD); // digit 3
    board.getBus().writeByte(0x17C8, 0xBD); // digit 2
    board.getBus().writeByte(0x17C7, 0xBA); // digit 1
    board.getBus().writeByte(0x17C6, 0x8F); // digit 0

    std::string dispStr = cli->formatDisplayString();
    STRCMP_EQUAL("[ 1 0 0 0   3 E ]", dispStr.c_str());

    std::string hexStr = cli->formatDisplayRawHex();
    STRCMP_EQUAL("[ 30 BD BD BD   BA 8F ]", hexStr.c_str());
}

TEST(Abc80Cli, DisassemblerDecodesStandardOpcodeMnemonics)
{
    // Write instructions to test address 0x2000
    board.getBus().writeByte(0x2000, 0x00); // NOP
    board.getBus().writeByte(0x2001, 0x3E); // LD A, 42h
    board.getBus().writeByte(0x2002, 0x42);
    board.getBus().writeByte(0x2003, 0x21); // LD HL, 1000h
    board.getBus().writeByte(0x2004, 0x00);
    board.getBus().writeByte(0x2005, 0x10);
    board.getBus().writeByte(0x2006, 0xC3); // JP 0070h
    board.getBus().writeByte(0x2007, 0x70);
    board.getBus().writeByte(0x2008, 0x00);
    board.getBus().writeByte(0x2009, 0x76); // HALT

    size_t len = 0;
    std::string d0 = cli->disassembleInstruction(0x2000, &len);
    LONGS_EQUAL(1, len);
    CHECK_TRUE(d0.find("NOP") != std::string::npos);

    std::string d1 = cli->disassembleInstruction(0x2001, &len);
    LONGS_EQUAL(2, len);
    CHECK_TRUE(d1.find("LD A, 42h") != std::string::npos);

    std::string d2 = cli->disassembleInstruction(0x2003, &len);
    LONGS_EQUAL(3, len);
    CHECK_TRUE(d2.find("LD HL, 1000h") != std::string::npos);

    std::string d3 = cli->disassembleInstruction(0x2006, &len);
    LONGS_EQUAL(3, len);
    CHECK_TRUE(d3.find("JP 0070h") != std::string::npos);

    std::string d4 = cli->disassembleInstruction(0x2009, &len);
    LONGS_EQUAL(1, len);
    CHECK_TRUE(d4.find("HALT") != std::string::npos);
}

TEST(Abc80Cli, BreakpointAddRemoveAndHitHaltsExecution)
{
    // Set breakpoint at 'begin' entrypoint (0x0070)
    cli->addBreakpoint(0x0070);
    CHECK_TRUE(cli->hasBreakpoint(0x0070));

    // Run board from 0x0000; should hit breakpoint at 0x0070
    uint64_t t = cli->run(100000);
    CHECK_TRUE(t > 0);
    LONGS_EQUAL(0x0070, board.getCpu().getPC());

    // Remove breakpoint
    cli->removeBreakpoint(0x0070);
    CHECK_FALSE(cli->hasBreakpoint(0x0070));

    // Step past 0x0070
    cli->step();
    CHECK_TRUE(board.getCpu().getPC() != 0x0070);
}

TEST(Abc80Cli, FrontPanelAnsiRenderingContainsBannerAndDisplay)
{
    std::string screen = cli->renderFrontPanelAnsi();
    CHECK_TRUE(screen.find("ABC-80 MICROCOMPUTER") != std::string::npos);
    CHECK_TRUE(screen.find("LED DISPLAY:") != std::string::npos);
    CHECK_TRUE(screen.find("DISASSEMBLY:") != std::string::npos);
    CHECK_TRUE(screen.find("PC:") != std::string::npos);
}

TEST(Abc80Cli, CommandInterpreterExecutesCoreCommands)
{
    // Step command
    std::string rStep = cli->executeCommand("step");
    CHECK_TRUE(rStep.find("Stepped") != std::string::npos);

    // Register inspection
    std::string rReg = cli->executeCommand("reg");
    CHECK_TRUE(rReg.find("PC:") != std::string::npos);

    // Display inspection
    std::string rDisp = cli->executeCommand("disp");
    CHECK_TRUE(rDisp.find("[") != std::string::npos);

    // Poke & Peek
    cli->executeCommand("poke 2500 AA");
    BYTES_EQUAL(0xAA, board.getBus().readByte(0x2500));
    std::string rPeek = cli->executeCommand("peek 2500 1");
    CHECK_TRUE(rPeek.find("AA") != std::string::npos);

    // Break & Delete
    std::string rBreak = cli->executeCommand("break 1234");
    CHECK_TRUE(rBreak.find("Breakpoint set") != std::string::npos);
    CHECK_TRUE(cli->hasBreakpoint(0x1234));
    std::string rDel = cli->executeCommand("delete 1234");
    CHECK_TRUE(rDel.find("Breakpoint removed") != std::string::npos);
    CHECK_FALSE(cli->hasBreakpoint(0x1234));

    // Help
    std::string rHelp = cli->executeCommand("help");
    CHECK_TRUE(rHelp.find("Commands:") != std::string::npos);

    // Quit
    std::string rQuit = cli->executeCommand("quit");
    STRCMP_EQUAL("QUIT", rQuit.c_str());
}

TEST(Abc80Cli, InteractiveBatchRunScriptExecution)
{
    std::istringstream inputScript(
        "step\n"
        "poke 3000 55\n"
        "peek 3000 1\n"
        "reg\n"
        "quit\n"
    );
    std::ostringstream outputStream;

    int ret = cli->runInteractive(inputScript, outputStream);
    LONGS_EQUAL(0, ret);

    BYTES_EQUAL(0x55, board.getBus().readByte(0x3000));
    std::string out = outputStream.str();
    CHECK_TRUE(out.find("Stepped") != std::string::npos);
    CHECK_TRUE(out.find("Wrote 0x55") != std::string::npos);
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
