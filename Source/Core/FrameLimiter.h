#pragma once
#include <charconv>
#include <cstdint>
#include <string_view>

namespace OrbitX::Core {
struct FrameLimit {
    static constexpr unsigned MinFPS=1,MaxFPS=10000,DefaultFPS=120;
    bool enabled=false;
    unsigned fps=DefaultFPS;
    unsigned Effective(bool benchmark)const {return enabled&&!benchmark?fps:0;}
};
inline bool ParseFrameLimit(std::string_view text,unsigned& value){
    if(text.empty())return false;
    unsigned parsed=0;
    const auto [end,error]=std::from_chars(text.data(),text.data()+text.size(),parsed);
    if(error!=std::errc{}||end!=text.data()+text.size()||parsed<FrameLimit::MinFPS||parsed>FrameLimit::MaxFPS)return false;
    value=parsed;return true;
}
// The edit control stores wide digits: validate them directly, with no narrowing.
inline bool ParseFrameLimit(std::wstring_view text,unsigned& value){
    if(text.empty())return false;
    unsigned parsed=0;
    for(const wchar_t c:text){
        if(c<L'0'||c>L'9')return false;
        const unsigned digit=static_cast<unsigned>(c-L'0');
        if(parsed>(FrameLimit::MaxFPS-digit)/10)return false;
        parsed=parsed*10+digit;
    }
    if(parsed<FrameLimit::MinFPS)return false;
    value=parsed;return true;
}
// Frame starts are spaced without spinning or catching up after a slow frame.
// A mode/rate change starts a fresh schedule, including entry/exit from benchmarks.
class FramePacer {
public:
    int64_t Delay(int64_t nowNs,unsigned fps)const {
        if(!fps||!m_started||fps!=m_fps)return 0;
        const int64_t remaining=m_startNs+(1000000000LL+fps-1)/fps-nowNs;
        return remaining>0?remaining:0;
    }
    void Begin(int64_t nowNs,unsigned fps){m_startNs=nowNs;m_fps=fps;m_started=true;}
private:
    int64_t m_startNs=0;
    unsigned m_fps=0;
    bool m_started=false;
};
}
