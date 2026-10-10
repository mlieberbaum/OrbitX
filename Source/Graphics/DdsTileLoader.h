#pragma once
#include "DdsSurface.h"
#include "../Core/PlanetQuadtree.h"
#include <chrono>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <optional>
#include <set>
#include <thread>
namespace OrbitX::Graphics {
// CPU-only worker: no rendering state, GPU resources, or device context is accessed here.
class DdsTileLoader {
public:
    struct Request { Core::TileId id;std::filesystem::path path; };
    struct Result { Core::TileId id;DdsSurface surface;std::string error;double readMs=0; };
    ~DdsTileLoader(){Stop();}
    void Start(){worker=std::thread([this]{Run();});}
    void Stop(){
        {std::lock_guard lock(mutex);stopping=true;}
        wake.notify_all();if(worker.joinable())worker.join();
    }
    void SetRequests(const std::vector<Request>& requests){
        std::lock_guard lock(mutex);wanted.clear();for(const auto& r:requests)wanted.insert(r.id);
        for(auto it=ready.begin();it!=ready.end();) {
            if(!wanted.contains(it->id)){readyBytes-=it->surface.bytes.size();it=ready.erase(it);}else ++it;
        }
        std::set<Core::TileId> available;for(const auto& r:ready)available.insert(r.id);
        queue.clear();
        for(const auto& r:requests)if(!available.contains(r.id)&&(!active||*active!=r.id))queue.push_back(r);
        wake.notify_all();
    }
    std::optional<Result> TryPop(){
        std::lock_guard lock(mutex);if(ready.empty())return {};
        auto result=std::move(ready.front());ready.pop_front();readyBytes-=result.surface.bytes.size();wake.notify_all();return result;
    }
private:
    void Run(){
        for(;;){
            Request request;
            {std::unique_lock lock(mutex);wake.wait(lock,[&]{return stopping||(!queue.empty()&&ready.size()<32&&readyBytes<64ull*1024*1024);});
             if(stopping){return;}
             request=queue.front();queue.pop_front();active=request.id;}
            Result result{};result.id=request.id;auto start=std::chrono::steady_clock::now();
            try{result.surface=DdsSurface::Read(request.path);}catch(const std::exception& e){result.error=e.what();}
            result.readMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
            {std::lock_guard lock(mutex);active.reset();if(wanted.contains(request.id)){readyBytes+=result.surface.bytes.size();ready.push_back(std::move(result));}}
            wake.notify_all();
        }
    }
    std::thread worker;
    std::mutex mutex;std::condition_variable wake;
    std::deque<Request> queue;std::deque<Result> ready;
    std::set<Core::TileId> wanted;std::optional<Core::TileId> active;
    size_t readyBytes=0;bool stopping=false;
};
}
