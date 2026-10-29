#include "../Core/PlanetTextureLayers.h"
#include "../Core/RegressionLog.h"
#include "../Graphics/DdsSurface.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <optional>

using namespace OrbitX;
using Core::TileId;

static std::vector<uint8_t> Header(uint32_t code,uint32_t size,uint32_t mips=1,uint32_t format=0,uint32_t alphaMode=0) {
    const bool dx10=code==0x30315844;
    std::vector<uint8_t> bytes(dx10?148:128);
    auto word=[&](size_t offset,uint32_t value){std::memcpy(bytes.data()+offset,&value,4);};
    word(0,0x20534444);word(4,124);word(12,size);word(16,size);word(28,mips);word(76,32);word(80,4);word(84,code);
    if(dx10){word(128,format);word(132,3);word(140,1);word(144,alphaMode);}
    return bytes;
}

static Graphics::DdsSurface ReadFixture(const std::vector<uint8_t>& bytes,const std::filesystem::path& path) {
    {std::ofstream file(path,std::ios::binary);file.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));}
    return Graphics::DdsSurface::Read(path);
}

int main() {
    const TileId leaf{15,700,2000};
    auto ancestor=[&](int level){auto id=leaf;while(id.level>level)id=id.Parent();return id;};
    std::map<TileId,bool> ready;
    auto lookup=[&](TileId id)->std::optional<bool>{auto it=ready.find(id);return it==ready.end()?std::nullopt:std::optional<bool>(it->second);};
    ready[leaf]=false;
    auto layers=Core::PlanetTextureLayers(leaf,lookup);
    assert(layers.size()==1&&layers[0].source==leaf);
    assert((layers[0].uv==std::array<float,4>{0,0,1,1}));
    ready.clear();ready[ancestor(11)]=false;
    for(int level=12;level<=15;++level)ready[ancestor(level)]=true;
    layers=Core::PlanetTextureLayers(leaf,lookup);
    assert(layers.size()==5);
    for(size_t i=0;i<layers.size();++i) {
        assert(layers[i].source==ancestor(11+static_cast<int>(i)));
        const auto b=Core::Bounds(leaf),s=Core::Bounds(layers[i].source);
        const auto uv=layers[i].uv;
        assert(std::abs(s.west+uv[0]*(s.east-s.west)-b.west)<1e-6);
        assert(std::abs(s.north-uv[1]*(s.north-s.south)-b.north)<1e-6);
        assert(std::abs(uv[2]*(s.east-s.west)-(b.east-b.west))<1e-6);
        assert(std::abs(uv[3]*(s.north-s.south)-(b.north-b.south))<1e-6);
    }
    // Several transparent ancestors reveal the opaque base, not the scene clear.
    auto composite=[&](std::array<double,5> alpha){
        double color=.2;
        for(size_t i=1;i<layers.size();++i)color=(.2+.1*static_cast<double>(i))*alpha[i]+color*(1-alpha[i]);
        return color;
    };
    assert(std::abs(composite({1,0,0,0,0})-.2)<1e-12);
    assert(std::abs(composite({1,1,0,0,0})-.3)<1e-12);
    assert(std::abs(composite({1,0,.5,0,.25})-.375)<1e-12);
    assert(std::abs(composite({1,0,0,0,1})-.6)<1e-12);
    // Missing/failed/not-yet-ready intermediate levels are omitted safely.
    ready.erase(ancestor(13));ready.erase(ancestor(14));
    auto skipped=Core::PlanetTextureLayers(leaf,lookup);
    assert(skipped.size()==3&&skipped[0].source==ancestor(11)&&skipped[1].source==ancestor(12)&&skipped[2].source==leaf);
    Core::RegressionSnapshot before,after;
    before.textureLayers={layers};after.textureLayers={skipped};
    assert(before!=after); // Highest source can stay fixed as an intermediate arrives.
    after.textureLayers=before.textureLayers;assert(before==after);
    ready[ancestor(12)]=false;
    skipped=Core::PlanetTextureLayers(leaf,lookup);
    assert(skipped.size()==2&&skipped.front().source==ancestor(12));
    ready.clear();ready[ancestor(4)]=false;
    assert(Core::PlanetTextureLayers(leaf,lookup).size()==1);
    ready[ancestor(4)]=true;
    assert(Core::PlanetTextureLayers(leaf,lookup).empty());
    // Every layer is a traversal dependency; old draw fences still protect it.
    Core::PlanetQuadtree tree;
    for(const auto& layer:layers)tree.dependencies.insert(layer.source);
    for(const auto& layer:layers)assert(!tree.CanEvict(layer.source,true,7,9,false));
    tree.dependencies.clear();
    for(const auto& layer:layers)assert(!tree.CanEvict(layer.source,true,10,9,false));

    const auto folder=std::filesystem::temp_directory_path()/"orbitx-alpha-tests";
    std::filesystem::create_directories(folder);const auto path=folder/"fixture.dds";
    // BC1 transparency depends on endpoints AND selector, not format alone.
    auto bytes=Header(0x31545844,4);bytes.resize(136);
    assert(!ReadFixture(bytes,path).mayHaveAlpha);
    bytes[132]=3;assert(ReadFixture(bytes,path).mayHaveAlpha);
    bytes[128]=1;assert(!ReadFixture(bytes,path).mayHaveAlpha); // Four-color mode.
    // Alpha in a later mip must participate in classification.
    bytes=Header(0x31545844,4,2);bytes.resize(144);bytes[140]=3;
    assert(ReadFixture(bytes,path).mayHaveAlpha);
    // Invalid padding texels of a 1x1 mip are not coverage holes.
    bytes=Header(0x31545844,1);bytes.resize(136);bytes[132]=12;
    assert(!ReadFixture(bytes,path).mayHaveAlpha);
    // BC3's all-opaque selectors and transparent/fractional selectors.
    bytes=Header(0x35545844,4);bytes.resize(144);bytes[128]=255;
    assert(!ReadFixture(bytes,path).mayHaveAlpha);
    bytes[130]=1;assert(ReadFixture(bytes,path).mayHaveAlpha);
    bytes[130]=2;assert(ReadFixture(bytes,path).mayHaveAlpha);
    bytes[128]=0;bytes[129]=0;for(int i=130;i<136;++i)bytes[static_cast<size_t>(i)]=255;
    assert(!ReadFixture(bytes,path).mayHaveAlpha); // selector 7 = 255 in six-alpha mode.
    // BC2 explicit alpha and BC7 conservatism / declared opaque DX10 headers.
    bytes=Header(0x33545844,4);bytes.resize(144);std::fill(bytes.begin()+128,bytes.begin()+136,255);
    assert(!ReadFixture(bytes,path).mayHaveAlpha);
    bytes[128]=254;assert(ReadFixture(bytes,path).mayHaveAlpha);
    bytes=Header(0x30315844,4,1,98);bytes.resize(164);
    assert(ReadFixture(bytes,path).mayHaveAlpha);
    bytes=Header(0x30315844,4,1,98,3);bytes.resize(164);
    assert(!ReadFixture(bytes,path).mayHaveAlpha);
    bool truncated=false;bytes.pop_back();try{ReadFixture(bytes,path);}catch(const std::runtime_error&){truncated=true;}
    assert(truncated);
    std::filesystem::remove_all(folder);
    std::cout<<"Planet alpha layers, UVs, blending reference, residency and DDS opacity tests passed\n";
}
