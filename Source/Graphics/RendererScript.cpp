#include "Renderer.h"
#include "../Core/ScriptPanelLayout.h"
#include <iomanip>
#include <sstream>
#include <cmath>
#include <cwchar>
namespace OrbitX::Graphics {
namespace {
uint64_t ScriptNow(){return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());}
}
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
    auto button=[&](Core::ScriptPanelRect r,const wchar_t* name,bool selected,bool enabled=true){
        auto* brush=enabled?m_scriptGreen.Get():m_scriptDisabled.Get();
        if(selected){brush->SetOpacity(.12f);m_d2dContext->FillRoundedRectangle(D2D1::RoundedRect(rect(r),8*l.scale,8*l.scale),brush);brush->SetOpacity(1);}
        m_d2dContext->DrawRoundedRectangle(D2D1::RoundedRect(rect(r),8*l.scale,8*l.scale),brush,selected?2.f:1.f);
        text(name,r.left+16*l.scale,r.top+12*l.scale,brush);
    };
    const bool active=m_scriptOperation!=ScriptOperation::Idle;
    auto panel=D2D1::RoundedRect(rect(l.panel),18*l.scale,18*l.scale);
    m_d2dContext->FillRoundedRectangle(panel,m_scriptGlass.Get());
    m_scriptGreen->SetOpacity(.08f);m_d2dContext->DrawRoundedRectangle(panel,m_scriptGreen.Get(),8.f);m_scriptGreen->SetOpacity(1);
    m_d2dContext->DrawRoundedRectangle(panel,m_scriptGreen.Get(),1.5f);
    text(L"ENVIRONMENT TEST / CAMERA SCRIPTS",l.panel.left+28*l.scale,l.panel.top+22*l.scale,m_scriptGreen.Get());
    button(l.recordTab,L"RECORD",m_recordTab,!active);button(l.runTab,L"RUN",!m_recordTab,!active);
    m_d2dContext->DrawRoundedRectangle(D2D1::RoundedRect(rect(l.script),8*l.scale,8*l.scale),m_scriptGreen.Get());
    if(m_recordTab){
        text(L"SCRIPT NAME",l.script.left+16*l.scale,l.script.top+12*l.scale,m_scriptWhite.Get());
        auto name=m_scriptName+(m_scriptNameFocused?L"_":L"");
        text(name.c_str(),l.script.left+16*l.scale,l.script.top+46*l.scale,m_scriptGreen.Get());
        text(L"Click to name / letters, numbers, spaces, - and _",l.script.left+16*l.scale,l.script.top+94*l.scale,m_scriptDisabled.Get());
        text(L"Camera path sampled at 100 Hz / pauses are recorded too.",l.script.left+16*l.scale,l.script.top+122*l.scale,m_scriptWhite.Get());
    }else{
        if(m_savedScripts.empty())text(L"No saved scripts / record a camera path first",l.script.left+16*l.scale,l.script.top+16*l.scale,m_scriptDisabled.Get());
        for(size_t row=0;row<5&&m_scriptListOffset+row<m_savedScripts.size();++row){
            const size_t i=m_scriptListOffset+row;
            auto name=(i==m_selectedScript?L">  ":L"   ")+m_savedScripts[i].stem().wstring();
            text(name.c_str(),l.script.left+16*l.scale,l.script.top+(8+30*float(row))*l.scale,i==m_selectedScript?m_scriptGreen.Get():m_scriptWhite.Get());
        }
    }
    text(L"LOGGING MODE",l.panel.left+28*l.scale,l.panel.top+322*l.scale,m_scriptWhite.Get());
    auto radio=[&](Core::ScriptPanelRect r,const wchar_t* name,bool enabled){
        auto* brush=enabled?m_scriptGreen.Get():m_scriptDisabled.Get();
        auto center=D2D1::Point2F(r.left+12*l.scale,(r.top+r.bottom)*.5f);
        m_d2dContext->DrawEllipse(D2D1::Ellipse(center,8*l.scale,8*l.scale),brush);
        if(enabled)m_d2dContext->FillEllipse(D2D1::Ellipse(center,4*l.scale,4*l.scale),brush);
        text(name,r.left+29*l.scale,r.top+12*l.scale,brush);
    };
    radio(l.benchmark,L"BENCHMARK",false);radio(l.livestream,L"FULL",true);radio(l.regression,L"REGRESSION",false);
    text(m_recordTab?L"Ctrl+R hides the panel / recording continues":L"Smooth replay / original timing / mouse wheel scrolls scripts",
         l.panel.left+28*l.scale,l.panel.top+418*l.scale,m_scriptWhite.Get());
    if(active){
        wchar_t progress[160];
        if(m_scriptOperation==ScriptOperation::Recording)swprintf_s(progress,L"RECORDING / %.1f s / %zu samples",double(m_scriptClock.Elapsed(ScriptNow()))/1.e6,m_cameraScript.samples.size());
        else swprintf_s(progress,L"PLAYING / %.1f / %.1f s",double(m_scriptClock.Elapsed(ScriptNow()))/1.e6,double(m_cameraScript.durationUs)/1.e6);
        text(progress,l.panel.left+28*l.scale,l.panel.top+470*l.scale,m_scriptWhite.Get());
    }else if(!m_scriptStatus.empty())text(m_scriptStatus.c_str(),l.panel.left+28*l.scale,l.panel.top+503*l.scale,m_scriptWhite.Get());
    const bool enabled=active||m_recordTab||!m_savedScripts.empty();
    button(l.action,active?(m_scriptOperation==ScriptOperation::Recording?L"STOP & SAVE":L"STOP SCRIPT"):(m_recordTab?L"START RECORDING":L"RUN SCRIPT"),m_scriptHover==0,enabled);
}
void Renderer::RefreshScripts(){
    const auto old=m_selectedScript<m_savedScripts.size()?m_savedScripts[m_selectedScript]:std::filesystem::path{};
    m_savedScripts.clear();
    try{
        const auto directory=std::filesystem::path(m_root)/L"Scripts";
        std::filesystem::create_directories(directory);
        for(const auto& entry:std::filesystem::directory_iterator(directory))if(entry.is_regular_file()&&entry.path().extension()==L".oxscript")m_savedScripts.push_back(entry.path());
        std::sort(m_savedScripts.begin(),m_savedScripts.end());
        const auto found=std::find(m_savedScripts.begin(),m_savedScripts.end(),old);
        m_selectedScript=found==m_savedScripts.end()?0:static_cast<size_t>(found-m_savedScripts.begin());
        m_scriptListOffset=m_selectedScript>=5?m_selectedScript-4:0;
    }catch(const std::exception& e){SetError(e.what());m_scriptStatus=L"Cannot read Runtime/Scripts";}
}
void Renderer::OnCharacter(wchar_t c){
    if(m_appState!=AppState::Simulation||!m_scriptPanelOpen||!m_recordTab||!m_scriptNameFocused||m_scriptOperation!=ScriptOperation::Idle)return;
    if(c==L'\b'){if(!m_scriptName.empty())m_scriptName.pop_back();return;}
    if(c==L'\r'){m_scriptNameFocused=false;return;}
    if(m_scriptName.size()<40&&((c>=L'A'&&c<=L'Z')||(c>=L'a'&&c<=L'z')||(c>=L'0'&&c<=L'9')||c==L' '||c==L'_'||c==L'-'))m_scriptName+=c;
}
void Renderer::ScriptPanelClick(float x,float y){
    const auto l=Core::ScriptPanelLayout::Build(float(m_width),float(m_height));
    if(m_scriptOperation!=ScriptOperation::Idle){
        if(l.action.Contains(x,y))StopLivestream("USER_STOP");
        return;
    }
    m_scriptNameFocused=false;
    if(l.recordTab.Contains(x,y))m_recordTab=true;
    else if(l.runTab.Contains(x,y)){m_recordTab=false;RefreshScripts();}
    else if(l.script.Contains(x,y)){
        if(m_recordTab)m_scriptNameFocused=true;
        else{
            const auto row=static_cast<size_t>((y-l.script.top)/(30*l.scale));
            if(row<5&&m_scriptListOffset+row<m_savedScripts.size())m_selectedScript=m_scriptListOffset+row;
        }
    }else if(l.action.Contains(x,y)){if(m_recordTab)StartCameraRecording();else StartCameraPlayback();}
}
Core::ScriptCamera Renderer::CurrentScriptCamera()const{return {m_yaw,m_pitch,m_distanceKm,m_fov,m_planet.frozen};}
void Renderer::ApplyScriptCamera(const Core::ScriptCamera& c){m_yaw=c.yaw;m_pitch=c.pitch;m_distanceKm=c.distanceKm;m_fov=c.fov;m_planet.frozen=c.frozen;}
void Renderer::StartCameraRecording(){
    if(m_scriptName.find_first_not_of(L" ")==std::wstring::npos){m_scriptStatus=L"Enter a script name";return;}
    StartLivestream();if(!m_trace.Active())return;
    m_cameraScript={};m_cameraScript.width=m_width;m_cameraScript.height=m_height;
    m_scriptClock={ScriptNow()};m_nextCameraSampleUs=10000;m_scriptOperation=ScriptOperation::Recording;
    Core::CameraSample initial{0,CurrentScriptCamera(),false};
    if(initial.camera.frozen){initial.frozenSelection=true;initial.leaves=m_planet.leaves;}
    m_cameraScript.samples.push_back(std::move(initial));m_scriptNameFocused=false;
    m_trace.Log([](auto& out){out<<"SCRIPT_RECORD_BEGIN format=1 logging=FULL sample_hz=100";});
}
void Renderer::RecordCamera(bool force){
    if(m_scriptOperation!=ScriptOperation::Recording)return;
    const auto elapsed=m_scriptClock.Elapsed(ScriptNow());
    if(!force&&elapsed<m_nextCameraSampleUs)return;
    if(m_cameraScript.samples.size()>=999999||elapsed>24ull*60*60*1000000)return;
    const auto state=CurrentScriptCamera();
    Core::CameraSample sample{elapsed,state,false};
    if(state.frozen&&!m_cameraScript.samples.back().camera.frozen){sample.frozenSelection=true;sample.leaves=m_planet.leaves;}
    m_cameraScript.samples.push_back(std::move(sample));
    // Sample real state on available render frames. Do not invent historical
    // positions if a slow frame missed one or more 10-ms sampling intervals.
    m_nextCameraSampleUs=(elapsed/10000+1)*10000;
}
void Renderer::StartCameraPlayback(){
    if(m_selectedScript>=m_savedScripts.size())return;
    Core::CameraScript script;std::string error;
    if(!Core::CameraScript::Load(m_savedScripts[m_selectedScript],script,error)){SetError(error);m_scriptStatus=L"Cannot load selected script / see OrbitX.log";return;}
    if(script.width!=m_width||script.height!=m_height){m_scriptStatus=L"Match the recorded viewport: "+std::to_wstring(script.width)+L" x "+std::to_wstring(script.height);return;}
    OnMouseUp();m_savedCamera=CurrentScriptCamera();m_savedOverlay=m_tileOverlay;m_savedGraph=m_showFrameGraph;
    m_savedFrozenLeaves=m_planet.leaves;m_savedFrozenRequests=m_planet.requests;m_savedFrozenDependencies=m_planet.dependencies;
    StartLivestream();if(!m_trace.Active())return;
    m_cameraScript=std::move(script);m_nextSample=0;m_scriptClock={ScriptNow()};m_nextCameraSampleUs=10000;m_scriptOperation=ScriptOperation::Playing;
    m_trace.Log([&](auto& out){out<<"SCRIPT_RUN_BEGIN logging=FULL timing=RECORDED interpolation=LINEAR file=\""<<Core::EnvironmentTrace::Escape(m_savedScripts[m_selectedScript].filename().string())<<"\" cache_policy=EXISTING entries="<<m_tiles.size();});
}
void Renderer::UpdateScriptPlayback(){
    if(m_cameraScript.width!=m_width||m_cameraScript.height!=m_height){StopLivestream("VIEWPORT_CHANGED");return;}
    const auto elapsed=m_scriptClock.Elapsed(ScriptNow());bool applied=false;
    while(m_nextSample<m_cameraScript.samples.size()&&m_cameraScript.samples[m_nextSample].timeUs<=elapsed){
        const auto& sample=m_cameraScript.samples[m_nextSample++];ApplyScriptCamera(sample.camera);applied=true;
        if(sample.frozenSelection){m_planet.leaves=sample.leaves;m_planet.requests.clear();m_planet.dependencies.clear();}
    }
    if(m_nextSample>0&&m_nextSample<m_cameraScript.samples.size())
        ApplyScriptCamera(Core::InterpolateCamera(m_cameraScript.samples[m_nextSample-1],m_cameraScript.samples[m_nextSample],elapsed));
    TraceCamera("SCRIPT_CAMERA");
    // Render the final recorded view once before completion and restoration.
    if(!applied&&m_nextSample==m_cameraScript.samples.size()&&elapsed>=m_cameraScript.durationUs)StopLivestream("SCRIPT_COMPLETE");
}
void Renderer::FinishCameraScript(const char* reason){
    const auto operation=m_scriptOperation;m_scriptOperation=ScriptOperation::Idle;
    if(operation==ScriptOperation::Recording){
        auto base=m_scriptName;for(auto& c:base)if(c==L' ')c=L'_';
        const auto file=std::filesystem::path(m_root)/L"Scripts"/(base+L"_"+m_trace.Path().stem().wstring()+L".oxscript");
        std::string error;
        try{std::filesystem::create_directories(file.parent_path());}catch(const std::exception& e){error=e.what();}
        if(error.empty()&&m_cameraScript.Save(file,error)){m_scriptStatus=L"Saved: "+file.filename().wstring();RefreshScripts();}
        else{SetError(error);m_scriptStatus=L"Camera script save failed / see OrbitX.log";}
    }else if(operation==ScriptOperation::Playing){
        ApplyScriptCamera(m_savedCamera);
        if(m_savedCamera.frozen){m_planet.leaves=std::move(m_savedFrozenLeaves);m_planet.requests=std::move(m_savedFrozenRequests);m_planet.dependencies=std::move(m_savedFrozenDependencies);}
        m_tileOverlay=m_savedOverlay;m_showFrameGraph=m_savedGraph;m_telemetryText.clear();
        const std::string why=reason;m_scriptStatus=why=="SCRIPT_COMPLETE"?L"Playback complete / original view restored":L"Playback stopped / original view restored";
        if(why=="VIEWPORT_CHANGED")m_scriptStatus=L"Viewport changed / playback stopped";
    }
}
void Renderer::TraceCamera(const char* event){
    // LOD freeze changes are discrete commands; capture their exact input time
    // in addition to the 100-Hz camera samples.
    if(m_scriptOperation==ScriptOperation::Recording&&!m_cameraScript.samples.empty()
       &&m_planet.frozen!=m_cameraScript.samples.back().camera.frozen)RecordCamera(true);
    if(!m_trace.Active())return;
    const std::array<double,11> state={double(m_yaw),double(m_pitch),double(m_distanceKm),double(m_fov),double(m_width),double(m_height),double(m_tileOverlay),double(m_showFrameGraph),double(m_planet.frozen),double(m_vsync),double(m_scriptPanelOpen)};
    const bool changed=m_traceCameraStates.Update(0,state);
    if(!changed&&(std::string(event)=="CAMERA_STATE"||std::string(event)=="SCRIPT_CAMERA"))return;
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
    std::map<int,size_t> levels;std::set<Core::TileId> draws,requests(m_planet.requests.begin(),m_planet.requests.end());
    for(const auto& draw:m_tileDraws){
        ++levels[draw.source.level];draws.insert(draw.id);
        if(m_traceDrawStates.Update(draw.id,draw))m_trace.Log([&](auto& out){out<<"DRAW_PATCH tile="<<draw.id.level<<"/"<<draw.id.row<<"/"<<draw.id.column
            <<" source="<<draw.source.level<<"/"<<draw.source.row<<"/"<<draw.source.column<<" slot="<<draw.slot<<" grid="<<draw.grid
            <<" uv="<<draw.uv[0]<<","<<draw.uv[1]<<","<<draw.uv[2]<<","<<draw.uv[3];});
    }
    for(auto id:m_traceDrawIds)if(!draws.contains(id)){m_traceDrawStates.Erase(id);m_trace.Log([&](auto& out){out<<"DRAW_REMOVE tile="<<id.level<<"/"<<id.row<<"/"<<id.column;});}
    for(auto id:requests)if(!m_traceRequestIds.contains(id))m_trace.Log([&](auto& out){out<<"REQUEST_ADD tile="<<id.level<<"/"<<id.row<<"/"<<id.column;});
    for(auto id:m_traceRequestIds)if(!requests.contains(id))m_trace.Log([&](auto& out){out<<"REQUEST_REMOVE tile="<<id.level<<"/"<<id.row<<"/"<<id.column;});
    m_traceDrawIds=std::move(draws);m_traceRequestIds=std::move(requests);
    m_trace.Log([&](auto& out){out<<"FRAME_STATE bytes="<<m_tileBytes<<" capacity="<<TileCapacity<<" budget_bytes="<<TileBudget
        <<" entries="<<m_tiles.size()<<" dependencies="<<m_planet.dependencies.size()<<" requests="<<m_planet.requests.size()
        <<" patches="<<m_tileDraws.size()<<" tile_clock="<<m_tileClock<<" draw_fence_completed="<<m_fence->GetCompletedValue()
        <<" upload_fence_completed="<<m_uploadFence->GetCompletedValue()<<" next_draw_fence="<<m_nextFenceValue+1<<" levels=";
        for(auto [level,count]:levels)out<<level<<":"<<count<<",";});
}
void Renderer::StartLivestream(){
    m_traceDrawIds.clear();m_traceRequestIds.clear();
    m_traceDrawStates.Clear();m_traceCameraStates.Clear();m_traceMatrixStates.Clear();
    m_trace.SetFrame(m_traceFrame);
    if(!m_trace.Start(std::filesystem::path(m_root)/L"Logs")){
        SetError(m_trace.Error());m_scriptStatus=L"Cannot open Full log";return;
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
    if(m_scriptOperation==ScriptOperation::Recording){
        const auto state=CurrentScriptCamera();const auto elapsed=std::min<uint64_t>(m_scriptClock.Elapsed(ScriptNow()),24ull*60*60*1000000);
        m_cameraScript.durationUs=elapsed;
        if(m_cameraScript.samples.size()<1000000)m_cameraScript.samples.push_back({elapsed,state,false});
    }
    if(m_trace.Active()){
        TraceCamera("FINAL_CAMERA");TraceFrameSnapshot();
        for(const auto& [id,tile]:m_tiles)m_trace.Log([&](auto& out){out<<"FINAL_TILE tile="<<id.level<<"/"<<id.row<<"/"<<id.column
            <<" ready="<<tile.ready<<" slot="<<tile.slot<<" touched="<<tile.touched
            <<" upload_fence="<<tile.uploadFence<<" last_draw_fence="<<tile.lastDrawFence;});
    }
    if(m_scriptOperation!=ScriptOperation::Idle)m_trace.Log([&](auto& out){out<<"SCRIPT_END reason="<<reason<<" samples="<<m_cameraScript.samples.size()<<" duration_us="<<m_cameraScript.durationUs;});
    if(m_scriptOperation==ScriptOperation::Playing){FinishCameraScript(reason);TraceCamera("SCRIPT_CAMERA_RESTORED");}
    m_trace.Stop(reason);m_scriptPanelOpen=false;m_scriptHover=-1;
    m_traceSession=false;
    if(m_scriptOperation==ScriptOperation::Recording)FinishCameraScript(reason);
    if(!m_trace.Error().empty())SetError(m_trace.Error());
}
}
