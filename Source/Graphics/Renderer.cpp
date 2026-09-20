#include "Renderer.h"
#include "../Core/MenuLayout.h"
#include <d3dcompiler.h>
#include <wincodec.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <filesystem>
#include <array>
#include <cctype>
#include <sstream>
#include <initializer_list>

#pragma comment(lib,"d3d12.lib")
#pragma comment(lib,"dxgi.lib")
#pragma comment(lib,"d3dcompiler.lib")
#pragma comment(lib,"windowscodecs.lib")
#pragma comment(lib,"d3d11.lib")
#pragma comment(lib,"d2d1.lib")
#pragma comment(lib,"dwrite.lib")
using namespace DirectX; using Microsoft::WRL::ComPtr;
namespace OrbitX::Graphics {
static D3D12_HEAP_PROPERTIES Heap(D3D12_HEAP_TYPE t){ D3D12_HEAP_PROPERTIES h{}; h.Type=t; return h; }
static D3D12_RESOURCE_DESC BufferDesc(UINT64 n){ D3D12_RESOURCE_DESC d{}; d.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER; d.Width=n; d.Height=1; d.DepthOrArraySize=1; d.MipLevels=1; d.SampleDesc.Count=1; d.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR; return d; }
static std::string HrText(HRESULT hr){ char b[64]; sprintf_s(b,"HRESULT 0x%08X",(unsigned)hr); return b; }
void Renderer::SetError(const std::string&s){m_error=s; std::ofstream(m_root+L"\\Logs\\OrbitX.log",std::ios::app)<<s<<"\n";}

bool Renderer::Initialize(HWND hwnd,UINT w,UINT h,const std::wstring& root){m_hwnd=hwnd;m_width=w;m_height=h;m_root=root;
#if defined(_DEBUG)
 ComPtr<ID3D12Debug> dbg; if(SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&dbg)))) dbg->EnableDebugLayer();
#endif
 if(!InitDevice()||!InitAssets()) return false; return true; }

bool Renderer::InitDevice(){ HRESULT hr=CreateDXGIFactory2(0,IID_PPV_ARGS(&m_factory)); if(FAILED(hr)){SetError("CreateDXGIFactory2: "+HrText(hr));return false;}
 ComPtr<IDXGIAdapter1>a; for(UINT i=0;m_factory->EnumAdapters1(i,&a)!=DXGI_ERROR_NOT_FOUND;i++){DXGI_ADAPTER_DESC1 d{};a->GetDesc1(&d);if(d.Flags&DXGI_ADAPTER_FLAG_SOFTWARE){a.Reset();continue;} if(SUCCEEDED(D3D12CreateDevice(a.Get(),D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&m_device))))break;a.Reset();}
 if(!m_device && FAILED(D3D12CreateDevice(nullptr,D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&m_device)))){SetError("No D3D12 device.");return false;}
 D3D12_COMMAND_QUEUE_DESC q{}; if(FAILED(m_device->CreateCommandQueue(&q,IID_PPV_ARGS(&m_queue))))return false;
 DXGI_SWAP_CHAIN_DESC1 sd{};sd.BufferCount=FrameCount;sd.Width=m_width;sd.Height=m_height;sd.Format=DXGI_FORMAT_R8G8B8A8_UNORM;sd.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;sd.SwapEffect=DXGI_SWAP_EFFECT_FLIP_DISCARD;sd.SampleDesc.Count=1;
 ComPtr<IDXGISwapChain1>s1; if(FAILED(m_factory->CreateSwapChainForHwnd(m_queue.Get(),m_hwnd,&sd,nullptr,nullptr,&s1)))return false;s1.As(&m_swap);m_factory->MakeWindowAssociation(m_hwnd,DXGI_MWA_NO_ALT_ENTER);m_frame=m_swap->GetCurrentBackBufferIndex();
 D3D12_DESCRIPTOR_HEAP_DESC hd{};hd.NumDescriptors=FrameCount;hd.Type=D3D12_DESCRIPTOR_HEAP_TYPE_RTV;m_device->CreateDescriptorHeap(&hd,IID_PPV_ARGS(&m_rtvHeap));m_rtvStride=m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
 auto r=m_rtvHeap->GetCPUDescriptorHandleForHeapStart();for(UINT i=0;i<FrameCount;i++){m_swap->GetBuffer(i,IID_PPV_ARGS(&m_back[i]));m_device->CreateRenderTargetView(m_back[i].Get(),nullptr,r);r.ptr+=m_rtvStride;m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&m_alloc[i]));}
 hd={};hd.NumDescriptors=1;hd.Type=D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;hd.Flags=D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;m_device->CreateDescriptorHeap(&hd,IID_PPV_ARGS(&m_srvHeap));
 hd={};hd.NumDescriptors=1;hd.Type=D3D12_DESCRIPTOR_HEAP_TYPE_DSV;m_device->CreateDescriptorHeap(&hd,IID_PPV_ARGS(&m_dsvHeap));
 m_device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,m_alloc[m_frame].Get(),nullptr,IID_PPV_ARGS(&m_cmd));m_cmd->Close();m_device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&m_fence));m_fenceEvent=CreateEvent(nullptr,FALSE,FALSE,nullptr);return m_fenceEvent!=nullptr; }

static bool Compile(const std::wstring&p,const char*entry,const char*target,ComPtr<ID3DBlob>&out,std::string&err){ComPtr<ID3DBlob>e;UINT f=D3DCOMPILE_ENABLE_STRICTNESS;
#if defined(_DEBUG)
 f|=D3DCOMPILE_DEBUG|D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
 HRESULT hr=D3DCompileFromFile(p.c_str(),nullptr,D3D_COMPILE_STANDARD_FILE_INCLUDE,entry,target,f,0,&out,&e);if(FAILED(hr)){err=e?(char*)e->GetBufferPointer():HrText(hr);return false;}return true;}

bool Renderer::CreateScenePipeline(){ComPtr<ID3DBlob>vs,ps,rs;std::string e;auto p=m_root+L"\\Shaders\\Moon.hlsl";if(!Compile(p,"VSMain","vs_5_1",vs,e)||!Compile(p,"PSMain","ps_5_1",ps,e)){SetError(e);return false;}
 D3D12_DESCRIPTOR_RANGE range{};range.RangeType=D3D12_DESCRIPTOR_RANGE_TYPE_SRV;range.NumDescriptors=1;range.BaseShaderRegister=0;D3D12_ROOT_PARAMETER rp[2]{};rp[0].ParameterType=D3D12_ROOT_PARAMETER_TYPE_CBV;rp[0].Descriptor.ShaderRegister=0;rp[0].ShaderVisibility=D3D12_SHADER_VISIBILITY_ALL;rp[1].ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;rp[1].DescriptorTable.NumDescriptorRanges=1;rp[1].DescriptorTable.pDescriptorRanges=&range;rp[1].ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;
 D3D12_STATIC_SAMPLER_DESC samp{};samp.Filter=D3D12_FILTER_ANISOTROPIC;samp.MaxAnisotropy=16;samp.AddressU=D3D12_TEXTURE_ADDRESS_MODE_WRAP;samp.AddressV=D3D12_TEXTURE_ADDRESS_MODE_CLAMP;samp.AddressW=D3D12_TEXTURE_ADDRESS_MODE_CLAMP;samp.MaxLOD=D3D12_FLOAT32_MAX;samp.ShaderRegister=0;samp.ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;
 D3D12_ROOT_SIGNATURE_DESC rd{};rd.NumParameters=2;rd.pParameters=rp;rd.NumStaticSamplers=1;rd.pStaticSamplers=&samp;rd.Flags=D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;ComPtr<ID3DBlob>re;D3D12SerializeRootSignature(&rd,D3D_ROOT_SIGNATURE_VERSION_1,&rs,&re);m_device->CreateRootSignature(0,rs->GetBufferPointer(),rs->GetBufferSize(),IID_PPV_ARGS(&m_sceneRoot));
 D3D12_INPUT_ELEMENT_DESC il[]={{"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},{"NORMAL",0,DXGI_FORMAT_R32G32B32_FLOAT,0,12,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},{"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,24,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0}};
 D3D12_GRAPHICS_PIPELINE_STATE_DESC d{};d.pRootSignature=m_sceneRoot.Get();d.VS={vs->GetBufferPointer(),vs->GetBufferSize()};d.PS={ps->GetBufferPointer(),ps->GetBufferSize()};d.InputLayout={il,3};d.PrimitiveTopologyType=D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;d.RTVFormats[0]=DXGI_FORMAT_R8G8B8A8_UNORM;d.NumRenderTargets=1;d.DSVFormat=DXGI_FORMAT_D32_FLOAT;d.SampleDesc.Count=1;d.SampleMask=UINT_MAX;d.RasterizerState.FillMode=D3D12_FILL_MODE_SOLID;d.RasterizerState.CullMode=D3D12_CULL_MODE_BACK;d.RasterizerState.FrontCounterClockwise=FALSE;d.RasterizerState.DepthClipEnable=TRUE;d.BlendState.RenderTarget[0].RenderTargetWriteMask=D3D12_COLOR_WRITE_ENABLE_ALL;d.DepthStencilState.DepthEnable=TRUE;d.DepthStencilState.DepthWriteMask=D3D12_DEPTH_WRITE_MASK_ALL;d.DepthStencilState.DepthFunc=D3D12_COMPARISON_FUNC_LESS;return SUCCEEDED(m_device->CreateGraphicsPipelineState(&d,IID_PPV_ARGS(&m_scenePSO))); }

