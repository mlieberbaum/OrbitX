#pragma once
#include <windows.h>
#include <algorithm>
#include <cstdint>

namespace OrbitX {
// A single UI-thread-owned, auto-reset, one-shot timer. Never spins or waits
// indefinitely. The caller rechecks its monotonic deadline after every wake.
class FrameWaiter {
public:
    FrameWaiter(){
        m_timer=CreateWaitableTimerExW(nullptr,nullptr,CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,TIMER_MODIFY_STATE|SYNCHRONIZE);
        if(!m_timer){
            ReportFallback();
            m_timer=CreateWaitableTimerW(nullptr,FALSE,nullptr);
        }
    }
    ~FrameWaiter(){if(m_timer)CloseHandle(m_timer);}
    FrameWaiter(const FrameWaiter&)=delete;
    FrameWaiter& operator=(const FrameWaiter&)=delete;
    void Wait(int64_t delayNs){
        // Current limits are 1..10000 FPS. Bound conversion even on bad input;
        // one second is also the maximum time before the main loop rechecks.
        delayNs=std::clamp<int64_t>(delayNs,1,1000000000LL);
        const DWORD timeout=static_cast<DWORD>((delayNs+999999)/1000000);
        if(m_timer){
            LARGE_INTEGER due{};due.QuadPart=-((delayNs+99)/100);
            if(SetWaitableTimer(m_timer,&due,0,nullptr,nullptr,FALSE)){
                const DWORD result=MsgWaitForMultipleObjectsEx(1,&m_timer,timeout,QS_ALLINPUT,MWMO_INPUTAVAILABLE);
                if(result==WAIT_OBJECT_0||result==WAIT_OBJECT_0+1||result==WAIT_TIMEOUT)return;
            }
            // Do not repeatedly arm or wait on a failed handle.
            CloseHandle(m_timer);m_timer=nullptr;ReportFallback();
        }
        const DWORD result=MsgWaitForMultipleObjectsEx(0,nullptr,timeout,QS_ALLINPUT,MWMO_INPUTAVAILABLE);
        if(result!=WAIT_OBJECT_0&&result!=WAIT_TIMEOUT){
            ReportFallback();
            // Even failure of the message-wait API must not become a hot retry
            // loop. Keep this last-resort sleep short so close/input stays usable.
            Sleep(std::min<DWORD>(timeout,10));
        }
    }
private:
    void ReportFallback(){
        if(!m_reported){OutputDebugStringW(L"OrbitX: frame timer unavailable or failed; using blocking fallback.\n");m_reported=true;}
    }
    HANDLE m_timer=nullptr;
    bool m_reported=false;
};
}
