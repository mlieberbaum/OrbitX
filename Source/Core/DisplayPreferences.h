#pragma once
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
namespace OrbitX::Core {
struct DisplayPreferences {bool fullscreen=true,vsync=false;};
inline bool SaveDisplayPreferences(const std::filesystem::path& path,DisplayPreferences preferences){
    std::vector<std::string> lines;std::ifstream input(path);std::string line;bool displayFound=false,vsyncFound=false;
    const auto display=std::string("DisplayMode=")+(preferences.fullscreen?"Fullscreen":"Windowed");
    const auto vsync=std::string("VSync=")+(preferences.vsync?"On":"Off");
    while(std::getline(input,line)){
        if(!line.empty()&&line.back()=='\r')line.pop_back();
        if(line.rfind("DisplayMode=",0)==0){if(displayFound)continue;line=display;displayFound=true;}
        else if(line.rfind("VSync=",0)==0){if(vsyncFound)continue;line=vsync;vsyncFound=true;}
        lines.push_back(line);
    }
    input.close();if(!displayFound)lines.push_back(display);if(!vsyncFound)lines.push_back(vsync);
    std::error_code error;std::filesystem::create_directories(path.parent_path(),error);if(error)return false;
    std::ofstream output(path,std::ios::trunc);for(const auto& entry:lines)output<<entry<<"\n";return bool(output);
}
inline DisplayPreferences LoadDisplayPreferences(const std::filesystem::path& path){
    DisplayPreferences result;std::ifstream input(path);std::string line;
    if(!input){SaveDisplayPreferences(path,result);return result;}
    while(std::getline(input,line)){
        if(!line.empty()&&line.back()=='\r')line.pop_back();
        if(line=="DisplayMode=Windowed")result.fullscreen=false;
        else if(line=="DisplayMode=Fullscreen")result.fullscreen=true;
        else if(line=="VSync=On")result.vsync=true;
        else if(line=="VSync=Off")result.vsync=false;
    }
    return result;
}
}
