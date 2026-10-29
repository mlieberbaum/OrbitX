"""Portable command-recording checks of the actual DrawPlanet and PSO methods.

Requires Python 3 and g++ with C++20. This is not a Windows/DX12 shader build.
The method bodies are extracted verbatim so tests cannot drift to a mirrored draw
implementation. GPU interfaces below only record commands and PSO descriptions.
"""
from pathlib import Path
import subprocess
import tempfile

source = Path(__file__).resolve().parents[1]
planet = (source / 'Graphics/RendererPlanet.cpp').read_text()
renderer = (source / 'Graphics/Renderer.cpp').read_text()
draw = planet[planet.index('void Renderer::DrawPlanet() {'):planet.index('void Renderer::DrawPlanetLabels()')]
pipeline = renderer[renderer.index(' D3D12_GRAPHICS_PIPELINE_STATE_DESC d{};'):renderer.index('\nbool Renderer::CreateDepth()')]
# Include the unchanged base PSO setup and every alpha/debug PSO assignment.
pipeline = 'bool Renderer::Pipelines(){ Blob *vs=&vertex,*ps=&pixel,*overlay=&debug; int il[3]{};\n' + pipeline
cpp = r'''
#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <map>
#include <vector>
#include "Core/PlanetTextureLayers.h"
namespace Core=OrbitX::Core;
using UINT=unsigned;using UINT64=uint64_t;
constexpr int TRUE=1,D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST=1,D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE=2;
constexpr int DXGI_FORMAT_R8G8B8A8_UNORM=3,DXGI_FORMAT_D32_FLOAT=4,D3D12_FILL_MODE_SOLID=5,D3D12_CULL_MODE_BACK=6;
constexpr int D3D12_COLOR_WRITE_ENABLE_ALL=15,D3D12_DEPTH_WRITE_MASK_ALL=1,D3D12_DEPTH_WRITE_MASK_ZERO=0;
constexpr int D3D12_COMPARISON_FUNC_LESS=7,D3D12_COMPARISON_FUNC_EQUAL=8;
constexpr int D3D12_BLEND_SRC_ALPHA=9,D3D12_BLEND_INV_SRC_ALPHA=10,D3D12_BLEND_ONE=11,D3D12_BLEND_OP_ADD=12;
#define IID_PPV_ARGS(x) x
#define FAILED(x) ((x)<0)
#define SUCCEEDED(x) ((x)>=0)
struct Pipeline {int id=0;};using ID3D12PipelineState=Pipeline;
template<class T>struct Ptr{T value;T* Get(){return &value;}T* operator->(){return &value;}};
struct Handle{UINT64 ptr=0;};
struct Heap{Handle GetGPUDescriptorHandleForHeapStart(){return {};}};
struct Blend{int RenderTargetWriteMask=0,BlendEnable=0,SrcBlend=0,DestBlend=0,BlendOp=0,SrcBlendAlpha=0,DestBlendAlpha=0,BlendOpAlpha=0;};
struct Bytecode{void* data;size_t size;};
struct D3D12_GRAPHICS_PIPELINE_STATE_DESC {
    void* pRootSignature=nullptr;Bytecode VS{},PS{};struct {void* data=nullptr;unsigned size=0;}InputLayout;
    int PrimitiveTopologyType=0,RTVFormats[1]{},NumRenderTargets=0,DSVFormat=0;
    struct{int Count=0;}SampleDesc;unsigned SampleMask=0;
    struct{int FillMode=0,CullMode=0,FrontCounterClockwise=0,DepthClipEnable=0;}RasterizerState;
    struct{Blend RenderTarget[1];}BlendState;
    struct{int DepthEnable=0,DepthWriteMask=0,DepthFunc=0;}DepthStencilState;
};
struct Blob{char byte=0;void* GetBufferPointer(){return &byte;}size_t GetBufferSize(){return 1;}};
struct Device {
    std::vector<D3D12_GRAPHICS_PIPELINE_STATE_DESC> descriptions;int failAt=0;
    int CreateGraphicsPipelineState(const D3D12_GRAPHICS_PIPELINE_STATE_DESC* d,Ptr<Pipeline>* p){
        descriptions.push_back(*d);p->value.id=static_cast<int>(descriptions.size());
        return p->value.id==failAt?-1:0;
    }
};
struct Call{Pipeline* pipeline;UINT64 slot;std::array<float,16> constants;unsigned count;};
struct Commands {
    Pipeline* pipeline=nullptr;UINT64 slot=0;std::array<float,16> constants{};std::vector<Call> calls;
    void IASetPrimitiveTopology(int){}void IASetVertexBuffers(int,int,const int*){}void IASetIndexBuffer(const int*){}
    void SetPipelineState(Pipeline* p){pipeline=p;}
    void SetGraphicsRootDescriptorTable(int,Handle h){slot=h.ptr;}
    void SetGraphicsRoot32BitConstants(int,unsigned n,const float* v,int){assert(n==16);std::copy(v,v+n,constants.begin());}
    void DrawIndexedInstanced(unsigned count,int,int,int,int){calls.push_back({pipeline,slot,constants,count});}
};
constexpr std::array<UINT,8> Palette={0xff3030,0x00e5ff,0xffe600,0xff39de,0x32ff57,0xff8a00,0x4285ff,0xffffff};
struct Renderer {
    struct TileDraw{Core::TileId id,source;UINT slot=0,grid=0;float uv[4]{};std::vector<Core::PlanetTextureLayer> layers;};
    struct Tile{UINT slot=0;UINT64 lastDrawFence=0;};
    struct Mesh{int vbv=0,ibv=0;unsigned count=123;};
    Ptr<Pipeline> m_scenePSO,m_sceneAlphaPSO,m_sceneOverlayPSO;Ptr<int> m_sceneRoot;
    Blob vertex,pixel,debug;Device* m_device;Commands* m_cmd;Ptr<Heap> m_srvHeap;
    unsigned m_srvStride=1,m_tileOverlayMode=1;UINT64 m_nextFenceValue=99;
    std::map<Core::TileId,Tile>m_tiles;std::vector<TileDraw>m_tileDraws;std::array<Mesh,4>m_patchMeshes;
    void DrawPlanet();bool Pipelines();
};
'''
cpp += draw + '\n' + pipeline + r'''
int main(){
    Device device;Commands commands;Renderer r;r.m_device=&device;r.m_cmd=&commands;
    assert(r.Pipelines());assert(device.descriptions.size()==3);
    const auto& base=device.descriptions[0];const auto& alpha=device.descriptions[1];const auto& debug=device.descriptions[2];
    assert(base.DepthStencilState.DepthFunc==D3D12_COMPARISON_FUNC_LESS&&base.DepthStencilState.DepthWriteMask==D3D12_DEPTH_WRITE_MASK_ALL);
    assert(!base.BlendState.RenderTarget[0].BlendEnable);
    for(const auto* d:{&alpha,&debug}) {
        assert(d->VS.data==base.VS.data&&d->pRootSignature==base.pRootSignature);
        assert(d->DepthStencilState.DepthFunc==D3D12_COMPARISON_FUNC_EQUAL&&d->DepthStencilState.DepthWriteMask==D3D12_DEPTH_WRITE_MASK_ZERO);
        const auto& b=d->BlendState.RenderTarget[0];assert(b.BlendEnable);
        assert(b.SrcBlend==D3D12_BLEND_SRC_ALPHA&&b.DestBlend==D3D12_BLEND_INV_SRC_ALPHA&&b.BlendOp==D3D12_BLEND_OP_ADD);
        assert(b.SrcBlendAlpha==D3D12_BLEND_ONE&&b.DestBlendAlpha==D3D12_BLEND_INV_SRC_ALPHA);
    }
    assert(alpha.PS.data==base.PS.data&&debug.PS.data!=base.PS.data);
    for(int fail=1;fail<=3;++fail){device.descriptions.clear();device.failAt=fail;assert(!r.Pipelines());assert(device.descriptions.size()==static_cast<size_t>(fail));}
    device.failAt=0;device.descriptions.clear();assert(r.Pipelines());
    Renderer::TileDraw draw;draw.id={15,700,2000};
    auto root=draw.id;while(root.level>11)root=root.Parent();
    draw.layers=Core::PlanetTextureLayers(draw.id,[&](Core::TileId id)->std::optional<bool>{return id.level<11?std::nullopt:std::optional<bool>(id.level>11);});
    draw.source=draw.layers.back().source;std::copy(draw.layers.back().uv.begin(),draw.layers.back().uv.end(),draw.uv);
    unsigned slot=1;for(const auto& layer:draw.layers)r.m_tiles[layer.source].slot=slot++;
    r.m_tileDraws={draw};
    for(unsigned mode:{1u,2u,3u}) {
        r.m_tileOverlayMode=mode;commands.calls.clear();r.DrawPlanet();
        assert(commands.calls.size()==draw.layers.size()+(mode>1?1:0));
        for(size_t i=0;i<draw.layers.size();++i){const auto& c=commands.calls[i];
            assert(c.pipeline==(i?r.m_sceneAlphaPSO.Get():r.m_scenePSO.Get()));
            assert(c.slot==r.m_tiles.at(draw.layers[i].source).slot);
            assert(std::equal(c.constants.begin(),c.constants.begin()+4,draw.layers[i].uv.begin()));
            assert(c.constants[8]==0&&c.constants[9]==0);
            assert(c.constants[11]==(i?1.f:0.f));
            for(int j=10;j<16;++j)if(j!=11)assert(c.constants[static_cast<size_t>(j)]==commands.calls[0].constants[static_cast<size_t>(j)]);
            assert(r.m_tiles.at(draw.layers[i].source).lastDrawFence==100);
        }
        if(mode>1){const auto& c=commands.calls.back();assert(c.pipeline==r.m_sceneOverlayPSO.Get());assert(c.constants[8]==1&&c.constants[9]==1);}
        assert(commands.pipeline==r.m_scenePSO.Get());
    }
    // Opaque fast path remains one draw, with original debug tint/borders.
    draw.layers={draw.layers.back()};r.m_tileDraws={draw};commands.calls.clear();r.DrawPlanet();
    assert(commands.calls.size()==1&&commands.calls[0].pipeline==r.m_scenePSO.Get());
    assert(commands.calls[0].constants[8]==1&&commands.calls[0].constants[9]==1);
    std::cout<<"Actual renderer methods: layer ordering, UVs, geometry, fences, debug-once, opaque fast path, PSO state and failure handling passed\n";
}
'''
with tempfile.TemporaryDirectory(prefix='orbitx-alpha-render-') as folder:
    root = Path(folder)
    (root / 'test.cpp').write_text(cpp)
    subprocess.run(['g++', '-std=c++20', '-O1', '-g', '-fsanitize=address,undefined',
                    '-fno-omit-frame-pointer', '-I', str(source), str(root/'test.cpp'),
                    '-o', str(root/'test')], check=True)
    subprocess.run([str(root/'test')], check=True)

# Both shader deployments must stay identical; alpha and the debug pass are
# independently compiled entry points by CreateScenePipeline on Windows.
shader = (source/'Shaders/Moon.hlsl').read_text()
assert shader == (source.parent/'Runtime/Shaders/Moon.hlsl').read_text()
assert 'return float4(color,gDebug.w>0?texel.a:1);' in shader and 'float4 OverlayPSMain(' in shader
print('Source/runtime shader copies match; native HLSL/DX12 compilation remains a Windows check.')
