#include "../Core/CameraScript.h"
#include "../Core/ScriptPanelLayout.h"
#include <cassert>
#include <iostream>
using namespace OrbitX::Core;
int main(){
    const auto directory=std::filesystem::temp_directory_path()/("orbitx-camera-test-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(directory);
    CameraScript script;script.width=3840;script.height=2160;script.durationUs=20000000;
    script.samples={{0,{.7f,4.75f,2012.97363f,40.f,true},false},
        {10000000,{.71f,4.75495863f,2137.4f,40.f,false},true},
        {10000000,{.71f,4.75495863f,2137.4f,40.f,false},false},
        {15000000,{1.f,4.8f,1750.f,55.f,false},false}};
    script.samples[0].frozenSelection=true;script.samples[0].leaves={{4,0,0},{4,0,1}};
    std::string error;const auto path=directory/"polar.oxscript";
    assert(script.Save(path,error));assert(error.empty());
    CameraScript loaded;assert(CameraScript::Load(path,loaded,error));
    assert(loaded.durationUs==script.durationUs&&loaded.width==script.width&&loaded.height==script.height);
    assert(loaded.samples.size()==script.samples.size());
    for(size_t i=0;i<script.samples.size();i++){
        assert(loaded.samples[i].timeUs==script.samples[i].timeUs&&loaded.samples[i].checkpoint==script.samples[i].checkpoint);
        assert(loaded.samples[i].camera==script.samples[i].camera);
        assert(loaded.samples[i].leaves==script.samples[i].leaves&&loaded.samples[i].frozenSelection==script.samples[i].frozenSelection);
        auto a=PlanetCamera::Orbit(loaded.samples[i].camera.yaw,loaded.samples[i].camera.pitch,loaded.samples[i].camera.distanceKm);
        auto b=PlanetCamera::Orbit(script.samples[i].camera.yaw,script.samples[i].camera.pitch,script.samples[i].camera.distanceKm);
        assert(a.position.x==b.position.x&&a.position.z==b.position.z&&a.up.z==b.up.z);
    }
    assert(!script.Save(path,error)); // Never overwrite an existing recording.
    assert(!script.Save(path/"unwritable.oxscript",error));
    auto bad=script;bad.samples[2].timeUs=1;assert(!bad.Valid());
    bad=script;bad.samples[0].timeUs=1;assert(!bad.Valid());
    bad=script;bad.samples[1].camera.distanceKm=1737.3f;assert(!bad.Valid());
    bad=script;bad.samples[1].camera.fov=180;assert(!bad.Valid());
    bad=script;bad.durationUs=1;assert(!bad.Valid());
    bad=script;bad.samples[1].camera.yaw=std::numeric_limits<float>::infinity();assert(!bad.Valid());
    auto invalid=directory/"bad.oxscript";
    for(const auto& contents:{"ORBITX_CAMERA_SCRIPT 9 Moon\n", "ORBITX_CAMERA_SCRIPT 1 Earth\n", "ORBITX_CAMERA_SCRIPT 1 Moon\nVIEWPORT 10 10\nDURATION_US 10\nCAMERA 0 0 0 1800 40 2 0 0 0 0 0\n", "ORBITX_CAMERA_SCRIPT 1 Moon\nVIEWPORT 10 10\nDURATION_US 10\nCAMERA 0 0 0 1800 40 0 0 0 0 0 62.6\nGARBAGE\n"}){
        std::ofstream out(invalid);out<<contents;out.close();assert(!CameraScript::Load(invalid,loaded,error));
        assert(loaded.samples.size()==script.samples.size()); // Failed loads do not destroy a good script.
    }
    ScriptClock clock{100};assert(clock.Elapsed(110)==10);assert(clock.Elapsed(1000100)==1000000);
    // Sample endpoints are exact; intermediate frames smoothly cross angle wrap
    // and retain discrete freeze state until its timestamp.
    CameraSample first{0,{6.27f,6.27f,1800.f,40.f,false},false};
    CameraSample second{10000,{.01f,.01f,2000.f,60.f,true},false};
    assert(InterpolateCamera(first,second,0)==first.camera);
    assert(InterpolateCamera(first,second,10000)==second.camera);
    auto middle=InterpolateCamera(first,second,5000);
    assert(std::abs(std::remainder(double(middle.yaw),2*PlanetPi))<.01);
    assert(std::abs(std::remainder(double(middle.pitch),2*PlanetPi))<.01);
    assert(middle.distanceKm==1900.f&&middle.fov==50.f&&!middle.frozen);
    // North-pole crossing retains the existing continuous orbit orientation.
    first.camera.pitch=4.70f;second.camera.pitch=4.73f;
    middle=InterpolateCamera(first,second,5000);
    auto pole=PlanetCamera::Orbit(middle.yaw,middle.pitch,middle.distanceKm);
    assert(pole.position.z/middle.distanceKm>.999);
    assert(std::abs(Dot(pole.up,pole.forward))<1.e-12);
    // Stationary samples retain a manually recorded loading pause.
    second.camera=first.camera;second.timeUs=5000000;
    assert(InterpolateCamera(first,second,2500000)==first.camera);
    // Duplicate timestamps are deterministic and avoid division by zero.
    second.timeUs=first.timeUs;second.camera.yaw=1.f;
    assert(InterpolateCamera(first,second,first.timeUs)==second.camera);
    for(auto [w,h]:{std::pair{3840.f,2160.f},std::pair{1280.f,720.f},std::pair{320.f,180.f}}){
        auto l=ScriptPanelLayout::Build(w,h);assert(l.panel.left>=0&&l.panel.top>=0&&l.panel.right<=w&&l.panel.bottom<=h);
        assert(!l.action.Contains(l.script.left,l.script.top));
        assert(!l.recordTab.Contains(l.runTab.left,l.runTab.top));
    }
    std::filesystem::remove_all(directory);std::cout<<"Camera script tests passed\n";
}
