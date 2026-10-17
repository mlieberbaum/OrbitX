#pragma once
#include <charconv>
#include <cstdint>
#include <limits>
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
// Keep intended deadlines, rather than adding a full interval to each late wake.
// Small lateness is recovered by shorter later sleeps. A full interval of debt
// is discarded, so loading stalls never create an unbounded catch-up backlog.
class FramePacer {
public:
    void Reset(){m_started=false;m_fps=0;m_lastNs=0;m_nextNs=0;}
    int64_t Delay(int64_t nowNs,unsigned fps){
        if(fps<FrameLimit::MinFPS||fps>FrameLimit::MaxFPS||nowNs<0){Reset();return 0;}
        if(fps!=m_fps||nowNs<m_lastNs)m_started=false;
        m_fps=fps;m_lastNs=nowNs;
        return m_started&&nowNs<m_nextNs?m_nextNs-nowNs:0;
    }
    // Call exactly once when admitting a frame, after Delay returned zero.
    void Begin(int64_t nowNs,unsigned fps){
        Delay(nowNs,fps);
        if(!m_fps)return;
        const int64_t interval=(1000000000LL+fps-1)/fps;
        const int64_t origin=m_started&&nowNs>=m_nextNs&&nowNs-m_nextNs<interval?m_nextNs:nowNs;
        if(origin>std::numeric_limits<int64_t>::max()-interval){Reset();return;}
        m_nextNs=origin+interval;m_started=true;
    }
private:
    int64_t m_nextNs=0,m_lastNs=0;
    unsigned m_fps=0;
    bool m_started=false;
};
}
