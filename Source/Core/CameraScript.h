#pragma once
#include "PlanetCoordinates.h"
#include "PlanetQuadtree.h"
#include <stdexcept>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <string>
#include <vector>

namespace OrbitX::Core {
// Store the actual float camera state as well as geographic metadata. Geographic
// position alone cannot distinguish the continuous orientation after a pole crossing.
struct ScriptCamera {
    float yaw{},pitch{},distanceKm=5200.f,fov=40.f;
    bool frozen=false;
    bool operator==(const ScriptCamera&)const=default;
    bool Valid()const{return std::isfinite(yaw)&&std::isfinite(pitch)&&std::isfinite(distanceKm)&&std::isfinite(fov)
        &&distanceKm>1737.4&&distanceKm<=1.e9f&&fov>0&&fov<180;}
};
// Snapshot scripts intentionally do not accept the former sampled-path format.
struct CameraScript {
    unsigned width{},height{};
    std::vector<ScriptCamera> views;
    bool Valid()const{
        if(!width||!height||views.empty()||views.size()>4096)return false;
        for(const auto& c:views)if(!c.Valid()||c.frozen)return false;
        return true;
    }
    bool Save(const std::filesystem::path& path,std::string& error)const{
        if(!Valid()){error="Capture at least one valid, unfrozen snapshot";return false;}
        const auto temporary=std::filesystem::path(path.wstring()+L".tmp");
        try{
            if(std::filesystem::exists(path)||std::filesystem::exists(temporary)){error="Script name already exists";return false;}
            std::ofstream out(temporary,std::ios::binary);out.imbue(std::locale::classic());
            out<<std::setprecision(std::numeric_limits<float>::max_digits10)<<"ORBITX_SNAPSHOT_SCRIPT 1 Moon\nVIEWPORT "<<width<<' '<<height<<'\n';
            for(const auto& c:views)out<<"VIEW "<<c.yaw<<' '<<c.pitch<<' '<<c.distanceKm<<' '<<c.fov<<'\n';
            out.close();if(!out){std::filesystem::remove(temporary);error="Cannot write snapshot script";return false;}
            std::filesystem::rename(temporary,path);error.clear();return true;
        }catch(const std::exception& e){error=e.what();std::error_code ignored;std::filesystem::remove(temporary,ignored);return false;}
    }
    static bool Load(const std::filesystem::path& path,CameraScript& result,std::string& error){
        try{
            if(std::filesystem::file_size(path)>1024*1024)throw std::runtime_error("Snapshot script exceeds 1 MiB");
            std::ifstream in(path);in.imbue(std::locale::classic());CameraScript script;
            std::string key,body;int version=0;
            if(!(in>>key>>version>>body)||key!="ORBITX_SNAPSHOT_SCRIPT"||version!=1||body!="Moon")throw std::runtime_error("Unsupported script: recreate using TAKE SNAPSHOT");
            if(!(in>>key>>script.width>>script.height)||key!="VIEWPORT")throw std::runtime_error("Missing viewport");
            while(in>>key){
                ScriptCamera c;
                if(key!="VIEW"||!(in>>c.yaw>>c.pitch>>c.distanceKm>>c.fov))throw std::runtime_error("Invalid snapshot");
                script.views.push_back(c);if(script.views.size()>4096)throw std::runtime_error("Too many snapshots");
            }
            if(!in.eof()||!script.Valid())throw std::runtime_error("Invalid snapshot script");
            result=std::move(script);error.clear();return true;
        }catch(const std::exception& e){error=e.what();return false;}
    }
};
}
