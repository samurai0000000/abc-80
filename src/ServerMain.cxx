/*
 * ServerMain.cxx
 *
 * Standalone ABC-80 web front panel server.
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include <abc80/Abc80Config.hxx>
#include <abc80/ServerRunner.hxx>
#include <iostream>

int main(int argc, char **argv)
{
    abc80::Abc80Config appConfig;
    std::string configErr;
    if (!appConfig.load("", &configErr)) {
        std::cerr << "abc80: error loading config: " << configErr << "\n";
        return 2;
    }

    abc80::ServerConfig cfg = appConfig.toServerConfig(".");
    std::string err;
    bool help = false;
    if (!abc80::parseServerArgs(argc, argv, cfg, err, help)) {
        std::cerr << "error: " << err << "\n\n" << abc80::serverUsage();
        return 2;
    }
    if (help) {
        std::cout << abc80::serverUsage();
        return 0;
    }
    return abc80::runServerUntilSignalled(cfg, &appConfig);
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
