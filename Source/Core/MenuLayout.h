#pragma once
#include <algorithm>
#include <array>

namespace OrbitX::UI {

constexpr float kDesignWidth  = 1920.0f;
constexpr float kDesignHeight = 1080.0f;
constexpr int   kMainMenuCount = 9;

struct RectF {
    float left{}, top{}, right{}, bottom{};
    bool Contains(float x, float y) const {
        return x >= left && x <= right && y >= top && y <= bottom;
    }
    float Width() const { return right-left; }
    float Height() const { return bottom-top; }
};

struct MainMenuLayout {
    float scale{};
    float offsetX{};
    float offsetY{};
    float panelRight=690.0f;
    float logoOrbitX=92.0f, logoOrbitY=58.0f;
    float logoXX=512.0f, logoXY=48.0f;
    float textX=116.0f;
    float cut=18.0f;
    std::array<RectF,kMainMenuCount> buttons{};
    RectF tagline{96,900,330,1000};
    RectF version{60,1020,300,1060};
};

struct SubmenuLayout {
    float scale{};
    float offsetX{};
    float offsetY{};
    float panelRight=690.0f;
    // Submenus deliberately begin below the OrbitX wordmark.  Keep this geometry
    // canonical: rendering and hit-testing consume these exact rectangles.
    RectF back{};
    RectF title{72,347,620,392};
    RectF moonView{72,432,560,507};
    RectF graphicsWindowed{72,477,286,542};
    RectF graphicsFullscreen{304,477,550,542};
};

inline float ScaleFor(float width,float height){return std::min(width/kDesignWidth,height/kDesignHeight);}
inline float OffsetXFor(float width,float scale){return (width-kDesignWidth*scale)*0.5f;}
inline float OffsetYFor(float height,float scale){return (height-kDesignHeight*scale)*0.5f;}

inline MainMenuLayout BuildMainMenuLayout(float width,float height){
    MainMenuLayout out{};
    out.scale=ScaleFor(width,height); out.offsetX=OffsetXFor(width,out.scale); out.offsetY=OffsetYFor(height,out.scale);
    constexpr float left=52.0f,right=624.0f,first=250.0f,buttonH=62.0f,pitch=69.0f;
    for(int i=0;i<kMainMenuCount;i++){float top=first+i*pitch;out.buttons[i]={left,top,right,top+buttonH};}
    return out;
}
inline SubmenuLayout BuildSubmenuLayout(float width,float height){
    SubmenuLayout out{}; out.scale=ScaleFor(width,height);out.offsetX=OffsetXFor(width,out.scale);out.offsetY=OffsetYFor(height,out.scale);
    // Keep every submenu BACK control identical in position and size to SCENARIOS.
    out.back=BuildMainMenuLayout(width,height).buttons[0];
    return out;
}
inline void ScreenToDesign(float sx,float sy,float scale,float ox,float oy,float& x,float& y){x=(sx-ox)/scale;y=(sy-oy)/scale;}
inline bool PointInButton(const RectF& r,float cut,float x,float y){
    if(!r.Contains(x,y))return false;
    const float cornerStart=r.right-cut;
    if(x<=cornerStart)return true;
    const float dx=x-cornerStart;
    return y>=r.top+dx && y<=r.bottom-dx;
}
inline int HitTestMainMenu(const MainMenuLayout& l,float sx,float sy){
    float x,y;ScreenToDesign(sx,sy,l.scale,l.offsetX,l.offsetY,x,y);
    for(int i=0;i<kMainMenuCount;i++)if(PointInButton(l.buttons[i],l.cut,x,y))return i;
    return -1;
}
inline bool HitTestButton(const RectF& r,float cut,float sx,float sy,float scale,float ox,float oy){float x,y;ScreenToDesign(sx,sy,scale,ox,oy,x,y);return PointInButton(r,cut,x,y);}

} // namespace OrbitX::UI
