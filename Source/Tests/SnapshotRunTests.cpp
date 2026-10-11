#include "../Core/SnapshotRun.h"
#include "../Graphics/DdsTileLoader.h"
#include <cassert>
#include <iostream>
using namespace OrbitX::Core;
int main(){
    SnapshotRun run;run.Begin(0);run.Observe(1000000,false,false);assert(run.State()==SnapshotRun::Phase::Loading);
    run.Observe(2000000,true,true);run.Observe(3999999,true,false);assert(run.State()==SnapshotRun::Phase::Loading);
    run.Observe(4000000,true,false);assert(run.State()==SnapshotRun::Phase::Measuring&&run.Result().loadingUs==4000000);
    for(uint64_t t=4001000;t<=14000000;t+=1000)run.Observe(t,true,false);
    assert(run.State()==SnapshotRun::Phase::Done&&run.Result().status=="READY");
    assert(run.Result().frames==10000&&run.Result().buckets.size()==100&&run.Result().measurementUs==10000000);
    uint64_t frames=0,elapsed=0;for(auto b:run.Result().buckets){frames+=b.frames;elapsed+=b.durationUs;assert(b.durationUs==100000&&b.frames==100);}
    assert(frames==10000&&elapsed==10000000);
    run.Begin(0);run.Observe(0,true,true);run.Observe(1000000,false,false);run.Observe(2000000,true,false);
    run.Observe(3000000,true,true);run.Observe(4999999,true,false);assert(run.State()==SnapshotRun::Phase::Loading);
    run.Observe(5000000,true,false);assert(run.State()==SnapshotRun::Phase::Measuring);
    run.Observe(5001300,true,true);run.Observe(15001000,true,false);assert(run.Result().status=="UNSTABLE");
    run.Begin(0);run.Observe(SnapshotTimeoutUs,false,false);assert(run.Result().status=="TIMEOUT");
    run.Begin(10);run.Observe(10,true,true);run.Observe(2000010,true,false);run.Observe(2102345,true,false);
    assert(run.Result().buckets[0].durationUs==102335); // Actual bucket duration, including jitter/stalls.
    ScriptFrameRecorder all;all.Begin(0);all.Frame(200000,0,"RESET_LOADING");
    assert(all.Buckets().size()==1&&all.Buckets()[0].durationUs==200000&&all.Buckets()[0].frames==1);
    all.Frame(250000,0,"LOADING");all.Frame(300000,0,"MEASURING");
    assert(all.Buckets()[1].phase=="MIXED"&&all.Buckets()[1].frames==2);
    all.Frame(350000,1,"RESET_LOADING");all.Finish(360000);assert(all.Buckets().size()==3&&all.Buckets()[2].durationUs==60000);
    const auto directory=std::filesystem::temp_directory_path()/("orbitx-snapshot-test-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    CameraScript script;script.width=10;script.height=10;script.views={{0,0,1800,40,false}};
    std::filesystem::path path;std::string error;
    assert(SaveSnapshotPerformance(directory,script,{run.Result()},{},"BENCHMARK",false,false,false,"USER_STOP",path,error));assert(std::filesystem::file_size(path)>100);
    // A reset discards queued/ready results and drains an active CPU read. The
    // same tile can then be requested anew, without old-view result pollution.
    auto file=directory/"tile.dds";
    std::vector<uint8_t> raw(136);auto word=[&](size_t i,uint32_t v){std::memcpy(raw.data()+i,&v,4);};
    word(0,0x20534444);word(4,124);word(12,4);word(16,4);word(28,1);word(76,32);word(84,0x31545844);
    {std::ofstream out(file,std::ios::binary);out.write(reinterpret_cast<const char*>(raw.data()),raw.size());}
    OrbitX::Graphics::DdsTileLoader loader;loader.Start();
    for(int i=0;i<30;++i){loader.SetRequests({{{5,0,0},file},{{5,0,1},file}});loader.Reset();assert(!loader.TryPop());}
    loader.SetRequests({{{5,0,0},file}});const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
    for(;;){auto r=loader.TryPop();if(r){assert(r->id==TileId(5,0,0)&&r->error.empty()&&r->surface.bytes.size()==8);break;}
        assert(std::chrono::steady_clock::now()<deadline);std::this_thread::yield();}
    loader.Reset();loader.Stop();std::filesystem::remove_all(directory);
    std::cout<<"Snapshot timing, performance buckets and loader reset tests passed\n";
}
