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

    
// Each body's surface hierarchy can live independently of the shared texture root.
inline bool LoadBodySurfacePath(const std::filesystem::path& runtimeRoot,
                                const std::filesystem::path& textureRoot,
                                const std::string& body,
                                std::filesystem::path& surfaceRoot,
                                std::string& error) {
    const auto configPath=runtimeRoot / "Config" / "Bodies" / (body+".cfg");
    std::ifstream config(configPath);
    if(!config){error="Cannot open body configuration: "+configPath.string();return false;}
    surfaceRoot=textureRoot/body/"Surf";
    std::string line;
    while(std::getline(config,line)) {
        auto first=line.find_first_not_of(" \t\r");
        if(first==std::string::npos || line[first]=='#' || line[first]==';')continue;
        auto equals=line.find('=',first);if(equals==std::string::npos || equals==first)continue;
        auto end=line.find_last_not_of(" \t",equals-1);
        if(end==std::string::npos || line.substr(first,end-first+1)!="SurfaceTiles")continue;
        auto begin=line.find_first_not_of(" \t\r",equals+1),last=line.find_last_not_of(" \t\r");
        if(begin==std::string::npos){error="SurfaceTiles is empty in "+configPath.string();return false;}
        auto configured=std::filesystem::path(line.substr(begin,last-begin+1));
        surfaceRoot=(configured.is_absolute()?configured:textureRoot/configured).lexically_normal();
        break;
    }
    if(!std::filesystem::is_directory(surfaceRoot)) {
        error="DDS surface folder does not exist: "+surfaceRoot.string()+". Set SurfaceTiles in "+configPath.string();return false;
    }
    return true;
}
} // namespace OrbitX::Core