bool Renderer::CreateSphere(){const int lon=256,lat=128;std::vector<Vertex>v;std::vector<uint32_t>idx;v.reserve((lon+1)*(lat+1));for(int j=0;j<=lat;j++){float vv=float(j)/lat;float phi=XM_PIDIV2-vv*XM_PI;for(int i=0;i<=lon;i++){float u=float(i)/lon;float lam=(u-.5f)*XM_2PI;float cp=cosf(phi);XMFLOAT3 n{cp*cosf(lam),sinf(phi),cp*sinf(lam)};v.push_back({n,n,{u,vv}});}}for(int j=0;j<lat;j++)for(int i=0;i<lon;i++){uint32_t a=j*(lon+1)+i,b=a+1,c=a+lon+1,d=c+1;idx.insert(idx.end(),{a,c,b,b,c,d});}m_indexCount=(UINT)idx.size();UINT vbBytes=(UINT)(v.size()*sizeof(Vertex)),ibBytes=(UINT)(idx.size()*4);auto hp=Heap(D3D12_HEAP_TYPE_UPLOAD);auto bd=BufferDesc(vbBytes);m_device->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_NONE,&bd,D3D12_RESOURCE_STATE_GENERIC_READ,nullptr,IID_PPV_ARGS(&m_vb));void*p;m_vb->Map(0,nullptr,&p);memcpy(p,v.data(),vbBytes);m_vb->Unmap(0,nullptr);bd=BufferDesc(ibBytes);m_device->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_NONE,&bd,D3D12_RESOURCE_STATE_GENERIC_READ,nullptr,IID_PPV_ARGS(&m_ib));m_ib->Map(0,nullptr,&p);memcpy(p,idx.data(),ibBytes);m_ib->Unmap(0,nullptr);m_vbv={m_vb->GetGPUVirtualAddress(),vbBytes,sizeof(Vertex)};m_ibv={m_ib->GetGPUVirtualAddress(),ibBytes,DXGI_FORMAT_R32_UINT};return true;}

bool Renderer::LoadTextureWIC(const std::wstring&path){
 ComPtr<IWICImagingFactory>f;HRESULT hr=CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&f));
 if(FAILED(hr)){SetError("WIC factory failed");return false;}
 ComPtr<IWICBitmapDecoder>d;hr=f->CreateDecoderFromFilename(path.c_str(),nullptr,GENERIC_READ,WICDecodeMetadataCacheOnLoad,&d);
 if(FAILED(hr)){SetError("Cannot open Moon texture.");return false;}
 ComPtr<IWICBitmapFrameDecode>fr;d->GetFrame(0,&fr);UINT sw,sh;fr->GetSize(&sw,&sh);
 UINT tw=sw,th=sh;const UINT maxW=8192;ComPtr<IWICBitmapSource>src=fr;
 if(sw>maxW){tw=maxW;th=(UINT)((uint64_t)sh*tw/sw);ComPtr<IWICBitmapScaler>sc;f->CreateBitmapScaler(&sc);sc->Initialize(fr.Get(),tw,th,WICBitmapInterpolationModeFant);src=sc;}
 ComPtr<IWICFormatConverter>cv;f->CreateFormatConverter(&cv);cv->Initialize(src.Get(),GUID_WICPixelFormat32bppRGBA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom);
 std::vector<std::vector<uint8_t>> mipData;std::vector<UINT> mipW,mipH;
 mipW.push_back(tw);mipH.push_back(th);mipData.emplace_back((size_t)tw*th*4);cv->CopyPixels(nullptr,tw*4,(UINT)mipData[0].size(),mipData[0].data());
 while(mipW.back()>1||mipH.back()>1){
  UINT pw=mipW.back(),ph=mipH.back(),nw=std::max(1u,pw/2),nh=std::max(1u,ph/2);const auto&prev=mipData.back();std::vector<uint8_t>next((size_t)nw*nh*4);
  for(UINT y=0;y<nh;y++)for(UINT x=0;x<nw;x++)for(UINT c=0;c<4;c++){
   unsigned sum=0,count=0;for(UINT oy=0;oy<2;oy++)for(UINT ox=0;ox<2;ox++){UINT sx=std::min(pw-1,2*x+ox),sy=std::min(ph-1,2*y+oy);sum+=prev[((size_t)sy*pw+sx)*4+c];count++;}
   next[((size_t)y*nw+x)*4+c]=(uint8_t)((sum+count/2)/count);
  }
  mipW.push_back(nw);mipH.push_back(nh);mipData.push_back(std::move(next));
 }
 const UINT mipCount=(UINT)mipData.size();
 D3D12_RESOURCE_DESC td{};td.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;td.Width=tw;td.Height=th;td.DepthOrArraySize=1;td.MipLevels=(UINT16)mipCount;td.Format=DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;td.SampleDesc.Count=1;
 auto hp=Heap(D3D12_HEAP_TYPE_DEFAULT);if(FAILED(m_device->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_NONE,&td,D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&m_texture)))){SetError("Moon texture allocation failed");return false;}
 std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> fp(mipCount);std::vector<UINT> rows(mipCount);std::vector<UINT64> rowBytes(mipCount);UINT64 total=0;
 m_device->GetCopyableFootprints(&td,0,mipCount,0,fp.data(),rows.data(),rowBytes.data(),&total);
 auto uph=Heap(D3D12_HEAP_TYPE_UPLOAD);auto bd=BufferDesc(total);ComPtr<ID3D12Resource>up;if(FAILED(m_device->CreateCommittedResource(&uph,D3D12_HEAP_FLAG_NONE,&bd,D3D12_RESOURCE_STATE_GENERIC_READ,nullptr,IID_PPV_ARGS(&up)))){SetError("Moon upload allocation failed");return false;}
 uint8_t*map=nullptr;up->Map(0,nullptr,(void**)&map);for(UINT m=0;m<mipCount;m++)for(UINT y=0;y<mipH[m];y++)memcpy(map+fp[m].Offset+(size_t)y*fp[m].Footprint.RowPitch,mipData[m].data()+(size_t)y*mipW[m]*4,(size_t)mipW[m]*4);up->Unmap(0,nullptr);
 m_alloc[m_frame]->Reset();m_cmd->Reset(m_alloc[m_frame].Get(),nullptr);for(UINT m=0;m<mipCount;m++){D3D12_TEXTURE_COPY_LOCATION dst{m_texture.Get(),D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX};dst.SubresourceIndex=m;D3D12_TEXTURE_COPY_LOCATION sr{up.Get(),D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT};sr.PlacedFootprint=fp[m];m_cmd->CopyTextureRegion(&dst,0,0,0,&sr,nullptr);}
 D3D12_RESOURCE_BARRIER b{};b.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;b.Transition.pResource=m_texture.Get();b.Transition.StateBefore=D3D12_RESOURCE_STATE_COPY_DEST;b.Transition.StateAfter=D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;b.Transition.Subresource=D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;m_cmd->ResourceBarrier(1,&b);m_cmd->Close();ID3D12CommandList*l[]={m_cmd.Get()};m_queue->ExecuteCommandLists(1,l);WaitForGPU();
 D3D12_SHADER_RESOURCE_VIEW_DESC sv{};sv.Format=td.Format;sv.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;sv.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;sv.Texture2D.MipLevels=mipCount;m_device->CreateShaderResourceView(m_texture.Get(),&sv,m_srvHeap->GetCPUDescriptorHandleForHeapStart());
 std::ofstream(m_root+L"\\Logs\\OrbitX.log",std::ios::app)<<"Texture source "<<sw<<"x"<<sh<<", upload "<<tw<<"x"<<th<<", mip levels "<<mipCount<<", 16x anisotropic filtering\n";return true;}

bool Renderer::CreateDepth(){D3D12_RESOURCE_DESC d{};d.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;d.Width=m_width;d.Height=m_height;d.DepthOrArraySize=1;d.MipLevels=1;d.Format=DXGI_FORMAT_D32_FLOAT;d.SampleDesc.Count=1;d.Flags=D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;D3D12_CLEAR_VALUE cv{};cv.Format=d.Format;cv.DepthStencil.Depth=1;auto hp=Heap(D3D12_HEAP_TYPE_DEFAULT);if(FAILED(m_device->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_NONE,&d,D3D12_RESOURCE_STATE_DEPTH_WRITE,&cv,IID_PPV_ARGS(&m_depth))))return false;m_device->CreateDepthStencilView(m_depth.Get(),nullptr,m_dsvHeap->GetCPUDescriptorHandleForHeapStart());return true;}
bool Renderer::CreateConstantBuffer(){auto hp=Heap(D3D12_HEAP_TYPE_UPLOAD);auto d=BufferDesc(256);if(FAILED(m_device->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_NONE,&d,D3D12_RESOURCE_STATE_GENERIC_READ,nullptr,IID_PPV_ARGS(&m_cb))))return false;m_cb->Map(0,nullptr,(void**)&m_cbPtr);return true;}

