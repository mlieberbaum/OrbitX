#pragma once
#include "CameraScript.h"
#include <bit>
#include <array>

namespace OrbitX::Core {
inline constexpr uint64_t RegressionMinimumDwellUs=4000000;
struct RegressionView {uint64_t timeUs{},endUs{};ScriptCamera camera;};
inline std::vector<RegressionView> RegressionViews(const CameraScript& script){
    std::vector<RegressionView> views;size_t begin=0;
    for(size_t end=1;end<=script.samples.size();++end){
        if(end<script.samples.size()&&script.samples[end].camera==script.samples[begin].camera)continue;
        const auto& first=script.samples[begin];const auto& last=script.samples[end-1];
        if(last.timeUs-first.timeUs>RegressionMinimumDwellUs)views.push_back({last.timeUs-100000,last.timeUs,first.camera});
        begin=end;
    }
    return views;
}
// Versioned, explicitly byte-ordered signature of camera commands, independent
// of filename, wall clock, GPU allocation, and legacy checkpoint markers.
inline uint64_t ScriptSignature(const CameraScript& script){
    uint64_t hash=14695981039346656037ull;
    auto number=[&](uint64_t value){for(int i=0;i<8;++i){hash^=value&255;hash*=1099511628211ull;value>>=8;}};
    number(script.width);number(script.height);number(script.durationUs);number(script.samples.size());
    for(const auto& s:script.samples){
        number(s.timeUs);number(std::bit_cast<uint32_t>(s.camera.yaw));number(std::bit_cast<uint32_t>(s.camera.pitch));
        number(std::bit_cast<uint32_t>(s.camera.distanceKm));number(std::bit_cast<uint32_t>(s.camera.fov));number(s.camera.frozen);
        number(s.frozenSelection);number(s.leaves.size());
        for(auto id:s.leaves){number(id.level);number(id.row);number(id.column);}
    }
    return hash;
}
struct RegressionDraw {
    TileId id,source;uint32_t grid{},indexCount{};std::array<float,4> uv{};
    auto operator<=>(const RegressionDraw&)const=default;
};
struct RegressionSnapshot {
    ScriptCamera camera;std::vector<TileId> leaves;std::vector<RegressionDraw> draws;
    std::array<float,35> scene{};
    bool operator==(const RegressionSnapshot&)const=default;
};
// Render-thread-only writer. Classic locale, LF newlines, sorted tile records,
// and round-trip float precision make healthy runs byte-comparable.
class RegressionLog {
public:
    bool Active()const{return active;}
    const std::string& Error()const{return error;}
    const std::filesystem::path& Path()const{return path;}
    bool Start(const std::filesystem::path& directory,const CameraScript& script,size_t views,bool illuminated){
        if(active)return false;
        if(file.is_open())file.close();file.clear();error.clear();incomplete=0;
        try{
            std::filesystem::create_directories(directory);unsigned index=1;
            do{path=directory/("Regression_"+std::to_string(index++)+".log");}while(std::filesystem::exists(path));
            file.open(path,std::ios::out|std::ios::binary);file.imbue(std::locale::classic());
            if(!file){error="Cannot open regression log";return false;}
            active=true;file<<std::setprecision(std::numeric_limits<float>::max_digits10);
            Line(0);file<<"REGRESSION format=1 body=Moon radius_km=1737.4 policy=stationary_tail dwell_policy=2 minimum_pause_us="<<RegressionMinimumDwellUs<<" dwell_comparison=GT margin_us=100000 stable_checks=4\n";
            Line(0);file<<"INPUT script_signature="<<std::hex<<ScriptSignature(script)<<std::dec
                <<" duration_us="<<script.durationUs<<" viewport="<<script.width<<","<<script.height<<" views="<<views<<" illumination="<<(illuminated?"FULL":"STANDARD")<<'\n';
            Check();return active;
        }catch(const std::exception& e){error=e.what();active=false;return false;}
    }
    void View(size_t index,const RegressionView& view,RegressionSnapshot snapshot,const char* status){
        if(!active)return;
        if(std::string(status)!="READY")++incomplete;
        std::sort(snapshot.leaves.begin(),snapshot.leaves.end());
        std::sort(snapshot.draws.begin(),snapshot.draws.end(),[](const auto& a,const auto& b){return a.id<b.id;});
        const auto t=view.timeUs;
        Line(t);file<<"VIEW index="<<index<<" status="<<status<<" selected="<<snapshot.leaves.size()<<" drawn="<<snapshot.draws.size()<<'\n';
        Line(t);file<<"CAMERA yaw="<<snapshot.camera.yaw<<" pitch="<<snapshot.camera.pitch<<" distance_km="<<snapshot.camera.distanceKm
            <<" fov="<<snapshot.camera.fov<<" frozen="<<snapshot.camera.frozen<<'\n';
        Line(t);file<<"SCENE mvp=";for(size_t i=0;i<16;++i)file<<snapshot.scene[i]<<",";
        file<<" model=";for(size_t i=16;i<32;++i)file<<snapshot.scene[i]<<",";
        file<<" sun="<<snapshot.scene[32]<<","<<snapshot.scene[33]<<","<<snapshot.scene[34]<<'\n';
        for(auto id:snapshot.leaves){Line(t);file<<"SELECT ";Id(id);file<<'\n';}
        for(const auto& d:snapshot.draws){Line(t);file<<"DRAW tile=";Id(d.id);file<<" source=";Id(d.source);file<<" grid="<<d.grid<<" indices="<<d.indexCount<<" uv=";
            for(size_t i=0;i<4;++i)file<<(i?",":"")<<d.uv[i];file<<'\n';}
        Check();
    }
    void Missing(size_t index,const RegressionView& view){
        if(!active)return;++incomplete;Line(view.timeUs);file<<"VIEW index="<<index<<" status=MISSED\n";Check();
    }
    void Stop(uint64_t elapsed,const char* reason){
        if(active){Line(elapsed);file<<"RESULT status="<<(std::string(reason)=="SCRIPT_COMPLETE"&&!incomplete?"COMPLETE":"INVALID")
            <<" incomplete_views="<<incomplete<<" reason="<<reason<<'\n';Check();}
        active=false;
        if(file.is_open()){file.flush();if(!file&&error.empty())error="Regression log flush failed";file.close();if(file.fail()&&error.empty())error="Regression log close failed";}
    }
    unsigned Incomplete()const{return incomplete;}
private:
    void Line(uint64_t t){file<<"elapsed_us="<<t<<' ';}
    void Id(TileId id){file<<id.level<<'/'<<id.row<<'/'<<id.column;}
    void Check(){if(!file){active=false;error="Regression log write failed";}}
    std::ofstream file;std::filesystem::path path;std::string error;bool active=false;unsigned incomplete=0;
};
}
