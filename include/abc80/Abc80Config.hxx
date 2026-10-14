/*
 * Abc80Config.hxx
 *
 * Configuration loader and validator using libconfig++ (plan Section 2.3.1).
 * Loads and saves ~/.config/abc-80/abc-80.cfg, validates max_sessions >= 1,
 * clamps max_sessions to 64, and provides ServerConfig with unconditional
 * 0.0.0.0 bind.
 *
 * Copyright (C) 2026, Charles Chiou
 */

#ifndef ABC80_CONFIG_HXX
#define ABC80_CONFIG_HXX

#include <abc80/Abc80WebServer.hxx>
#include <cstddef>
#include <string>

namespace abc80 {

constexpr size_t kMaxSessionsHardLimit = 64;

struct ServerConfigSettings {
    int port{8080};
    std::string webRoot{"web"};
    std::string rom{"monitor"};
    std::string bootMode{"warm"};
    bool bootTurbo{false};
    size_t maxSessions{8};
    unsigned sessionGraceSeconds{300};
};

struct AimonConfigSettings {
    bool enabled{true};
    std::string host{"localhost"};
    int port{3885};
    unsigned reconnectIntervalSec{5};
};

struct AiKeypadConfigSettings {
    unsigned punchIntervalMs{500};
    unsigned holdDurationMs{120};
};

class Abc80Config {
public:
    Abc80Config();
    ~Abc80Config() = default;

    // Returns standard default config path: ~/.config/abc-80/abc-80.cfg
    static std::string defaultPath();

    // Loads config from file path (or defaultPath() if empty).
    // If file does not exist, automatically creates directories and saves default configuration.
    // Validates max_sessions >= 1 (returns false with error if < 1).
    // Clamps max_sessions to kMaxSessionsHardLimit if > 64.
    bool load(const std::string &path = "", std::string *error = nullptr);

    // Loads config from an in-memory string representation.
    bool loadFromString(const std::string &content, std::string *error = nullptr);

    // Saves current config to file path (or defaultPath() if empty).
    bool save(const std::string &path = "", std::string *error = nullptr) const;

    // Emits string representation of configuration.
    std::string toString() const;

    // Produces a ServerConfig suitable for Abc80WebServer with unconditional 0.0.0.0 bind.
    ServerConfig toServerConfig(const std::string &root = ".") const;

    const ServerConfigSettings &server() const noexcept { return _server; }
    ServerConfigSettings &server() noexcept { return _server; }

    const AimonConfigSettings &aimon() const noexcept { return _aimon; }
    AimonConfigSettings &aimon() noexcept { return _aimon; }

    const AiKeypadConfigSettings &aiKeypad() const noexcept { return _aiKeypad; }
    AiKeypadConfigSettings &aiKeypad() noexcept { return _aiKeypad; }

private:
    bool parseAndValidate(void *configHandle, std::string *error);

    ServerConfigSettings _server;
    AimonConfigSettings _aimon;
    AiKeypadConfigSettings _aiKeypad;
};

} // namespace abc80

#endif /* ABC80_CONFIG_HXX */

/*
 * Local variables:
 * mode: C++
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
