#pragma once
#include <algorithm>
#include <array>

namespace OrbitX::UI {

constexpr float kDesignWidth  = 1920.0f;
constexpr float kDesignHeight = 1080.0f;
constexpr int   kMainMenuCount = 8;
// Graphics cards scale about their top-left corner, leaving navigation unchanged.
constexpr float kGraphicsControlScale=2.0f/3.0f;
constexpr float kGraphicsControlLeft=52.0f,kGraphicsControlTop=452.0f;

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
    RectF title{72,372,700,430};
    // Scenario browser: design-space rectangles shared by rendering and hit-testing.
    RectF solarSystem{52,445,800,519};
    RectF moonView{100,540,780,594};
    RectF earthView{100,605,780,659}; // Future scenario: visible but disabled.
    RectF marsView{100,670,780,724};  // Future scenario: visible but disabled.
    RectF description{860,338,1392,950};
    RectF preview{892,422,1360,592};
    RectF launch{1440,865,1800,948};
    // One left-hand rail; shared geometry keeps drawing and hit-testing aligned.
    RectF graphicsDisplayPanel{52,452,756,595};
    RectF graphicsVsyncPanel{52,613,756,756};
    RectF graphicsLimitPanel{52,774,756,995};
    RectF graphicsWindowed{82,510,390,574};
    RectF graphicsFullscreen{418,510,726,574};
    RectF graphicsVsyncOff{82,671,390,735};
    RectF graphicsVsyncOn{418,671,726,735};
    RectF graphicsCapped{82,832,390,896};
    RectF graphicsUncapped{418,832,726,896};
    RectF graphicsCapInput{82,914,302,974};
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
    for(RectF* r:{&out.graphicsDisplayPanel,&out.graphicsVsyncPanel,&out.graphicsLimitPanel,
        &out.graphicsWindowed,&out.graphicsFullscreen,&out.graphicsVsyncOff,&out.graphicsVsyncOn,
        &out.graphicsCapped,&out.graphicsUncapped,&out.graphicsCapInput}){
        r->left=kGraphicsControlLeft+(r->left-kGraphicsControlLeft)*kGraphicsControlScale;
        r->right=kGraphicsControlLeft+(r->right-kGraphicsControlLeft)*kGraphicsControlScale;
        r->top=kGraphicsControlTop+(r->top-kGraphicsControlTop)*kGraphicsControlScale;
        r->bottom=kGraphicsControlTop+(r->bottom-kGraphicsControlTop)*kGraphicsControlScale;
    }
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
