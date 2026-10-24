/*
 * ServerRunner.hxx
 *
 * Command-line parsing and the run loop shared by abc80_server and abc80_cli --web.
 *
 * Copyright (C) 2026, Charles Chiou
 */

#ifndef ABC80_SERVER_RUNNER_HXX
#define ABC80_SERVER_RUNNER_HXX

#include <abc80/Abc80WebServer.hxx>
#include <abc80/Abc80Config.hxx>
#include <string>

namespace abc80 {

using ServerCliArgs = ServerConfig;

// Parses abc80_server options into cfg. Returns false with err set on a bad option or value;
// help is set when -h/--help was given, and cfg.showVersion is set when -v/--version was given.
// Options:
//   -v/--version  --config PATH  --port N  --bind ADDR  --web-dir DIR  --root DIR
//   --rom monitor|1993  --boot warm|cold  --boot-turbo  --max-sessions N  --grace SECONDS
//   --aimon  --no-aimon
bool parseServerArgs(int argc, const char *const *argv, ServerConfig &cfg, std::string &err, bool &help);

const char *serverUsage();

// Starts the server, prints "LISTENING <port>" on stdout, and runs until SIGINT or SIGTERM.
// The signal handler only sets a flag; the server is stopped on this thread. Returns the exit code.
int runServerUntilSignalled(const ServerConfig &cfg, const Abc80Config *appConfig = nullptr);

} // namespace abc80

#endif /* ABC80_SERVER_RUNNER_HXX */

/*
 * Local variables:
 * mode: C++
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
