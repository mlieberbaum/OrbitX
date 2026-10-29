#pragma once
#include <charconv>
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
// Directory levels 04..19; omitted settings retain the conservative L11 cap.
inline bool LoadMaxQuadtreeLevel(const std::filesystem::path& configPath,
                                 int& maximum, std::string& error) {
    maximum=11;
    std::ifstream input(configPath);
    if(!input){error="Cannot open engine configuration: "+configPath.string();return false;}
    auto trim=[](std::string_view text){
        const auto first=text.find_first_not_of(" \t\r");
        if(first==std::string_view::npos)return std::string_view{};
        return text.substr(first,text.find_last_not_of(" \t\r")-first+1);
    };
    std::string line;
    while(std::getline(input,line)) {
        const auto entry=trim(std::string_view(line).substr(0,line.find_first_of("#;")));
        const auto equals=entry.find('=');
        if(equals==std::string_view::npos||trim(entry.substr(0,equals))!="MaxQuadtreeLevel")continue;
        const auto value=trim(entry.substr(equals+1));
        if(value.empty()){error="MaxQuadtreeLevel must be an integer from 4 to 19 in "+configPath.string();return false;}
        int parsed=0;
        const auto result=std::from_chars(value.data(),value.data()+value.size(),parsed);
        if(result.ec!=std::errc{}||result.ptr!=value.data()+value.size()||parsed<4||parsed>19) {
            error="MaxQuadtreeLevel must be an integer from 4 to 19 in "+configPath.string();
            return false;
        }
        maximum=parsed;
    }
    if(input.bad()){error="Cannot read engine configuration: "+configPath.string();return false;}
    return true;
}

}
