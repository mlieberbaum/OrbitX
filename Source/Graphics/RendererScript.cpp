#include "Renderer.h"
#include "../Core/ScriptPanelLayout.h"
#include <iomanip>
#include <sstream>
#include <cmath>
#include <cwchar>
namespace OrbitX::Graphics {
void Renderer::DrawScriptPanel(){
    if(!m_scriptGreen){
        if(FAILED(m_d2dContext->CreateSolidColorBrush(D2D1::ColorF(.15f,1.f,.5f),&m_scriptGreen)) ||
           FAILED(m_d2dContext->CreateSolidColorBrush(D2D1::ColorF(.01f,.045f,.03f,.82f),&m_scriptGlass)) ||
           FAILED(m_d2dContext->CreateSolidColorBrush(D2D1::ColorF(.9f,.98f,.94f),&m_scriptWhite)) ||
           FAILED(m_d2dContext->CreateSolidColorBrush(D2D1::ColorF(.4f,.45f,.43f),&m_scriptDisabled))){
            m_scriptGreen.Reset();return;
        }
    }
    const auto l=Core::ScriptPanelLayout::Build(float(m_width),float(m_height));
    auto rect=[](Core::ScriptPanelRect r){return D2D1::RectF(r.left,r.top,r.right,r.bottom);};
    auto text=[&](const wchar_t* value,float x,float y,ID2D1Brush* brush){
        m_d2dContext->DrawTextW(value,static_cast<UINT32>(wcslen(value)),m_textLeft.Get(),
            D2D1::RectF(x,y,l.panel.right-20*l.scale,l.panel.bottom),brush);
    };
    auto panel=D2D1::RoundedRect(rect(l.panel),18*l.scale,18*l.scale);
    m_d2dContext->FillRoundedRectangle(panel,m_scriptGlass.Get());
    m_scriptGreen->SetOpacity(.08f);m_d2dContext->DrawRoundedRectangle(panel,m_scriptGreen.Get(),8.f);m_scriptGreen->SetOpacity(1);
    m_d2dContext->DrawRoundedRectangle(panel,m_scriptGreen.Get(),1.5f);
    m_scriptWhite->SetOpacity(.08f);
    m_d2dContext->DrawLine(D2D1::Point2F(l.panel.left+20*l.scale,l.panel.top+5*l.scale),
                         D2D1::Point2F(l.panel.right-20*l.scale,l.panel.top+5*l.scale),m_scriptWhite.Get());
    m_scriptWhite->SetOpacity(1);
    m_d2dContext->DrawLine(D2D1::Point2F(l.panel.left+28*l.scale,l.panel.top+66*l.scale),
                         D2D1::Point2F(l.panel.right-28*l.scale,l.panel.top+66*l.scale),m_scriptGreen.Get());
    text(L"ENVIRONMENT TEST / SCRIPT RUNNER",l.panel.left+28*l.scale,l.panel.top+22*l.scale,m_scriptGreen.Get());
    text(L"SCRIPT",l.script.left,l.script.top-29*l.scale,m_scriptWhite.Get());
    m_d2dContext->DrawRoundedRectangle(D2D1::RoundedRect(rect(l.script),8*l.scale,8*l.scale),m_scriptGreen.Get());
    text(L"01   MANUAL EXPLORATION",l.script.left+16*l.scale,l.script.top+17*l.scale,m_scriptWhite.Get());
    text(L"RECORDING MODE",l.panel.left+28*l.scale,l.panel.top+188*l.scale,m_scriptWhite.Get());
    auto radio=[&](Core::ScriptPanelRect r,const wchar_t* name,bool enabled){
        auto* brush=enabled?m_scriptGreen.Get():m_scriptDisabled.Get();
        const auto center=D2D1::Point2F(r.left+12*l.scale,(r.top+r.bottom)*.5f);
        const auto circle=D2D1::Ellipse(center,8*l.scale,8*l.scale);
        m_d2dContext->DrawEllipse(circle,brush);
        if(enabled)m_d2dContext->FillEllipse(D2D1::Ellipse(center,4*l.scale,4*l.scale),brush);
        text(name,r.left+29*l.scale,r.top+12*l.scale,brush);
    };
    radio(l.benchmark,L"BENCHMARK",false);radio(l.regression,L"REGRESSION",false);radio(l.livestream,L"LIVESTREAM",true);
    text(m_trace.Active()?L"RECORDING / Ctrl+R hides panel; recording continues":L"Ctrl+R closes panel / manual camera exploration",l.panel.left+28*l.scale,l.panel.top+286*l.scale,m_scriptWhite.Get());
    const auto error=m_trace.Error();
    if(!error.empty())text(L"LOG ERROR / recording stopped; see OrbitX.log",l.panel.left+28*l.scale,l.panel.top+317*l.scale,m_scriptDisabled.Get());
    else if(!m_scriptStatus.empty())text(m_scriptStatus.c_str(),l.panel.left+28*l.scale,l.panel.top+317*l.scale,m_scriptWhite.Get());
    const auto action=D2D1::RoundedRect(rect(l.action),10*l.scale,10*l.scale);
    if(m_scriptHover==0){m_scriptGreen->SetOpacity(.12f);m_d2dContext->FillRoundedRectangle(action,m_scriptGreen.Get());m_scriptGreen->SetOpacity(1);}
    m_d2dContext->DrawRoundedRectangle(action,m_scriptGreen.Get(),m_scriptHover==0?2.f:1.f);
    text(m_trace.Active()?L"STOP SCRIPT":L"RUN SCRIPT",l.action.left+24*l.scale,l.action.top+20*l.scale,m_scriptGreen.Get());
}
void Renderer::TraceCamera(const char* event){
    m_trace.Log([&](auto& out){
        const auto camera=Core::PlanetCamera::Orbit(m_yaw,m_pitch,m_distanceKm);
        out<<event<<" yaw="<<m_yaw<<" pitch="<<m_pitch<<" distance_km="<<m_distanceKm
           <<" altitude_km="<<double(m_distanceKm)-1737.4<<" fov_vertical_deg="<<m_fov
           <<" lat_deg="<<std::atan2(camera.position.z,std::hypot(camera.position.x,camera.position.y))*180/Core::PlanetPi
           <<" lon_deg="<<std::atan2(camera.position.y,camera.position.x)*180/Core::PlanetPi
           <<" position="<<camera.position.x<<","<<camera.position.y<<","<<camera.position.z
           <<" up="<<camera.up.x<<","<<camera.up.y<<","<<camera.up.z
           <<" viewport="<<m_width<<","<<m_height<<" overlay="<<m_tileOverlay<<" graph="<<m_showFrameGraph
           <<" frozen="<<m_planet.frozen<<" vsync="<<m_vsync<<" panel="<<m_scriptPanelOpen;
    });
}
void Renderer::TraceFrameSnapshot(){
    if(!m_trace.Active())return;
    m_trace.Log([&](auto& out){out<<"CACHE_STATE bytes="<<m_tileBytes<<" capacity=2048 budget_bytes=536870912"
        <<" entries="<<m_tiles.size()<<" tile_clock="<<m_tileClock<<" draw_fence_completed="<<m_fence->GetCompletedValue()
        <<" upload_fence_completed="<<m_uploadFence->GetCompletedValue()<<" next_draw_fence="<<m_nextFenceValue+1;});
    for(const auto& draw:m_tileDraws)m_trace.Log([&](auto& out){out<<"DRAW_PATCH tile="<<draw.id.level<<"/"<<draw.id.row<<"/"<<draw.id.column
        <<" source="<<draw.source.level<<"/"<<draw.source.row<<"/"<<draw.source.column<<" slot="<<draw.slot<<" grid="<<draw.grid
        <<" uv="<<draw.uv[0]<<","<<draw.uv[1]<<","<<draw.uv[2]<<","<<draw.uv[3];});
    for(const auto& id:m_planet.requests)m_trace.Log([&](auto& out){out<<"SELECTED_REQUEST tile="<<id.level<<"/"<<id.row<<"/"<<id.column;});
}
void Renderer::StartLivestream(){
    m_trace.SetFrame(m_traceFrame);
    if(!m_trace.Start(std::filesystem::path(m_root)/L"Logs")){
        SetError(m_trace.Error());m_scriptStatus=L"Cannot open livestream log";return;
    }
    m_scriptPanelOpen=false;m_scriptHover=-1;
    m_traceSession=true;
    m_scriptStatus=L"Log: "+m_trace.Path().filename().wstring();
    m_trace.Log([&](auto& out){out<<"SESSION scenario=Moon_View script=Manual_Exploration body=Moon radius_km=1737.4"
        <<" surface_root=\""<<Core::EnvironmentTrace::Escape(std::filesystem::path(m_surfaceRoot).string())<<"\""
        <<" indexed_dds="<<m_planet.files.size()<<" rejected_dds="<<m_planet.rejectedFiles
#ifdef _DEBUG
        <<" debug_build=1"
#else
        <<" debug_build=0"
#endif
#ifdef PLANET_DEBUG_MODE
        <<" environment_diagnostics=1"
#else
        <<" environment_diagnostics=0"
#endif
        <<" build_date=\""<<__DATE__<<" "<<__TIME__<<"\"";});
    TraceCamera("INITIAL_CAMERA");
    for(const auto& [id,tile]:m_tiles)m_trace.Log([&](auto& out){out<<"INITIAL_TILE tile="<<id.level<<"/"<<id.row<<"/"<<id.column
        <<" ready="<<tile.ready<<" slot="<<tile.slot<<" bytes="<<tile.bytes<<" touched="<<tile.touched
        <<" upload_fence="<<tile.uploadFence<<" last_draw_fence="<<tile.lastDrawFence
        <<" path=\""<<Core::EnvironmentTrace::Escape(m_planet.files.at(id).string())<<"\"";});
    for(const auto& id:m_failedTiles)m_trace.Log([&](auto& out){out<<"INITIAL_FAILED_TILE tile="<<id.level<<"/"<<id.row<<"/"<<id.column;});
    for(const auto& batch:m_uploadBatches)m_trace.Log([&](auto& out){out<<"INITIAL_UPLOAD_BATCH fence="<<batch.fence<<" tiles="<<batch.tiles.size()<<" used="<<batch.used;});
    if(m_deferredTile)m_trace.Log([&](auto& out){out<<"INITIAL_DEFERRED_TILE tile="<<m_deferredTile->id.level<<"/"<<m_deferredTile->id.row<<"/"<<m_deferredTile->id.column;});
    m_tileLoader.TraceSnapshot();
    TraceFrameSnapshot();
}
void Renderer::StopLivestream(const char* reason){
    if(m_trace.Active()){
        TraceCamera("FINAL_CAMERA");TraceFrameSnapshot();
        for(const auto& [id,tile]:m_tiles)m_trace.Log([&](auto& out){out<<"FINAL_TILE tile="<<id.level<<"/"<<id.row<<"/"<<id.column
            <<" ready="<<tile.ready<<" slot="<<tile.slot<<" touched="<<tile.touched
            <<" upload_fence="<<tile.uploadFence<<" last_draw_fence="<<tile.lastDrawFence;});
    }
    m_trace.Stop(reason);m_scriptPanelOpen=false;m_scriptHover=-1;
    m_traceSession=false;
    m_scriptStatus=L"Recording complete";
    if(!m_trace.Error().empty())SetError(m_trace.Error());
}
}
