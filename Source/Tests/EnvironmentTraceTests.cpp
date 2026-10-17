#include "../Core/EnvironmentTrace.h"
#include "../Core/PlanetQuadtree.h"
#include "../Core/ScriptPanelLayout.h"
#include <cassert>
#include <iostream>
#include <regex>
#include <thread>
#include <vector>
using namespace OrbitX::Core;
int main(){
    auto directory=std::filesystem::temp_directory_path()/("OrbitXTraceTest_"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    EnvironmentTrace trace;
    struct PatchState {TileId id,source;unsigned slot,grid;float uv[4];bool operator==(const PatchState&) const = default;};
    TraceStateCache<TileId,PatchState> numeric;
    PatchState patch{{11,0,0},{11,0,0},1,3,{0,0,1,1}};
    unsigned formattedStates=0;
    for(int i=0;i<1000;i++)if(numeric.Update(patch.id,patch))++formattedStates;
    assert(formattedStates==1);
    patch.source={10,0,0};assert(numeric.Update(patch.id,patch));
    patch.slot=2;assert(numeric.Update(patch.id,patch));
    patch.uv[0]=.5f;assert(numeric.Update(patch.id,patch));
    numeric.Erase(patch.id);assert(numeric.Update(patch.id,patch));
    numeric.Clear();assert(numeric.Update(patch.id,patch));
    bool formatted=false;trace.Log([&](auto& out){formatted=true;out<<"INACTIVE";});assert(!formatted);
    trace.SetFrame(42);assert(trace.Start(directory));const auto first=trace.Path();const auto firstSession=trace.Session();
    std::vector<std::thread> workers;
    for(int worker=0;worker<4;worker++)workers.emplace_back([&,worker]{for(int i=0;i<250;i++)trace.Log([&](auto& out){out<<"WORKER worker="<<worker<<" item="<<i;});});
    for(auto& worker:workers)worker.join();
    trace.Log([](auto& out){out<<"MULTILINE message=first\nsecond\rthird";});
    trace.Stop("USER_STOP");assert(!trace.Active()&&trace.Error().empty());
    std::ifstream file(first);std::string line;uint64_t expected=1;long long previous=-1;unsigned count=0;
    const std::regex pattern(R"(^\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}\.\d{6} seq=(\d+) frame=42 elapsed_us=(\d+) .+$)");
    while(std::getline(file,line)){
        std::smatch match;assert(std::regex_match(line,match,pattern));assert(std::stoull(match[1])==expected++);
        const auto elapsed=std::stoll(match[2]);assert(elapsed>=previous);previous=elapsed;++count;
    }
    assert(count==1003);
    // Delta logging keeps transitions, suppresses unchanged state, resets per session.
    assert(trace.Start(directory));const auto compact=trace.Path();
    assert(trace.Session()==firstSession+1);
    for(int i=0;i<1000;i++)trace.LogChanged("tile",[](auto& out){out<<"DRAW_PATCH source=11";});
    trace.LogChanged("tile",[](auto& out){out<<"DRAW_REMOVE";});
    trace.LogChanged("tile",[](auto& out){out<<"DRAW_PATCH source=11";});
    trace.Stop("COMPACT_TEST");
    std::ifstream compactFile(compact);count=0;while(std::getline(compactFile,line))++count;
    assert(count==5);compactFile.close();
    assert(trace.Start(directory));assert(trace.Path()!=first);
    trace.LogChanged("tile",[](auto& out){out<<"DRAW_PATCH source=11";});
    PlanetQuadtree original,traced;
    for(int level=4;level<=8;level++)for(int row=0;row<(1<<(level-4));row++)for(int column=0;column<(2<<(level-4));column++){
        TileId id{level,row,column};original.files[id]="tile.dds";original.nodes.insert(id);
    }
    traced=original;
    auto resident=[](TileId id){return id.level<=6||(id.row+id.column)%3==0;};
    for(double latitude:{-89.99,-45.,0.,45.,89.99})for(double longitude:{-179.,0.,90.}){
        const auto position=SurfaceDirection(latitude*PlanetPi/180,longitude*PlanetPi/180)*1750.;
        const auto camera=PlanetCamera::Orbit(longitude*PlanetPi/180+PlanetPi*.5,-latitude*PlanetPi/180,1750.);
        auto view=[&](PlanetVector center,double radius){return camera.SphereInView(center,radius,std::tan(20*PlanetPi/180),16./9.);};
        original.Select(position.x,position.y,position.z,1737.4,1500,resident,view);
        traced.Select(position.x,position.y,position.z,1737.4,1500,resident,view,&trace);
        assert(original.leaves==traced.leaves&&original.requests==traced.requests);
    }
    original.frozen=traced.frozen=true;
    original.Select(0,5000,0,1737.4,1500,resident);traced.Select(0,5000,0,1737.4,1500,resident,{},&trace);
    assert(original.leaves==traced.leaves&&original.requests==traced.requests);
    trace.Stop("TEST_COMPLETE");
    // Stop while producers are still alive; later writes must not reopen or append.
    assert(trace.Start(directory));const auto closing=trace.Path();
    std::atomic<int> readyCount=0;std::atomic<bool> continueWriting=false;workers.clear();
    for(int worker=0;worker<4;worker++)workers.emplace_back([&,worker]{
        trace.Log([&](auto& out){out<<"BEFORE_CLOSE worker="<<worker;});++readyCount;
        while(!continueWriting.load())std::this_thread::yield();
        for(int i=0;i<50;i++)trace.Log([](auto& out){out<<"AFTER_CLOSE";});
    });
    while(readyCount.load()!=4)std::this_thread::yield();
    trace.Stop("CONCURRENT_CLOSE");continueWriting=true;
    for(auto& worker:workers)worker.join();
    std::ifstream closed(closing);count=0;while(std::getline(closed,line)){assert(line.find("AFTER_CLOSE")==std::string::npos);++count;}
    assert(count==6);
    for(auto [width,height]:{std::pair{1920.f,1080.f},std::pair{1280.f,720.f},std::pair{320.f,180.f}}){
        auto layout=ScriptPanelLayout::Build(width,height);
        assert(layout.panel.left>=0&&layout.panel.top>=0&&layout.panel.right<=width&&layout.panel.bottom<=height);
        assert(layout.action.Contains((layout.action.left+layout.action.right)*.5f,(layout.action.top+layout.action.bottom)*.5f));
        assert(!layout.action.Contains(layout.benchmark.left,layout.benchmark.top));
    }
    assert(!trace.Start(first/"not_a_directory"));assert(!trace.Active()&&!trace.Error().empty());
    assert(trace.Start(directory));trace.Stop("RECOVERED");assert(trace.Error().empty());
    file.close();closed.close();
    std::filesystem::remove_all(directory);
    std::cout<<"Environment trace tests passed\n";
}