// Load the tracked TrueType asset into a private DirectWrite collection. Users do
// not need to install OrbitX Header, and the app never changes Windows font state.
bool Renderer::LoadBundledHeaderFont(){
 const std::wstring path=m_root+L"\\Fonts\\OrbitXHeader.ttf";
 if(!std::filesystem::is_regular_file(std::filesystem::path(path))){
  SetError("OrbitX Header font asset missing: Runtime/Fonts/OrbitXHeader.ttf");
  return false;
 }
 Microsoft::WRL::ComPtr<IDWriteFactory3> factory3;
 HRESULT hr=m_dwriteFactory.As(&factory3);
 if(FAILED(hr)){SetError("IDWriteFactory3 unavailable: "+HrText(hr));return false;}
 Microsoft::WRL::ComPtr<IDWriteFontFile> fontFile;
 hr=factory3->CreateFontFileReference(path.c_str(),nullptr,&fontFile);
 if(FAILED(hr)){SetError("Load OrbitX Header font file: "+HrText(hr));return false;}
 BOOL supported=FALSE;DWRITE_FONT_FILE_TYPE type{};UINT32 faceCount=0;
 hr=fontFile->Analyze(&supported,&type,nullptr,&faceCount);
 if(FAILED(hr)||!supported||faceCount!=1){SetError("OrbitX Header asset is not a supported single-face font.");return false;}
 Microsoft::WRL::ComPtr<IDWriteFontFaceReference> face;
 hr=factory3->CreateFontFaceReference(fontFile.Get(),0,DWRITE_FONT_SIMULATIONS_NONE,&face);
 if(FAILED(hr)){SetError("Create OrbitX Header face reference: "+HrText(hr));return false;}
 Microsoft::WRL::ComPtr<IDWriteFontSetBuilder> builder;
 hr=factory3->CreateFontSetBuilder(&builder);
 if(FAILED(hr)){SetError("Create OrbitX Header font set builder: "+HrText(hr));return false;}
 hr=builder->AddFontFaceReference(face.Get());
 if(FAILED(hr)){SetError("Add OrbitX Header font face: "+HrText(hr));return false;}
 Microsoft::WRL::ComPtr<IDWriteFontSet> fontSet;
 hr=builder->CreateFontSet(&fontSet);
 if(FAILED(hr)){SetError("Create OrbitX Header font set: "+HrText(hr));return false;}
 hr=factory3->CreateFontCollectionFromFontSet(fontSet.Get(),&m_headerFontCollection);
 if(FAILED(hr)){SetError("Create OrbitX Header collection: "+HrText(hr));return false;}
 UINT32 familyIndex=0;BOOL exists=FALSE;
 hr=m_headerFontCollection->FindFamilyName(L"OrbitX Header",&familyIndex,&exists);
 if(FAILED(hr)||!exists){SetError("Bundled font does not contain the OrbitX Header family.");return false;}
 return true;
}

bool Renderer::InitVectorText(){
 UINT flags=D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#if defined(_DEBUG)
 flags|=D3D11_CREATE_DEVICE_DEBUG;
#endif
 IUnknown* queues[]={m_queue.Get()};
 D3D_FEATURE_LEVEL levels[]={D3D_FEATURE_LEVEL_11_0};
 HRESULT hr=D3D11On12CreateDevice(m_device.Get(),flags,levels,1,queues,1,0,&m_d3d11Device,&m_d3d11Context,nullptr);
#if defined(_DEBUG)
 // Some systems do not have the optional D3D11 debug layer installed. Retry without it.
 if(FAILED(hr)){flags&=~D3D11_CREATE_DEVICE_DEBUG;hr=D3D11On12CreateDevice(m_device.Get(),flags,levels,1,queues,1,0,&m_d3d11Device,&m_d3d11Context,nullptr);}
#endif
 if(FAILED(hr)){SetError("D3D11On12CreateDevice: "+HrText(hr));return false;}
 if(FAILED(m_d3d11Device.As(&m_d3d11On12))){SetError("ID3D11On12Device unavailable");return false;}

 D2D1_FACTORY_OPTIONS fo{};
#if defined(_DEBUG)
 fo.debugLevel=D2D1_DEBUG_LEVEL_INFORMATION;
#endif
 hr=D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,__uuidof(ID2D1Factory3),&fo,reinterpret_cast<void**>(m_d2dFactory.GetAddressOf()));
 if(FAILED(hr)){SetError("D2D1CreateFactory: "+HrText(hr));return false;}
 Microsoft::WRL::ComPtr<IDXGIDevice> dxgiDevice;
 if(FAILED(m_d3d11Device.As(&dxgiDevice))||FAILED(m_d2dFactory->CreateDevice(dxgiDevice.Get(),&m_d2dDevice))||FAILED(m_d2dDevice->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE,&m_d2dContext))){SetError("Direct2D device creation failed");return false;}
 hr=DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,__uuidof(IDWriteFactory),reinterpret_cast<IUnknown**>(m_dwriteFactory.GetAddressOf()));
 if(FAILED(hr)){SetError("DWriteCreateFactory: "+HrText(hr));return false;}
 if(!LoadBundledHeaderFont())return false;

 // Restore the original requested "size 2" HUD scale.  The prior size 1.25
 // DirectWrite HUD used 12 DIP, so size 2 on the same scale is 19.2 DIP.
 constexpr FLOAT fontSize=19.2f;
 hr=m_dwriteFactory->CreateTextFormat(L"Segoe UI",nullptr,DWRITE_FONT_WEIGHT_NORMAL,DWRITE_FONT_STYLE_NORMAL,DWRITE_FONT_STRETCH_NORMAL,fontSize,L"en-us",&m_textLeft);
 if(FAILED(hr)){SetError("CreateTextFormat: "+HrText(hr));return false;}
 hr=m_dwriteFactory->CreateTextFormat(L"Segoe UI",nullptr,DWRITE_FONT_WEIGHT_NORMAL,DWRITE_FONT_STYLE_NORMAL,DWRITE_FONT_STRETCH_NORMAL,fontSize,L"en-us",&m_textRight);
 if(FAILED(hr))return false;
 m_textLeft->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);m_textLeft->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
 m_textRight->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);m_textRight->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
 // OrbitX Header is loaded from Runtime/Fonts via a private app-only
 // collection. Do not use the system collection here: Windows font installation is optional.
 hr=m_dwriteFactory->CreateTextFormat(L"OrbitX Header",m_headerFontCollection.Get(),DWRITE_FONT_WEIGHT_NORMAL,DWRITE_FONT_STYLE_NORMAL,DWRITE_FONT_STRETCH_NORMAL,88.0f,L"en-us",&m_textLogo);
 if(FAILED(hr)){SetError("OrbitX Header logo format: "+HrText(hr));return false;}
 hr=m_dwriteFactory->CreateTextFormat(L"OrbitX Header",m_headerFontCollection.Get(),DWRITE_FONT_WEIGHT_NORMAL,DWRITE_FONT_STYLE_NORMAL,DWRITE_FONT_STRETCH_NORMAL,100.0f,L"en-us",&m_textLogoX);
 if(FAILED(hr)){SetError("OrbitX Header logo X format: "+HrText(hr));return false;}
 hr=m_dwriteFactory->CreateTextFormat(L"OrbitX Header",m_headerFontCollection.Get(),DWRITE_FONT_WEIGHT_NORMAL,DWRITE_FONT_STYLE_NORMAL,DWRITE_FONT_STRETCH_NORMAL,24.0f,L"en-us",&m_textMenu);
 if(FAILED(hr)){SetError("OrbitX Header menu format: "+HrText(hr));return false;}
 // Scenario list labels are smaller than main-menu labels; the submenu heading is larger.
 hr=m_dwriteFactory->CreateTextFormat(L"OrbitX Header",m_headerFontCollection.Get(),DWRITE_FONT_WEIGHT_NORMAL,DWRITE_FONT_STYLE_NORMAL,DWRITE_FONT_STRETCH_NORMAL,20.0f,L"en-us",&m_textScenarioButton);
 if(FAILED(hr)){SetError("OrbitX Header scenario button format: "+HrText(hr));return false;}
 hr=m_dwriteFactory->CreateTextFormat(L"OrbitX Header",m_headerFontCollection.Get(),DWRITE_FONT_WEIGHT_NORMAL,DWRITE_FONT_STYLE_NORMAL,DWRITE_FONT_STRETCH_NORMAL,42.0f,L"en-us",&m_textScenarioTitle);
 if(FAILED(hr)){SetError("OrbitX Header scenario title format: "+HrText(hr));return false;}
 m_textLogo->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING); m_textLogoX->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
 m_textMenu->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
 m_textScenarioButton->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
 m_textScenarioTitle->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);

 // OrbitX is Per-Monitor-V2 DPI aware, so the swap-chain dimensions are physical pixels.
 // Keep the HUD coordinate system at 96 DPI intentionally: 1 D2D DIP == 1 back-buffer pixel.
 // Use grayscale AA (not ClearType/subpixel AA) because the HUD is composed into a DXGI
 // back buffer and must remain neutral and stable on every monitor/compositor path.
 hr=m_dwriteFactory->CreateCustomRenderingParams(
     2.2f, 1.0f, 0.0f,
     DWRITE_PIXEL_GEOMETRY_FLAT,
     DWRITE_RENDERING_MODE_NATURAL_SYMMETRIC,
     &m_textRenderingParams);
 if(FAILED(hr)){SetError("CreateCustomRenderingParams: "+HrText(hr));return false;}
 m_d2dContext->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
 m_d2dContext->SetTextRenderingParams(m_textRenderingParams.Get());
 m_d2dContext->SetDpi(96.0f,96.0f);
 if(FAILED(m_d2dContext->CreateSolidColorBrush(D2D1::ColorF(0.88f,0.92f,0.88f,1.0f),&m_textBrush)))return false;

 if(!CreateHudBackBufferTargets())return false;
 // Diagnostic proof that no hidden DPI virtualization is in the render path.
 {
   UINT dpi=GetDpiForWindow(m_hwnd); RECT cr{}; GetClientRect(m_hwnd,&cr);
   std::ofstream log(std::filesystem::path(m_root)/L"Logs"/L"OrbitX.log",std::ios::app);
   if(log){log<<"HUD/DPI: awareness=PerMonitorV2 windowDpi="<<dpi
              <<" client="<<(cr.right-cr.left)<<"x"<<(cr.bottom-cr.top)
              <<" swapchain="<<m_width<<"x"<<m_height
              <<" d2dDpi=96 grayscaleAA=1\n";}
 }
 return true;
}

