#pragma once
#include <chrono>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <string>
#include <stdexcept>
namespace OrbitX::Core {
// The start timestamp belongs to the session, not to an individual log writer.
// None recording still gets a unique script suffix, without creating a log folder.
struct ScriptSession {
    std::string stamp;
    std::filesystem::path directory;
    bool Begin(const std::filesystem::path& logs,bool logging,std::string& error,
               std::chrono::system_clock::time_point now=std::chrono::system_clock::now()){
        directory.clear();error.clear();
        try{
            using namespace std::chrono;
            const auto secondsPoint=floor<seconds>(now);const auto micros=duration_cast<microseconds>(now-secondsPoint).count();
            const auto time=system_clock::to_time_t(secondsPoint);std::tm local{};
#ifdef _WIN32
            if(localtime_s(&local,&time))throw std::runtime_error("Cannot format script start time");
#else
            if(!localtime_r(&time,&local))throw std::runtime_error("Cannot format script start time");
#endif
            std::ostringstream out;out<<std::put_time(&local,"%Y-%m-%d_%H%M%S")<<'_'<<std::setw(6)<<std::setfill('0')<<micros;
            stamp=out.str();
            if(logging){
                std::filesystem::create_directories(logs);const auto base=stamp;unsigned suffix=0;
                while(!std::filesystem::create_directory(logs/stamp))stamp=base+"_"+std::to_string(++suffix);
                directory=logs/stamp;
            }
            return true;
        }catch(const std::exception& e){error=e.what();directory.clear();return false;}
    }
};
}
