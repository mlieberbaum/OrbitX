#pragma once
#include <filesystem>
#include <fstream>
#include <string>

namespace OrbitX::Core {

// The texture database is external to Runtime and the source repository.
// Relative TextureRoot values, if used, are resolved relative to Runtime.
inline bool LoadTextureRoot(const std::filesystem::path& runtimeRoot,
                            std::filesystem::path& textureRoot,
                            std::string& error) {
    const auto configPath = runtimeRoot / L"Config" / L"OrbitX.cfg";
    std::ifstream config(configPath);
    if (!config) {
        error = "Cannot open Runtime/Config/OrbitX.cfg; set TextureRoot to your external texture folder.";
        return false;
    }
    std::string line;
    while (std::getline(config, line)) {
        const auto first = line.find_first_not_of(" \t\r");
        if (first == std::string::npos || line[first] == '#' || line[first] == ';')
            continue;
        const auto equals = line.find('=', first);
        if (equals == std::string::npos || line.substr(first, equals - first) != "TextureRoot")
            continue;
        const auto valueStart = line.find_first_not_of(" \t", equals + 1);
        if (valueStart == std::string::npos) break;
        const auto valueEnd = line.find_last_not_of(" \t\r");
        auto root = std::filesystem::path(line.substr(valueStart, valueEnd - valueStart + 1));
        if (root.is_relative()) root = runtimeRoot / root;
        textureRoot = root.lexically_normal();
        if (!std::filesystem::is_directory(textureRoot)) {
            error = "TextureRoot folder does not exist: " + textureRoot.string();
            return false;
        }
        return true;
    }
    error = "TextureRoot is missing or empty in Runtime/Config/OrbitX.cfg.";
    return false;
}

    
// Moon.cfg is the single source of truth for the Moon image's relative path.
// Both the globe renderer and scenario preview consume this resolved path.
inline bool LoadMoonTexturePath(const std::filesystem::path& runtimeRoot,
                               const std::filesystem::path& textureRoot,
                               std::filesystem::path& moonTexture,
                               std::string& error) {
    const auto configPath = runtimeRoot / L"Config" / L"Bodies" / L"Moon.cfg";
    std::ifstream config(configPath);
    if (!config) {
        error = "Cannot open Runtime/Config/Bodies/Moon.cfg.";
        return false;
    }
    std::string line;
    while (std::getline(config, line)) {
        const auto first = line.find_first_not_of(" \t\r");
        if (first == std::string::npos || line[first] == '#' || line[first] == ';')
            continue;
        const auto equals = line.find('=', first);
        if (equals == std::string::npos) continue;
        auto keyEnd = line.find_last_not_of(" \t", equals - 1);
        if (keyEnd == std::string::npos || line.substr(first, keyEnd - first + 1) != "Texture")
            continue;
        const auto valueStart = line.find_first_not_of(" \t", equals + 1);
        if (valueStart == std::string::npos) break;
        const auto valueEnd = line.find_last_not_of(" \t\r");
        auto configured = std::filesystem::path(line.substr(valueStart, valueEnd - valueStart + 1));
        moonTexture = (configured.is_absolute() ? configured : textureRoot / configured).lexically_normal();
        if (!std::filesystem::is_regular_file(moonTexture)) {
            error = "Moon texture not found: " + moonTexture.string()
                  + ". Check Texture in Runtime/Config/Bodies/Moon.cfg.";
            return false;
        }
        return true;
    }
    error = "Texture is missing or empty in Runtime/Config/Bodies/Moon.cfg.";
    return false;
}

} // namespace OrbitX::Core
