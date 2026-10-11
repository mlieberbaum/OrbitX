#pragma once
#include "RegressionLog.h"
namespace OrbitX::Core {
struct FrameBucket {uint64_t endUs{},durationUs{},frames{};};
struct SnapshotResult {
    uint64_t loadingUs{},measurementUs{},frames{};
    std::string status="INCOMPLETE";
    std::vector<FrameBucket> buckets;
};
// Uses completed render-loop frames, not smoothed HUD FPS or system time.
class SnapshotRun {
public:
    enum class Phase { Loading, Measuring, Done };
    void Begin(uint64_t now){
        phase=Phase::Loading;start=now;stable=false;unstable=false;result={};result.buckets.reserve(128);
    }
    void Observe(uint64_t now,bool ready,bool changed){
        if(phase==Phase::Loading){
            if(!ready){stable=false;}
            else if(!stable||changed){stable=true;steadyStart=now;}
            if(stable&&now-steadyStart>=SnapshotSteadyUs){
                phase=Phase::Measuring;measureStart=bucketStart=now;bucketFrames=0;result.loadingUs=now-start;
            }else if(now-start>=SnapshotTimeoutUs){phase=Phase::Done;result.loadingUs=now-start;result.status="TIMEOUT";}
            return;
        }
        if(phase!=Phase::Measuring)return;
        if(!ready||changed)unstable=true;
        ++bucketFrames;++result.frames;result.measurementUs=now-measureStart;
        if(now-bucketStart>=SnapshotBucketUs||result.measurementUs>=SnapshotHoldUs){
            result.buckets.push_back({result.measurementUs,now-bucketStart,bucketFrames});bucketStart=now;bucketFrames=0;
        }
        if(result.measurementUs>=SnapshotHoldUs){phase=Phase::Done;result.status=unstable?"UNSTABLE":"READY";}
    }
    Phase State()const{return phase;}
    const SnapshotResult& Result()const{return result;}
private:
    Phase phase=Phase::Done;uint64_t start{},steadyStart{},measureStart{},bucketStart{},bucketFrames{};
    bool stable=false,unstable=false;SnapshotResult result;
};
struct ScriptFrameBucket {
    uint64_t endUs{},durationUs{},frames{};size_t firstView{},lastView{};std::string phase;
};
class ScriptFrameRecorder {
public:
    void Begin(uint64_t now){start=previous=now;frames=0;buckets.clear();buckets.reserve(1024);phase.clear();}
    void Frame(uint64_t now,size_t view,const char* currentPhase){
        if(!frames){firstView=view;phase=currentPhase;}else if(phase!=currentPhase)phase="MIXED";
        lastView=view;++frames;
        if(now-previous>=SnapshotBucketUs)Flush(now);
    }
    void Finish(uint64_t now){if(frames)Flush(now);}
    const std::vector<ScriptFrameBucket>& Buckets()const{return buckets;}
private:
    void Flush(uint64_t now){buckets.push_back({now-start,now-previous,frames,firstView,lastView,phase});previous=now;frames=0;}
    uint64_t start{},previous{},frames{};size_t firstView{},lastView{};std::string phase;
    std::vector<ScriptFrameBucket> buckets;
};
// Called only after a run stops. No file I/O in loading/measurement windows.
inline bool SaveSnapshotPerformance(const std::filesystem::path& directory,const CameraScript& script,
    const std::vector<SnapshotResult>& results,const std::vector<ScriptFrameBucket>& wholeRun,const char* mode,bool overlay,bool graph,bool vsync,
    const char* reason,std::filesystem::path& path,std::string& error){
    try{
        std::filesystem::create_directories(directory);unsigned index=1;
        do{path=directory/("Performance_"+std::to_string(index++)+".csv");}while(std::filesystem::exists(path));
        std::ofstream out(path,std::ios::binary);out.imbue(std::locale::classic());out<<std::setprecision(12);
        out<<"# OrbitX snapshot performance format=1 mode="<<mode<<" signature="<<std::hex<<ScriptSignature(script)<<std::dec
           <<" viewport="<<script.width<<","<<script.height<<" cache=ROOT_ONLY os_disk_cache=UNCONTROLLED steady_us="<<SnapshotSteadyUs
           <<" hold_us="<<SnapshotHoldUs<<" bucket_target_us="<<SnapshotBucketUs<<" overlay="<<overlay<<" graph="<<graph<<" vsync="<<vsync<<" reason="<<reason<<'\n';
        out<<"view,status,loading_us,measurement_us,frames,mean_fps\n";
        for(size_t i=0;i<results.size();++i){const auto& r=results[i];out<<i<<','<<r.status<<','<<r.loadingUs<<','<<r.measurementUs<<','<<r.frames<<','<<(r.measurementUs?double(r.frames)*1.e6/r.measurementUs:0)<<'\n';}
        out<<"view,bucket,end_us,duration_us,frames,fps\n";
        for(size_t i=0;i<results.size();++i)for(size_t j=0;j<results[i].buckets.size();++j){const auto b=results[i].buckets[j];out<<i<<','<<j<<','<<b.endUs<<','<<b.durationUs<<','<<b.frames<<','<<(b.durationUs?double(b.frames)*1.e6/b.durationUs:0)<<'\n';}
        out<<"script_bucket,end_us,duration_us,frames,fps,first_view,last_view,phase\n";
        for(size_t i=0;i<wholeRun.size();++i){const auto& b=wholeRun[i];out<<i<<','<<b.endUs<<','<<b.durationUs<<','<<b.frames<<','<<(b.durationUs?double(b.frames)*1.e6/b.durationUs:0)<<','<<b.firstView<<','<<b.lastView<<','<<b.phase<<'\n';}
        out.close();if(!out)throw std::runtime_error("Cannot write performance results");error.clear();return true;
    }catch(const std::exception& e){error=e.what();return false;}
}
}
