#pragma once
#include <atomic>
#include <chrono>
#include <cstdint>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <map>
#include <sstream>
#include <string>

namespace OrbitX::Core {
// Livestream deliberately favors a complete, ordered trace over frame rate.
// All writers (including the DDS worker) serialize through this lock. When idle,
// the atomic gate prevents formatting, locking and file I/O on the render path.
class EnvironmentTrace {
public:
    ~EnvironmentTrace(){Stop("TRACE_DESTROYED");}
    bool Active() const {return active.load(std::memory_order_relaxed);}
    void SetFrame(uint64_t value){frame.store(value,std::memory_order_relaxed);}
    std::filesystem::path Path() const {std::lock_guard lock(mutex);return path;}
    std::string Error() const {std::lock_guard lock(mutex);return error;}
    bool Start(const std::filesystem::path& directory){
        std::lock_guard lock(mutex);
        if(active.load())return true;
        if(file.is_open())file.close();
        file.clear();
        error.clear();changes.clear();sequence=0;started=std::chrono::steady_clock::now();
        try {
            std::filesystem::create_directories(directory);
            const auto stamp=Timestamp(std::chrono::system_clock::now(),true);
            path=directory/ ("Livestream_"+stamp+".log");
            unsigned suffix=0;
            while(std::filesystem::exists(path))path=directory/("Livestream_"+stamp+"_"+std::to_string(++suffix)+".log");
            file.open(path,std::ios::out|std::ios::trunc);
            if(!file){error="Cannot open livestream log";file.clear();return false;}
            active.store(true);
            WriteLocked("TRACE_START mode=livestream format=2 detail=changes");
            return active.load();
        }catch(const std::exception& e){error=e.what();return false;}
    }
    void Stop(const std::string& reason){
        std::lock_guard lock(mutex);
        if(active.load())WriteLocked("TRACE_STOP reason="+Escape(reason));
        active.store(false);
        if(file.is_open()){
            file.flush();if(!file && error.empty())error="Livestream log flush failed";
            file.close();if(file.fail() && error.empty())error="Livestream log close failed";
        }
        file.clear();
    }
    template<class Build> void Log(Build&& build){
        if(!Active())return;
        std::lock_guard lock(mutex);if(!active.load())return;
        std::ostringstream event;event<<std::setprecision(17);build(event);
        WriteLocked(event.str());
    }
    static std::string Escape(const std::string& text){
        std::string result;for(char c:text){if(c=='\n')result+="\\n";else if(c=='\r')result+="\\r";else if(c=='\\')result+="\\\\";else if(c=='\"')result+="\\\"";else result+=c;}return result;
    }
    template<class Build> void LogChanged(const std::string& key,Build&& build){
        if(!Active())return;
        std::lock_guard lock(mutex);if(!active.load())return;
        std::ostringstream event;event<<std::setprecision(17);build(event);
        auto value=event.str();auto it=changes.find(key);
        if(it!=changes.end()&&it->second==value)return;
        changes[key]=value;WriteLocked(value);
    }
private:
    static std::string Timestamp(std::chrono::system_clock::time_point now,bool filename){
        using namespace std::chrono;
        const auto secondsPoint=floor<seconds>(now);
        const auto micros=duration_cast<microseconds>(now-secondsPoint).count();
        const auto time=system_clock::to_time_t(secondsPoint);std::tm local{};
#ifdef _WIN32
        localtime_s(&local,&time);
#else
        localtime_r(&time,&local);
#endif
        std::ostringstream out;out<<std::put_time(&local,filename?"%Y-%m-%d_%H%M%S":"%Y-%m-%d %H:%M:%S")
            <<(filename?'_':'.')<<std::setw(6)<<std::setfill('0')<<micros;return out.str();
    }
    void WriteLocked(const std::string& event){
        const auto elapsed=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-started).count();
        file<<Timestamp(std::chrono::system_clock::now(),false)<<" seq="<<++sequence
            <<" frame="<<frame.load()<<" elapsed_us="<<elapsed<<" "<<EscapeLine(event)<<'\n';
        if(!file){error="Livestream log write failed";active.store(false);}
    }
    static std::string EscapeLine(const std::string& text){
        std::string result;for(char c:text){if(c=='\n')result+="\\n";else if(c=='\r')result+="\\r";else result+=c;}return result;
    }
    mutable std::mutex mutex;
    std::ofstream file;
    std::filesystem::path path;
    std::string error;
    std::map<std::string,std::string> changes;
    std::atomic<bool> active=false;
    std::atomic<uint64_t> frame=0;
    uint64_t sequence=0;
    std::chrono::steady_clock::time_point started{};
};
}
