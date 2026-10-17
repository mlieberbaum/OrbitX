#pragma once
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include "FrameLimiter.h"
namespace OrbitX::Core {
struct DisplayPreferences {bool fullscreen=true,vsync=false;bool frameCapEnabled=false;unsigned frameCap=FrameLimit::DefaultFPS;};
inline bool SaveDisplayPreferences(const std::filesystem::path& path,DisplayPreferences preferences){
    std::vector<std::string> lines;std::ifstream input(path);std::string line;bool displayFound=false,vsyncFound=false,capFound=false,fpsFound=false;
    const auto display=std::string("DisplayMode=")+(preferences.fullscreen?"Fullscreen":"Windowed");
    const auto vsync=std::string("VSync=")+(preferences.vsync?"On":"Off");
    const auto cap=std::string("FrameRateLimit=")+(preferences.frameCapEnabled?"Capped":"Uncapped");
    const auto fps="MaxFPS="+std::to_string(preferences.frameCap>=FrameLimit::MinFPS&&preferences.frameCap<=FrameLimit::MaxFPS?preferences.frameCap:FrameLimit::DefaultFPS);
    while(std::getline(input,line)){
        if(!line.empty()&&line.back()=='\r')line.pop_back();
        if(line.rfind("DisplayMode=",0)==0){if(displayFound)continue;line=display;displayFound=true;}
        else if(line.rfind("VSync=",0)==0){if(vsyncFound)continue;line=vsync;vsyncFound=true;}
        else if(line.rfind("FrameRateLimit=",0)==0){if(capFound)continue;line=cap;capFound=true;}
        else if(line.rfind("MaxFPS=",0)==0){if(fpsFound)continue;line=fps;fpsFound=true;}
        lines.push_back(line);
    }
    input.close();if(!displayFound)lines.push_back(display);if(!vsyncFound)lines.push_back(vsync);if(!capFound)lines.push_back(cap);if(!fpsFound)lines.push_back(fps);
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
        else if(line=="FrameRateLimit=Capped")result.frameCapEnabled=true;
        else if(line=="FrameRateLimit=Uncapped")result.frameCapEnabled=false;
        else if(line.rfind("MaxFPS=",0)==0)ParseFrameLimit(std::string_view(line).substr(7),result.frameCap);
    }
    return result;
}
}