bool Renderer::CreateHudBackBufferTargets(){
 D3D11_RESOURCE_FLAGS rf{};rf.BindFlags=D3D11_BIND_RENDER_TARGET;
 for(UINT i=0;i<FrameCount;i++){
   HRESULT hr=m_d3d11On12->CreateWrappedResource(m_back[i].Get(),&rf,D3D12_RESOURCE_STATE_RENDER_TARGET,D3D12_RESOURCE_STATE_PRESENT,IID_PPV_ARGS(&m_wrappedBack[i]));
   if(FAILED(hr)){SetError("CreateWrappedResource: "+HrText(hr));return false;}
   Microsoft::WRL::ComPtr<IDXGISurface> surface;if(FAILED(m_wrappedBack[i].As(&surface)))return false;
   auto props=D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_TARGET|D2D1_BITMAP_OPTIONS_CANNOT_DRAW,D2D1::PixelFormat(DXGI_FORMAT_R8G8B8A8_UNORM,D2D1_ALPHA_MODE_IGNORE),96.0f,96.0f);
   hr=m_d2dContext->CreateBitmapFromDxgiSurface(surface.Get(),&props,&m_d2dTarget[i]);
   if(FAILED(hr)){SetError("CreateBitmapFromDxgiSurface: "+HrText(hr));return false;}
 }
 return true;
}

bool Renderer::InitAssets(){if(!CreateScenePipeline()||!CreateSphere()||!CreateDepth()||!CreateConstantBuffer())return false;if(!LoadTextureWIC(m_root+L"\\Textures\\Moon\\Moon_Orbiter_L8.jpg"))return false;if(!InitVectorText())return false;return LoadMenuBackground() && LoadMenuLogo() && LoadScenarioPreview();}

bool Renderer::LoadMenuBackground(){
 ComPtr<IWICImagingFactory> f; HRESULT hr=CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&f));
 if(FAILED(hr))return false;
 const std::wstring path=m_root+L"\\Textures\\Menu\\Home.png";
 ComPtr<IWICBitmapDecoder> d; hr=f->CreateDecoderFromFilename(path.c_str(),nullptr,GENERIC_READ,WICDecodeMetadataCacheOnLoad,&d);
 if(FAILED(hr)){SetError("Cannot open menu background.");return false;}
 ComPtr<IWICBitmapFrameDecode> fr; d->GetFrame(0,&fr);
 ComPtr<IWICFormatConverter> cv; f->CreateFormatConverter(&cv);
 cv->Initialize(fr.Get(),GUID_WICPixelFormat32bppPBGRA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom);
 hr=m_d2dContext->CreateBitmapFromWicBitmap(cv.Get(),nullptr,&m_menuBackground);
 if(FAILED(hr)){SetError("Create menu bitmap: "+HrText(hr));return false;}
 return true;
}

bool Renderer::LoadMenuLogo(){
 ComPtr<IWICImagingFactory> f; HRESULT hr=CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&f));
 if(FAILED(hr))return false;
 const std::wstring path=m_root+L"\\Textures\\Menu\\OrbitXLogo.png";
 ComPtr<IWICBitmapDecoder> d; hr=f->CreateDecoderFromFilename(path.c_str(),nullptr,GENERIC_READ,WICDecodeMetadataCacheOnLoad,&d);
 if(FAILED(hr)){SetError("Cannot open menu logo.");return false;}
 ComPtr<IWICBitmapFrameDecode> fr; d->GetFrame(0,&fr);
 ComPtr<IWICFormatConverter> cv; f->CreateFormatConverter(&cv);
 cv->Initialize(fr.Get(),GUID_WICPixelFormat32bppPBGRA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom);
 hr=m_d2dContext->CreateBitmapFromWicBitmap(cv.Get(),nullptr,&m_menuLogo);
 if(FAILED(hr)){SetError("Create menu logo bitmap: "+HrText(hr));return false;}
 return true;
}

// A lightweight terrain thumbnail derived from the already-shipped lunar map.
// Resize before uploading to Direct2D; do not decode a second full-resolution GPU texture.
bool Renderer::LoadScenarioPreview(){
 ComPtr<IWICImagingFactory> f;
 if(FAILED(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&f))))return false;
 ComPtr<IWICBitmapDecoder> d;
 const auto artwork=m_root+L"\\Textures\\Menu\\MoonViewPreview.jpg";
 m_scenarioPreviewArtwork=SUCCEEDED(f->CreateDecoderFromFilename(
     artwork.c_str(),nullptr,GENERIC_READ,WICDecodeMetadataCacheOnLoad,&d));
 if(!m_scenarioPreviewArtwork){
   const auto map=m_root+L"\\Textures\\Moon\\Moon_Orbiter_L8.jpg";
   if(FAILED(f->CreateDecoderFromFilename(map.c_str(),nullptr,GENERIC_READ,WICDecodeMetadataCacheOnLoad,&d)))return false;
 }
 ComPtr<IWICBitmapFrameDecode> fr;
 if(FAILED(d->GetFrame(0,&fr)))return false;
 ComPtr<IWICBitmapSource> source=fr;
 ComPtr<IWICBitmapScaler> scaler;
 if(!m_scenarioPreviewArtwork){
   if(FAILED(f->CreateBitmapScaler(&scaler)))return false;
   if(FAILED(scaler->Initialize(fr.Get(),1024,512,WICBitmapInterpolationModeFant)))return false;
   source=scaler;
 }
 ComPtr<IWICFormatConverter> cv;
 if(FAILED(f->CreateFormatConverter(&cv)))return false;
 if(FAILED(cv->Initialize(source.Get(),GUID_WICPixelFormat32bppPBGRA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom)))return false;
 return SUCCEEDED(m_d2dContext->CreateBitmapFromWicBitmap(cv.Get(),nullptr,&m_scenarioPreview));
}

void Renderer::UpdateCB(){
 // Faithful port of Orbiter Camera::SetRelPos() target-relative camera geometry.
 // Orbiter stores two unrestricted angles, normalizes each to 2*pi, constructs
 // the complete relative rotation matrix from them, then derives camera position
 // from that matrix.  Critically, camera up comes from the same rotation matrix;
 // it is NOT reconstructed from a fixed world-up vector.  This is what allows
 // continuous pole crossings without a LookAt/world-up discontinuity.
 XMMATRIX model=XMMatrixScaling(1737.4f,1737.4f,1737.4f);
 const float sinph=sinf(m_yaw),   cosph=cosf(m_yaw);
 const float sinth=sinf(m_pitch), costh=cosf(m_pitch);

 // Orbiter rrot (Camera.cpp::SetRelPos):
 // [ cosph,  sinph*sinth, -sinph*costh ]
 // [ 0,      costh,        sinth       ]
 // [ sinph, -cosph*sinth,  cosph*costh ]
 // rpos = third column * (-distance).
 XMVECTOR cam=XMVectorSet(
     m_distanceKm*sinph*costh,
    -m_distanceKm*sinth,
    -m_distanceKm*cosph*costh,1.0f);
 XMVECTOR up=XMVectorSet(sinph*sinth,costh,-cosph*sinth,0.0f);
 XMMATRIX view=XMMatrixLookAtRH(cam,XMVectorZero(),up);
 XMMATRIX proj=XMMatrixPerspectiveFovRH(XMConvertToRadians(m_fov),float(m_width)/m_height,1.0f,1000000.0f);
 auto moon=m_state.moonSSB();auto sd=m_state.sunSSB-moon;double n=sqrt(sd.x*sd.x+sd.y*sd.y+sd.z*sd.z);SceneCB c{};XMStoreFloat4x4(&c.mvp,XMMatrixTranspose(model*view*proj));XMStoreFloat4x4(&c.model,XMMatrixTranspose(model));c.sunDir={(float)(sd.x/n),(float)(sd.y/n),(float)(sd.z/n),0};memcpy(m_cbPtr,&c,sizeof(c));}

