/*
 * Abc80Types.hxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#ifndef ABC80_TYPES_HXX
#define ABC80_TYPES_HXX

#include <cstdint>
#include <cstddef>

namespace abc80 {

// Fundamental status & error codes
enum class Abc80Status {
    OK = 0,
    ERROR = -1,
    TIMEOUT = -2,
    INVALID_ARG = -3,
    NOT_FOUND = -4,
    BUS_ERROR = -5,
};

// Master hardware clock & timing
constexpr uint64_t ABC80_CPU_CLOCK_HZ = 1790000ULL; // 1.79 MHz Z80 clock (book p.80; 2.5 MHz is only the CPU limit)
constexpr uint32_t ABC80_CPU_CLOCK_PERIOD_NS = 559; // 1e9 / 1.79e6 = 558.66 ns per T-state, rounded

// Display / front-panel frame: 60 Hz of emulated time
constexpr uint32_t ABC80_FRAME_HZ = 60;
constexpr uint32_t ABC80_FRAME_TSTATES = 29833; // 1,790,000 / 60 = 29,833.3 T-states per frame

// Memory Architecture (Unified Hybrid Architecture: Dual EEPROM + Contiguous 60KB RAM)
constexpr uint16_t ABC80_ROM0_BASE = 0x0000;
constexpr uint16_t ABC80_ROM0_SIZE = 0x0800; // 2KB Monitor ROM (U2)
constexpr uint16_t ABC80_ROM0_END  = 0x07FF;

constexpr uint16_t ABC80_ROM1_BASE = 0x0800;
constexpr uint16_t ABC80_ROM1_SIZE = 0x0800; // 2KB Expansion ROM (U3, PC Link server at 0x0B00)
constexpr uint16_t ABC80_ROM1_END  = 0x0FFF;

constexpr uint16_t ABC80_RAM_BASE  = 0x1000;
constexpr uint16_t ABC80_RAM_SIZE  = 0xF000; // Full 60KB Contiguous RAM (0x1000 - 0xFFFF)
constexpr uint16_t ABC80_RAM_END   = 0xFFFF;

constexpr uint16_t ABC80_SYSTEM_RAM_BASE = 0x1000;
constexpr uint16_t ABC80_SYSTEM_RAM_SIZE = 0x0800; // 2KB Onboard System RAM (U4)
constexpr uint16_t ABC80_SYSTEM_RAM_END  = 0x17FF;

constexpr uint16_t ABC80_EXPANSION_RAM_BASE = 0x1800;
constexpr uint16_t ABC80_EXPANSION_RAM_SIZE = 0xE800; // 58KB High Expansion RAM
constexpr uint16_t ABC80_EXPANSION_RAM_END  = 0xFFFF;

// Operating System Workspace & Register Dump Markers (in System RAM)
constexpr uint16_t ABC80_REG_DUMP_BASE = 0x13CC; // Breakpoint/single-step register save (13CCh-13DFh)
constexpr uint16_t ABC80_USER_STACK_TOP = 0x17AF;
constexpr uint16_t ABC80_STACK_TOP     = 0x17BF; // System stack initial top address
constexpr uint16_t ABC80_STEP_BUF_BASE = 0x17BF; // Single-step trap buffer (7 bytes)
constexpr uint16_t ABC80_DISP_BUF_BASE = 0x17C6; // 6-digit display buffer (LED segments, 6 bytes)
constexpr uint16_t ABC80_ADSAVE_ADDR   = 0x17EE; // Current displayed memory address (2 bytes)
constexpr uint16_t ABC80_STMONI_ADDR   = 0x17F3; // Parameter counter for tape/monitor input
constexpr uint16_t ABC80_STATE_ADDR    = 0x17F4; // Keypad state: 0=fix, 1=adrs, 2=data, 3=to tape, 4=from tape
constexpr uint16_t ABC80_PWUP_ADDR     = 0x17F5; // Power-on flag (0x80 = warm reset)
constexpr uint8_t  ABC80_PWUP_CODE     = 0x80;   // Warm reset signature
constexpr uint16_t ABC80_TEST_ADDR     = 0x17F6; // Flags: bit 0=function key pressed, bit 7=error

// I/O Port Definitions
// Primary 8255 PPI (0x80 - 0x83)
constexpr uint8_t ABC80_PORT_KEYPAD   = 0x80; // Port A (In): Keypad matrix rows & tape in bit 7
constexpr uint8_t ABC80_PORT_SEGMENT  = 0x81; // Port B (Out): 7-segment LED segments (a..g, dp)
constexpr uint8_t ABC80_PORT_DIGIT    = 0x82; // Port C (Out): Digit strobes & cassette/speaker
constexpr uint8_t ABC80_PORT_PPI_CTRL = 0x83; // PPI Control (0x90 = Mode 0, PA=in, PB=out, PC=out)

// Auxiliary 8255 PPI - EPROM Programmer (0x40 - 0x43)
constexpr uint8_t ABC80_PORT_AUX_ADDLOW = 0x40;
constexpr uint8_t ABC80_PORT_AUX_DATA   = 0x41;
constexpr uint8_t ABC80_PORT_AUX_ADDHIG = 0x42;
constexpr uint8_t ABC80_PORT_AUX_CTRL   = 0x43;

// Parallel Link 8255 PPI (0xC0 - 0xC3) - Firmware PC-Link server at 0x0B00
constexpr uint8_t ABC80_PORT_LINK_DATA_OUT = 0xC0;
constexpr uint8_t ABC80_PORT_LINK_DATA_IN  = 0xC1;
constexpr uint8_t ABC80_PORT_LINK_STROBE   = 0xC2;
constexpr uint8_t ABC80_PORT_LINK_CTRL     = 0xC3;

// Host PC-5523 ISA Card Emulation (Ports 0x304 - 0x307)
constexpr uint16_t ABC80_HOST_ISA_BASE = 0x304;

// PC Link Sync Bytes
constexpr uint8_t ABC80_SYNC_BYTE_55 = 0x55;
constexpr uint8_t ABC80_SYNC_BYTE_AA = 0xAA;

// Keypad Key Codes (Codes 0x00 - 0x16)
constexpr uint8_t KEY_0     = 0x00;
constexpr uint8_t KEY_1     = 0x01;
constexpr uint8_t KEY_2     = 0x02;
constexpr uint8_t KEY_3     = 0x03;
constexpr uint8_t KEY_4     = 0x04;
constexpr uint8_t KEY_5     = 0x05;
constexpr uint8_t KEY_6     = 0x06;
constexpr uint8_t KEY_7     = 0x07;
constexpr uint8_t KEY_8     = 0x08;
constexpr uint8_t KEY_9     = 0x09;
constexpr uint8_t KEY_A     = 0x0A;
constexpr uint8_t KEY_B     = 0x0B;
constexpr uint8_t KEY_C     = 0x0C;
constexpr uint8_t KEY_D     = 0x0D;
constexpr uint8_t KEY_E     = 0x0E;
constexpr uint8_t KEY_F     = 0x0F;
constexpr uint8_t KEY_PLUS  = 0x10;
constexpr uint8_t KEY_MINUS = 0x11;
constexpr uint8_t KEY_RUN   = 0x12;
constexpr uint8_t KEY_DATA  = 0x13;
constexpr uint8_t KEY_ADRS  = 0x14;
constexpr uint8_t KEY_STEP  = 0x15;
constexpr uint8_t KEY_BP    = 0x16;

} // namespace abc80

#endif // ABC80_TYPES_HXX

/*
 * Local variables:
 * mode: C++
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
