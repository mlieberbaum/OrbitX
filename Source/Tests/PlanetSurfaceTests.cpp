#include "../Core/PlanetQuadtree.h"
#include "../Graphics/DdsSurface.h"
#include "../Core/TexturePaths.h"
#include "../Core/DisplayPreferences.h"
#include "../Graphics/DdsTileLoader.h"
#include "../Core/MenuLayout.h"
#include "../Core/PlanetZoom.h"
#include "../Core/FrameRateHistory.h"
#include <cassert>
#include <iostream>
using namespace OrbitX;
int main() {
    auto close=[](double a,double b){return std::abs(a-b)<1e-9;};
    auto x=Core::SurfaceDirection(0,0),y=Core::SurfaceDirection(0,Core::PlanetPi*.5),z=Core::SurfaceDirection(Core::PlanetPi*.5,0);
    assert(close(x.x,1)&&close(y.y,1)&&close(z.z,1));assert(close(Core::Dot(Core::Cross(x,y),z),1));
    auto camera=Core::PlanetCamera::Orbit(Core::PlanetPi*.5,0,5200);
    assert(close(Core::Dot(camera.right,y),1)&&close(Core::Dot(camera.up,z),1));
    for(double pitch=-7;pitch<=7;pitch+=.1){auto c=Core::PlanetCamera::Orbit(.7,pitch,5200);assert(close(Core::Dot(c.up,c.forward),0)&&close(Core::Dot(c.right,c.right),1));}
    assert(camera.SphereInView({1737,0,0},100,.3,1.7));assert(!camera.SphereInView({6000,0,0},100,.3,1.7));assert(!camera.SphereInView({5000,2000,0},10,.3,1.7));
    // Geographic grid winding is outward in the right-handed system.
    auto a=Core::SurfaceDirection(.01,0),b=Core::SurfaceDirection(.01,.01),e=Core::SurfaceDirection(0,0);
    assert(Core::Dot(Core::Cross(e-a,b-a),a)>0);
    // Orbiter external camera truth cases: signed delta, fractional detents, and metre units.
    constexpr double radius=1737.4;
    assert(close(Core::PlanetWheelDistance(2*radius,radius,120),2*radius/1.06));
    assert(close(Core::PlanetWheelDistance(2*radius,radius,-120),2*radius*1.06));
    assert(close(Core::PlanetWheelDistance(2*radius,radius,60),2*radius/1.03));
    assert(close(Core::PlanetWheelDistance(2*radius,radius,240),2*radius/1.12));
    assert(close(Core::PlanetWheelDistance(2*radius,radius,0),2*radius));
    auto farStep=2*radius-Core::PlanetWheelDistance(2*radius,radius,120);
    auto nearStep=radius+.01-Core::PlanetWheelDistance(radius+.01,radius,120);
    assert(nearStep>0&&nearStep<farStep/1000);
    assert(Core::PlanetWheelDistance(radius+.001,radius,120)>=radius);
    assert(close(Core::PlanetWheelDistance(.002,.001,120),.002/1.12)); // 1 metre radius: 1/Size floor.
    Core::FrameRateHistory history;
    for(int i=0;i<1000;i++)history.AddFrame(.001);
    assert(history.Count()>=9&&history.Count()<=10);
    for(size_t i=0;i<history.Count();i++)assert(close(history.At(i).fps,1000));
    for(int i=0;i<20000;i++)history.AddFrame(.001);
    assert(history.Count()==Core::FrameRateHistory::Capacity);
    for(size_t i=1;i<history.Count();i++)assert(history.At(i).time>history.At(i-1).time);
    history.AddFrame(15);assert(history.Count()==1&&history.At(0).fps<1); // hitch expires old samples.
    history.AddFrame(0);history.AddFrame(-1);history.AddFrame(NAN);assert(history.Count()==1);
    history.Clear();assert(history.Count()==0&&history.Time()==0);
    auto west=Core::Bounds({4,0,0}),east=Core::Bounds({4,0,1});
    assert(west.west==-180 && west.east==0 && east.west==0 && east.east==180);
    assert(west.north==90 && west.south==-90);
    auto child=Core::Bounds({5,1,3});assert(child.west==90 && child.east==180 && child.north==0 && child.south==-90);
    assert((Core::TileId{8,7,9}.Parent()==Core::TileId{7,3,4}));
    auto folder=std::filesystem::temp_directory_path()/"orbitx-surface-tests";
    // Regression: keep traversal dependencies resident under polar cache pressure.
    {
    using namespace Core;

 PlanetQuadtree tree;
 for(int l=4;l<=11;++l)for(int r=0;r<(1<<(l-4));++r)for(int c=0;c<(2<<(l-4));++c){TileId id{l,r,c};tree.files[id]="tile.dds";tree.nodes.insert(id);}
 auto camera=PlanetCamera::Orbit(.7101706862449646,4.753438949584961,2046.786865234375);
 auto view=[&](PlanetVector p,double r){return camera.SphereInView(p,r,std::tan(20*PlanetPi/180),3099./1743.);};
 auto all=[](TileId){return true;};tree.Select(camera.position.x,camera.position.y,camera.position.z,1737.4,1743/(2*std::tan(20*PlanetPi/180)),all,view);
 auto stable=tree.leaves;auto deps=tree.dependencies;
 assert(deps.contains({5,0,0})&&deps.contains({5,0,3}));
 // Every active branch's siblings and ancestors survive despite not being drawn.
 for(auto id:stable){assert(deps.contains(id));while(id.level>4){id=id.Parent();assert(deps.contains(id));for(int r=0;r<2;++r)for(int c=0;c<2;++c)assert(deps.contains({id.level+1,id.row*2+r,id.column*2+c}));}}
 for(auto id:deps)assert(!tree.CanEvict(id,true,0,100,false));
 std::set<TileId> cache=deps;TileId obsolete{11,127,255};assert(!deps.contains(obsolete));cache.insert(obsolete);
 assert(tree.CanEvict(obsolete,true,0,100,false));assert(!tree.CanEvict(obsolete,false,0,100,false));assert(!tree.CanEvict(obsolete,true,101,100,false));assert(!tree.CanEvict(obsolete,true,0,100,true));
 // Pressure evicts stale work, never a traversal dependency. All-protected cache defers.
 auto victim=cache.end();for(auto it=cache.begin();it!=cache.end();++it)if(tree.CanEvict(*it,true,0,100,false)){victim=it;break;}
 assert(victim!=cache.end()&&*victim==obsolete);cache.erase(victim);
 for(auto id:cache)assert(!tree.CanEvict(id,true,0,100,false));
 for(int f=0;f<100;++f){tree.Select(camera.position.x,camera.position.y,camera.position.z,1737.4,1743/(2*std::tan(20*PlanetPi/180)),[&](TileId id){return cache.contains(id);},view);assert(tree.leaves==stable&&tree.requests.empty());}
 // Moving the camera releases old dependencies, including the problematic level-5 tile.
 auto pos=PlanetCamera::Orbit(.71,0,1740.);auto smallView=[&](PlanetVector p,double r){return pos.SphereInView(p,r,std::tan(20*PlanetPi/180),3099./1743.);};
 tree.Select(pos.position.x,pos.position.y,pos.position.z,1737.4,1743/(2*std::tan(20*PlanetPi/180)),all,smallView);
 size_t released=0;for(auto id:deps)if(tree.CanEvict(id,true,0,100,false))++released;assert(released>0);
 tree.frozen=true;auto frozen=tree.dependencies;tree.Select(0,0,5000,1737.4,100,all);assert(tree.dependencies==frozen);

    }
    std::filesystem::remove_all(folder);
    std::filesystem::create_directories(folder/"06"/"000001");
    std::ofstream(folder/"06"/"000001"/"000002.dds").put('x');
    std::ofstream(folder/"06"/"000001"/"000099.dds").put('x');
    auto runtime=folder/"runtime",textureRoot=folder/"textures";
    std::filesystem::create_directories(runtime/"Config"/"Bodies");
    std::filesystem::create_directories(textureRoot/"Moon"/"Surf");
    auto cfg=runtime/"Config"/"Bodies"/"Moon.cfg";
    std::filesystem::path resolved;std::string error;
    std::ofstream(cfg)<<"SurfaceTiles = Moon/Surf\n";
    assert(Core::LoadBodySurfacePath(runtime,textureRoot,"Moon",resolved,error));
    assert(resolved==textureRoot/"Moon"/"Surf");
    std::ofstream(cfg)<<"SurfaceTiles = "<<folder.string()<<"\n";
    assert(Core::LoadBodySurfacePath(runtime,textureRoot,"Moon",resolved,error));assert(resolved==folder);
    std::ofstream(cfg)<<"Name = Moon\n";
    assert(Core::LoadBodySurfacePath(runtime,textureRoot,"Moon",resolved,error));assert(resolved==textureRoot/"Moon"/"Surf");
    std::ofstream(cfg)<<"SurfaceTiles = nonexistent\n";
    assert(!Core::LoadBodySurfacePath(runtime,textureRoot,"Moon",resolved,error));
    std::ofstream(cfg)<<"SurfaceTiles =   \n";
    assert(!Core::LoadBodySurfacePath(runtime,textureRoot,"Moon",resolved,error));
    // No Moon.jpg is created or required by any surface configuration test.
    Core::PlanetQuadtree tree;tree.Scan(folder);
    assert(tree.files.size()==1 && tree.rejectedFiles==1);
    assert(tree.nodes.contains({4,0,0}) && tree.nodes.contains({5,0,1}));
    tree.Select(0,-1800,0,1737.4,1000,[](auto){return false;});
    assert(!tree.leaves.empty() && !tree.requests.empty());
    for(auto leaf:tree.leaves)assert(leaf.level<6); // parent remains while the child DDS is unavailable.
    tree.Select(0,-1800,0,1737.4,1000,[](auto){return true;});
    assert(std::find(tree.leaves.begin(),tree.leaves.end(),Core::TileId{6,1,2})!=tree.leaves.end());
    auto selected=tree.leaves;tree.frozen=true;tree.Select(0,0,5200,1737.4,1000,[](auto){return true;});assert(tree.leaves==selected);
    tree.frozen=false;tree.Select(0,0,500000,1737.4,1000,[](auto){return true;});assert(tree.leaves.size()==2);
    std::vector<uint8_t> raw(128+8);auto word=[&](size_t offset,uint32_t value){std::memcpy(raw.data()+offset,&value,4);};
    word(0,0x20534444);word(4,124);word(12,4);word(16,4);word(28,1);word(76,32);word(84,0x31545844);
    auto file=folder/"test.dds";auto save=[&](){std::ofstream f(file,std::ios::binary);f.write(reinterpret_cast<const char*>(raw.data()),raw.size());};save();
    auto d=Graphics::DdsSurface::Read(file);assert(d.width==4 && d.height==4 && d.bytes.size()==8 && d.format==71);
    raw.resize(128+16);word(84,0x35545844);save();d=Graphics::DdsSurface::Read(file);assert(d.format==77 && d.blockBytes==16);
    raw.resize(130);save();bool rejected=false;try{Graphics::DdsSurface::Read(file);}catch(const std::runtime_error&){rejected=true;}assert(rejected);
    raw.resize(128+16);word(28,15);save();rejected=false;try{Graphics::DdsSurface::Read(file);}catch(const std::runtime_error&){rejected=true;}assert(rejected);
    // Background loader returns validated payloads and discards obsolete work without blocking TryPop.
    raw.resize(136);word(28,1);word(84,0x31545844);save();
    Graphics::DdsTileLoader loader;loader.SetRequests({{{4,0,0},file},{{4,0,1},file}});loader.Start();
    auto receive=[&](){std::optional<Graphics::DdsTileLoader::Result> result;auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(3);
        while(!(result=loader.TryPop())&&std::chrono::steady_clock::now()<deadline){std::this_thread::sleep_for(std::chrono::milliseconds(1));}
        assert(result);return std::move(*result);};
    auto first=receive(),second=receive();assert((first.id==Core::TileId{4,0,0})&&(second.id==Core::TileId{4,0,1}));assert(first.error.empty()&&first.surface.bytes.size()==8);
    loader.SetRequests({{{5,0,0},folder/"missing.dds"}});auto failure=receive();assert(!failure.error.empty());
    loader.SetRequests({});assert(!loader.TryPop());loader.Stop();
    auto prefsPath=runtime/"Config"/"UserPreferences.cfg";
    auto prefs=Core::LoadDisplayPreferences(prefsPath);assert(prefs.fullscreen&&!prefs.vsync);
    std::ofstream(prefsPath)<<"DisplayMode=Fullscreen\r\nVSync=Off\r\nOther=keep\r\n";
    assert(Core::SaveDisplayPreferences(prefsPath,{false,true}));prefs=Core::LoadDisplayPreferences(prefsPath);assert(!prefs.fullscreen&&prefs.vsync);
    std::ifstream prefsFile(prefsPath);std::string saved((std::istreambuf_iterator<char>(prefsFile)),{});assert(saved.find("Other=keep")!=std::string::npos);prefsFile.close();
    auto menu=UI::BuildSubmenuLayout(1920,1080);assert(UI::HitTestButton(menu.graphicsVsyncOff,18,150,760,menu.scale,menu.offsetX,menu.offsetY));assert(!menu.graphicsFullscreen.Contains(150,760));
    // Regression: keep traversal dependencies resident under polar cache pressure.
    {
    using namespace Core;

 PlanetQuadtree tree;
 for(int l=4;l<=11;++l)for(int r=0;r<(1<<(l-4));++r)for(int c=0;c<(2<<(l-4));++c){TileId id{l,r,c};tree.files[id]="tile.dds";tree.nodes.insert(id);}
 auto camera=PlanetCamera::Orbit(.7101706862449646,4.753438949584961,2046.786865234375);
 auto view=[&](PlanetVector p,double r){return camera.SphereInView(p,r,std::tan(20*PlanetPi/180),3099./1743.);};
 auto all=[](TileId){return true;};tree.Select(camera.position.x,camera.position.y,camera.position.z,1737.4,1743/(2*std::tan(20*PlanetPi/180)),all,view);
 auto stable=tree.leaves;auto deps=tree.dependencies;
 assert(deps.contains({5,0,0})&&deps.contains({5,0,3}));
 // Every active branch's siblings and ancestors survive despite not being drawn.
 for(auto id:stable){assert(deps.contains(id));while(id.level>4){id=id.Parent();assert(deps.contains(id));for(int r=0;r<2;++r)for(int c=0;c<2;++c)assert(deps.contains({id.level+1,id.row*2+r,id.column*2+c}));}}
 for(auto id:deps)assert(!tree.CanEvict(id,true,0,100,false));
 std::set<TileId> cache=deps;TileId obsolete{11,127,255};assert(!deps.contains(obsolete));cache.insert(obsolete);
 assert(tree.CanEvict(obsolete,true,0,100,false));assert(!tree.CanEvict(obsolete,false,0,100,false));assert(!tree.CanEvict(obsolete,true,101,100,false));assert(!tree.CanEvict(obsolete,true,0,100,true));
 // Pressure evicts stale work, never a traversal dependency. All-protected cache defers.
 auto victim=cache.end();for(auto it=cache.begin();it!=cache.end();++it)if(tree.CanEvict(*it,true,0,100,false)){victim=it;break;}
 assert(victim!=cache.end()&&*victim==obsolete);cache.erase(victim);
 for(auto id:cache)assert(!tree.CanEvict(id,true,0,100,false));
 for(int f=0;f<100;++f){tree.Select(camera.position.x,camera.position.y,camera.position.z,1737.4,1743/(2*std::tan(20*PlanetPi/180)),[&](TileId id){return cache.contains(id);},view);assert(tree.leaves==stable&&tree.requests.empty());}
 // Moving the camera releases old dependencies, including the problematic level-5 tile.
 auto pos=PlanetCamera::Orbit(.71,0,1740.);auto smallView=[&](PlanetVector p,double r){return pos.SphereInView(p,r,std::tan(20*PlanetPi/180),3099./1743.);};
 tree.Select(pos.position.x,pos.position.y,pos.position.z,1737.4,1743/(2*std::tan(20*PlanetPi/180)),all,smallView);
 size_t released=0;for(auto id:deps)if(tree.CanEvict(id,true,0,100,false))++released;assert(released>0);
 tree.frozen=true;auto frozen=tree.dependencies;tree.Select(0,0,5000,1737.4,100,all);assert(tree.dependencies==frozen);

    }
    std::filesystem::remove_all(folder);std::cout<<"Planet surface tests passed\n";
}