void Renderer::DrawHudText(){
 ID3D11Resource* wrapped[]={m_wrappedBack[m_frame].Get()};
 m_d3d11On12->AcquireWrappedResources(wrapped,1);
 m_d2dContext->SetTarget(m_d2dTarget[m_frame].Get());
 m_d2dContext->BeginDraw();

 auto drawFmt=[&](const std::wstring& text,float x,float y,IDWriteTextFormat* fmt,ID2D1Brush* brush,bool right=false){
   D2D1_RECT_F rc=right?D2D1::RectF(0,y,x,(float)m_height):D2D1::RectF(x,y,(float)m_width,(float)m_height);
   m_d2dContext->DrawTextW(text.c_str(),(UINT32)text.size(),fmt,rc,brush,D2D1_DRAW_TEXT_OPTIONS_NONE,DWRITE_MEASURING_MODE_NATURAL);
 };
 auto draw=[&](const std::wstring&t,float x,float y,bool right){drawFmt(t,x,y,right?m_textRight.Get():m_textLeft.Get(),m_textBrush.Get(),right);};

 if(m_appState!=AppState::Simulation){
   // Artwork is intentionally UI-free.  Downsample the 8K plate with high quality filtering.
   if(m_menuBackground){
     auto sz=m_menuBackground->GetSize();
     float scale=std::max((float)m_width/sz.width,(float)m_height/sz.height);
     float dw=sz.width*scale,dh=sz.height*scale;
     D2D1_RECT_F dst=D2D1::RectF(((float)m_width-dw)*.5f,((float)m_height-dh)*.5f,((float)m_width+dw)*.5f,((float)m_height+dh)*.5f);
     m_d2dContext->DrawBitmap(m_menuBackground.Get(),dst,1.0f,D2D1_INTERPOLATION_MODE_HIGH_QUALITY_CUBIC);
   }

   Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> green,greenGlow,greenGlow2,greenFill,glass,glassInner,glassHighlight,white,muted;
   m_d2dContext->CreateSolidColorBrush(D2D1::ColorF(0.18f,1.0f,0.53f,1.00f),&green);
   m_d2dContext->CreateSolidColorBrush(D2D1::ColorF(0.10f,1.0f,0.45f,0.11f),&greenGlow);
   m_d2dContext->CreateSolidColorBrush(D2D1::ColorF(0.10f,1.0f,0.45f,0.24f),&greenGlow2);
   m_d2dContext->CreateSolidColorBrush(D2D1::ColorF(0.02f,0.48f,0.20f,0.24f),&greenFill);
   m_d2dContext->CreateSolidColorBrush(D2D1::ColorF(0.015f,0.085f,0.060f,0.28f),&glass);
   m_d2dContext->CreateSolidColorBrush(D2D1::ColorF(0.040f,0.140f,0.095f,0.10f),&glassInner);
   m_d2dContext->CreateSolidColorBrush(D2D1::ColorF(0.42f,0.86f,0.66f,0.26f),&glassHighlight);
   m_d2dContext->CreateSolidColorBrush(D2D1::ColorF(0.96f,0.98f,0.98f,1.0f),&white);
   m_d2dContext->CreateSolidColorBrush(D2D1::ColorF(0.18f,0.48f,0.34f,0.68f),&muted);

   // Vertical gradient fills give each control actual smoked-glass depth rather than a flat wireframe look.
   Microsoft::WRL::ComPtr<ID2D1GradientStopCollection> glassStops,hotStops;
   // Reference.png uses floating smoked glass, not a dark backing slab.  Keep the
   // panels transparent enough that the star field remains visible through them.
   D2D1_GRADIENT_STOP gs[4]={
     {0.00f,D2D1::ColorF(0.08f,0.20f,0.15f,0.18f)},
     {0.18f,D2D1::ColorF(0.025f,0.11f,0.075f,0.22f)},
     {0.62f,D2D1::ColorF(0.008f,0.035f,0.026f,0.30f)},
     {1.00f,D2D1::ColorF(0.002f,0.012f,0.009f,0.36f)}};
   D2D1_GRADIENT_STOP hs[4]={
     {0.00f,D2D1::ColorF(0.09f,0.95f,0.39f,0.34f)},
     {0.18f,D2D1::ColorF(0.025f,0.48f,0.18f,0.34f)},
     {0.62f,D2D1::ColorF(0.006f,0.15f,0.065f,0.40f)},
     {1.00f,D2D1::ColorF(0.002f,0.035f,0.016f,0.46f)}};
   m_d2dContext->CreateGradientStopCollection(gs,4,D2D1_GAMMA_2_2,D2D1_EXTEND_MODE_CLAMP,&glassStops);
   m_d2dContext->CreateGradientStopCollection(hs,4,D2D1_GAMMA_2_2,D2D1_EXTEND_MODE_CLAMP,&hotStops);
   Microsoft::WRL::ComPtr<ID2D1LinearGradientBrush> glassGradient,hotGradient;
   m_d2dContext->CreateLinearGradientBrush(D2D1::LinearGradientBrushProperties(D2D1::Point2F(0,0),D2D1::Point2F(0,64)),glassStops.Get(),&glassGradient);
   m_d2dContext->CreateLinearGradientBrush(D2D1::LinearGradientBrushProperties(D2D1::Point2F(0,0),D2D1::Point2F(0,64)),hotStops.Get(),&hotGradient);

   const auto mainLayout=OrbitX::UI::BuildMainMenuLayout((float)m_width,(float)m_height);
   const auto subLayout =OrbitX::UI::BuildSubmenuLayout((float)m_width,(float)m_height);
   const float scale=(m_appState==AppState::MainMenu)?mainLayout.scale:subLayout.scale;
   const float ox=(m_appState==AppState::MainMenu)?mainLayout.offsetX:subLayout.offsetX;
   const float oy=(m_appState==AppState::MainMenu)?mainLayout.offsetY:subLayout.offsetY;
   D2D1_MATRIX_3X2_F uiTransform{scale,0,0,scale,ox,oy};
   m_d2dContext->SetTransform(uiTransform);

   // Reference.png has NO menu backing rectangle of any kind.  Do not tint, shade,
   // fill, or darken the left side here: the single continuous artwork plate is drawn
   // once above, and only floating glass controls/instrumentation are composited over it.
   m_d2dContext->DrawLine(D2D1::Point2F(42,0),D2D1::Point2F(42,930),muted.Get(),1.0f);
   m_d2dContext->DrawLine(D2D1::Point2F(43,54),D2D1::Point2F(43,160),greenGlow2.Get(),3.0f);
   for(int di=0;di<6;di++)m_d2dContext->FillEllipse(D2D1::Ellipse(D2D1::Point2F(24.0f,760.0f+di*14.0f),1.4f,1.4f),di<2?green.Get():muted.Get());

   auto tracked=[&](const wchar_t* text,float x,float y,IDWriteTextFormat* fmt,ID2D1Brush* brush,float spacing){
     Microsoft::WRL::ComPtr<IDWriteTextLayout> baseLayout;
     if(SUCCEEDED(m_dwriteFactory->CreateTextLayout(text,(UINT32)wcslen(text),fmt,1000.0f,120.0f,&baseLayout))){
       Microsoft::WRL::ComPtr<IDWriteTextLayout1> layout1;
       if(SUCCEEDED(baseLayout.As(&layout1))){DWRITE_TEXT_RANGE range{0,(UINT32)wcslen(text)};layout1->SetCharacterSpacing(0.0f,spacing,0.0f,range);}
       m_d2dContext->DrawTextLayout(D2D1::Point2F(x,y),baseLayout.Get(),brush,D2D1_DRAW_TEXT_OPTIONS_NONE);
     }
   };
   // Center labels and chevrons using measured DirectWrite line-box geometry.
   auto centeredButtonText=[&](const wchar_t* value,const OrbitX::UI::RectF& r,float x,
                               IDWriteTextFormat* fmt,ID2D1Brush* brush,float spacing,
                               bool centerHorizontally=false){
     Microsoft::WRL::ComPtr<IDWriteTextLayout> layout;
     if(FAILED(m_dwriteFactory->CreateTextLayout(value,(UINT32)wcslen(value),fmt,
         1000.0f,120.0f,&layout)))return;
     Microsoft::WRL::ComPtr<IDWriteTextLayout1> layout1;
     if(SUCCEEDED(layout.As(&layout1))){
       DWRITE_TEXT_RANGE range{0,(UINT32)wcslen(value)};
       layout1->SetCharacterSpacing(0.0f,spacing,0.0f,range);
     }
     DWRITE_TEXT_METRICS metrics{};
     if(FAILED(layout->GetMetrics(&metrics)))return;
     const float y=r.top+(r.Height()-metrics.height)*0.5f-metrics.top;
     const float drawX=centerHorizontally ? r.left+(r.Width()-metrics.width)*0.5f : x;
     m_d2dContext->DrawTextLayout(D2D1::Point2F(drawX,y),layout.Get(),brush,
         D2D1_DRAW_TEXT_OPTIONS_NONE);
   };
   auto makeButton=[&](const OrbitX::UI::RectF& r,float inset){
     Microsoft::WRL::ComPtr<ID2D1PathGeometry> geo; m_d2dFactory->CreatePathGeometry(&geo);
     Microsoft::WRL::ComPtr<ID2D1GeometrySink> sink; geo->Open(&sink);
     float l=r.left+inset,rr=r.right-inset,t=r.top+inset,b=r.bottom-inset,c=std::max(7.0f,18.0f-inset);
     sink->BeginFigure(D2D1::Point2F(l,t),D2D1_FIGURE_BEGIN_FILLED);
     sink->AddLine(D2D1::Point2F(rr-c,t)); sink->AddLine(D2D1::Point2F(rr,t+c));
     sink->AddLine(D2D1::Point2F(rr,b-c)); sink->AddLine(D2D1::Point2F(rr-c,b));
     sink->AddLine(D2D1::Point2F(l,b)); sink->EndFigure(D2D1_FIGURE_END_CLOSED); sink->Close(); return geo;
   };

   // v0.25 logo swap: use the locked transparent OrbitX logo asset directly so
   // the on-screen mark matches the approved artwork exactly. Everything else in
   // the menu remains unchanged.
   if(m_menuLogo){
     const auto sz=m_menuLogo->GetSize();
     const float logoLeft=72.0f;
     const float logoTop=36.0f;
     const float logoWidth=640.0f;
     const float logoHeight=logoWidth*(sz.height/std::max(1.0f,sz.width));
     const D2D1_RECT_F dst=D2D1::RectF(logoLeft,logoTop,logoLeft+logoWidth,logoTop+logoHeight);
     m_d2dContext->DrawBitmap(m_menuLogo.Get(),dst,1.0f,D2D1_INTERPOLATION_MODE_HIGH_QUALITY_CUBIC,nullptr);
   }
   // Main-menu and submenu BACK controls share the SCENARIOS button renderer.
   auto drawMenuButton=[&](const OrbitX::UI::RectF& r,const wchar_t* label,bool hot,bool showHoverArrow=true,IDWriteTextFormat* labelFormat=nullptr){
       auto outer=makeButton(r,0.0f), mid=makeButton(r,2.0f), inner=makeButton(r,5.0f);
       ID2D1Brush* fillBrush = (hot && hotGradient.Get()!=nullptr) ? static_cast<ID2D1Brush*>(hotGradient.Get()) : (glassGradient.Get()!=nullptr ? static_cast<ID2D1Brush*>(glassGradient.Get()) : static_cast<ID2D1Brush*>(glass.Get()));
       if(glassGradient.Get()!=nullptr){ glassGradient->SetStartPoint(D2D1::Point2F(0,r.top)); glassGradient->SetEndPoint(D2D1::Point2F(0,r.bottom)); }
       if(hotGradient.Get()!=nullptr){ hotGradient->SetStartPoint(D2D1::Point2F(0,r.top)); hotGradient->SetEndPoint(D2D1::Point2F(0,r.bottom)); }
       m_d2dContext->FillGeometry(mid.Get(),fillBrush);
       m_d2dContext->FillGeometry(inner.Get(),hot?greenGlow.Get():glassInner.Get());
       if(hot){
         m_d2dContext->DrawGeometry(outer.Get(),greenGlow.Get(),12.0f);
         m_d2dContext->DrawGeometry(outer.Get(),greenGlow2.Get(),5.0f);
         m_d2dContext->DrawGeometry(outer.Get(),green.Get(),1.6f);
         m_d2dContext->DrawGeometry(inner.Get(),glassHighlight.Get(),0.9f);
       }else{
         m_d2dContext->DrawGeometry(outer.Get(),muted.Get(),0.9f);
         m_d2dContext->DrawGeometry(inner.Get(),glassHighlight.Get(),0.65f);
       }
       m_d2dContext->DrawLine(D2D1::Point2F(r.left+8,r.top+5),D2D1::Point2F(r.right-34,r.top+5),hot?green.Get():glassHighlight.Get(),hot?1.6f:0.8f);
       m_d2dContext->DrawLine(D2D1::Point2F(r.left+2,r.top+9),D2D1::Point2F(r.left+2,r.bottom-9),hot?green.Get():muted.Get(),hot?4.0f:1.4f);
       centeredButtonText(label,r,r.left+(mainLayout.textX-mainLayout.buttons[0].left),
           labelFormat?labelFormat:m_textMenu.Get(),white.Get(),labelFormat?2.5f:3.4f,
           wcscmp(label,L"LAUNCH")==0);
       if(hot&&showHoverArrow)centeredButtonText(L">>",r,r.right-62,m_textMenu.Get(),green.Get(),0.0f);
   };
   if(m_appState==AppState::MainMenu){
     static const wchar_t* labels[]={L"SCENARIOS",L"PARAMETERS",L"VISUAL EFFECTS",L"MODULES",L"GRAPHICS",L"HARDWARE",L"EXTRA",L"EXIT"};
     // Mouse hover owns the visible selection while the cursor is over a button. Keyboard selection is the fallback.
     const int visualSelection=m_mouseNavigation?m_hoverSelection:m_menuSelection;
     for(int i=0;i<OrbitX::UI::kMainMenuCount;i++){
       const auto&r=mainLayout.buttons[i]; bool hot=(i==visualSelection);
       drawMenuButton(r,labels[i],hot);
     }
   }else{
     // Submenus share the main menu's button position, styling, and hover treatment.
     drawMenuButton(subLayout.back,L"BACK",m_mouseNavigation && m_hoverSelection==0,false);
     centeredButtonText(L"<<",subLayout.back,subLayout.back.left+18,m_textMenu.Get(),green.Get(),0.0f);
     const wchar_t* title=L"";
     if(m_appState==AppState::ScenarioSelect)title=L"SCENARIOS"; else if(m_appState==AppState::Parameters)title=L"PARAMETERS"; else if(m_appState==AppState::VisualEffects)title=L"VISUAL EFFECTS"; else if(m_appState==AppState::Modules)title=L"MODULES"; else if(m_appState==AppState::Graphics)title=L"GRAPHICS"; else if(m_appState==AppState::Joystick)title=L"HARDWARE"; else if(m_appState==AppState::Extra)title=L"EXTRA";
     if(m_appState==AppState::ScenarioSelect){
       // Underline only the SCENARIOS heading, retaining the existing header font.
       Microsoft::WRL::ComPtr<IDWriteTextLayout> heading;
       const UINT32 titleLength=(UINT32)wcslen(title);
       if(SUCCEEDED(m_dwriteFactory->CreateTextLayout(title,titleLength,m_textScenarioTitle.Get(),1000.0f,120.0f,&heading))){
         const DWRITE_TEXT_RANGE headingRange{0,titleLength};
         heading->SetUnderline(TRUE,headingRange);
         Microsoft::WRL::ComPtr<IDWriteTextLayout1> headingSpacing;
         if(SUCCEEDED(heading.As(&headingSpacing)))headingSpacing->SetCharacterSpacing(0.0f,3.0f,0.0f,headingRange);
         m_d2dContext->DrawTextLayout(D2D1::Point2F(subLayout.title.left,subLayout.title.top),
                                       heading.Get(),white.Get(),D2D1_DRAW_TEXT_OPTIONS_NONE);
       }
     }else{
       tracked(title,subLayout.title.left,subLayout.title.top,m_textMenu.Get(),white.Get(),3.0f);
     }
     if(m_appState==AppState::ScenarioSelect){
       // Expandable category and nested selectable scenario; no decorative planet icons.
       drawMenuButton(subLayout.solarSystem,L"SOLAR SYSTEM",m_scenarioHover==0,false);
       centeredButtonText(m_solarSystemExpanded?L"v":L">",subLayout.solarSystem,subLayout.solarSystem.right-47,m_textMenu.Get(),green.Get(),0.0f);
       if(m_solarSystemExpanded){
         // Indented glass panel uses the exact same illuminated hover style as the main menu.
         drawMenuButton(subLayout.moonView,L"MOON VIEW",m_scenarioHover==1||m_scenarioSelected,true,m_textScenarioButton.Get());
         m_d2dContext->DrawLine(D2D1::Point2F(72,subLayout.solarSystem.bottom+6),
             D2D1::Point2F(72,(subLayout.moonView.top+subLayout.moonView.bottom)*0.5f),muted.Get(),1.2f);
         m_d2dContext->DrawLine(D2D1::Point2F(72,(subLayout.moonView.top+subLayout.moonView.bottom)*0.5f),
             D2D1::Point2F(subLayout.moonView.left-4,(subLayout.moonView.top+subLayout.moonView.bottom)*0.5f),muted.Get(),1.2f);
         // Disabled future scenarios match the concept art, without showing planet icons
         // or registering click targets until their actual scenarios exist.
         auto futureScenario=[&](const OrbitX::UI::RectF& r,const wchar_t* name){
           auto g=makeButton(r,0.0f);
           m_d2dContext->FillGeometry(g.Get(),glass.Get());
           m_d2dContext->DrawGeometry(g.Get(),muted.Get(),0.9f);
           centeredButtonText(name,r,r.left+(mainLayout.textX-mainLayout.buttons[0].left),m_textScenarioButton.Get(),muted.Get(),2.2f);
           const float midY=(r.top+r.bottom)*0.5f;
           m_d2dContext->DrawLine(D2D1::Point2F(72,midY),
               D2D1::Point2F(r.left-4,midY),muted.Get(),0.9f);
         };
         futureScenario(subLayout.earthView,L"EARTH VIEW");
         futureScenario(subLayout.marsView,L"MARS VIEW");
         m_d2dContext->DrawLine(D2D1::Point2F(72,(subLayout.moonView.top+subLayout.moonView.bottom)*0.5f),
             D2D1::Point2F(72,(subLayout.marsView.top+subLayout.marsView.bottom)*0.5f),muted.Get(),0.9f);
       }
       if(m_solarSystemExpanded&&m_scenarioSelected){
         // Floating dark-smoked-glass information panel with green glass edging.
         const auto& p=subLayout.description;
         Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> panel,previewShade,detail;
         m_d2dContext->CreateSolidColorBrush(D2D1::ColorF(0.075f,0.095f,0.10f,0.86f),&panel);
         m_d2dContext->CreateSolidColorBrush(D2D1::ColorF(0.015f,0.027f,0.035f,0.92f),&previewShade);
         m_d2dContext->CreateSolidColorBrush(D2D1::ColorF(0.57f,0.71f,0.70f,0.88f),&detail);
         auto pg=makeButton(p,0.0f);
         m_d2dContext->FillGeometry(pg.Get(),panel.Get());
         m_d2dContext->DrawGeometry(pg.Get(),greenGlow.Get(),9.0f);
         m_d2dContext->DrawGeometry(pg.Get(),green.Get(),1.7f);
         // The scenario name is the sole panel heading; remove the redundant "SCENARIO" label.
         tracked(L"MOON VIEW",p.left+34,p.top+30,m_textMenu.Get(),green.Get(),4.5f);
         m_d2dContext->DrawLine(D2D1::Point2F(p.left+38,p.top+85),
             D2D1::Point2F(p.right-38,p.top+85),muted.Get(),1.0f);
         const auto& v=subLayout.preview;
         auto previewGeo=makeButton(v,0.0f);
         m_d2dContext->FillGeometry(previewGeo.Get(),previewShade.Get());
         if(m_scenarioPreview){
           // Clip thumbnail inside the bevelled preview viewport.
           m_d2dContext->PushLayer(D2D1::LayerParameters1(
               D2D1::InfiniteRect(),previewGeo.Get()),nullptr);
           m_d2dContext->DrawBitmap(m_scenarioPreview.Get(),
               D2D1::RectF(v.left,v.top,v.right,v.bottom),1.0f,
               D2D1_INTERPOLATION_MODE_HIGH_QUALITY_CUBIC,
               m_scenarioPreviewArtwork?
                 D2D1::RectF(0,0,m_scenarioPreview->GetSize().width,m_scenarioPreview->GetSize().height):
                 D2D1::RectF(120,110,900,370));
           m_d2dContext->PopLayer();
         }
         m_d2dContext->DrawGeometry(previewGeo.Get(),glassHighlight.Get(),1.1f);
         auto panelText=[&](const wchar_t* value,float top){
           const auto rc=D2D1::RectF(p.left+36,top,p.right-36,p.bottom-76);
           m_d2dContext->DrawTextW(value,(UINT32)wcslen(value),m_textLeft.Get(),rc,
               white.Get(),D2D1_DRAW_TEXT_OPTIONS_NONE,DWRITE_MEASURING_MODE_NATURAL);
         };
         panelText(L"Observe the Moon from OrbitX's current\nexternal-view prototype.",p.top+285);
         panelText(L"This scenario showcases the lunar rendering\npipeline, camera controls, and basic scene\npresentation. More solar-system flyby and\nsurface-view scenarios will be added here\nover time.",p.top+378);
         m_d2dContext->DrawLine(D2D1::Point2F(p.left+32,p.bottom-63),
             D2D1::Point2F(p.right-30,p.bottom-63),muted.Get(),1.0f);
         tracked(L"SOLAR SYSTEM",p.left+38,p.bottom-47,m_textLeft.Get(),detail.Get(),2.0f);
         for(int i=0;i<4;i++)m_d2dContext->FillRectangle(
             D2D1::RectF(p.right-72+i*11,p.bottom-41,p.right-67+i*11,p.bottom-36),
             i==0?green.Get():muted.Get());
         const auto& launch=subLayout.launch;
         drawMenuButton(launch,L"LAUNCH",m_scenarioHover==2);
       }
     }else if(m_appState==AppState::Graphics){
       drawFmt(L"DISPLAY MODE",subLayout.title.left,subLayout.graphicsWindowed.top-65,m_textLeft.Get(),white.Get());
       auto a=makeButton(subLayout.graphicsWindowed,0.0f),b=makeButton(subLayout.graphicsFullscreen,0.0f);
       m_d2dContext->FillGeometry(a.Get(),!m_fullscreen?greenFill.Get():glass.Get());m_d2dContext->FillGeometry(b.Get(),m_fullscreen?greenFill.Get():glass.Get());
       m_d2dContext->DrawGeometry(a.Get(),!m_fullscreen?green.Get():muted.Get(),1.5f);m_d2dContext->DrawGeometry(b.Get(),m_fullscreen?green.Get():muted.Get(),1.5f);
       centeredButtonText(L"WINDOWED",subLayout.graphicsWindowed,subLayout.graphicsWindowed.left+24,m_textLeft.Get(),!m_fullscreen?green.Get():white.Get(),0.8f);
       centeredButtonText(L"FULL SCREEN",subLayout.graphicsFullscreen,subLayout.graphicsFullscreen.left+24,m_textLeft.Get(),m_fullscreen?green.Get():white.Get(),0.8f);
       drawFmt(L"Changes apply immediately and are saved on exit.",subLayout.title.left,subLayout.graphicsWindowed.bottom+30,m_textLeft.Get(),white.Get());
     }else drawFmt(L"Placeholder - interface coming next",subLayout.title.left,subLayout.title.bottom+45,m_textLeft.Get(),white.Get());
   }
   m_d2dContext->SetTransform(D2D1::Matrix3x2F::Identity());
 }else{
   constexpr float left=12,rightPad=12,line=22,top=8;draw(L"ORBITX 0.25",left,top,false);draw(L"VIEW EXTERNAL",left,top+line,false);draw(L"FRAME ICRF",left,top+2*line,false);wchar_t buf[96];swprintf_s(buf,L"DIST %.1F KM",m_distanceKm);draw(buf,left,top+3*line,false);swprintf_s(buf,L"FOV %.1F DEG",m_fov);draw(buf,left,top+4*line,false);draw(L"2000 JAN 01 12:00:00 TDB",(float)m_width-rightPad,top,true);draw(L"TIME 1X",(float)m_width-rightPad,top+line,true);
 }
 HRESULT hr=m_d2dContext->EndDraw();
 m_d2dContext->SetTarget(nullptr);
 m_d3d11On12->ReleaseWrappedResources(wrapped,1);
 m_d3d11Context->Flush();
 if(FAILED(hr)&&hr!=D2DERR_RECREATE_TARGET)SetError("Direct2D EndDraw: "+HrText(hr));
}

