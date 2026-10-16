#pragma once
#include <algorithm>
namespace OrbitX::Core {
struct ScriptPanelRect {
    float left,top,right,bottom;
    bool Contains(float x,float y)const{return x>=left&&x<=right&&y>=top&&y<=bottom;}
};
struct ScriptPanelLayout {
    float scale;
    ScriptPanelRect panel,script,benchmark,regression,livestream,action,recordTab,runTab,snapshot,none;
    static ScriptPanelLayout Build(float width,float height,bool recording=false){
        const float s=std::min({1.f,width/800.f,height/700.f});
        const float x=(width-760*s)*.5f,y=(height-650*s)*.5f;
        auto r=[&](float l,float t,float w,float h){return ScriptPanelRect{x+l*s,y+t*s,x+(l+w)*s,y+(t+h)*s};};
        return {s,r(0,0,760,650),r(28,148,704,160),r(28,352,recording?164.f:216.f,48),r(recording?388.f:492.f,352,recording?164.f:240.f,48),r(recording?208.f:260.f,352,recording?164.f:216.f,48),
            r(28,558,704,64),r(28,80,340,44),r(392,80,340,44),r(28,450,704,48),r(568,352,164,48)};
    }
};
}

