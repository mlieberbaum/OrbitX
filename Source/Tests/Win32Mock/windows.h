// Only used by FrameWaiterTests.cpp via an explicit test-only include path.
#pragma once
#include <cstdint>
#include <vector>
#include <algorithm>
#include <cassert>
using DWORD=uint32_t;using BOOL=int;using HANDLE=void*;
struct LARGE_INTEGER{int64_t QuadPart;};
constexpr DWORD CREATE_WAITABLE_TIMER_HIGH_RESOLUTION=2,TIMER_MODIFY_STATE=2,SYNCHRONIZE=0x100000,QS_ALLINPUT=0x4ff,MWMO_INPUTAVAILABLE=4;
constexpr BOOL FALSE=0;
constexpr DWORD WAIT_OBJECT_0=0,WAIT_TIMEOUT=258,WAIT_FAILED=0xffffffff;
namespace Mock {
inline bool high=true,normal=true,arm=true;
inline DWORD result=WAIT_TIMEOUT,fallbackResult=WAIT_TIMEOUT;
inline int creates=0,normalCreates=0,arms=0,closes=0,reports=0,sleeps=0,waits=0;
inline DWORD lastTimeout=0,lastSleep=0,lastCount=0;
inline int64_t due=0;
inline void Reset(){high=normal=arm=true;result=fallbackResult=WAIT_TIMEOUT;creates=normalCreates=arms=closes=reports=sleeps=waits=0;lastTimeout=lastSleep=lastCount=0;due=0;}
}
inline HANDLE CreateWaitableTimerExW(void*,void*,DWORD flags,DWORD access){assert(flags==CREATE_WAITABLE_TIMER_HIGH_RESOLUTION&&access==(TIMER_MODIFY_STATE|SYNCHRONIZE));++Mock::creates;return Mock::high?reinterpret_cast<HANDLE>(1):nullptr;}
inline HANDLE CreateWaitableTimerW(void*,BOOL manual,void*){assert(!manual);++Mock::normalCreates;return Mock::normal?reinterpret_cast<HANDLE>(2):nullptr;}
inline BOOL CloseHandle(HANDLE){++Mock::closes;return 1;}
inline BOOL SetWaitableTimer(HANDLE,const LARGE_INTEGER* due,int period,void*,void*,BOOL resume){assert(!period&&!resume);++Mock::arms;Mock::due=due->QuadPart;return Mock::arm;}
inline DWORD MsgWaitForMultipleObjectsEx(DWORD count,const HANDLE*,DWORD timeout,DWORD mask,DWORD flags){assert(timeout>0&&timeout<=1000&&mask==QS_ALLINPUT&&flags==MWMO_INPUTAVAILABLE);++Mock::waits;Mock::lastTimeout=timeout;Mock::lastCount=count;return count?Mock::result:Mock::fallbackResult;}
inline void Sleep(DWORD ms){assert(ms>0&&ms<=10);++Mock::sleeps;Mock::lastSleep=ms;}
inline void OutputDebugStringW(const wchar_t*){++Mock::reports;}
