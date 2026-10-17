#include "../Core/FrameLimiter.h"
#include "../Core/DisplayPreferences.h"
#include "../Core/MenuLayout.h"
#include "../Core/EngineConfig.h"
#include <cassert>
#include <chrono>
#include <iostream>
#include <iterator>
#include <cmath>
using namespace OrbitX::Core;
int main(){
    unsigned value=120;
    for(auto text:{"","0","-1","+60","1.5"," 60","60 ","10001","999999999999999999","abc"}){
        assert(!ParseFrameLimit(text,value));assert(value==120);
    }
    assert(ParseFrameLimit("1",value)&&value==1);
    assert(ParseFrameLimit("10000",value)&&value==10000);
    assert(ParseFrameLimit("144",value)&&value==144);
    unsigned wide=120;
    for(auto text:{L"",L"0",L"-1",L"+60",L"1.5",L" 60",L"60 ",L"10001",L"999999999999999999",L"\u0661",L"\uFF11"}){
        assert(!ParseFrameLimit(std::wstring_view(text),wide));assert(wide==120);
    }
    assert(ParseFrameLimit(L"1",wide)&&wide==1);
    assert(ParseFrameLimit(L"10000",wide)&&wide==10000);
    assert(ParseFrameLimit(L"00144",wide)&&wide==144);
    FrameLimit limit{true,144};assert(limit.Effective(false)==144&&limit.Effective(true)==0);
    // Bypass never mutates the preference, so every exit path restores it.
    for(int i=0;i<4;++i){assert(limit.Effective(true)==0);assert(limit.Effective(false)==144);}
    limit.enabled=false;assert(limit.Effective(false)==0&&limit.Effective(true)==0&&limit.fps==144);
    FramePacer pacer;assert(pacer.Delay(0,100)==0);pacer.Begin(0,100);
    assert(pacer.Delay(0,100)==10000000&&pacer.Delay(9999999,100)==1);
    assert(pacer.Delay(10000000,100)==0);pacer.Begin(10000000,100);
    // The .030 deadline wakes at .033; the next deadline remains .040.
    pacer.Begin(20000000,100);assert(pacer.Delay(33000000,100)==0);pacer.Begin(33000000,100);
    assert(pacer.Delay(33000000,100)==7000000);
    // A full interval overdue discards debt; .053 -> .063, never .060.
    assert(pacer.Delay(53000000,100)==0);pacer.Begin(53000000,100);
    assert(pacer.Delay(53000000,100)==10000000);
    // Just below the threshold keeps phase; exactly at it resets.
    pacer.Reset();pacer.Begin(0,100);pacer.Begin(19999999,100);
    assert(pacer.Delay(19999999,100)==1);
    pacer.Reset();pacer.Begin(0,100);pacer.Begin(20000000,100);
    assert(pacer.Delay(20000000,100)==10000000);
    // Input/early timer wakes recheck the same deadline without consuming it.
    for(int64_t t=20000000;t<30000000;t+=1000)assert(pacer.Delay(t,100)==30000000-t);
    assert(pacer.Delay(20000000,60)==0);pacer.Begin(20000000,60);
    assert(pacer.Delay(20000000,60)==16666667);
    assert(pacer.Delay(20000001,0)==0&&pacer.Delay(20000002,60)==0);
    pacer.Begin(20000002,60);pacer.Reset();assert(pacer.Delay(20000003,60)==0);
    // Defensive arithmetic and clock discontinuities never wrap into a long wait.
    pacer.Begin(20000003,60);assert(pacer.Delay(1,60)==0);
    pacer.Begin(1,60);assert(pacer.Delay(-1,60)==0);
    pacer.Begin(std::numeric_limits<int64_t>::max(),1);
    assert(pacer.Delay(std::numeric_limits<int64_t>::max(),1)==0);
    for(unsigned invalid:{0u,10001u,std::numeric_limits<unsigned>::max()}){
        pacer.Begin(10,invalid);assert(pacer.Delay(11,invalid)==0);
    }
    // Exercise actual C++ pacing over 1.5 million frames, across every capacity
    // region, rounded periods, ordinary/large oversleeps and a loading stall.
    unsigned steps=0;
    for(unsigned fps:{1u,59u,60u,143u,144u,299u,300u,599u,600u,1499u,1500u,10000u}){
        const int64_t period=(1000000000LL+fps-1)/fps;
        for(double capacity:{.25,.99,1.,1.01,10.})for(double late:{0.,.01,.1,.9,1.1}){
            FramePacer p;int64_t t=0;
            const int64_t work=std::max<int64_t>(1,static_cast<int64_t>(std::llround(static_cast<double>(period)/capacity)));
            for(unsigned i=0;i<5000;++i){
                const auto wait=p.Delay(t,fps);
                if(i&&work>=period)assert(wait==0);
                t+=wait;
                if(wait)t+=static_cast<int64_t>(std::llround(static_cast<double>(period)*late));
                assert(p.Delay(t,fps)==0);p.Begin(t,fps);
                const auto next=p.Delay(t,fps);assert(next>0&&next<=period);
                t+=work;if(i==2500)t+=1000000000LL;
                ++steps;
            }
        }
    }
    assert(steps==1500000);
    auto measured=[&](unsigned fps,int64_t work,int64_t oversleep){
        FramePacer p;int64_t t=0,last=0;constexpr unsigned count=30000;
        for(unsigned i=0;i<count;++i){
            const auto wait=p.Delay(t,fps);t+=wait+(wait?oversleep:0);
            p.Begin(t,fps);last=t;t+=work;
        }
        return static_cast<double>(count-1)*1e9/static_cast<double>(last);
    };
    // The previously observed ~286 us wake bias must not become permanent drift.
    assert(std::abs(measured(1500,357143,286000)-1500)<1.5);
    assert(std::abs(measured(300,333333,286000)-300)<.3);
    assert(std::abs(measured(1000,5714286,286000)-175)<.01);
    // Becoming capable again must resume pacing with no below-target latch.
    FramePacer transition;int64_t tick=0;
    for(unsigned i=0;i<1000;++i){assert(transition.Delay(tick,1000)==0);transition.Begin(tick,1000);tick+=5714286;}
    transition.Begin(tick,1000);tick+=100000;
    assert(transition.Delay(tick,1000)==900000);
    const auto dir=std::filesystem::temp_directory_path()/("orbitx-cap-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    const auto path=dir/"Config"/"UserPreferences.cfg";
    auto prefs=LoadDisplayPreferences(path);assert(prefs.fullscreen&&!prefs.vsync&&!prefs.frameCapEnabled&&prefs.frameCap==120);
    std::ofstream(path)<<"DisplayMode=Windowed\r\nVSync=On\r\nOther=keep\r\n";
    prefs=LoadDisplayPreferences(path);assert(!prefs.fullscreen&&prefs.vsync&&!prefs.frameCapEnabled&&prefs.frameCap==120);
    assert(SaveDisplayPreferences(path,{false,true,true,144}));prefs=LoadDisplayPreferences(path);
    assert(!prefs.fullscreen&&prefs.vsync&&prefs.frameCapEnabled&&prefs.frameCap==144);
    assert(SaveDisplayPreferences(path,{true,false,false,144}));prefs=LoadDisplayPreferences(path);
    assert(prefs.fullscreen&&!prefs.vsync&&!prefs.frameCapEnabled&&prefs.frameCap==144);
    std::ifstream input(path);std::string content((std::istreambuf_iterator<char>(input)),{});input.close();
    assert(content.find("Other=keep")!=std::string::npos);
    std::ofstream(path)<<"FrameRateLimit=Capped\nMaxFPS=-1\nMaxFPS=10001\nMaxFPS=\n";
    prefs=LoadDisplayPreferences(path);assert(prefs.frameCapEnabled&&prefs.frameCap==120);
    assert(SaveDisplayPreferences(path,{false,false,true,999999}));prefs=LoadDisplayPreferences(path);assert(prefs.frameCap==120);
    std::ofstream(path)<<"FrameRateLimit=Capped\nFrameRateLimit=Uncapped\nMaxFPS=60\nMaxFPS=144\n";
    assert(SaveDisplayPreferences(path,{true,true,true,240}));prefs=LoadDisplayPreferences(path);assert(prefs.frameCapEnabled&&prefs.frameCap==240);
    // Engine configuration is independent of user display preferences and never rewritten.
    const auto engine=dir/"Config"/"OrbitX.cfg";
    assert(!LoadPlanetOptimizations(engine));assert(!std::filesystem::exists(engine));
    std::ofstream(engine)<<"# PlanetOptimizations=On\nTextureRoot=C:\\Textures\n";
    assert(!LoadPlanetOptimizations(engine));
    std::ofstream(engine)<<"; comment\r\n PlanetOptimizations = On  ; experiment\r\n";
    assert(LoadPlanetOptimizations(engine));
    std::ofstream(path)<<"PlanetOptimizations=Off\nOther=keep\nVSync=Off\n";
    prefs=LoadDisplayPreferences(path);prefs.vsync=true;assert(SaveDisplayPreferences(path,prefs));
    std::ifstream prefInput(path);std::string savedPrefs((std::istreambuf_iterator<char>(prefInput)),{});prefInput.close();
    assert(savedPrefs.find("PlanetOptimizations")==std::string::npos&&savedPrefs.find("Other=keep")!=std::string::npos);
    assert(LoadPlanetOptimizations(engine));
    std::ifstream engineInput(engine);std::string engineText((std::istreambuf_iterator<char>(engineInput)),{});engineInput.close();
    assert(engineText=="; comment\r\n PlanetOptimizations = On  ; experiment\r\n");
    std::ofstream(engine)<<"PlanetOptimizations=On\nPlanetOptimizations=Off\n";assert(!LoadPlanetOptimizations(engine));
    std::ofstream(engine)<<"PlanetOptimizations=invalid\n";assert(!LoadPlanetOptimizations(engine));
    std::filesystem::remove_all(dir);
    for(auto size:{std::pair{1920.f,1080.f},std::pair{3840.f,2160.f},std::pair{1280.f,720.f}}){
        auto l=OrbitX::UI::BuildSubmenuLayout(size.first,size.second);
        for(auto r:{l.graphicsWindowed,l.graphicsFullscreen,l.graphicsVsyncOff,l.graphicsVsyncOn,l.graphicsCapped,l.graphicsUncapped,l.graphicsCapInput}){
            const float x=(r.left+r.right)*.5f*l.scale+l.offsetX,y=(r.top+r.bottom)*.5f*l.scale+l.offsetY;
            assert(OrbitX::UI::HitTestButton(r,0,x,y,l.scale,l.offsetX,l.offsetY));
        }
        const auto main=OrbitX::UI::BuildMainMenuLayout(size.first,size.second);
        assert(l.back.left==main.buttons[0].left&&l.back.top==main.buttons[0].top);
        assert(l.title.left==72&&l.title.top==372&&l.title.bottom==430);
        assert(l.graphicsDisplayPanel.top>l.title.bottom);
        assert(l.graphicsDisplayPanel.bottom<l.graphicsVsyncPanel.top&&l.graphicsVsyncPanel.bottom<l.graphicsLimitPanel.top);
        assert(l.graphicsLimitPanel.bottom<=1080&&l.graphicsLimitPanel.right<1920*.4f);
        assert(l.graphicsWindowed.left==l.graphicsVsyncOff.left&&l.graphicsWindowed.left==l.graphicsCapped.left);
        assert(l.graphicsFullscreen.left==l.graphicsVsyncOn.left&&l.graphicsFullscreen.left==l.graphicsUncapped.left);
        assert(l.graphicsCapped.right<l.graphicsUncapped.left&&l.graphicsCapped.bottom<l.graphicsCapInput.top);
    }
    std::cout<<"Frame limiter/preferences tests passed\n";
}
