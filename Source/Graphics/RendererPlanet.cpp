#include "Renderer.h"
#include <array>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <sstream>
using namespace DirectX;
using Microsoft::WRL::ComPtr;
namespace OrbitX::Graphics {
namespace {
constexpr UINT64 Budget=1024ull*1024*1024;
D3D12_HEAP_PROPERTIES PlanetHeap(D3D12_HEAP_TYPE type){D3D12_HEAP_PROPERTIES h{};h.Type=type;return h;}
D3D12_RESOURCE_DESC PlanetBuffer(UINT64 size){D3D12_RESOURCE_DESC d{};d.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;d.Width=size;d.Height=1;d.DepthOrArraySize=1;d.MipLevels=1;d.SampleDesc.Count=1;d.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;return d;}
constexpr std::array<UINT,8> Palette={0xFF3030,0x00E5FF,0xFFE600,0xFF39DE,0x32FF57,0xFF8A00,0x4285FF,0xFFFFFF};
double Milliseconds(std::chrono::steady_clock::time_point start){return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();}
}
bool Renderer::InitPlanetStreaming() {
    D3D12_COMMAND_QUEUE_DESC queue{};
    if(FAILED(m_device->CreateCommandQueue(&queue,IID_PPV_ARGS(&m_uploadQueue))) || FAILED(m_device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&m_uploadFence))))return false;
    for(auto& batch:m_uploadBatches) {
        if(FAILED(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&batch.allocator))) || FAILED(m_device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,batch.allocator.Get(),nullptr,IID_PPV_ARGS(&batch.commands))))return false;
        if(FAILED(batch.commands->Close()))return false;
    }
    D3D12_QUERY_HEAP_DESC query{};query.Type=D3D12_QUERY_HEAP_TYPE_TIMESTAMP;query.Count=FrameCount*2;
    auto heap=PlanetHeap(D3D12_HEAP_TYPE_READBACK);auto buffer=PlanetBuffer(FrameCount*2*sizeof(UINT64));
    if(FAILED(m_device->CreateQueryHeap(&query,IID_PPV_ARGS(&m_timestampHeap))) || FAILED(m_device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&buffer,D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&m_timestampReadback))) || FAILED(m_queue->GetTimestampFrequency(&m_timestampFrequency)))return false;
    return BuildPlanetMesh();
}
bool Renderer::QueuePlanetTile(Core::TileId id,const DdsSurface& data,UploadBatch& batch) {
    D3D12_RESOURCE_DESC desc{};desc.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Width=data.width;desc.Height=data.height;desc.DepthOrArraySize=1;desc.MipLevels=static_cast<UINT16>(data.mips);desc.Format=static_cast<DXGI_FORMAT>(data.format);desc.SampleDesc.Count=1;
    auto allocation=m_device->GetResourceAllocationInfo(0,1,&desc).SizeInBytes;
    if(allocation==UINT64_MAX || allocation>Budget){m_failedTiles.insert(id);SetError("DDS allocation exceeds cache budget: "+m_planet.files.at(id).string());return false;}
    std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> footprints(data.mips);
    std::vector<UINT> rows(data.mips);std::vector<UINT64> rowSizes(data.mips);UINT64 total=0;
    m_device->GetCopyableFootprints(&desc,0,data.mips,0,footprints.data(),rows.data(),rowSizes.data(),&total);
    total=(total+511)&~UINT64(511);
    if(batch.used+total>batch.capacity) {
        if(batch.used){m_trace.Log([&](auto& out){out<<"UPLOAD_DEFER tile="<<id.level<<"/"<<id.row<<"/"<<id.column<<" reason=PACKET_FULL";});return false;} // Submit this packet first; retry the held result next frame.
        if(batch.arena)batch.arena->Unmap(0,nullptr);
        batch.arena.Reset();batch.mapped=nullptr;batch.capacity=0;
        auto heap=PlanetHeap(D3D12_HEAP_TYPE_UPLOAD);auto buffer=PlanetBuffer(std::max<UINT64>(16ull*1024*1024,total));
        if(FAILED(m_device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&buffer,D3D12_RESOURCE_STATE_GENERIC_READ,nullptr,IID_PPV_ARGS(&batch.arena))) || FAILED(batch.arena->Map(0,nullptr,reinterpret_cast<void**>(&batch.mapped)))){
            m_failedTiles.insert(id);SetError("DDS upload arena allocation failed");return false;
        }
        batch.capacity=buffer.Width;
    }
    // Never recycle a descriptor/resource still referenced by either queue.
    std::set<UINT> pinned;for(const auto& draw:m_tileDraws)pinned.insert(draw.slot);
    auto completed=m_fence->GetCompletedValue();
    while(m_tileBytes+allocation>Budget || m_tiles.size()>=TileCapacity) {
        auto oldest=m_tiles.end();
        for(auto it=m_tiles.begin();it!=m_tiles.end();++it){
            m_trace.Log([&](auto& out){out<<"EVICTION_QUERY tile="<<it->first.level<<"/"<<it->first.row<<"/"<<it->first.column
                <<" ready="<<it->second.ready<<" root="<<(it->first.level==4)<<" pinned="<<pinned.contains(it->second.slot)
                <<" touched="<<it->second.touched<<" last_draw_fence="<<it->second.lastDrawFence<<" draw_fence_completed="<<completed;});
            if(it->first.level!=4 && it->second.ready && it->second.lastDrawFence<=completed && !pinned.contains(it->second.slot)
               && (oldest==m_tiles.end() || it->second.touched<oldest->second.touched))oldest=it;
        }
        if(oldest==m_tiles.end()){
            m_trace.Log([&](auto& out){out<<"UPLOAD_DEFER tile="<<id.level<<"/"<<id.row<<"/"<<id.column<<" reason=NO_SAFE_EVICTION"
                <<" cache_bytes="<<m_tileBytes<<" incoming_bytes="<<allocation<<" entries="<<m_tiles.size()<<" draw_fence_completed="<<completed;});return false;
        }
        m_trace.Log([&](auto& out){out<<"TILE_EVICT tile="<<oldest->first.level<<"/"<<oldest->first.row<<"/"<<oldest->first.column
            <<" reason="<<(m_tileBytes+allocation>Budget?"CACHE_BYTES":"DESCRIPTOR_CAPACITY")<<" slot="<<oldest->second.slot
            <<" bytes="<<oldest->second.bytes<<" touched="<<oldest->second.touched<<" last_draw_fence="<<oldest->second.lastDrawFence
            <<" draw_fence_completed="<<completed<<" upload_fence="<<oldest->second.uploadFence<<" pinned=0";});
        m_tileBytes-=oldest->second.bytes;m_tiles.erase(oldest);
    }
    std::set<UINT> used;for(const auto& [key,tile]:m_tiles)used.insert(tile.slot);
    UINT slot=1;while(used.contains(slot))++slot;
    auto heap=PlanetHeap(D3D12_HEAP_TYPE_DEFAULT);ComPtr<ID3D12Resource> texture;
    if(FAILED(m_device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&desc,D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&texture)))){m_failedTiles.insert(id);SetError("DDS GPU allocation failed");return false;}
    size_t source=0;
    for(UINT mip=0;mip<data.mips;mip++) {
        footprints[mip].Offset+=batch.used;
        for(UINT row=0;row<rows[mip];row++) {
            std::memcpy(batch.mapped+footprints[mip].Offset+size_t(row)*footprints[mip].Footprint.RowPitch,data.bytes.data()+source,static_cast<size_t>(rowSizes[mip]));source+=static_cast<size_t>(rowSizes[mip]);
        }
        D3D12_TEXTURE_COPY_LOCATION dest{};dest.pResource=texture.Get();dest.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;dest.SubresourceIndex=mip;
        D3D12_TEXTURE_COPY_LOCATION src{};src.pResource=batch.arena.Get();src.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;src.PlacedFootprint=footprints[mip];
        batch.commands->CopyTextureRegion(&dest,0,0,0,&src,nullptr);
    }
    D3D12_RESOURCE_BARRIER barrier{};barrier.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;barrier.Transition.pResource=texture.Get();barrier.Transition.StateBefore=D3D12_RESOURCE_STATE_COPY_DEST;barrier.Transition.StateAfter=D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;barrier.Transition.Subresource=D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;batch.commands->ResourceBarrier(1,&barrier);
    D3D12_SHADER_RESOURCE_VIEW_DESC srv{};srv.Format=desc.Format;srv.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;srv.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;srv.Texture2D.MipLevels=data.mips;
    auto handle=m_srvHeap->GetCPUDescriptorHandleForHeapStart();handle.ptr+=size_t(slot)*m_srvStride;m_device->CreateShaderResourceView(texture.Get(),&srv,handle);
    m_tiles.emplace(id,TileResource{texture,slot,allocation,m_tileClock,0,0,false});m_tileBytes+=allocation;
    m_trace.Log([&](auto& out){out<<"TILE_UPLOAD_PREPARED tile="<<id.level<<"/"<<id.row<<"/"<<id.column<<" slot="<<slot
        <<" bytes="<<allocation<<" cache_bytes="<<m_tileBytes<<" width="<<data.width<<" height="<<data.height
        <<" mips="<<data.mips<<" format="<<data.format<<" batch_offset="<<batch.used<<" upload_bytes="<<total;});
    batch.tiles.push_back(id);batch.used+=total;return true;
}
bool Renderer::SubmitPlanetUploads(UploadBatch& batch) {
    if(FAILED(batch.commands->Close()))return false;
    if(batch.tiles.empty())return true;
    ID3D12CommandList* commands[]={batch.commands.Get()};m_uploadQueue->ExecuteCommandLists(1,commands);
    batch.fence=++m_uploadFenceValue;
    if(FAILED(m_uploadQueue->Signal(m_uploadFence.Get(),batch.fence)))return false;
    for(auto id:batch.tiles){m_tiles.at(id).uploadFence=batch.fence;
        m_trace.Log([&](auto& out){out<<"TILE_UPLOAD_SUBMIT tile="<<id.level<<"/"<<id.row<<"/"<<id.column<<" upload_fence="<<batch.fence;});
    }
    return true;
}
void Renderer::CollectPlanetUploads() {
    auto completed=m_uploadFence->GetCompletedValue();
    for(auto& [id,tile]:m_tiles)if(!tile.ready && tile.uploadFence && tile.uploadFence<=completed){tile.ready=true;
        m_trace.Log([&](auto& out){out<<"TILE_ACTIVATE tile="<<id.level<<"/"<<id.row<<"/"<<id.column<<" slot="<<tile.slot
            <<" upload_fence="<<tile.uploadFence<<" upload_fence_completed="<<completed;});
    }
}
bool Renderer::LoadPlanetTile(Core::TileId id) {
    // The only blocking path is startup of the two required DDS roots.
    try {
        auto data=DdsSurface::Read(m_planet.files.at(id));auto& batch=m_uploadBatches[0];batch.tiles.clear();batch.used=0;
        if(FAILED(batch.allocator->Reset()) || FAILED(batch.commands->Reset(batch.allocator.Get(),nullptr)))return false;
        if(!QueuePlanetTile(id,data,batch)||!SubmitPlanetUploads(batch))return false;
        if(FAILED(m_uploadFence->SetEventOnCompletion(batch.fence,m_fenceEvent)))return false;
        if(WaitForSingleObject(m_fenceEvent,INFINITE)!=WAIT_OBJECT_0)return false;
        CollectPlanetUploads();return m_tiles.at(id).ready;
    } catch(const std::exception& e){SetError("DDS "+m_planet.files.at(id).string()+": "+e.what());return false;}
}
bool Renderer::UpdatePlanet() {
    ++m_tileClock;CollectPlanetUploads();
    auto start=std::chrono::steady_clock::now();
    auto camera=Core::PlanetCamera::Orbit(m_yaw,m_pitch,m_distanceKm);
    const double tanY=std::tan(m_fov*Core::PlanetPi/360),aspect=double(m_width)/m_height;
    auto resident=[&](Core::TileId id){auto it=m_tiles.find(id);return (it!=m_tiles.end()&&it->second.ready)||m_failedTiles.contains(id);};
    std::vector<Core::TileId> previousLeaves;
    if(m_trace.Active())previousLeaves=m_planet.leaves;
    m_planet.Select(camera.position.x,camera.position.y,camera.position.z,1737.4,double(m_height)/(2*tanY),resident,
                    [&](Core::PlanetVector center,double radius){return camera.SphereInView(center,radius,tanY,aspect);},m_trace.Active()?&m_trace:nullptr);
    if(m_trace.Active())for(auto old:previousLeaves)if(std::find(m_planet.leaves.begin(),m_planet.leaves.end(),old)==m_planet.leaves.end()){
        m_trace.Log([&](auto& out){out<<"LEAF_REMOVE tile="<<old.level<<"/"<<old.row<<"/"<<old.column<<" reason=SELECTION_CHANGED";});
        auto parent=old;
        while(parent.level>4){parent=parent.Parent();if(std::find(m_planet.leaves.begin(),m_planet.leaves.end(),parent)!=m_planet.leaves.end()){
            m_trace.Log([&](auto& out){out<<"TILE_COARSEN old="<<old.level<<"/"<<old.row<<"/"<<old.column
                <<" parent="<<parent.level<<"/"<<parent.row<<"/"<<parent.column;});break;
        }}
    }
    if(m_planet.frozen) {
        std::set<Core::TileId> pending;
        for(auto id:m_planet.leaves)for(;;){if(m_planet.files.contains(id)&&!resident(id))pending.insert(id);if(id.level==4)break;id=id.Parent();}
        m_planet.requests.assign(pending.begin(),pending.end());
    }
    if(m_deferredTile && std::find(m_planet.requests.begin(),m_planet.requests.end(),m_deferredTile->id)==m_planet.requests.end()){
        m_trace.Log([&](auto& out){const auto id=m_deferredTile->id;out<<"DEFERRED_DISCARD tile="<<id.level<<"/"<<id.row<<"/"<<id.column<<" reason=NO_LONGER_REQUESTED";});m_deferredTile.reset();
    }
    std::vector<DdsTileLoader::Request> requests;
    for(auto id:m_planet.requests)if(!m_tiles.contains(id)&&(!m_deferredTile||m_deferredTile->id!=id))requests.push_back({id,m_planet.files.at(id)});
    // Coarse coverage first; within a level, favor the screen center.
    std::stable_sort(requests.begin(),requests.end(),[&](const auto& a,const auto& b){
        if(a.id.level!=b.id.level)return a.id.level<b.id.level;
        auto score=[&](auto id){auto bounds=Core::Bounds(id);auto normal=Core::SurfaceDirection((bounds.north+bounds.south)*Core::PlanetPi/360,(bounds.east+bounds.west)*Core::PlanetPi/360);return Core::Dot(normal,camera.position);};
        return score(a.id)>score(b.id);
    });
    m_tileLoader.SetRequests(requests);
    m_selectMs=m_selectMs*.9+Milliseconds(start)*.1;
    start=std::chrono::steady_clock::now();
    // Three independent packets permit CPU/GPU overlap. No upload queue Wait on the draw path.
    for(auto& batch:m_uploadBatches)if(batch.fence<=m_uploadFence->GetCompletedValue()) {
        m_trace.Log([&](auto& out){out<<"UPLOAD_PACKET_REUSE previous_fence="<<batch.fence<<" completed_fence="<<m_uploadFence->GetCompletedValue()<<" previous_tiles="<<batch.tiles.size();});
        batch.tiles.clear();batch.used=0;
        if(FAILED(batch.allocator->Reset()) || FAILED(batch.commands->Reset(batch.allocator.Get(),nullptr))){SetError("Tile upload command reset failed");return false;}
        for(UINT attempts=0;attempts<16;attempts++) {
            auto result=m_deferredTile?std::move(m_deferredTile):m_tileLoader.TryPop();m_deferredTile.reset();
            if(!result)break;
            if(m_tiles.contains(result->id)||m_failedTiles.contains(result->id))continue;
            m_readMs=m_readMs*.9+result->readMs*.1;
            if(!result->error.empty()){
                m_trace.Log([&](auto& out){out<<"TILE_FAILED tile="<<result->id.level<<"/"<<result->id.row<<"/"<<result->id.column<<" reason=DDS_READ";});
                m_failedTiles.insert(result->id);SetError("DDS "+m_planet.files.at(result->id).string()+": "+result->error);m_tileStatus=L"DDS load failures: see Logs/OrbitX.log";continue;
            }
            if(!QueuePlanetTile(result->id,result->surface,batch)) {
                if(!m_failedTiles.contains(result->id)){
                    m_trace.Log([&](auto& out){out<<"DEFERRED_HOLD tile="<<result->id.level<<"/"<<result->id.row<<"/"<<result->id.column;});m_deferredTile=std::move(result);
                }
                break;
            }
            if(Milliseconds(start)>=2.0)break;
        }
        if(!SubmitPlanetUploads(batch)){SetError("Tile upload submission failed");return false;}
        break; // One batched submission per frame, bounded CPU submission time.
    }
    m_uploadCpuMs=m_uploadCpuMs*.9+Milliseconds(start)*.1;
    m_tileDraws.clear();m_renderedLOD=0;m_residentLOD=0;
    for(auto id:m_planet.leaves) {
        TileDraw draw{};draw.id=id;draw.source=id;
        while(!resident(draw.source)||!m_tiles.contains(draw.source)||!m_tiles.at(draw.source).ready) {
            if(draw.source.level==4){SetError("No resident DDS root for selected patch");return false;}draw.source=draw.source.Parent();
        }
        auto& tile=m_tiles.at(draw.source);tile.touched=m_tileClock;draw.slot=tile.slot;
        if(draw.id!=draw.source)m_trace.Log([&](auto& out){out<<"TILE_FALLBACK tile="<<draw.id.level<<"/"<<draw.id.row<<"/"<<draw.id.column
            <<" source="<<draw.source.level<<"/"<<draw.source.row<<"/"<<draw.source.column;});
        m_renderedLOD=std::max(m_renderedLOD,draw.source.level);
        auto bounds=Core::Bounds(id),sourceBounds=Core::Bounds(draw.source);
        draw.uv[0]=float((bounds.west-sourceBounds.west)/(sourceBounds.east-sourceBounds.west));draw.uv[1]=float((sourceBounds.north-bounds.north)/(sourceBounds.north-sourceBounds.south));
        draw.uv[2]=float((bounds.east-bounds.west)/(sourceBounds.east-sourceBounds.west));draw.uv[3]=float((bounds.north-bounds.south)/(sourceBounds.north-sourceBounds.south));
        draw.grid=static_cast<UINT>(std::min(3,id.level-4));m_tileDraws.push_back(draw);
    }
    for(const auto& [id,tile]:m_tiles)if(tile.ready)m_residentLOD=std::max(m_residentLOD,id.level);
    return true;
}
bool Renderer::BuildPlanetMesh() {
    // Four persistent parameter grids. The vertex shader places them on each geographic patch.
    // Texture arrivals only change descriptors/UV ranges; geometry is never rebuilt during streaming.
    auto& batch=m_uploadBatches[0];
    if(FAILED(batch.allocator->Reset()) || FAILED(batch.commands->Reset(batch.allocator.Get(),nullptr)))return false;
    std::vector<ComPtr<ID3D12Resource>> staging;
    for(UINT meshIndex=0;meshIndex<4;meshIndex++) {
        const UINT grid=128u>>meshIndex;
        std::vector<Vertex> vertices;std::vector<uint32_t> indices;
        for(UINT r=0;r<=grid;r++)for(UINT c=0;c<=grid;c++)vertices.push_back({{0,0,0},{0,0,0},{float(c)/grid,float(r)/grid}});
        for(UINT r=0;r<grid;r++)for(UINT c=0;c<grid;c++){UINT a=r*(grid+1)+c,b=a+1,e=a+grid+1,d=e+1;indices.insert(indices.end(),{a,e,b,b,e,d});}
        std::vector<UINT> perimeter;
        for(UINT c=0;c<grid;c++)perimeter.push_back(c);
        for(UINT r=0;r<grid;r++)perimeter.push_back(r*(grid+1)+grid);
        for(UINT c=grid;c>0;c--)perimeter.push_back(grid*(grid+1)+c);
        for(UINT r=grid;r>0;r--)perimeter.push_back(r*(grid+1));
        for(size_t i=0;i<perimeter.size();i++) {
            UINT a=perimeter[i],b=perimeter[(i+1)%perimeter.size()],c=static_cast<UINT>(vertices.size());auto va=vertices[a],vb=vertices[b];va.pos.z=vb.pos.z=1;vertices.push_back(va);vertices.push_back(vb);indices.insert(indices.end(),{a,b,c,b,c+1,c});
        }
        auto upload=[&](const void* data,size_t bytes,ComPtr<ID3D12Resource>& resource,D3D12_RESOURCE_STATES finalState){
            auto gpuHeap=PlanetHeap(D3D12_HEAP_TYPE_DEFAULT),cpuHeap=PlanetHeap(D3D12_HEAP_TYPE_UPLOAD);auto desc=PlanetBuffer(bytes);
            ComPtr<ID3D12Resource> source;
            if(FAILED(m_device->CreateCommittedResource(&gpuHeap,D3D12_HEAP_FLAG_NONE,&desc,D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&resource))) ||
               FAILED(m_device->CreateCommittedResource(&cpuHeap,D3D12_HEAP_FLAG_NONE,&desc,D3D12_RESOURCE_STATE_GENERIC_READ,nullptr,IID_PPV_ARGS(&source))))return false;
            void* mapped=nullptr;if(FAILED(source->Map(0,nullptr,&mapped)))return false;
            std::memcpy(mapped,data,bytes);source->Unmap(0,nullptr);
            batch.commands->CopyBufferRegion(resource.Get(),0,source.Get(),0,bytes);
            D3D12_RESOURCE_BARRIER barrier{};barrier.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Transition.pResource=resource.Get();barrier.Transition.StateBefore=D3D12_RESOURCE_STATE_COPY_DEST;
            barrier.Transition.StateAfter=finalState;barrier.Transition.Subresource=D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            batch.commands->ResourceBarrier(1,&barrier);staging.push_back(source);return true;
        };
        auto& mesh=m_patchMeshes[meshIndex];
        if(!upload(vertices.data(),vertices.size()*sizeof(Vertex),mesh.vb,D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER)||!upload(indices.data(),indices.size()*sizeof(uint32_t),mesh.ib,D3D12_RESOURCE_STATE_INDEX_BUFFER))return false;
        mesh.vbv={mesh.vb->GetGPUVirtualAddress(),static_cast<UINT>(vertices.size()*sizeof(Vertex)),sizeof(Vertex)};mesh.ibv={mesh.ib->GetGPUVirtualAddress(),static_cast<UINT>(indices.size()*sizeof(uint32_t)),DXGI_FORMAT_R32_UINT};mesh.count=static_cast<UINT>(indices.size());
    }
    if(FAILED(batch.commands->Close()))return false;
    ID3D12CommandList* commands[]={batch.commands.Get()};m_uploadQueue->ExecuteCommandLists(1,commands);
    batch.fence=++m_uploadFenceValue;
    if(FAILED(m_uploadQueue->Signal(m_uploadFence.Get(),batch.fence)) || FAILED(m_uploadFence->SetEventOnCompletion(batch.fence,m_fenceEvent)))return false;
    // Startup only: keep staging alive until all four immutable grids are in GPU-local memory.
    return WaitForSingleObject(m_fenceEvent,INFINITE)==WAIT_OBJECT_0;
}
void Renderer::DrawPlanet() {
    m_cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    for(const auto& draw:m_tileDraws) {
        const auto& mesh=m_patchMeshes[draw.grid];m_cmd->IASetVertexBuffers(0,1,&mesh.vbv);m_cmd->IASetIndexBuffer(&mesh.ibv);
        auto descriptor=m_srvHeap->GetGPUDescriptorHandleForHeapStart();descriptor.ptr+=UINT64(draw.slot)*m_srvStride;m_cmd->SetGraphicsRootDescriptorTable(1,descriptor);
        auto color=Palette[(draw.id.level-1)%Palette.size()];auto b=Core::Bounds(draw.id);double rad=Core::PlanetPi/180;
        double step=(b.north-b.south)*rad/(128u>>draw.grid);
        float constants[16]={draw.uv[0],draw.uv[1],draw.uv[2],draw.uv[3],float((color>>16)&255)/255,float((color>>8)&255)/255,float(color&255)/255,1,
            m_tileOverlay?1.f:0.f,m_tileOverlay?1.f:0.f,float(std::max(1e-6,1-std::cos(step))+1e-5),0,
            float(b.west*rad),float(b.north*rad),float((b.east-b.west)*rad),float((b.north-b.south)*rad)};
        m_cmd->SetGraphicsRoot32BitConstants(2,16,constants,0);m_cmd->DrawIndexedInstanced(mesh.count,1,0,0,0);
        m_tiles.at(draw.source).lastDrawFence=m_nextFenceValue+1;
        m_trace.Log([&](auto& out){out<<"DRAW_SUBMIT tile="<<draw.id.level<<"/"<<draw.id.row<<"/"<<draw.id.column
            <<" source="<<draw.source.level<<"/"<<draw.source.row<<"/"<<draw.source.column<<" draw_fence="<<m_nextFenceValue+1<<" tile_clock="<<m_tileClock;});
    }
}
void Renderer::DrawPlanetLabels() {
    // Use the exact scene matrix written for this frame, including near-plane changes.
    SceneCB scene{};std::memcpy(&scene,m_cbPtr+256*m_frame,sizeof(scene));
    const auto transform=XMMatrixTranspose(XMLoadFloat4x4(&scene.mvp));
    const auto camera=Core::PlanetCamera::Orbit(m_yaw,m_pitch,m_distanceKm);
    if(!m_tileLabelBrush && FAILED(m_d2dContext->CreateSolidColorBrush(D2D1::ColorF(0,0,0),&m_tileLabelBrush)))return;
    for(const auto& draw:m_tileDraws) {
        const auto bounds=Core::Bounds(draw.id);
        const auto normal=Core::SurfaceDirection((bounds.north+bounds.south)*Core::PlanetPi/360,
                                                (bounds.east+bounds.west)*Core::PlanetPi/360);
        // A selected patch can straddle the limb; its center must itself be visible.
        if(Core::Dot(normal,camera.position-normal*1737.4)<=0)continue;
        XMFLOAT4 clip{};XMStoreFloat4(&clip,XMVector4Transform(XMVectorSet(float(normal.x),float(normal.y),float(normal.z),1),transform));
        if(clip.w<=0 || clip.z<0 || clip.z>clip.w || std::abs(clip.x)>clip.w || std::abs(clip.y)>clip.w)continue;
        auto& layout=m_tileLabelLayouts.at(draw.id.level);
        if(!layout) {
            const auto text=std::to_wstring(draw.id.level);
            if(FAILED(m_dwriteFactory->CreateTextLayout(text.c_str(),static_cast<UINT32>(text.size()),m_textLeft.Get(),64,40,&layout)))continue;
            layout->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
            layout->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
        }
        DWRITE_TEXT_METRICS metrics{};if(FAILED(layout->GetMetrics(&metrics)))continue;
        const float x=(clip.x/clip.w+1)*.5f*m_width-metrics.width*.5f-metrics.left;
        const float y=(1-clip.y/clip.w)*.5f*m_height-metrics.height*.5f-metrics.top;
        // Cached colored LOD text matches the patch palette; a shadow keeps it legible.
        m_tileLabelBrush->SetColor(D2D1::ColorF(0,0,0));
        m_d2dContext->DrawTextLayout(D2D1::Point2F(x+1,y+1),layout.Get(),m_tileLabelBrush.Get());
        m_tileLabelBrush->SetColor(D2D1::ColorF(Palette[(draw.id.level-1)%Palette.size()]));
        m_d2dContext->DrawTextLayout(D2D1::Point2F(x,y),layout.Get(),m_tileLabelBrush.Get());
    }
}
void Renderer::DrawPlanetTelemetry(float x,float y) {
    auto now=std::chrono::steady_clock::now();
    if(m_telemetryText.empty()||now-m_telemetryTime>=std::chrono::milliseconds(100)){
        m_telemetryTime=now;
        size_t inFlight=0,residentCount=0;for(const auto& [id,tile]:m_tiles)if(tile.ready)++residentCount;else ++inFlight;
        size_t pending=0;for(auto id:m_planet.requests){auto it=m_tiles.find(id);if((it==m_tiles.end()||!it->second.ready)&&!m_failedTiles.contains(id))++pending;}
        std::wostringstream text;text<<L"BODY MOON\nRendered texture LOD "<<m_renderedLOD<<L"  Resident LOD "<<m_residentLOD
            <<L"\nRendered leaves "<<m_tileDraws.size()<<L"  Resident DDS "<<residentCount<<L"\nIndexed DDS "<<m_planet.files.size()<<L"  Rejected "<<m_planet.rejectedFiles
            <<L"\nPending "<<pending<<L"  Uploading "<<inFlight<<L"  Failed "<<m_failedTiles.size()
            <<L"\nDDS GPU allocations "<<std::fixed<<std::setprecision(1)<<double(m_tileBytes)/(1024*1024)<<L" / 512 MiB"
            <<std::setprecision(2)<<L"\nSelect "<<m_selectMs<<L" ms  Upload CPU "<<m_uploadCpuMs<<L" ms"
            <<L"\nRead (worker) "<<m_readMs<<L" ms  Scene GPU "<<m_sceneGpuMs<<L" ms"
            <<L"\nScene CPU "<<m_sceneCpuMs<<L" ms  HUD CPU "<<m_hudCpuMs<<L" ms"
            <<L"\nPresent / frame wait "<<m_presentMs<<L" ms  VSync "<<(m_vsync?L"ON":L"OFF")
            <<L"\nTile overlay "<<(m_tileOverlay?L"ON":L"OFF")<<L"\nLOD frozen "<<(m_planet.frozen?L"YES":L"NO")
            <<L"\nCtrl+O tint / edges / LOD  Ctrl+L freeze  Ctrl+F FPS graph";
        if(!m_tileStatus.empty())text<<L"\n"<<m_tileStatus;
        if(m_tileOverlay) {
            auto camera=Core::PlanetCamera::Orbit(m_yaw,m_pitch,m_distanceKm);
            double lat=std::asin(camera.position.z/m_distanceKm)*180/Core::PlanetPi,lon=std::atan2(camera.position.y,camera.position.x)*180/Core::PlanetPi;
            for(const auto& draw:m_tileDraws){auto b=Core::Bounds(draw.id);if(lat>=b.south&&lat<=b.north&&lon>=b.west&&lon<=b.east){
                text<<L"\nCENTER TILE L"<<draw.id.level<<L" row "<<draw.id.row<<L" col "<<draw.id.column<<L"\nCenter lat "<<(b.south+b.north)*.5<<L" lon "<<(b.west+b.east)*.5
                    <<L"\nLat ["<<b.south<<L", "<<b.north<<L"] Lon ["<<b.west<<L", "<<b.east<<L"]\nTexture source L"<<draw.source.level<<L"\n"<<m_planet.files.at(draw.source).wstring();break;}}
        }
        m_telemetryText=text.str();
    }
    DrawCachedHudText(m_telemetryText,x,y,true,650);
}
}
