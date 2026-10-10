#include "../Core/PlanetQuadtree.h"
#include "../Graphics/DdsSurface.h"
#include <cassert>
#include <iostream>
using namespace OrbitX;
int main() {
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
    Core::PlanetQuadtree tree;tree.Scan(folder);
    assert(tree.files.size()==1 && tree.rejectedFiles==1);
    assert(tree.nodes.contains({4,0,0}) && tree.nodes.contains({5,0,1}));
    tree.Select(0,0,-1800,1737.4,1000,[](auto){return false;});
    assert(!tree.leaves.empty() && !tree.requests.empty());
    auto selected=tree.leaves;tree.frozen=true;tree.Select(0,0,5200,1737.4,1000,[](auto){return true;});assert(tree.leaves==selected);
    tree.frozen=false;tree.Select(0,0,500000,1737.4,1000,[](auto){return true;});assert(tree.leaves.size()==2);
    std::vector<uint8_t> raw(128+8);auto word=[&](size_t offset,uint32_t value){std::memcpy(raw.data()+offset,&value,4);};
    word(0,0x20534444);word(4,124);word(12,4);word(16,4);word(28,1);word(76,32);word(84,0x31545844);
    auto file=folder/"test.dds";auto save=[&](){std::ofstream f(file,std::ios::binary);f.write(reinterpret_cast<const char*>(raw.data()),raw.size());};save();
    auto d=Graphics::DdsSurface::Read(file);assert(d.width==4 && d.height==4 && d.bytes.size()==8 && d.format==71);
    raw.resize(128+16);word(84,0x35545844);save();d=Graphics::DdsSurface::Read(file);assert(d.format==77 && d.blockBytes==16);
    raw.resize(130);save();bool rejected=false;try{Graphics::DdsSurface::Read(file);}catch(const std::runtime_error&){rejected=true;}assert(rejected);
    raw.resize(128+16);word(28,15);save();rejected=false;try{Graphics::DdsSurface::Read(file);}catch(const std::runtime_error&){rejected=true;}assert(rejected);
    std::filesystem::remove_all(folder);std::cout<<"Planet surface tests passed\n";
}
