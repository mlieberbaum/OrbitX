#include "Renderer.h"
#include "../Core/ScriptPanelLayout.h"
#include <iomanip>
#include <sstream>
#include <cmath>
#include <cwchar>
#include <cstring>
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
    const auto l=Core::ScriptPanelLayout::Build(float(m_width),float(m_height),m_recordTab);
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
        text(L"Position camera / reopen panel / TAKE SNAPSHOT.",l.script.left+16*l.scale,l.script.top+122*l.scale,m_scriptWhite.Get());
    }else{
        if(m_savedScripts.empty())text(L"No saved scripts / capture camera snapshots first",l.script.left+16*l.scale,l.script.top+16*l.scale,m_scriptDisabled.Get());
        for(size_t row=0;row<5&&m_scriptListOffset+row<m_savedScripts.size();++row){
            const size_t i=m_scriptListOffset+row;
            auto name=(i==m_selectedScript?L">  ":L"   ")+m_savedScripts[i].stem().wstring();
            text(name.c_str(),l.script.left+16*l.scale,l.script.top+(8+30*float(row))*l.scale,i==m_selectedScript?m_scriptGreen.Get():m_scriptWhite.Get());
        }
    }
    text(L"LOGGING MODE",l.panel.left+28*l.scale,l.panel.top+322*l.scale,m_scriptWhite.Get());
    auto radio=[&](Core::ScriptPanelRect r,const wchar_t* name,size_t index,bool selected){
        auto* brush=m_scriptGreen.Get();
        auto center=D2D1::Point2F(r.left+12*l.scale,(r.top+r.bottom)*.5f);
        m_d2dContext->DrawEllipse(D2D1::Ellipse(center,8*l.scale,8*l.scale),brush);
        if(selected)m_d2dContext->FillEllipse(D2D1::Ellipse(center,4*l.scale,4*l.scale),brush);
        auto& layout=m_scriptRadioLayouts[index];auto& metrics=m_scriptRadioMetrics[index];
        if(!layout){
            if(FAILED(m_dwriteFactory->CreateTextLayout(name,static_cast<UINT32>(wcslen(name)),m_textLeft.Get(),240,40,&layout)))return;
            layout->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);layout->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
            if(FAILED(layout->GetMetrics(&metrics))){layout.Reset();return;}
        }
        m_d2dContext->DrawTextLayout(D2D1::Point2F(r.left+29*l.scale,center.y-metrics.height*.5f-metrics.top),layout.Get(),brush);
    };
    radio(l.livestream,L"FULL",0,m_scriptLogging==ScriptLogging::Full);
    radio(l.regression,L"REGRESSION",1,m_scriptLogging==ScriptLogging::Regression);
    radio(l.benchmark,L"BENCHMARK",2,m_scriptLogging==ScriptLogging::Benchmark);
    radio(l.none,L"NONE",3,m_scriptLogging==ScriptLogging::None);
    text(m_recordTab?L"Ctrl+R hides the panel / only button clicks capture views":L"Cold tile cache / FPS throughout / 10 s steady hold",
         l.panel.left+28*l.scale,l.panel.top+418*l.scale,m_scriptWhite.Get());
    if(m_scriptOperation==ScriptOperation::Recording){
        button(l.snapshot,L"TAKE SNAPSHOT",false);
        std::wstring progress=L"Captured views: "+std::to_wstring(m_cameraScript.views.size())+L" / "+m_scriptStatus;
        text(progress.c_str(),l.panel.left+28*l.scale,l.panel.top+503*l.scale,m_scriptWhite.Get());
    }else if(m_scriptOperation==ScriptOperation::Playing){
        const auto phase=m_snapshotRun.State()==Core::SnapshotRun::Phase::Measuring?L"MEASURING":L"LOADING / WAITING FOR STABILITY";
        auto progress=L"View "+std::to_wstring(m_currentView+1)+L" / "+std::to_wstring(m_cameraScript.views.size())+L" / "+phase;
        text(progress.c_str(),l.panel.left+28*l.scale,l.panel.top+470*l.scale,m_scriptWhite.Get());
        if(m_scriptLogging!=ScriptLogging::None)text(L"Panel visibility changes invalidate performance measurements.",l.panel.left+28*l.scale,l.panel.top+503*l.scale,m_scriptDisabled.Get());
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
    if(FrameCapCharacter(c))return;
    if(m_appState!=AppState::Simulation||!m_scriptPanelOpen||!m_recordTab||!m_scriptNameFocused||m_scriptOperation!=ScriptOperation::Idle)return;
    if(c==L'\b'){if(!m_scriptName.empty())m_scriptName.pop_back();return;}
    if(c==L'\r'){m_scriptNameFocused=false;return;}
    if(m_scriptName.size()<40&&((c>=L'A'&&c<=L'Z')||(c>=L'a'&&c<=L'z')||(c>=L'0'&&c<=L'9')||c==L' '||c==L'_'||c==L'-'))m_scriptName+=c;
}
void Renderer::ScriptPanelClick(float x,float y){
    const auto l=Core::ScriptPanelLayout::Build(float(m_width),float(m_height),m_recordTab);
    if(m_scriptOperation!=ScriptOperation::Idle){
        if(m_scriptOperation==ScriptOperation::Recording&&l.snapshot.Contains(x,y))TakeCameraSnapshot();
        else if(l.action.Contains(x,y))StopLivestream("USER_STOP");
        return;
    }
    m_scriptNameFocused=false;
    if(l.recordTab.Contains(x,y)){m_recordTab=true;}
    else if(l.runTab.Contains(x,y)){m_recordTab=false;RefreshScripts();}
    else if(l.script.Contains(x,y)){
        if(m_recordTab)m_scriptNameFocused=true;
        else{
            const auto row=static_cast<size_t>((y-l.script.top)/(30*l.scale));
            if(row<5&&m_scriptListOffset+row<m_savedScripts.size())m_selectedScript=m_scriptListOffset+row;
        }
    }else if(l.none.Contains(x,y))m_scriptLogging=ScriptLogging::None;
    else if(l.livestream.Contains(x,y))m_scriptLogging=ScriptLogging::Full;
    else if(l.benchmark.Contains(x,y))m_scriptLogging=ScriptLogging::Benchmark;
    else if(l.regression.Contains(x,y))m_scriptLogging=ScriptLogging::Regression;
    else if(l.action.Contains(x,y)){if(m_recordTab)StartCameraRecording();else StartCameraPlayback();}
}
Core::ScriptCamera Renderer::CurrentScriptCamera()const{return {m_yaw,m_pitch,m_distanceKm,m_fov,m_planet.frozen};}
void Renderer::ApplyScriptCamera(const Core::ScriptCamera& c){m_yaw=c.yaw;m_pitch=c.pitch;m_distanceKm=c.distanceKm;m_fov=c.fov;m_planet.frozen=c.frozen;}
void Renderer::StartCameraRecording(){
    if(m_scriptName.find_first_not_of(L" ")==std::wstring::npos){m_scriptStatus=L"Enter a script name";return;}
    if(m_planet.frozen){m_scriptStatus=L"Unfreeze LOD (Ctrl+L) before recording";return;}
    std::string error;
    if(!m_scriptSession.Begin(std::filesystem::path(m_root)/L"Logs",m_scriptLogging!=ScriptLogging::None,error)){
        SetError(error);m_scriptStatus=L"Cannot create recording log folder";return;
    }
    if(m_scriptLogging==ScriptLogging::Full){StartLivestream();if(!m_trace.Active())return;}
    else{m_scriptPanelOpen=false;m_scriptHover=-1;}
    m_savedOverlayMode=m_tileOverlayMode;m_savedGraph=m_showFrameGraph;
    if(m_scriptLogging==ScriptLogging::Benchmark){m_tileOverlayMode=1;m_showFrameGraph=false;m_telemetryText.clear();}
    m_runOverlayMode=m_tileOverlayMode;m_runGraph=m_showFrameGraph;m_runVsync=m_vsync;m_runInvalid=false;
    m_recordedRegressionViews.clear();m_recordingCapturePending=false;m_snapshotResults.clear();
    if(m_scriptLogging!=ScriptLogging::None)m_scriptFrames.Begin(ScriptNow());
    m_cameraScript={};m_cameraScript.width=m_width;m_cameraScript.height=m_height;
    m_scriptOperation=ScriptOperation::Recording;m_scriptNameFocused=false;
    m_scriptStatus=L"No views yet";
    m_trace.Log([](auto& out){out<<"SCRIPT_RECORD_BEGIN format=SNAPSHOTS capture=BUTTON";});
}
void Renderer::TakeCameraSnapshot(){
    if(m_scriptOperation!=ScriptOperation::Recording)return;
    const auto camera=CurrentScriptCamera();
    if(camera.frozen){m_scriptStatus=L"Unfreeze LOD (Ctrl+L) to capture";return;}
    if(!camera.Valid()||m_cameraScript.views.size()>=4096){m_scriptStatus=L"Invalid camera or snapshot limit reached";return;}
    m_cameraScript.views.push_back(camera);m_scriptStatus=L"Snapshot "+std::to_wstring(m_cameraScript.views.size())+L" captured";
    if(m_scriptLogging==ScriptLogging::Full||m_scriptLogging==ScriptLogging::Regression){
        // Capture after the next UpdateCB, not from the next back buffer's
        // older matrix while handling this input event.
        m_recordedRegressionViews.push_back({{},"NOT_CAPTURED"});m_recordingCapturePending=true;
    }
    m_trace.Log([&](auto& out){out<<"SCRIPT_SNAPSHOT index="<<m_cameraScript.views.size()-1<<" yaw="<<camera.yaw<<" pitch="<<camera.pitch<<" distance_km="<<camera.distanceKm<<" fov="<<camera.fov;});
}
void Renderer::StartCameraPlayback(){
    if(m_selectedScript>=m_savedScripts.size())return;
    Core::CameraScript script;std::string error;
    if(!Core::CameraScript::Load(m_savedScripts[m_selectedScript],script,error)){SetError(error);m_scriptStatus=L"Cannot load script / recreate old paths with TAKE SNAPSHOT";return;}
    if(script.width!=m_width||script.height!=m_height){m_scriptStatus=L"Match recorded viewport: "+std::to_wstring(script.width)+L" x "+std::to_wstring(script.height);return;}
    if(!m_scriptSession.Begin(std::filesystem::path(m_root)/L"Logs",m_scriptLogging!=ScriptLogging::None,error)){SetError(error);m_scriptStatus=L"Cannot create script log folder";return;}
    OnMouseUp();m_savedCamera=CurrentScriptCamera();m_savedOverlayMode=m_tileOverlayMode;m_savedGraph=m_showFrameGraph;
    m_savedFrozenLeaves=m_planet.leaves;m_savedFrozenRequests=m_planet.requests;m_savedFrozenDependencies=m_planet.dependencies;
    if(m_scriptLogging==ScriptLogging::Regression||m_scriptLogging==ScriptLogging::Full){
#ifdef PLANET_DEBUG_MODE
        constexpr bool illuminated=true;
#else
        constexpr bool illuminated=false;
#endif
        if(!m_regressionLog.Start(m_scriptSession.directory,script,script.views.size(),illuminated,m_tileOverlayMode,m_showFrameGraph,m_vsync)){
            SetError(m_regressionLog.Error());m_scriptStatus=L"Cannot open regression log";return;
        }
        m_regressionSession=true;
    }
    if(m_scriptLogging==ScriptLogging::Full){
        StartLivestream();
        if(!m_trace.Active()){
            m_regressionLog.Stop(0,"START_FAILED");m_regressionSession=false;m_trace.Stop("START_FAILED");m_traceSession=false;return;
        }
    }else if(m_scriptLogging==ScriptLogging::Benchmark){m_tileOverlayMode=1;m_showFrameGraph=false;m_telemetryText.clear();}
    m_runOverlayMode=m_tileOverlayMode;m_runGraph=m_showFrameGraph;m_runVsync=m_vsync;
    m_cameraScript=std::move(script);m_currentView=0;m_beginSnapshot=true;m_snapshotResults.clear();m_snapshotResults.reserve(m_cameraScript.views.size());m_runInvalid=false;
    if(m_scriptLogging!=ScriptLogging::None)m_scriptFrames.Begin(ScriptNow());
    m_scriptOperation=ScriptOperation::Playing;m_scriptPanelOpen=false;m_scriptHover=-1;
    m_trace.Log([&](auto& out){out<<"SCRIPT_RUN_BEGIN format=SNAPSHOTS views="<<m_cameraScript.views.size()<<" cache_policy=ROOT_ONLY steady_us="<<Core::SnapshotSteadyUs<<" hold_us="<<Core::SnapshotHoldUs;});
}
bool Renderer::ResetScriptCache(){
    m_settledPlanetCache.Invalidate();
    m_tileLoader.Reset();m_deferredTile.reset();
    // Both queues must finish before textures or descriptor slots are recycled.
    const auto drawFence=++m_nextFenceValue;
    if(FAILED(m_queue->Signal(m_fence.Get(),drawFence))){SetError("Script cache draw fence signal failed");return false;}
    if(m_fence->GetCompletedValue()<drawFence){
        if(FAILED(m_fence->SetEventOnCompletion(drawFence,m_fenceEvent))||WaitForSingleObject(m_fenceEvent,INFINITE)!=WAIT_OBJECT_0){SetError("Script cache draw wait failed");return false;}
    }
    if(m_uploadFence->GetCompletedValue()<m_uploadFenceValue){
        if(FAILED(m_uploadFence->SetEventOnCompletion(m_uploadFenceValue,m_fenceEvent))||WaitForSingleObject(m_fenceEvent,INFINITE)!=WAIT_OBJECT_0){SetError("Script cache upload wait failed");return false;}
    }
    CollectPlanetUploads();
    for(auto it=m_tiles.begin();it!=m_tiles.end();){
        if(it->first.level==4){++it;continue;}
        const auto id=it->first;
        m_trace.Log([&](auto& out){out<<"TILE_EVICT tile="<<id.level<<"/"<<id.row<<"/"<<id.column<<" reason=SCRIPT_CACHE_RESET slot="<<it->second.slot<<" bytes="<<it->second.bytes
            <<" last_draw_fence="<<it->second.lastDrawFence<<" draw_fence_completed="<<m_fence->GetCompletedValue()<<" upload_fence="<<it->second.uploadFence;});
        m_tileBytes-=it->second.bytes;if(m_planetOptimizations)m_descriptorSlots.Release(it->second.slot);it=m_tiles.erase(it);
    }
    m_failedTiles.clear();m_tileDraws.clear();m_planet.leaves.clear();m_planet.requests.clear();m_planet.dependencies.clear();
    for(auto& batch:m_uploadBatches){batch.tiles.clear();batch.used=0;}
    m_tileStatus.clear();m_telemetryText.clear();
    return true;
}
void Renderer::UpdateScriptPlayback(){
    if(m_cameraScript.width!=m_width||m_cameraScript.height!=m_height){StopLivestream("VIEWPORT_CHANGED");return;}
    m_resetThisFrame=false;
    if(!m_beginSnapshot)return;
    if(m_currentView>=m_cameraScript.views.size()){StopLivestream("SCRIPT_COMPLETE");return;}
    m_resetThisFrame=true;
    if(!ResetScriptCache()){StopLivestream("CACHE_RESET_FAILED");return;}
    // Reset/drain work is deliberately outside this view's loading timer.
    const auto now=ScriptNow();m_snapshotRun.Begin(now);ApplyScriptCamera(m_cameraScript.views[m_currentView]);
    m_nextSnapshotCheckUs=now;m_haveSnapshot=false;m_snapshotReady=false;m_snapshotChanged=false;m_beginSnapshot=false;
    m_trace.Log([&](auto& out){out<<"SCRIPT_VIEW_BEGIN index="<<m_currentView;});TraceCamera("SCRIPT_CAMERA");
}
Core::RegressionSnapshot Renderer::RegressionSnapshot()const{
    Core::RegressionSnapshot snapshot;snapshot.camera=CurrentScriptCamera();snapshot.leaves=m_planet.leaves;
    snapshot.draws.reserve(m_tileDraws.size());
    for(const auto& draw:m_tileDraws){
        Core::RegressionDraw d;d.id=draw.id;d.source=draw.source;d.grid=draw.grid;
        if(draw.grid<m_patchMeshes.size())d.indexCount=m_patchMeshes[draw.grid].count;
        std::copy(std::begin(draw.uv),std::end(draw.uv),d.uv.begin());snapshot.draws.push_back(d);
    }
    SceneCB scene{};std::memcpy(&scene,m_cbPtr+256*m_frame,sizeof(scene));size_t index=0;
    for(const auto& row:scene.mvp.m)for(float value:row)snapshot.scene[index++]=value;
    for(const auto& row:scene.model.m)for(float value:row)snapshot.scene[index++]=value;
    snapshot.scene[index++]=scene.sunDir.x;snapshot.scene[index++]=scene.sunDir.y;snapshot.scene[index++]=scene.sunDir.z;
    return snapshot;
}
const char* Renderer::RegressionSnapshotStatus(const Core::RegressionSnapshot& snapshot,const Core::ScriptCamera& camera)const{
    const char* status="READY";
    if(snapshot.camera!=camera)status="CAMERA_CHANGED";
    else if(!m_planet.requests.empty())status="PENDING";
    else if(snapshot.leaves.empty()||snapshot.leaves.size()!=snapshot.draws.size())status="COVERAGE_INVALID";
    else{
        for(const auto id:m_planet.dependencies){
            if(m_failedTiles.contains(id)){status="DDS_FAILED";break;}
            const auto tile=m_tiles.find(id);if(tile==m_tiles.end()||!tile->second.ready){status="PENDING";break;}
        }
        std::set<Core::TileId> selected(snapshot.leaves.begin(),snapshot.leaves.end()),drawn;
        if(selected.size()!=snapshot.leaves.size())status="COVERAGE_INVALID";
        for(const auto& draw:m_tileDraws){
            const auto source=m_tiles.find(draw.source);
            if(!selected.contains(draw.id)||!drawn.insert(draw.id).second||draw.grid>=m_patchMeshes.size())status="COVERAGE_INVALID";
            else if(source==m_tiles.end()||!source->second.ready)status="PENDING";
            else if(source->second.slot!=draw.slot)status="BINDING_INVALID";
            else if(draw.id!=draw.source&&m_planet.files.contains(draw.id))status="FALLBACK_PENDING";
        }
    }
    return status;
}
void Renderer::CaptureRecordingRegression(){
    if(!m_recordingCapturePending)return;
    m_recordingCapturePending=false;bool incomplete=false;
    for(size_t i=0;i<m_recordedRegressionViews.size();++i){
        auto& capture=m_recordedRegressionViews[i];if(capture.status!="NOT_CAPTURED")continue;
        capture.snapshot=RegressionSnapshot();capture.status=RegressionSnapshotStatus(capture.snapshot,m_cameraScript.views[i]);
        if(capture.status!="READY")incomplete=true;
    }
    if(incomplete)m_scriptStatus+=L" / regression capture incomplete";
}
void Renderer::RecordScriptFrame(){
    if(m_scriptLogging==ScriptLogging::None)return;
    if(m_tileOverlayMode!=m_runOverlayMode||m_showFrameGraph!=m_runGraph||m_vsync!=m_runVsync)m_runInvalid=true;
    const auto view=m_cameraScript.views.empty()?0:m_cameraScript.views.size()-1;
    m_scriptFrames.Frame(ScriptNow(),view,m_planet.requests.empty()?"RECORDING":"RECORDING_LOADING");
}
void Renderer::CaptureRegression(){
    const auto now=ScriptNow();m_snapshotChanged=false;
    if(m_tileOverlayMode!=m_runOverlayMode||m_showFrameGraph!=m_runGraph||m_vsync!=m_runVsync||m_scriptPanelOpen)m_runInvalid=true;
    if(now<m_nextSnapshotCheckUs||(m_scriptLogging==ScriptLogging::Benchmark&&m_snapshotRun.State()==Core::SnapshotRun::Phase::Measuring)){
        // Cheap continuous coverage checks between the 10-Hz full integrity checks.
        if(!m_planet.requests.empty())m_snapshotReady=false;
        if(m_haveSnapshot){
            if(m_planet.leaves!=m_previousRegressionSnapshot.leaves||m_tileDraws.size()!=m_previousRegressionSnapshot.draws.size())m_snapshotChanged=true;
            else for(size_t i=0;i<m_tileDraws.size();++i){const auto& a=m_tileDraws[i];const auto& b=m_previousRegressionSnapshot.draws[i];
                if(a.id!=b.id||a.source!=b.source||a.grid!=b.grid||!std::equal(std::begin(a.uv),std::end(a.uv),b.uv.begin())){m_snapshotChanged=true;break;}
            }
        }
        return;
    }
    m_nextSnapshotCheckUs=now+Core::SnapshotBucketUs;
    auto snapshot=RegressionSnapshot();const char* status=RegressionSnapshotStatus(snapshot,m_cameraScript.views[m_currentView]);
    m_snapshotChanged=!m_haveSnapshot||snapshot!=m_previousRegressionSnapshot;
    m_snapshotReady=std::string(status)=="READY";
    if(m_tileOverlayMode!=m_runOverlayMode||m_showFrameGraph!=m_runGraph||m_vsync!=m_runVsync||m_scriptPanelOpen)m_runInvalid=true;
    m_previousRegressionSnapshot=std::move(snapshot);m_haveSnapshot=true;
}
void Renderer::CompleteScriptFrame(){
    const auto now=ScriptNow();const auto before=m_snapshotRun.State();
    if(m_scriptLogging!=ScriptLogging::None)m_scriptFrames.Frame(now,m_currentView,m_resetThisFrame?"RESET_LOADING":before==Core::SnapshotRun::Phase::Measuring?"MEASURING":"LOADING");
    m_snapshotRun.Observe(now,m_snapshotReady,m_snapshotChanged);
    if(before!=m_snapshotRun.State()&&m_snapshotRun.State()==Core::SnapshotRun::Phase::Measuring)
        m_trace.Log([&](auto& out){out<<"SCRIPT_STEADY index="<<m_currentView<<" loading_us="<<m_snapshotRun.Result().loadingUs;});
    if(m_snapshotRun.State()!=Core::SnapshotRun::Phase::Done)return;
    auto result=m_snapshotRun.Result();
    // Tile correctness is independent of UI and display state. Preserve the
    // existing performance status separately, after the regression capture.
    if(m_regressionSession)m_regressionLog.View(m_currentView,Core::SnapshotRegressionView(m_currentView,m_cameraScript.views[m_currentView]),m_previousRegressionSnapshot,result.status.c_str());
    if(m_runInvalid&&result.status=="READY")result.status="SETTINGS_CHANGED";
    m_trace.Log([&](auto& out){out<<"SCRIPT_VIEW_END index="<<m_currentView<<" status="<<result.status<<" loading_us="<<result.loadingUs<<" measurement_us="<<result.measurementUs<<" frames="<<result.frames;});
    m_snapshotResults.push_back(std::move(result));++m_currentView;m_beginSnapshot=true;
}
void Renderer::FinishCameraScript(const char* reason){
    const auto operation=m_scriptOperation;m_scriptOperation=ScriptOperation::Idle;
    if(operation==ScriptOperation::Recording){
        if(m_scriptLogging==ScriptLogging::Benchmark){m_tileOverlayMode=m_savedOverlayMode;m_showFrameGraph=m_savedGraph;m_telemetryText.clear();}
        auto base=m_scriptName;for(auto& c:base)if(c==L' ')c=L'_';
        const auto file=std::filesystem::path(m_root)/L"Scripts"/(base+L"_"+std::wstring(m_scriptSession.stamp.begin(),m_scriptSession.stamp.end())+L".oxscript");
        std::string error;
        try{std::filesystem::create_directories(file.parent_path());}catch(const std::exception& e){error=e.what();}
        if(error.empty()&&m_cameraScript.Save(file,error)){m_scriptStatus=L"Saved: "+file.filename().wstring();RefreshScripts();}
        else{SetError(error);m_scriptStatus=L"Camera script save failed / see OrbitX.log";}
    }else if(operation==ScriptOperation::Playing){
        m_settledPlanetCache.Invalidate();
        ApplyScriptCamera(m_savedCamera);
        if(m_savedCamera.frozen){m_planet.leaves=std::move(m_savedFrozenLeaves);m_planet.requests=std::move(m_savedFrozenRequests);m_planet.dependencies=std::move(m_savedFrozenDependencies);}
        if(m_scriptLogging==ScriptLogging::Benchmark){m_tileOverlayMode=m_savedOverlayMode;m_showFrameGraph=m_savedGraph;}m_telemetryText.clear();
        const std::string why=reason;m_scriptStatus=why=="SCRIPT_COMPLETE"?L"Playback complete / original view restored":L"Playback stopped / original view restored";
        if(why=="VIEWPORT_CHANGED")m_scriptStatus=L"Viewport changed / playback stopped";
    }
}
void Renderer::TraceCamera(const char* event){
    if(!m_trace.Active())return;
    const std::array<double,11> state={double(m_yaw),double(m_pitch),double(m_distanceKm),double(m_fov),double(m_width),double(m_height),double(m_tileOverlayMode),double(m_showFrameGraph),double(m_planet.frozen),double(m_vsync),double(m_scriptPanelOpen)};
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
           <<" viewport="<<m_width<<","<<m_height<<" overlay="<<(m_tileOverlayMode>1)<<" overlay_mode="<<m_tileOverlayMode<<" graph="<<m_showFrameGraph
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
    m_settledPlanetCache.Invalidate();
    m_traceDrawIds.clear();m_traceRequestIds.clear();
    m_traceDrawStates.Clear();m_traceCameraStates.Clear();m_traceMatrixStates.Clear();
    m_trace.SetFrame(m_traceFrame);
    if(!m_trace.Start(m_scriptSession.directory)){
        SetError(m_trace.Error());m_scriptStatus=L"Cannot open Full log";return;
    }
    m_scriptPanelOpen=false;m_scriptHover=-1;
    m_traceSession=true;
    m_scriptStatus=L"Log: "+m_trace.Path().filename().wstring();
    m_trace.Log([&](auto& out){out<<"SESSION scenario=Moon_View script=Manual_Exploration body=Moon radius_km=1737.4"
        <<" surface_root=\""<<Core::EnvironmentTrace::Escape(std::filesystem::path(m_surfaceRoot).string())<<"\""
        <<" planet_optimizations="<<m_planetOptimizations
        <<" frame_cap_enabled="<<m_frameLimit.enabled<<" frame_cap_fps="<<m_frameLimit.fps<<" effective_frame_cap_fps="<<EffectiveFrameLimit()
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
    const bool playing=m_scriptOperation==ScriptOperation::Playing;
    const bool recording=m_scriptOperation==ScriptOperation::Recording;
    const bool recordingLogs=recording&&m_scriptLogging!=ScriptLogging::None;
    const bool recordingRegression=recording&&(m_scriptLogging==ScriptLogging::Full||m_scriptLogging==ScriptLogging::Regression);
    const bool complete=std::string(reason)=="SCRIPT_COMPLETE";
    const bool regression=m_regressionSession;
    std::filesystem::path performancePath;std::string performanceError;
    if((playing||recordingLogs)&&m_scriptLogging!=ScriptLogging::None){
        if(playing&&!complete&&!m_beginSnapshot){auto partial=m_snapshotRun.Result();partial.status="CANCELLED";m_snapshotResults.push_back(std::move(partial));}
        m_scriptFrames.Finish(ScriptNow());
        const char* mode=m_scriptLogging==ScriptLogging::Benchmark?"BENCHMARK":m_scriptLogging==ScriptLogging::Regression?"REGRESSION":"FULL";
        Core::SaveSnapshotPerformance(m_scriptSession.directory,m_cameraScript,m_snapshotResults,m_scriptFrames.Buckets(),mode,m_runOverlayMode,m_runGraph,m_runVsync,reason,performancePath,performanceError,recording?"RECORDING":"RUN");
    }
    bool recordingLogValid=false;
    if(recordingRegression){
#ifdef PLANET_DEBUG_MODE
        constexpr bool illuminated=true;
#else
        constexpr bool illuminated=false;
#endif
        if(m_regressionLog.Start(m_scriptSession.directory,m_cameraScript,m_cameraScript.views.size(),illuminated,m_runOverlayMode,m_runGraph,m_runVsync,true)){
            for(size_t i=0;i<m_cameraScript.views.size();++i){
                Core::RegressionView view{i,i,m_cameraScript.views[i]};
                if(i>=m_recordedRegressionViews.size())m_regressionLog.Missing(i,view);
                else{const auto& capture=m_recordedRegressionViews[i];m_regressionLog.View(i,view,capture.snapshot,capture.status.c_str());}
            }
            const std::string why=reason;
            const bool savedStop=!m_cameraScript.views.empty()&&(why=="USER_STOP"||why=="SCENARIO_EXIT"||why=="APPLICATION_SHUTDOWN");
            m_regressionLog.Stop(m_cameraScript.views.size(),savedStop?"RECORDING_COMPLETE":reason);
            recordingLogValid=savedStop&&!m_regressionLog.Incomplete()&&m_regressionLog.Error().empty();
        }
        if(!m_regressionLog.Error().empty())SetError(m_regressionLog.Error());
    }
    bool validRegression=false;
    if(regression){
        for(size_t i=m_currentView;i<m_cameraScript.views.size();++i)m_regressionLog.Missing(i,Core::SnapshotRegressionView(i,m_cameraScript.views[i]));
        m_regressionLog.Stop(m_cameraScript.views.size()*Core::SnapshotHoldUs,reason);
        validRegression=complete&&!m_regressionLog.Incomplete()&&m_regressionLog.Error().empty();
        m_regressionSession=false;
        if(!m_regressionLog.Error().empty())SetError(m_regressionLog.Error());
    }
    // Full owns both writers: closing the regression log must not bypass the
    // Full trace's final state, camera restoration, or terminal event.
    if(m_trace.Active()){
        TraceCamera("FINAL_CAMERA");TraceFrameSnapshot();
        for(const auto& [id,tile]:m_tiles)m_trace.Log([&](auto& out){out<<"FINAL_TILE tile="<<id.level<<"/"<<id.row<<"/"<<id.column
            <<" ready="<<tile.ready<<" slot="<<tile.slot<<" touched="<<tile.touched<<" upload_fence="<<tile.uploadFence<<" last_draw_fence="<<tile.lastDrawFence;});
    }
    if(m_scriptOperation!=ScriptOperation::Idle)m_trace.Log([&](auto& out){out<<"SCRIPT_END reason="<<reason<<" views="<<m_cameraScript.views.size();});
    if(playing){FinishCameraScript(reason);TraceCamera("SCRIPT_CAMERA_RESTORED");}
    m_trace.Stop(reason);m_scriptPanelOpen=false;m_scriptHover=-1;m_traceSession=false;
    if(m_scriptOperation==ScriptOperation::Recording)FinishCameraScript(reason);
    if(!m_trace.Error().empty()&&m_scriptLogging!=ScriptLogging::None)SetError(m_trace.Error());
    if(regression)m_scriptStatus=m_regressionLog.Path().filename().wstring()+(validRegression?L" / COMPLETE / compare baseline":L" / INVALID / inspect before comparing");
    if(!performanceError.empty()){SetError(performanceError);m_scriptStatus+=L" / performance write failed";}
    else if((playing&&m_scriptLogging!=ScriptLogging::None)||recordingLogs)m_scriptStatus+=L" / "+performancePath.filename().wstring();
    if(recordingRegression)m_scriptStatus+=recordingLogValid?L" / regression captured":L" / regression INVALID";
    if(recording)m_recordedRegressionViews.clear();
}
}
