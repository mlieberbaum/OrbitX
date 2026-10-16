#include "../Core/RegressionLog.h"
#include <cassert>
#include <iostream>
using namespace OrbitX::Core;
std::string Read(const std::filesystem::path& p){std::ifstream in(p,std::ios::binary);return {std::istreambuf_iterator<char>(in),std::istreambuf_iterator<char>()};}
int main(){
    const auto root=std::filesystem::temp_directory_path()/("orbitx-regression-test-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    CameraScript script;script.width=3840;script.height=2160;
    ScriptCamera a{.7f,4.75f,2100.f,40.f,false},b{1.f,.2f,2500.f,40.f,false};
    script.views={a,b};assert(script.Valid());
    std::vector<RegressionView> views={SnapshotRegressionView(0,a),SnapshotRegressionView(1,b)};
    assert(views[0].timeUs==10000000&&views[1].timeUs==20000000);
    auto other=script;other.views[1].yaw+=.01f;assert(ScriptSignature(script)!=ScriptSignature(other));
    RegressionSnapshot snapshot;snapshot.camera=a;snapshot.leaves={{5,0,1},{5,0,0}};
    snapshot.draws={{{5,0,1},{5,0,1},1,26000,{0,0,1,1}},{{5,0,0},{5,0,0},1,26000,{0,0,1,1}}};
    snapshot.scene[0]=1;snapshot.scene[5]=1;snapshot.scene[32]=.25f;
    RegressionLog log;
    auto write=[&](RegressionSnapshot value,const char* status="READY"){
        assert(log.Start(root,script,views.size(),true));const auto path=log.Path();
        for(size_t i=0;i<views.size();++i){value.camera=views[i].camera;log.View(i,views[i],value,status);}
        log.Stop(script.views.size()*SnapshotHoldUs,"SCRIPT_COMPLETE");assert(log.Error().empty());return Read(path);
    };
    const auto baseline=write(snapshot);
    assert(baseline.find("policy=snapshots cache=ROOT_ONLY time=CANONICAL_HOLD")!=std::string::npos);
    std::reverse(snapshot.leaves.begin(),snapshot.leaves.end());std::reverse(snapshot.draws.begin(),snapshot.draws.end());
    assert(write(snapshot)==baseline); // Different source order and output filename still compare byte-for-byte.
    auto changed=snapshot;changed.draws[0].source={4,0,0};assert(write(changed)!=baseline);
    changed=snapshot;changed.draws[0].uv[2]=.5f;assert(write(changed)!=baseline);
    changed=snapshot;changed.draws[0].grid=2;assert(write(changed)!=baseline);
    changed=snapshot;changed.draws[0].indexCount=42;assert(write(changed)!=baseline);
    changed=snapshot;changed.scene[0]=.99f;assert(write(changed)!=baseline);
    assert(log.Start(root,script,views.size(),true,3));const auto mode3=log.Path();log.Stop(0,"USER_STOP");
    assert(Read(mode3).find("overlay_mode=3")!=std::string::npos);
    const auto failed=write(snapshot,"PENDING");assert(failed.find("RESULT status=INVALID")!=std::string::npos&&failed!=baseline);
    assert(baseline.find("RESULT status=COMPLETE")!=std::string::npos);
    for(const auto* excluded:{"frame=","seq=","slot=","fence=","cpu=","2026-","Livestream_"})assert(baseline.find(excluded)==std::string::npos);
    assert(baseline.find('\r')==std::string::npos);
    assert(log.Start(root,script,views.size(),true));auto missing=log.Path();log.Missing(0,views[0]);log.Stop(1000,"USER_STOP");
    assert(Read(missing).find("RESULT status=INVALID")!=std::string::npos);
    const auto block=root/"file";{std::ofstream out(block);out<<"block";}
    assert(!log.Start(block/"not_a_directory",script,2,true));assert(!log.Active()&&!log.Error().empty());
    assert(log.Start(root,script,2,true));log.Stop(0,"RECOVERED");assert(log.Error().empty());
    std::filesystem::remove_all(root);std::cout<<"Regression log tests passed\n";
}
