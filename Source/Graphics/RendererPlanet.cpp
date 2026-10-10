#include "Renderer.h"
#include "DdsSurface.h"
#include <array>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <sstream>
using namespace DirectX;
using Microsoft::WRL::ComPtr;
namespace OrbitX::Graphics {
namespace {
constexpr double Pi=3.14159265358979323846;
constexpr UINT64 Budget=512ull*1024*1024;
D3D12_HEAP_PROPERTIES PlanetHeap(D3D12_HEAP_TYPE type){D3D12_HEAP_PROPERTIES h{};h.Type=type;return h;}
D3D12_RESOURCE_DESC PlanetBuffer(UINT64 size){D3D12_RESOURCE_DESC d{};d.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;d.Width=size;d.Height=1;d.DepthOrArraySize=1;d.MipLevels=1;d.SampleDesc.Count=1;d.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;return d;}
constexpr std::array<UINT,8> Palette={0xFF3030,0x00E5FF,0xFFE600,0xFF39DE,0x32FF57,0xFF8A00,0x4285FF,0xFFFFFF};
}
bool Renderer::LoadPlanetTile(Core::TileId id) {
    try {
        auto d=DdsSurface::Read(m_planet.files.at(id));
        D3D12_RESOURCE_DESC desc{};desc.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        desc.Width=d.width;desc.Height=d.height;desc.DepthOrArraySize=1;desc.MipLevels=static_cast<UINT16>(d.mips);
        desc.Format=static_cast<DXGI_FORMAT>(d.format);desc.SampleDesc.Count=1;
        auto allocation=m_device->GetResourceAllocationInfo(0,1,&desc).SizeInBytes;
        if(allocation==UINT64_MAX || allocation>Budget) throw std::runtime_error("DDS exceeds tile cache budget");
        // Protect every texture currently referenced by the draw set (also pins frozen selection).
        std::set<UINT> pinned;for(const auto& draw:m_tileDraws)pinned.insert(draw.slot);
        while(m_tileBytes+allocation>Budget || m_tiles.size()>=TileCapacity) {
            auto oldest=m_tiles.end();
            for(auto it=m_tiles.begin();it!=m_tiles.end();++it)
                if(it->first.level!=4 && !pinned.contains(it->second.slot) && (oldest==m_tiles.end() || it->second.touched<oldest->second.touched)) oldest=it;
            if(oldest==m_tiles.end()) return false;
            m_tileBytes-=oldest->second.bytes;m_tiles.erase(oldest);
        }
        std::set<UINT> used;for(const auto& [key,tile]:m_tiles)used.insert(tile.slot);
        UINT slot=1;while(used.contains(slot))++slot;
        auto heap=PlanetHeap(D3D12_HEAP_TYPE_DEFAULT);ComPtr<ID3D12Resource> texture;
        if(FAILED(m_device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&desc,D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&texture)))) throw std::runtime_error("DDS GPU allocation failed");
        std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> footprints(d.mips);
        std::vector<UINT> rows(d.mips);std::vector<UINT64> rowSizes(d.mips);UINT64 total=0;
        m_device->GetCopyableFootprints(&desc,0,d.mips,0,footprints.data(),rows.data(),rowSizes.data(),&total);
        heap=PlanetHeap(D3D12_HEAP_TYPE_UPLOAD);auto buffer=PlanetBuffer(total);ComPtr<ID3D12Resource> upload;
        if(FAILED(m_device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&buffer,D3D12_RESOURCE_STATE_GENERIC_READ,nullptr,IID_PPV_ARGS(&upload))))throw std::runtime_error("DDS upload allocation failed");
        void* mapped=nullptr;if(FAILED(upload->Map(0,nullptr,&mapped)))throw std::runtime_error("DDS upload map failed");
        size_t source=0;
        for(UINT mip=0;mip<d.mips;mip++)for(UINT row=0;row<rows[mip];row++) {
            std::memcpy(static_cast<UINT8*>(mapped)+footprints[mip].Offset+size_t(row)*footprints[mip].Footprint.RowPitch,d.bytes.data()+source,static_cast<size_t>(rowSizes[mip]));
            source+=static_cast<size_t>(rowSizes[mip]);
        }
        upload->Unmap(0,nullptr);
        if(FAILED(m_alloc[m_frame]->Reset()) || FAILED(m_cmd->Reset(m_alloc[m_frame].Get(),nullptr)))throw std::runtime_error("DDS command reset failed");
        for(UINT mip=0;mip<d.mips;mip++) {
            D3D12_TEXTURE_COPY_LOCATION dest{};dest.pResource=texture.Get();dest.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;dest.SubresourceIndex=mip;
            D3D12_TEXTURE_COPY_LOCATION src{};src.pResource=upload.Get();src.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;src.PlacedFootprint=footprints[mip];
            m_cmd->CopyTextureRegion(&dest,0,0,0,&src,nullptr);
        }
        D3D12_RESOURCE_BARRIER barrier{};barrier.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;barrier.Transition.pResource=texture.Get();barrier.Transition.StateBefore=D3D12_RESOURCE_STATE_COPY_DEST;barrier.Transition.StateAfter=D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;barrier.Transition.Subresource=D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        m_cmd->ResourceBarrier(1,&barrier);if(FAILED(m_cmd->Close()))throw std::runtime_error("DDS command close failed");
        ID3D12CommandList* lists[]={m_cmd.Get()};m_queue->ExecuteCommandLists(1,lists);WaitForGPU();
        D3D12_SHADER_RESOURCE_VIEW_DESC srv{};srv.Format=desc.Format;srv.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;srv.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;srv.Texture2D.MipLevels=d.mips;
        auto handle=m_srvHeap->GetCPUDescriptorHandleForHeapStart();handle.ptr+=size_t(slot)*m_srvStride;m_device->CreateShaderResourceView(texture.Get(),&srv,handle);
        m_tiles.emplace(id,TileResource{texture,slot,allocation,m_tileClock});m_tileBytes+=allocation;return true;
    } catch(const std::exception& e) {
        m_failedTiles.insert(id);SetError("DDS "+m_planet.files.at(id).string()+": "+e.what());
        m_tileStatus=L"DDS load failures: see Logs/OrbitX.log";return false;
    }
}
bool Renderer::UpdatePlanet() {
    ++m_tileClock;
    auto oldLeaves=m_planet.leaves;
    const double cx=m_distanceKm*std::sin(m_yaw)*std::cos(m_pitch),cy=-m_distanceKm*std::sin(m_pitch),cz=-m_distanceKm*std::cos(m_yaw)*std::cos(m_pitch);
    m_planet.Select(cx,cy,cz,1737.4,double(m_height)/(2*std::tan(m_fov*Pi/360)),[&](Core::TileId id){return m_tiles.contains(id)||m_failedTiles.contains(id);});
    // A frozen leaf set stays fixed; recompute outstanding loads for its ancestors only.
    if(m_planet.frozen) {
        std::set<Core::TileId> pending;
        for(auto id:m_planet.leaves)for(;;) {
            if(m_planet.files.contains(id)&&!m_tiles.contains(id)&&!m_failedTiles.contains(id))pending.insert(id);
            if(id.level==4)break;id=id.Parent();
        }
        m_planet.requests.assign(pending.begin(),pending.end());
    }
    bool changed=oldLeaves!=m_planet.leaves || m_planetDirty;
    // Prototype: bounded synchronous uploads. Retire previous GPU work before reusing resources/descriptors.
    if(changed || !m_planet.requests.empty()) WaitForGPU();
    UINT attempted=0;
    for(auto id:m_planet.requests) {
        if(m_tiles.contains(id)||m_failedTiles.contains(id))continue;
        if(LoadPlanetTile(id)) changed=true;
        if(++attempted==4)break;
    }
    m_planet.requests.erase(std::remove_if(m_planet.requests.begin(),m_planet.requests.end(),[&](auto id){return m_tiles.contains(id)||m_failedTiles.contains(id);}),m_planet.requests.end());
    if(changed && !BuildPlanetMesh())return false;
    m_planetDirty=false;m_residentLOD=0;for(const auto& [id,tile]:m_tiles)m_residentLOD=std::max(m_residentLOD,id.level);
    return true;
}
bool Renderer::BuildPlanetMesh() {
    std::vector<Vertex> vertices;std::vector<uint32_t> indices;std::vector<TileDraw> draws;

    m_renderedLOD=0;
    for(auto id:m_planet.leaves) {
        const UINT grid=std::max(16u,128u >> std::min(3,id.level-4));
        TileDraw draw{};draw.id=id;draw.source=id;
        while(!m_tiles.contains(draw.source)&&draw.source.level>4)draw.source=draw.source.Parent();
        auto bounds=Core::Bounds(id);Core::TileBounds sourceBounds{-90,90,-180,180};
        if(auto it=m_tiles.find(draw.source);it!=m_tiles.end()) {
            draw.slot=it->second.slot;it->second.touched=m_tileClock;sourceBounds=Core::Bounds(draw.source);m_renderedLOD=std::max(m_renderedLOD,draw.source.level);
        } else {SetError("Selected patch has no resident DDS ancestor");return false;}
        draw.uv[0]=float((bounds.west-sourceBounds.west)/(sourceBounds.east-sourceBounds.west));
        draw.uv[1]=float((sourceBounds.north-bounds.north)/(sourceBounds.north-sourceBounds.south));
        draw.uv[2]=float((bounds.east-bounds.west)/(sourceBounds.east-sourceBounds.west));
        draw.uv[3]=float((bounds.north-bounds.south)/(sourceBounds.north-sourceBounds.south));
        UINT base=static_cast<UINT>(vertices.size());draw.first=static_cast<UINT>(indices.size());
        for(UINT r=0;r<=grid;r++)for(UINT c=0;c<=grid;c++) {
            float u=float(c)/grid,v=float(r)/grid;
            double lat=(bounds.north-v*(bounds.north-bounds.south))*Pi/180,lon=(bounds.west+u*(bounds.east-bounds.west))*Pi/180;
            XMFLOAT3 n{float(std::cos(lat)*std::cos(lon)),float(std::sin(lat)),float(std::cos(lat)*std::sin(lon))};vertices.push_back({n,n,{u,v}});
        }
        for(UINT r=0;r<grid;r++)for(UINT c=0;c<grid;c++) {auto a=base+r*(grid+1)+c,b=a+1,d=a+grid+2,e=d-1;indices.insert(indices.end(),{a,e,b,b,e,d});}
        // Inward skirts cover cracks between different tessellation levels; same UVs/texture.
        std::vector<UINT> perimeter;
        for(UINT c=0;c<grid;c++)perimeter.push_back(base+c);
        for(UINT r=0;r<grid;r++)perimeter.push_back(base+r*(grid+1)+grid);
        for(UINT c=grid;c>0;c--)perimeter.push_back(base+grid*(grid+1)+c);
        for(UINT r=grid;r>0;r--)perimeter.push_back(base+r*(grid+1));
        const float depth=float(std::max(1e-6,1-std::cos((bounds.north-bounds.south)*Pi/(180*grid)))+1e-5);
        for(size_t i=0;i<perimeter.size();i++) {
            UINT a=perimeter[i],b=perimeter[(i+1)%perimeter.size()],c=static_cast<UINT>(vertices.size());
            auto va=vertices[a],vb=vertices[b];va.pos.x*=1-depth;va.pos.y*=1-depth;va.pos.z*=1-depth;vb.pos.x*=1-depth;vb.pos.y*=1-depth;vb.pos.z*=1-depth;
            vertices.push_back(va);vertices.push_back(vb);indices.insert(indices.end(),{a,b,c,b,c+1,c});
        }
        draw.count=static_cast<UINT>(indices.size())-draw.first;draws.push_back(draw);
    }
    if(vertices.empty()){m_tileDraws.clear();return true;}
    auto upload=[&](const void* data,size_t bytes,ComPtr<ID3D12Resource>& resource) {
        auto heap=PlanetHeap(D3D12_HEAP_TYPE_UPLOAD);auto desc=PlanetBuffer(bytes);
        if(FAILED(m_device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&desc,D3D12_RESOURCE_STATE_GENERIC_READ,nullptr,IID_PPV_ARGS(&resource))))return false;
        void* mapped=nullptr;if(FAILED(resource->Map(0,nullptr,&mapped)))return false;std::memcpy(mapped,data,bytes);resource->Unmap(0,nullptr);return true;
    };
    ComPtr<ID3D12Resource> vb,ib;
    if(!upload(vertices.data(),vertices.size()*sizeof(Vertex),vb)||!upload(indices.data(),indices.size()*sizeof(uint32_t),ib)){SetError("Planet mesh allocation failed");return false;}
    m_planetVB=vb;m_planetIB=ib;m_planetVBV={vb->GetGPUVirtualAddress(),static_cast<UINT>(vertices.size()*sizeof(Vertex)),sizeof(Vertex)};m_planetIBV={ib->GetGPUVirtualAddress(),static_cast<UINT>(indices.size()*sizeof(uint32_t)),DXGI_FORMAT_R32_UINT};m_tileDraws=std::move(draws);return true;
}
void Renderer::DrawPlanet() {
    m_cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_cmd->IASetVertexBuffers(0,1,&m_planetVBV);m_cmd->IASetIndexBuffer(&m_planetIBV);
    for(const auto& draw:m_tileDraws) {
        auto descriptor=m_srvHeap->GetGPUDescriptorHandleForHeapStart();descriptor.ptr+=UINT64(draw.slot)*m_srvStride;
        m_cmd->SetGraphicsRootDescriptorTable(1,descriptor);
        auto color=Palette[(draw.id.level-1)%Palette.size()];
        float constants[12]={draw.uv[0],draw.uv[1],draw.uv[2],draw.uv[3],float((color>>16)&255)/255,float((color>>8)&255)/255,float(color&255)/255,1,m_boundaries?1.f:0.f,m_shading?1.f:0.f,0,0};
        m_cmd->SetGraphicsRoot32BitConstants(2,12,constants,0);m_cmd->DrawIndexedInstanced(draw.count,1,draw.first,0,0);
    }
}
void Renderer::DrawPlanetTelemetry(float x,float y) {
    std::wostringstream text;text<<L"BODY MOON\nRendered texture LOD "<<m_renderedLOD<<L"  Resident LOD "<<m_residentLOD
        <<L"\nRendered leaves "<<m_tileDraws.size()<<L"  Resident DDS "<<m_tiles.size()<<L"\nIndexed DDS "<<m_planet.files.size()<<L"  Rejected "<<m_planet.rejectedFiles<<L"\nPending "<<m_planet.requests.size()<<L"  Failed "<<m_failedTiles.size()
        <<L"\nDDS GPU allocations "<<std::fixed<<std::setprecision(1)<<double(m_tileBytes)/(1024*1024)<<L" / 512 MiB"
        <<L"\nBoundaries "<<(m_boundaries?L"ON":L"OFF")<<L"  Shading "<<(m_shading?L"ON":L"OFF")<<L"\nLOD frozen "<<(m_planet.frozen?L"YES":L"NO")
        <<L"\nCtrl+I edges  Ctrl+O tint  Ctrl+L freeze";
    if(!m_tileStatus.empty())text<<L"\n"<<m_tileStatus;
    if(m_boundaries||m_shading) {
        double lat=std::asin(std::sin(-m_pitch))*180/Pi;
        double lon=std::atan2(-std::cos(m_yaw)*std::cos(m_pitch),std::sin(m_yaw)*std::cos(m_pitch))*180/Pi;
        for(const auto& draw:m_tileDraws) {
            auto b=Core::Bounds(draw.id);
            if(lat>=b.south&&lat<=b.north&&lon>=b.west&&lon<=b.east) {
                text<<L"\nCENTER TILE L"<<draw.id.level<<L" row "<<draw.id.row<<L" col "<<draw.id.column
                    <<L"\nCenter lat "<<(b.south+b.north)*.5<<L" lon "<<(b.west+b.east)*.5
                    <<L"\nLat ["<<b.south<<L", "<<b.north<<L"] Lon ["<<b.west<<L", "<<b.east<<L"]"
                    <<L"\nTexture source L"<<draw.source.level<<L"\n";
                text<<m_planet.files.at(draw.source).wstring();
                break;
            }
        }
        text<<L"\nLOD colors: 1 red, 2 cyan, 3 yellow, 4 magenta"
            <<L"\n5 green, 6 orange, 7 blue, 8 white (repeat)";
    }
    auto value=text.str();m_d2dContext->DrawTextW(value.c_str(),static_cast<UINT32>(value.size()),m_textRight.Get(),D2D1::RectF(std::max(0.f,x-650),y,x,float(m_height)),m_textBrush.Get());
}
}
