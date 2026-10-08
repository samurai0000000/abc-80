/*
 * Main.cxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include <CppUTest/CommandLineTestRunner.h>
#include <CppUTest/TestHarness.h>
#include <abc80/Abc80Types.hxx>

TEST_GROUP(MainRunner)
{
};

TEST(MainRunner, FrameworkBootAndSanity)
{
    CHECK_EQUAL(0, static_cast<int>(abc80::Abc80Status::OK));
    // Book p.80: the ABC-80 runs its Z80 at 1.79 MHz (2.5 MHz is only the CPU's limit).
    CHECK_EQUAL(1790000ULL, abc80::ABC80_CPU_CLOCK_HZ);
    CHECK_EQUAL(559, abc80::ABC80_CPU_CLOCK_PERIOD_NS); // 1e9 / 1.79e6 = 558.66, rounded
    CHECK_EQUAL(60, abc80::ABC80_FRAME_HZ);
    CHECK_EQUAL(29833, abc80::ABC80_FRAME_TSTATES);     // 1,790,000 / 60 = 29,833.3
    CHECK_EQUAL(0x0000, abc80::ABC80_ROM0_BASE);
    CHECK_EQUAL(0x0800, abc80::ABC80_ROM1_BASE);
    CHECK_EQUAL(0x1000, abc80::ABC80_RAM_BASE);
    CHECK_EQUAL(0x17BF, abc80::ABC80_STACK_TOP);
    CHECK_EQUAL(0x17C6, abc80::ABC80_DISP_BUF_BASE);
    CHECK_EQUAL(0x17EE, abc80::ABC80_ADSAVE_ADDR);
    CHECK_EQUAL(0x17F4, abc80::ABC80_STATE_ADDR);
    CHECK_EQUAL(0x17F5, abc80::ABC80_PWUP_ADDR);
    CHECK_EQUAL(0x80, abc80::ABC80_PWUP_CODE);
    CHECK_EQUAL(0x80, abc80::ABC80_PORT_KEYPAD);
    CHECK_EQUAL(0x81, abc80::ABC80_PORT_SEGMENT);
    CHECK_EQUAL(0x82, abc80::ABC80_PORT_DIGIT);
    CHECK_EQUAL(0x83, abc80::ABC80_PORT_PPI_CTRL);
    CHECK_EQUAL(0xC0, abc80::ABC80_PORT_LINK_DATA_OUT);
    CHECK_EQUAL(0xC1, abc80::ABC80_PORT_LINK_DATA_IN);
    CHECK_EQUAL(0xC2, abc80::ABC80_PORT_LINK_STROBE);
    CHECK_EQUAL(0xC3, abc80::ABC80_PORT_LINK_CTRL);
    CHECK_EQUAL(0x55, abc80::ABC80_SYNC_BYTE_55);
    CHECK_EQUAL(0xAA, abc80::ABC80_SYNC_BYTE_AA);
}

int main(int argc, char **argv)
{
    return CommandLineTestRunner::RunAllTests(argc, argv);
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
