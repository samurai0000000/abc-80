/*
 * Main.cxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include <abc80/Abc80Board.hxx>
#include <abc80/Abc80Cli.hxx>
#include <abc80/ServerRunner.hxx>
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
              << "  -r, --rom <name|path> ROM to boot: 'monitor' (book monitor, default),\n"
              << "                        '1993' (author's 1993 ROM), or a path to a ROM file\n"
              << "  -b, --batch <file>    Execute commands from script file and exit\n"
              << "  -c, --command <cmd>   Execute a single command string and exit\n"
              << "      --web [port]      Serve the web front panel (default port 8080) until Ctrl+C\n";
}

int main(int argc, char** argv)
{
    std::string romName = "monitor";
    std::string scriptPath;
    std::string singleCommand;
    bool webMode = false;
    std::string webPort = "8080";

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            return 0;
        } else if ((arg == "-r" || arg == "--rom") && i + 1 < argc) {
            romName = argv[++i];
        } else if ((arg == "-b" || arg == "--batch") && i + 1 < argc) {
            scriptPath = argv[++i];
        } else if ((arg == "-c" || arg == "--command") && i + 1 < argc) {
            singleCommand = argv[++i];
        } else if (arg == "--web") {
            webMode = true;
            if (i + 1 < argc && argv[i + 1][0] >= '0' && argv[i + 1][0] <= '9') {
                webPort = argv[++i];
            }
        } else {
            std::cerr << "Unknown option: " << arg << "\n";
            printUsage(argv[0]);
            return 1;
        }
    }

    if (webMode) {
        ServerConfig cfg;
        std::string err;
        bool help = false;
        const std::string portArg = "--port";
        const char *serverArgs[] = { argv[0], portArg.c_str(), webPort.c_str() };
        if (!parseServerArgs(3, serverArgs, cfg, err, help)) {
            std::cerr << "Error: " << err << "\n";
            return 2;
        }
        RomId romId;
        if (parseRomId(romName, romId)) {
            cfg.defaultRom = romId;
        }
        return runServerUntilSignalled(cfg);
    }

    Abc80Board board;
    Abc80Status st;
    RomId romId;
    if (parseRomId(romName, romId)) {
        st = board.powerOnRom(romId);
    } else {
        st = board.powerOn(romName); // anything else is a path to a ROM file
    }
    if (st != Abc80Status::OK) {
        std::cerr << "Error: Failed to load ROM '" << romName << "'\n";
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