void Renderer::Update(float){UpdateCB();}
void Renderer::Render(){
 m_alloc[m_frame]->Reset();m_cmd->Reset(m_alloc[m_frame].Get(),m_scenePSO.Get());
 D3D12_RESOURCE_BARRIER b{};b.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;b.Transition.pResource=m_back[m_frame].Get();b.Transition.StateBefore=D3D12_RESOURCE_STATE_PRESENT;b.Transition.StateAfter=D3D12_RESOURCE_STATE_RENDER_TARGET;b.Transition.Subresource=D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;m_cmd->ResourceBarrier(1,&b);
 auto r=m_rtvHeap->GetCPUDescriptorHandleForHeapStart();r.ptr+=m_frame*m_rtvStride;auto ds=m_dsvHeap->GetCPUDescriptorHandleForHeapStart();float clear[]={0,0,0,1};m_cmd->ClearRenderTargetView(r,clear,0,nullptr);m_cmd->ClearDepthStencilView(ds,D3D12_CLEAR_FLAG_DEPTH,1,0,0,nullptr);m_cmd->OMSetRenderTargets(1,&r,FALSE,&ds);
 D3D12_VIEWPORT vp{0,0,(float)m_width,(float)m_height,0,1};D3D12_RECT sc{0,0,(LONG)m_width,(LONG)m_height};m_cmd->RSSetViewports(1,&vp);m_cmd->RSSetScissorRects(1,&sc);m_cmd->SetGraphicsRootSignature(m_sceneRoot.Get());ID3D12DescriptorHeap*h[]={m_srvHeap.Get()};m_cmd->SetDescriptorHeaps(1,h);m_cmd->SetGraphicsRootConstantBufferView(0,m_cb->GetGPUVirtualAddress());m_cmd->SetGraphicsRootDescriptorTable(1,m_srvHeap->GetGPUDescriptorHandleForHeapStart());m_cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);m_cmd->IASetVertexBuffers(0,1,&m_vbv);m_cmd->IASetIndexBuffer(&m_ibv);m_cmd->DrawIndexedInstanced(m_indexCount,1,0,0,0);
 // Leave the back buffer in RENDER_TARGET. D3D11On12/Direct2D draws the vector HUD
 // and ReleaseWrappedResources performs the final RENDER_TARGET -> PRESENT transition.
 m_cmd->Close();ID3D12CommandList*l[]={m_cmd.Get()};m_queue->ExecuteCommandLists(1,l);
 DrawHudText();
 m_swap->Present(1,0);MoveToNextFrame();
}
void Renderer::WaitForGPU(){UINT64 v=++m_fenceValue[m_frame];m_queue->Signal(m_fence.Get(),v);if(m_fence->GetCompletedValue()<v){m_fence->SetEventOnCompletion(v,m_fenceEvent);WaitForSingleObject(m_fenceEvent,5000);}}
void Renderer::MoveToNextFrame(){UINT64 v=++m_fenceValue[m_frame];m_queue->Signal(m_fence.Get(),v);m_frame=m_swap->GetCurrentBackBufferIndex();if(m_fence->GetCompletedValue()<m_fenceValue[m_frame]){m_fence->SetEventOnCompletion(m_fenceValue[m_frame],m_fenceEvent);WaitForSingleObject(m_fenceEvent,5000);}m_fenceValue[m_frame]=v;}
void Renderer::Shutdown(){if(m_queue)WaitForGPU();if(m_cb)m_cb->Unmap(0,nullptr);if(m_fenceEvent)CloseHandle(m_fenceEvent);m_fenceEvent=nullptr;}
void Renderer::Resize(UINT width,UINT height){
 if(!m_swap||width==0||height==0||(width==m_width&&height==m_height))return;

 // ResizeBuffers requires every direct and indirect reference to the old swap-chain
 // buffers to be gone. With D3D11On12/D2D the destruction order matters.
 // First finish all work already submitted by both APIs.
 if(m_d2dContext){
  m_d2dContext->SetTarget(nullptr);
  m_d2dContext->Flush(nullptr,nullptr);
 }
 if(m_d3d11Context)m_d3d11Context->Flush();
 WaitForGPU();

 // Now release the complete D2D -> D3D11On12 -> D3D12 back-buffer chain.
 for(UINT i=0;i<FrameCount;i++)m_d2dTarget[i].Reset();
 if(m_d3d11Context){
  m_d3d11Context->ClearState();
  m_d3d11Context->Flush();
 }
 for(UINT i=0;i<FrameCount;i++)m_wrappedBack[i].Reset();
 if(m_d3d11Context)m_d3d11Context->Flush();

 // D3D11On12 can submit release/state-transition work during the flush above.
 // Wait a second time before dropping the native D3D12 references.
 WaitForGPU();
 for(UINT i=0;i<FrameCount;i++)m_back[i].Reset();
 m_depth.Reset();

 HRESULT hr=m_swap->ResizeBuffers(FrameCount,width,height,DXGI_FORMAT_R8G8B8A8_UNORM,0);
 if(FAILED(hr)){
  SetError("ResizeBuffers: "+HrText(hr));
  return;
 }

 m_width=width;m_height=height;m_frame=m_swap->GetCurrentBackBufferIndex();
 auto r=m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
 for(UINT i=0;i<FrameCount;i++){
  hr=m_swap->GetBuffer(i,IID_PPV_ARGS(&m_back[i]));
  if(FAILED(hr)){SetError("GetBuffer after resize: "+HrText(hr));return;}
  m_device->CreateRenderTargetView(m_back[i].Get(),nullptr,r);r.ptr+=m_rtvStride;
 }
 if(!CreateDepth()||!CreateHudBackBufferTargets()){SetError("Failed to recreate resize-dependent render targets");return;}
 for(UINT i=0;i<FrameCount;i++)m_fenceValue[i]=m_fence->GetCompletedValue();
 std::ofstream log(std::filesystem::path(m_root)/L"Logs"/L"OrbitX.log",std::ios::app);
 if(log)log<<"Resize: client="<<m_width<<"x"<<m_height<<" swapchain="<<m_width<<"x"<<m_height<<" aspect="<<(double(m_width)/double(m_height))<<"\n";
}
void Renderer::OnKeyDown(WPARAM k){
 if(k==VK_ESCAPE){
   // In simulation, Escape exits the active scenario back to the OrbitX main menu.
   // In menus/submenus, navigation is intentionally handled only by the on-screen BACK control.
   if(m_appState==AppState::Simulation){ OnMouseUp(); m_appState=AppState::MainMenu; m_hoverSelection=-1; }
   return;
 }
 if(m_appState==AppState::Simulation)return;
 if(m_appState==AppState::MainMenu){if(k==VK_UP){m_mouseNavigation=false;m_menuSelection=(m_menuSelection+OrbitX::UI::kMainMenuCount-1)%OrbitX::UI::kMainMenuCount;}else if(k==VK_DOWN){m_mouseNavigation=false;m_menuSelection=(m_menuSelection+1)%OrbitX::UI::kMainMenuCount;}else if(k==VK_RETURN){if(m_menuSelection==OrbitX::UI::kMainMenuCount-1){PostMessage(m_hwnd,WM_CLOSE,0,0);return;}static const AppState states[]={AppState::ScenarioSelect,AppState::Parameters,AppState::VisualEffects,AppState::Modules,AppState::Graphics,AppState::Joystick,AppState::Extra};m_appState=states[m_menuSelection];if(m_appState==AppState::ScenarioSelect){m_solarSystemExpanded=false;m_scenarioSelected=false;m_scenarioHover=-1;}m_hoverSelection=-1;m_mouseNavigation=false;}return;}
 // A scenario is launched only with the explicit LAUNCH control, never by selecting it.
}
void Renderer::OnLeftClick(int x,int y){
 if(m_appState==AppState::Simulation)return;
 if(m_appState==AppState::MainMenu){
   const auto layout=OrbitX::UI::BuildMainMenuLayout((float)m_width,(float)m_height);
   const int hit=OrbitX::UI::HitTestMainMenu(layout,(float)x,(float)y);
   if(hit>=0){m_menuSelection=hit;OnKeyDown(VK_RETURN);} return;
 }
 const auto layout=OrbitX::UI::BuildSubmenuLayout((float)m_width,(float)m_height);
 if(OrbitX::UI::HitTestButton(layout.back,18.0f,(float)x,(float)y,layout.scale,layout.offsetX,layout.offsetY)){m_appState=AppState::MainMenu;m_hoverSelection=-1;m_scenarioHover=-1;m_mouseNavigation=false;return;}
 if(m_appState==AppState::ScenarioSelect){
   const auto hit=[&](const OrbitX::UI::RectF& r){return OrbitX::UI::HitTestButton(r,18.0f,(float)x,(float)y,layout.scale,layout.offsetX,layout.offsetY);};
   if(hit(layout.solarSystem)){m_solarSystemExpanded=!m_solarSystemExpanded;m_scenarioHover=-1;return;}
   if(m_solarSystemExpanded&&hit(layout.moonView)){m_scenarioSelected=!m_scenarioSelected;m_scenarioHover=1;return;}
   if(m_solarSystemExpanded&&m_scenarioSelected&&hit(layout.launch)){m_appState=AppState::Simulation;m_scenarioHover=-1;return;}
   return;
 }
 if(m_appState==AppState::Graphics){
   if(OrbitX::UI::HitTestButton(layout.graphicsWindowed,18.0f,(float)x,(float)y,layout.scale,layout.offsetX,layout.offsetY))SetFullscreen(false);
   else if(OrbitX::UI::HitTestButton(layout.graphicsFullscreen,18.0f,(float)x,(float)y,layout.scale,layout.offsetX,layout.offsetY))SetFullscreen(true);
 }
}


