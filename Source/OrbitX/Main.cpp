#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <chrono>
#include <filesystem>
#include <algorithm>
#include <fstream>
#include <string>
#include <vector>
#include "../Graphics/Renderer.h"
#include "../Core/DisplayPreferences.h"
#include "FrameWaiter.h"
using OrbitX::Graphics::Renderer;
static Renderer* g_renderer=nullptr;
static bool g_resetFrameSchedule=true;
static bool g_windowDestroyed=false;
static constexpr double kAspect=16.0/9.0;

    
static void FrameExtentsForDpi(HWND h,UINT dpi,int& extraW,int& extraH){
    RECT r{0,0,1600,900};
    DWORD style=(DWORD)GetWindowLongPtr(h,GWL_STYLE), ex=(DWORD)GetWindowLongPtr(h,GWL_EXSTYLE);
    AdjustWindowRectExForDpi(&r,style,FALSE,ex,dpi);
    extraW=(r.right-r.left)-1600; extraH=(r.bottom-r.top)-900;
}
static void LockSizingTo169(HWND h,WPARAM edge,RECT* r){
    UINT dpi=GetDpiForWindow(h);int ew=0,eh=0;FrameExtentsForDpi(h,dpi,ew,eh);
    int cw=std::max(320,static_cast<int>(r->right-r->left)-ew), ch=std::max(180,static_cast<int>(r->bottom-r->top)-eh);
    const bool horizontal=edge==WMSZ_LEFT||edge==WMSZ_RIGHT;
    if(horizontal)ch=(int)lround(cw/kAspect); else cw=(int)lround(ch*kAspect);
    int ow=cw+ew,oh=ch+eh;
    switch(edge){
      case WMSZ_LEFT: case WMSZ_TOPLEFT: case WMSZ_BOTTOMLEFT:r->left=r->right-ow;break;
      default:r->right=r->left+ow;break;
    }
    switch(edge){
      case WMSZ_TOP: case WMSZ_TOPLEFT: case WMSZ_TOPRIGHT:r->top=r->bottom-oh;break;
      default:r->bottom=r->top+oh;break;
    }
}
LRESULT CALLBACK WndProc(HWND h,UINT m,WPARAM w,LPARAM l){switch(m){
case WM_GETMINMAXINFO:{
    auto* mm=reinterpret_cast<MINMAXINFO*>(l);
    HMONITOR mon=MonitorFromWindow(h,MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi{sizeof(mi)};GetMonitorInfo(mon,&mi);
    UINT dpi=GetDpiForWindow(h);int ew=0,eh=0;FrameExtentsForDpi(h,dpi,ew,eh);
    const int availW=static_cast<int>(mi.rcWork.right-mi.rcWork.left);
    const int availH=static_cast<int>(mi.rcWork.bottom-mi.rcWork.top);
    int cw=std::min(availW-ew,static_cast<int>(lround((availH-eh)*kAspect)));
    int ch=static_cast<int>(lround(cw/kAspect));
    if(ch+eh>availH){ch=availH-eh;cw=static_cast<int>(lround(ch*kAspect));}
    const int ow=cw+ew,oh=ch+eh;
    // A normal maximized window inherits the monitor work area's aspect ratio. OrbitX
    // instead uses the largest 16:9 client rectangle that fits the work area and centers it.
    mm->ptMaxSize={ow,oh};
    mm->ptMaxTrackSize={ow,oh};
    mm->ptMaxPosition.x=(mi.rcWork.left-mi.rcMonitor.left)+(availW-ow)/2;
    mm->ptMaxPosition.y=(mi.rcWork.top-mi.rcMonitor.top)+(availH-oh)/2;
    return 0;}
case WM_SIZING:LockSizingTo169(h,w,reinterpret_cast<RECT*>(l));return TRUE;
case WM_POWERBROADCAST:g_resetFrameSchedule=true;return TRUE;
case WM_ENTERSIZEMOVE:case WM_EXITSIZEMOVE:g_resetFrameSchedule=true;return 0;
case WM_SIZE:
    g_resetFrameSchedule=true;
    if(g_renderer&&w!=SIZE_MINIMIZED){
        // WM_SIZE is delivered continuously inside Windows' modal sizing loop. Resize
        // and render immediately here so DWM never has to stretch a stale back buffer.
        g_renderer->Resize(LOWORD(l),HIWORD(l));
        g_renderer->Update(0.0f);
        g_renderer->Render();
    }
    return 0;
case WM_KEYDOWN:if(g_renderer && (!(l & (1LL<<30)) || !(GetKeyState(VK_CONTROL)&0x8000)))g_renderer->OnKeyDown(w);return 0;
case WM_CHAR:if(g_renderer)g_renderer->OnCharacter(static_cast<wchar_t>(w));return 0;
case WM_LBUTTONDOWN:if(g_renderer)g_renderer->OnLeftClick(GET_X_LPARAM(l),GET_Y_LPARAM(l));return 0;
case WM_RBUTTONDOWN:if(g_renderer)g_renderer->OnMouseDown(GET_X_LPARAM(l),GET_Y_LPARAM(l));return 0;
case WM_RBUTTONUP:if(g_renderer)g_renderer->OnMouseUp();return 0;
case WM_MOUSEMOVE:{TRACKMOUSEEVENT tme{sizeof(tme),TME_LEAVE,h,0};TrackMouseEvent(&tme);if(g_renderer)g_renderer->OnMouseMove(GET_X_LPARAM(l),GET_Y_LPARAM(l));return 0;}
case WM_MOUSELEAVE:if(g_renderer)g_renderer->OnMouseLeave();return 0;
case WM_MOUSEWHEEL:if(g_renderer)g_renderer->OnMouseWheel(GET_WHEEL_DELTA_WPARAM(w));return 0;
case WM_KILLFOCUS:if(g_renderer)g_renderer->OnMouseUp();return 0;
case WM_CLOSE:DestroyWindow(h);return 0;case WM_DESTROY:g_windowDestroyed=true;PostQuitMessage(0);return 0;}return DefWindowProc(h,m,w,l);}
int WINAPI wWinMain(HINSTANCE hi,HINSTANCE,LPWSTR,int){
SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
CoInitializeEx(nullptr,COINIT_MULTITHREADED);std::wstring runtime=std::filesystem::current_path().wstring();std::filesystem::create_directories(runtime+L"\\Logs");const auto userPrefsPath=std::filesystem::path(runtime)/L"Config"/L"UserPreferences.cfg";const auto preferences=OrbitX::Core::LoadDisplayPreferences(userPrefsPath);WNDCLASSEX wc{sizeof(wc),CS_HREDRAW|CS_VREDRAW,WndProc,0,0,hi,LoadIcon(nullptr,IDI_APPLICATION),LoadCursor(nullptr,IDC_ARROW),(HBRUSH)GetStockObject(BLACK_BRUSH),nullptr,L"OrbitXWindow",nullptr};RegisterClassEx(&wc);RECT r{0,0,1920,1080};AdjustWindowRectExForDpi(&r,WS_OVERLAPPEDWINDOW,FALSE,0,GetDpiForSystem());HWND h=CreateWindowEx(0,wc.lpszClassName,L"OrbitX 0.25",WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,r.right-r.left,r.bottom-r.top,nullptr,nullptr,hi,nullptr);if(!h){CoUninitialize();return 1;}ShowWindow(h,SW_SHOW);UpdateWindow(h);RECT client{};GetClientRect(h,&client);UINT clientW=(UINT)(client.right-client.left),clientH=(UINT)(client.bottom-client.top);
Renderer renderer;g_renderer=&renderer;if(!renderer.Initialize(h,clientW,clientH,runtime)){MessageBoxA(h,renderer.LastError().c_str(),"OrbitX initialization failed",MB_OK|MB_ICONERROR);DestroyWindow(h);g_renderer=nullptr;CoUninitialize();return 2;}
// Display preference comes from auto-created, untracked UserPreferences.cfg.
renderer.SetFrameLimit(preferences.frameCapEnabled,preferences.frameCap);
renderer.SetFullscreen(preferences.fullscreen);renderer.SetVSync(preferences.vsync);
// Fixed deadlines recover small wake-up delays without spinning. One complete
// interval of lateness resets the timetable instead of repaying missed frames.
OrbitX::FrameWaiter frameWaiter;
OrbitX::Core::FramePacer framePacer;
auto last=std::chrono::steady_clock::now();MSG msg{};
bool quit=false;
while(!quit){
    // Bound each input batch: a continuous stream of mouse/window messages must
    // not starve rendering. Dispatch before checking the deadline, so input can
    // change the cap or start/stop a benchmark while a long wait is in progress.
    for(unsigned count=0;count<64&&PeekMessage(&msg,nullptr,0,0,PM_REMOVE);++count){
        if(msg.message==WM_QUIT){quit=true;break;}
        TranslateMessage(&msg);DispatchMessage(&msg);
        if(g_windowDestroyed){quit=true;break;}
    }
    if(quit||g_windowDestroyed)break;
    if(g_resetFrameSchedule){framePacer.Reset();g_resetFrameSchedule=false;}
    if(IsIconic(h)){
        framePacer.Reset();frameWaiter.Wait(100000000);continue;
    }
    const auto now=std::chrono::steady_clock::now();
    const auto nowNs=std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();
    const unsigned fps=renderer.EffectiveFrameLimit();
    const auto delayNs=framePacer.Delay(nowNs,fps);
    if(delayNs>0){frameWaiter.Wait(delayNs);continue;}
    framePacer.Begin(nowNs,fps);
    const float dt=std::chrono::duration<float>(now-last).count();last=now;
    renderer.Update(dt);renderer.Render();
}
renderer.SaveUserPreferences();renderer.Shutdown();g_renderer=nullptr;CoUninitialize();return 0;
}
