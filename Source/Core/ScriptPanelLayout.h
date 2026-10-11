#pragma once
#include <algorithm>
namespace OrbitX::Core {
struct ScriptPanelRect {
    float left,top,right,bottom;
    bool Contains(float x,float y)const{return x>=left&&x<=right&&y>=top&&y<=bottom;}
};
struct ScriptPanelLayout {
    float scale;
    ScriptPanelRect panel,script,benchmark,regression,livestream,action;
    static ScriptPanelLayout Build(float width,float height){
        const float s=std::min({1.f,width/760.f,height/530.f});
        const float x=(width-680*s)*.5f,y=(height-450*s)*.5f;
        auto r=[&](float l,float t,float w,float h){return ScriptPanelRect{x+l*s,y+t*s,x+(l+w)*s,y+(t+h)*s};};
        return {s,r(0,0,680,450),r(28,102,624,64),r(28,221,188,48),r(232,221,188,48),r(436,221,216,48),r(28,353,624,65)};
    }
};
}