void Renderer::SetFullscreen(bool enabled){
 if(enabled==m_fullscreen)return;
 if(enabled){
   m_windowedStyle=(DWORD)GetWindowLongPtr(m_hwnd,GWL_STYLE);m_windowedPlacement.length=sizeof(WINDOWPLACEMENT);GetWindowPlacement(m_hwnd,&m_windowedPlacement);
   MONITORINFO mi{sizeof(mi)};GetMonitorInfo(MonitorFromWindow(m_hwnd,MONITOR_DEFAULTTONEAREST),&mi);
   SetWindowLongPtr(m_hwnd,GWL_STYLE,(m_windowedStyle&~WS_OVERLAPPEDWINDOW)|WS_POPUP);
   SetWindowPos(m_hwnd,HWND_TOP,mi.rcMonitor.left,mi.rcMonitor.top,mi.rcMonitor.right-mi.rcMonitor.left,mi.rcMonitor.bottom-mi.rcMonitor.top,SWP_FRAMECHANGED|SWP_NOOWNERZORDER);
   m_fullscreen=true;
 }else{
   SetWindowLongPtr(m_hwnd,GWL_STYLE,m_windowedStyle);SetWindowPlacement(m_hwnd,&m_windowedPlacement);SetWindowPos(m_hwnd,nullptr,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_FRAMECHANGED);m_fullscreen=false;
 }
}
void Renderer::OnMouseDown(int,int){
 if(m_appState!=AppState::Simulation||m_drag)return;
 m_drag=true;SetCapture(m_hwnd);ShowCursor(FALSE);
 GetCursorPos(&m_dragAnchorScreen);
}
void Renderer::OnMouseUp(){
 if(!m_drag)return;
 m_drag=false;if(GetCapture()==m_hwnd)ReleaseCapture();ShowCursor(TRUE);
}
void Renderer::OnMouseMove(int x,int y){
 if(m_appState==AppState::MainMenu&&!m_drag){
   const auto layout=OrbitX::UI::BuildMainMenuLayout((float)m_width,(float)m_height);
   m_mouseNavigation=true;
   m_hoverSelection=OrbitX::UI::HitTestMainMenu(layout,(float)x,(float)y);
   return;
 }
 if(m_appState!=AppState::Simulation&&!m_drag){
   const auto layout=OrbitX::UI::BuildSubmenuLayout((float)m_width,(float)m_height);
   m_mouseNavigation=true;
   const auto hit=[&](const OrbitX::UI::RectF& r){return OrbitX::UI::HitTestButton(r,18.0f,(float)x,(float)y,layout.scale,layout.offsetX,layout.offsetY);};
   m_hoverSelection=hit(layout.back)?0:-1;
   m_scenarioHover=-1;
   if(m_appState==AppState::ScenarioSelect){
     if(hit(layout.solarSystem))m_scenarioHover=0;
     else if(m_solarSystemExpanded&&hit(layout.moonView))m_scenarioHover=1;
     else if(m_solarSystemExpanded&&m_scenarioSelected&&hit(layout.launch))m_scenarioHover=2;
   }
   return;
 }
 if(!m_drag)return;
 POINT pt{};GetCursorPos(&pt);
 const int dx=pt.x-m_dragAnchorScreen.x,dy=pt.y-m_dragAnchorScreen.y;
 if(!(dx||dy))return;
 const float radiusKm=1737.4f;
 const float rdist=m_distanceKm/radiusKm;
 const float proximity=std::max(1.0e-6f,(rdist-1.0f)/rdist);
 const float orbitPerPixel=0.0025f*proximity;
 m_yaw-=(float)dx*orbitPerPixel;
 m_pitch-=(float)dy*orbitPerPixel;
 const float twoPi=XM_2PI;
 m_yaw=fmodf(m_yaw,twoPi);if(m_yaw<0.0f)m_yaw+=twoPi;
 m_pitch=fmodf(m_pitch,twoPi);if(m_pitch<0.0f)m_pitch+=twoPi;
 SetCursorPos(m_dragAnchorScreen.x,m_dragAnchorScreen.y);
}
void Renderer::OnMouseLeave(){if(m_appState!=AppState::Simulation&&!m_drag){m_hoverSelection=-1;m_scenarioHover=-1;m_mouseNavigation=false;}}

void Renderer::OnMouseWheel(short d){if(m_appState!=AppState::Simulation)return;m_distanceKm=std::clamp(m_distanceKm*(d>0?.90f:1.10f),1800.0f,500000.0f);}
}
