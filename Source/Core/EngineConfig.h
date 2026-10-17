#pragma once
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

namespace OrbitX::Core {
// Engine startup settings are read only; user-preference saves never rewrite them.
inline bool LoadPlanetOptimizations(const std::filesystem::path& configPath){
    std::ifstream input(configPath);std::string line;bool enabled=false;
    auto trim=[](std::string_view text){
        const auto first=text.find_first_not_of(" \t\r");
        if(first==std::string_view::npos)return std::string_view{};
        return text.substr(first,text.find_last_not_of(" \t\r")-first+1);
    };
    while(std::getline(input,line)){
        const auto entry=trim(std::string_view(line).substr(0,line.find_first_of("#;")));
        const auto equals=entry.find('=');
        if(equals==std::string_view::npos||trim(entry.substr(0,equals))!="PlanetOptimizations")continue;
        const auto value=trim(entry.substr(equals+1));
        if(value=="On")enabled=true;
        else if(value=="Off")enabled=false;
    }
    return enabled;
}
}
