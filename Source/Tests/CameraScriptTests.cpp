#include "../Core/CameraScript.h"
#include "../Core/ScriptPanelLayout.h"
#include <cassert>
#include <chrono>
#include <iostream>
using namespace OrbitX::Core;
int main(){
    const auto directory=std::filesystem::temp_directory_path()/("orbitx-camera-test-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(directory);
    CameraScript script;script.width=3840;script.height=2160;
    script.views={{.7f,4.75f,2012.97363f,40.f,false},{.71f,4.75495863f,2137.4f,40.f,false},{1.f,4.8f,1750.f,55.f,false}};
    std::string error;const auto path=directory/"polar.oxscript";
    assert(script.Save(path,error));CameraScript loaded;assert(CameraScript::Load(path,loaded,error));
    assert(loaded.width==script.width&&loaded.height==script.height&&loaded.views==script.views);
    for(size_t i=0;i<script.views.size();++i){
        auto a=PlanetCamera::Orbit(loaded.views[i].yaw,loaded.views[i].pitch,loaded.views[i].distanceKm);
        auto b=PlanetCamera::Orbit(script.views[i].yaw,script.views[i].pitch,script.views[i].distanceKm);
        assert(a.position.x==b.position.x&&a.position.z==b.position.z&&a.up.z==b.up.z);
    }
    assert(!script.Save(path,error));assert(!script.Save(path/"unwritable.oxscript",error));
    auto bad=script;bad.views.clear();assert(!bad.Valid());
    bad=script;bad.views[0].frozen=true;assert(!bad.Valid());
    bad=script;bad.views[0].distanceKm=1737.3f;assert(!bad.Valid());
    bad=script;bad.views[0].fov=180;assert(!bad.Valid());
    bad=script;bad.views[0].yaw=std::numeric_limits<float>::infinity();assert(!bad.Valid());
    bad=script;bad.views.resize(4097);assert(!bad.Valid());
    const auto invalid=directory/"bad.oxscript";
    for(const auto& contents:{"ORBITX_CAMERA_SCRIPT 1 Moon\n", "ORBITX_SNAPSHOT_SCRIPT 1 Earth\n", "ORBITX_SNAPSHOT_SCRIPT 1 Moon\nVIEWPORT 10 10\nVIEW 0 0 1800 180\n", "ORBITX_SNAPSHOT_SCRIPT 1 Moon\nVIEWPORT 10 10\nVIEW 0 0 1800 40\nGARBAGE\n"}){
        std::ofstream out(invalid);out<<contents;out.close();assert(!CameraScript::Load(invalid,loaded,error));assert(loaded.views==script.views);
    }
    for(auto [w,h]:{std::pair{3840.f,2160.f},std::pair{1280.f,720.f},std::pair{320.f,180.f}}){
        auto l=ScriptPanelLayout::Build(w,h);assert(l.panel.left>=0&&l.panel.top>=0&&l.panel.right<=w&&l.panel.bottom<=h);
        const auto record=ScriptPanelLayout::Build(w,h,true);
        assert(record.none.right<=record.panel.right&&record.none.left>record.benchmark.right);
        assert(record.none.Contains((record.none.left+record.none.right)/2,(record.none.top+record.none.bottom)/2));
        assert(record.livestream.right<record.regression.left&&record.regression.right<record.benchmark.left);
        const auto gap1=record.regression.left-record.livestream.left;
        const auto gap2=record.benchmark.left-record.regression.left;
        const auto gap3=record.none.left-record.benchmark.left;
        assert(std::abs(gap1-gap2)<.001f&&std::abs(gap2-gap3)<.001f);
        for(auto pair:{std::pair{record.livestream,l.livestream},std::pair{record.regression,l.regression},std::pair{record.benchmark,l.benchmark},std::pair{record.none,l.none}}){
            assert(pair.first.left==pair.second.left&&pair.first.right==pair.second.right&&pair.first.top==pair.second.top&&pair.first.bottom==pair.second.bottom);
        }
        assert(!l.action.Contains(l.snapshot.left,l.snapshot.top));assert(!l.recordTab.Contains(l.runTab.left,l.runTab.top));
    }
    std::filesystem::remove_all(directory);std::cout<<"Snapshot camera script tests passed\n";
}

