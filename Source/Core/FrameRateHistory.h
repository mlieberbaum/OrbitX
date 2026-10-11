#pragma once
#include <array>
#include <cmath>
#include <cstddef>
namespace OrbitX::Core {
// Fixed storage; sample actual frame throughput at most 10 times/second.
class FrameRateHistory {
public:
    struct Sample {double time=0,fps=0;};
    static constexpr size_t Capacity=120;
    static constexpr double WindowSeconds=12;
    void Clear(){head=count=frames=0;time=accumulated=0;}
    void AddFrame(double seconds){
        if(!std::isfinite(seconds)||seconds<=0)return;
        time+=seconds;accumulated+=seconds;++frames;
        while(count && time-At(0).time>WindowSeconds)--count;
        if(accumulated<.1)return;
        samples[head]={time,double(frames)/accumulated};head=(head+1)%Capacity;
        if(count<Capacity)++count;
        accumulated=0;frames=0;
    }
    size_t Count()const{return count;}
    const Sample& At(size_t i)const{return samples[(head+Capacity-count+i)%Capacity];}
    double Time()const{return time;}
private:
    std::array<Sample,Capacity> samples{};
    size_t head=0,count=0,frames=0;
    double time=0,accumulated=0;
};
}
