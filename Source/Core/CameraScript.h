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
struct CameraSample {
    uint64_t timeUs{};ScriptCamera camera;bool checkpoint=false;
    // Snapshot at the initial frozen view and each transition into frozen LOD.
    bool frozenSelection=false;std::vector<TileId> leaves;
};
struct CameraScript {
    unsigned width{},height{};
    uint64_t durationUs{};
    std::vector<CameraSample> samples;
    bool Valid()const{
        if(!width||!height||samples.empty()||samples.size()>1000000||samples.front().timeUs!=0)return false;
        uint64_t previous=0;
        bool wasFrozen=false;size_t leafCount=0;
        for(const auto& s:samples){
            if(!s.camera.Valid()||s.timeUs<previous||s.timeUs>durationUs||(!s.leaves.empty()&&!s.frozenSelection))return false;
            if(s.frozenSelection&&!s.camera.frozen)return false;
            if(s.camera.frozen&&!wasFrozen&&!s.frozenSelection)return false;
            for(const auto id:s.leaves){if(id.level<4||id.level>19||id.row<0||id.row>=(1<<(id.level-4))||id.column<0||id.column>=(2<<(id.level-4)))return false;}
            leafCount+=s.leaves.size();if(leafCount>1000000)return false;
            previous=s.timeUs;wasFrozen=s.camera.frozen;
        }
        return durationUs<=24ull*60*60*1000000;
    }
    bool Save(const std::filesystem::path& path,std::string& error)const{
        if(!Valid()){error="Invalid camera script";return false;}
        const auto temporary=std::filesystem::path(path.wstring()+L".tmp");
        try{
            if(std::filesystem::exists(path)||std::filesystem::exists(temporary)){error="Script name already exists";return false;}
            std::ofstream out(temporary);out.imbue(std::locale::classic());
            out<<std::setprecision(17)<<"ORBITX_CAMERA_SCRIPT 1 Moon\nVIEWPORT "<<width<<' '<<height<<"\nDURATION_US "<<durationUs<<'\n';
            for(const auto& s:samples){
                const auto p=PlanetCamera::Orbit(s.camera.yaw,s.camera.pitch,s.camera.distanceKm).position;
                out<<"CAMERA "<<s.timeUs<<' '<<s.camera.yaw<<' '<<s.camera.pitch<<' '<<s.camera.distanceKm<<' '<<s.camera.fov
                    <<' '<<s.camera.frozen<<' '<<s.checkpoint<<' '<<s.frozenSelection<<' '<<std::atan2(p.z,std::hypot(p.x,p.y))*180/PlanetPi
                    <<' '<<std::atan2(p.y,p.x)*180/PlanetPi<<' '<<double(s.camera.distanceKm)-1737.4<<'\n';
                for(const auto id:s.leaves)out<<"FROZEN_LEAF "<<id.level<<' '<<id.row<<' '<<id.column<<'\n';
            }
            out.close();if(!out){std::filesystem::remove(temporary);error="Cannot write camera script";return false;}
            if(std::filesystem::file_size(temporary)>64ull*1024*1024){std::filesystem::remove(temporary);error="Script exceeds 64 MiB";return false;}
            std::filesystem::rename(temporary,path);error.clear();return true;
        }catch(const std::exception& e){error=e.what();std::error_code ignored;std::filesystem::remove(temporary,ignored);return false;}
    }
    static bool Load(const std::filesystem::path& path,CameraScript& result,std::string& error){
        try{
            if(std::filesystem::file_size(path)>64ull*1024*1024){error="Script exceeds 64 MiB";return false;}
            std::ifstream in(path);in.imbue(std::locale::classic());CameraScript script;
            std::string key,body;int version=0;
            if(!(in>>key>>version>>body)||key!="ORBITX_CAMERA_SCRIPT"||version!=1||body!="Moon")throw std::runtime_error("Unsupported script format or target");
            if(!(in>>key>>script.width>>script.height)||key!="VIEWPORT")throw std::runtime_error("Missing viewport");
            if(!(in>>key>>script.durationUs)||key!="DURATION_US")throw std::runtime_error("Missing duration");
            size_t leafCount=0;
            while(in>>key){
                if(key=="FROZEN_LEAF"){
                    TileId id;
                    if(script.samples.empty()||!script.samples.back().frozenSelection||!(in>>id.level>>id.row>>id.column)||++leafCount>1000000)throw std::runtime_error("Invalid frozen selection");
                    script.samples.back().leaves.push_back(id);continue;
                }
                CameraSample sample;int frozen=0,checkpoint=0,frozenSelection=0;double latitude=0,longitude=0,altitude=0;
                if(key!="CAMERA"||!(in>>sample.timeUs>>sample.camera.yaw>>sample.camera.pitch>>sample.camera.distanceKm>>sample.camera.fov>>frozen>>checkpoint>>frozenSelection>>latitude>>longitude>>altitude)
                    ||(frozen!=0&&frozen!=1)||(checkpoint!=0&&checkpoint!=1)||(frozenSelection!=0&&frozenSelection!=1)||!std::isfinite(latitude)||!std::isfinite(longitude)||!std::isfinite(altitude))throw std::runtime_error("Invalid camera sample");
                sample.camera.frozen=frozen!=0;sample.checkpoint=checkpoint!=0;sample.frozenSelection=frozenSelection!=0;script.samples.push_back(sample);
                if(script.samples.size()>1000000)throw std::runtime_error("Too many camera samples");
            }
            if(!in.eof()||!script.Valid())throw std::runtime_error("Invalid camera timeline");
            result=std::move(script);error.clear();return true;
        }catch(const std::exception& e){error=e.what();return false;}
    }
};
// Monotonic playback time excludes checkpoint waits but includes panel-open time.
struct ScriptClock {
    uint64_t start{},pausedAt{},pausedTotal{};bool waiting=false;
    uint64_t Elapsed(uint64_t now)const{return (waiting?pausedAt:now)-start-pausedTotal;}
    void Pause(uint64_t now){if(!waiting){pausedAt=now;waiting=true;}}
    void Resume(uint64_t now){if(waiting){pausedTotal+=now-pausedAt;waiting=false;}}
};
}
