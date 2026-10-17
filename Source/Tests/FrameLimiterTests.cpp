#include "../Core/FrameLimiter.h"
#include "../Core/DisplayPreferences.h"
#include "../Core/MenuLayout.h"
#include <cassert>
#include <chrono>
#include <iostream>
#include <iterator>
using namespace OrbitX::Core;
int main(){
    unsigned value=120;
    for(auto text:{"","0","-1","+60","1.5"," 60","60 ","10001","999999999999999999","abc"}){
        assert(!ParseFrameLimit(text,value));assert(value==120);
    }
    assert(ParseFrameLimit("1",value)&&value==1);
    assert(ParseFrameLimit("10000",value)&&value==10000);
    assert(ParseFrameLimit("144",value)&&value==144);
    FrameLimit limit{true,144};assert(limit.Effective(false)==144&&limit.Effective(true)==0);
    // Bypass never mutates the preference, so every exit path restores it.
    for(int i=0;i<4;++i){assert(limit.Effective(true)==0);assert(limit.Effective(false)==144);}
    limit.enabled=false;assert(limit.Effective(false)==0&&limit.Effective(true)==0&&limit.fps==144);
    FramePacer pacer;assert(pacer.Delay(0,120)==0);pacer.Begin(0,120);
    assert(pacer.Delay(0,120)==8333334&&pacer.Delay(1000000,120)==7333334);
    assert(pacer.Delay(8333333,120)==1&&pacer.Delay(8333334,120)==0);
    assert(pacer.Delay(9000000,120)==0&&pacer.Delay(1000,0)==0);
    // Rate changes/bypass are immediately effective, then a fresh schedule begins.
    assert(pacer.Delay(1000,60)==0);pacer.Begin(1000,60);
    assert(pacer.Delay(1000,60)==16666667);
    assert(pacer.Delay(2000,0)==0);pacer.Begin(2000,0);
    assert(pacer.Delay(2001,60)==0);pacer.Begin(2001,60);
    assert(pacer.Delay(2001,60)==16666667);
    // A loading stall consumes the budget; no extra sleep or catch-up frames.
    assert(pacer.Delay(1000000000,60)==0);pacer.Begin(1000000000,60);
    assert(pacer.Delay(1000000000,60)==16666667);
    for(unsigned fps:{1u,60u,120u,144u,300u,10000u}){
        FramePacer p;int64_t t=0;p.Begin(t,fps);
        for(int i=0;i<1000;++i){
            const auto wait=p.Delay(t+1234,fps);t+=1234+wait;
            assert(p.Delay(t,fps)==0);p.Begin(t,fps);
        }
        assert(t>=1000000000000LL/fps);
    }
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
    // Startup-only optimization choice defaults off, round-trips and survives cap/display saves.
    std::ofstream(path)<<"DisplayMode=Windowed\nVSync=On\nOther=keep\n";
    prefs=LoadDisplayPreferences(path);assert(!prefs.planetOptimizations);
    assert(SaveDisplayPreferences(path,{false,true,true,120,true}));prefs=LoadDisplayPreferences(path);
    assert(prefs.planetOptimizations&&prefs.frameCapEnabled&&prefs.frameCap==120);
    prefs.fullscreen=true;prefs.vsync=false;prefs.frameCapEnabled=false;
    assert(SaveDisplayPreferences(path,prefs));prefs=LoadDisplayPreferences(path);assert(prefs.planetOptimizations&&!prefs.frameCapEnabled);
    std::ofstream(path)<<"PlanetOptimizations=invalid\n";assert(!LoadDisplayPreferences(path).planetOptimizations);
    std::ofstream(path)<<"PlanetOptimizations=On\nPlanetOptimizations=Off\n";
    assert(SaveDisplayPreferences(path,{true,false,false,120,true}));assert(LoadDisplayPreferences(path).planetOptimizations);
    std::filesystem::remove_all(dir);
    for(auto size:{std::pair{1920.f,1080.f},std::pair{3840.f,2160.f},std::pair{1280.f,720.f}}){
        auto l=OrbitX::UI::BuildSubmenuLayout(size.first,size.second);
        for(auto r:{l.graphicsCapped,l.graphicsUncapped,l.graphicsCapInput}){
            const float x=(r.left+r.right)*.5f*l.scale+l.offsetX,y=(r.top+r.bottom)*.5f*l.scale+l.offsetY;
            assert(OrbitX::UI::HitTestButton(r,0,x,y,l.scale,l.offsetX,l.offsetY));
        }
        assert(l.graphicsCapped.right<l.graphicsUncapped.left&&l.graphicsCapped.bottom<l.graphicsCapInput.top);
    }
    std::cout<<"Frame limiter/preferences tests passed\n";
}
