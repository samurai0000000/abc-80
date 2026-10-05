/*
 * Main.cxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include <abc80/Abc80Board.hxx>
#include <abc80/Abc80Cli.hxx>
#include <iostream>
#include <fstream>
#include <string>

using namespace abc80;

static void printUsage(const char* prog)
{
    std::cout << "ABC-80 Vintage Microcomputer Terminal Emulator & Debugger\n"
              << "Copyright (C) 2026, Charles Chiou\n\n"
              << "Usage: " << prog << " [options]\n\n"
              << "Options:\n"
              << "  -h, --help            Show this help message and exit\n"
              << "  -r, --rom <path>      Path to ROM file (default: vintage/rom.abc)\n"
              << "  -b, --batch <file>    Execute commands from script file and exit\n"
              << "  -c, --command <cmd>   Execute a single command string and exit\n";
}

int main(int argc, char** argv)
{
    std::string romPath = "vintage/rom.abc";
    std::string scriptPath;
    std::string singleCommand;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            return 0;
        } else if ((arg == "-r" || arg == "--rom") && i + 1 < argc) {
            romPath = argv[++i];
        } else if ((arg == "-b" || arg == "--batch") && i + 1 < argc) {
            scriptPath = argv[++i];
        } else if ((arg == "-c" || arg == "--command") && i + 1 < argc) {
            singleCommand = argv[++i];
        } else {
            std::cerr << "Unknown option: " << arg << "\n";
            printUsage(argv[0]);
            return 1;
        }
    }

    Abc80Board board;
    Abc80Status st = board.powerOn(romPath);
    if (st != Abc80Status::OK) {
        std::cerr << "Error: Failed to load ROM from '" << romPath << "'\n";
        return 1;
    }

    Abc80Cli cli(board);

    if (!singleCommand.empty()) {
        std::string out = cli.executeCommand(singleCommand);
        std::cout << out << "\n";
        return 0;
    }

    if (!scriptPath.empty()) {
        std::ifstream scriptFile(scriptPath);
        if (!scriptFile.is_open()) {
            std::cerr << "Error: Failed to open script file '" << scriptPath << "'\n";
            return 1;
        }
        return cli.runInteractive(scriptFile, std::cout);
    }

    return cli.runInteractive(std::cin, std::cout);
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
