#include "../Core/PlanetQuadtree.h"
#include "../Graphics/DdsSurface.h"
#include "../Core/TexturePaths.h"
#include "../Core/DisplayPreferences.h"
#include "../Graphics/DdsTileLoader.h"
#include "../Core/MenuLayout.h"
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
    auto west=Core::Bounds({4,0,0}),east=Core::Bounds({4,0,1});
    assert(west.west==-180 && west.east==0 && east.west==0 && east.east==180);
    assert(west.north==90 && west.south==-90);
    auto child=Core::Bounds({5,1,3});assert(child.west==90 && child.east==180 && child.north==0 && child.south==-90);
    assert((Core::TileId{8,7,9}.Parent()==Core::TileId{7,3,4}));
    auto folder=std::filesystem::temp_directory_path()/"orbitx-surface-tests";
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
    std::filesystem::remove_all(folder);std::cout<<"Planet surface tests passed\n";
}
