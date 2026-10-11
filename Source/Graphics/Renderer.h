#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3d11.h>
#include <d3d11on12.h>
#include <d2d1_3.h>
#include <dwrite.h>
#include <dwrite_1.h>
#include <dwrite_3.h>
#include <DirectXMath.h>
#include <vector>
#include <string>
#include "../Core/SimulationState.h"
#include "../Core/PlanetQuadtree.h"
#include <set>
#include <chrono>
#include <array>
#include "DdsTileLoader.h"
#include "../Core/FrameRateHistory.h"
#include "../Core/EnvironmentTrace.h"

namespace OrbitX::Graphics {
class Renderer {
public:
    bool Initialize(HWND hwnd, UINT width, UINT height, const std::wstring& runtimeRoot);
    void Resize(UINT width, UINT height);
    void Update(float dt);
    void Render();
    void Shutdown();
    void OnKeyDown(WPARAM key);
    void OnLeftClick(int x,int y);
    void OnMouseDown(int x,int y);
    void OnMouseUp();
    void OnMouseMove(int x,int y);
    void OnMouseLeave();
    void OnMouseWheel(short delta);
    void SetFullscreen(bool enabled);
    const std::string& LastError() const { return m_error; }
    bool IsFullscreen() const { return m_fullscreen; }
    void SetVSync(bool enabled);
    bool IsVSync() const { return m_vsync; }
private:
    static constexpr UINT FrameCount=2;
    struct Vertex { DirectX::XMFLOAT3 pos, normal; DirectX::XMFLOAT2 uv; };
    struct SceneCB { DirectX::XMFLOAT4X4 mvp; DirectX::XMFLOAT4X4 model; DirectX::XMFLOAT4 sunDir; };
    bool InitDevice(); bool InitAssets();
    bool CreateScenePipeline(); bool CreateDepth(); bool CreateConstantBuffer(); bool InitVectorText(); bool LoadBundledHeaderFont(); bool CreateHudBackBufferTargets(); bool LoadMenuBackground(); bool LoadMenuLogo(); bool LoadScenarioPreview();
    bool InitPlanetStreaming();
    bool LoadPlanetTile(Core::TileId id); // Startup roots only.
    struct UploadBatch;
    bool QueuePlanetTile(Core::TileId id,const DdsSurface& data,UploadBatch& batch);
    bool SubmitPlanetUploads(UploadBatch& batch);
    void CollectPlanetUploads();
    bool UpdatePlanet();
    bool BuildPlanetMesh();
    void DrawPlanet();
    void DrawPlanetTelemetry(float x,float y);
    void DrawPlanetLabels();
    void DrawScriptPanel();
    void StartLivestream();
    void StopLivestream(const char* reason);
    void TraceCamera(const char* event);
    void TraceFrameSnapshot();
    bool m_scriptPanelOpen=false;
    bool m_traceSession=false;
    int m_scriptHover=-1;
    std::wstring m_scriptStatus;
    uint64_t m_traceFrame=0;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_scriptGreen,m_scriptGlass,m_scriptWhite,m_scriptDisabled;
    std::array<Microsoft::WRL::ComPtr<IDWriteTextLayout>,20> m_tileLabelLayouts;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_tileLabelBrush;
    void DrawFrameRateGraph();
    void DrawCachedHudText(const std::wstring& text,float x,float y,bool right,float maxWidth=0);
    struct HudText {
        std::wstring text;
        Microsoft::WRL::ComPtr<IDWriteTextLayout> layout;
        float width=0,height=0;bool right=false;
    };
    std::array<HudText,16> m_hudText;
    size_t m_hudTextIndex=0;
    std::wstring m_telemetryText;
    std::chrono::steady_clock::time_point m_telemetryTime{};
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_graphLine,m_graphGrid;
    Microsoft::WRL::ComPtr<ID2D1PathGeometry> m_graphGeometry;
    double m_graphSampleTime=-1;UINT m_graphWidth=0,m_graphHeight=0;float m_graphCeiling=0;
    Core::FrameRateHistory m_frameHistory;
    bool m_showFrameGraph=true;
    std::chrono::steady_clock::time_point m_lastFrameStart{};
    void WaitForGPU(); void MoveToNextFrame(); void UpdateCB(); void DrawHudText();
    void SetError(const std::string& s);

    HWND m_hwnd{}; UINT m_width{},m_height{}; std::wstring m_root, m_surfaceRoot; std::string m_error;
    Microsoft::WRL::ComPtr<IDXGIFactory6> m_factory; Microsoft::WRL::ComPtr<ID3D12Device> m_device;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_queue; Microsoft::WRL::ComPtr<IDXGISwapChain3> m_swap;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_rtvHeap,m_srvHeap,m_dsvHeap;
    UINT m_rtvStride{}; Microsoft::WRL::ComPtr<ID3D12Resource> m_back[FrameCount],m_depth;
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> m_alloc[FrameCount]; Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_cmd;
    Microsoft::WRL::ComPtr<ID3D12Fence> m_fence; UINT64 m_fenceValue[FrameCount]{},m_nextFenceValue=0; HANDLE m_fenceEvent{}; UINT m_frame{};
    Microsoft::WRL::ComPtr<ID3D12RootSignature> m_sceneRoot; Microsoft::WRL::ComPtr<ID3D12PipelineState> m_scenePSO;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_cb; UINT8* m_cbPtr{};

    static constexpr UINT TileCapacity=2048;
    struct TileResource {
        Microsoft::WRL::ComPtr<ID3D12Resource> texture;
        UINT slot{};UINT64 bytes{},touched{},lastDrawFence{},uploadFence{};
        bool ready=false;
    };
    struct TileDraw { Core::TileId id, source; UINT slot{},grid{}; float uv[4]{}; };
    struct PatchMesh {
        Microsoft::WRL::ComPtr<ID3D12Resource> vb,ib;
        D3D12_VERTEX_BUFFER_VIEW vbv{};D3D12_INDEX_BUFFER_VIEW ibv{};UINT count{};
    };
    struct UploadBatch {
        Microsoft::WRL::ComPtr<ID3D12CommandAllocator> allocator;
        Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commands;
        Microsoft::WRL::ComPtr<ID3D12Resource> arena;
        UINT8* mapped=nullptr;UINT64 capacity=0,used=0,fence=0;
        std::vector<Core::TileId> tiles;
    };
    Core::PlanetQuadtree m_planet;
    Core::EnvironmentTrace m_trace; // Must outlive the DDS worker and its trace callbacks.
    DdsTileLoader m_tileLoader;
    std::optional<DdsTileLoader::Result> m_deferredTile;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_uploadQueue;
    Microsoft::WRL::ComPtr<ID3D12Fence> m_uploadFence;
    UINT64 m_uploadFenceValue=0;
    std::array<UploadBatch,3> m_uploadBatches;
    std::array<PatchMesh,4> m_patchMeshes;
    std::map<Core::TileId,TileResource> m_tiles;
    std::set<Core::TileId> m_failedTiles;
    std::vector<TileDraw> m_tileDraws;
    double m_selectMs=0,m_uploadCpuMs=0,m_readMs=0,m_presentMs=0,m_sceneGpuMs=0,m_sceneCpuMs=0,m_hudCpuMs=0;
    Microsoft::WRL::ComPtr<ID3D12QueryHeap> m_timestampHeap;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_timestampReadback;
    UINT64 m_timestampFrequency=0;
    bool m_hasTimestamp[FrameCount]{};
    bool m_tileOverlay=false;
    UINT64 m_tileClock=0,m_tileBytes=0;
    UINT m_srvStride{};
    int m_renderedLOD=0,m_residentLOD=0;
    std::wstring m_tileStatus;
    std::chrono::steady_clock::time_point m_fpsStart=std::chrono::steady_clock::now();
    UINT m_fpsFrames=0;
    double m_fps=0;
    // DirectWrite/Direct2D HUD, layered over the DX12 back buffers through D3D11On12.
    Microsoft::WRL::ComPtr<ID3D11Device> m_d3d11Device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_d3d11Context;
    Microsoft::WRL::ComPtr<ID3D11On12Device> m_d3d11On12;
    Microsoft::WRL::ComPtr<ID2D1Factory3> m_d2dFactory;
    Microsoft::WRL::ComPtr<ID2D1Device2> m_d2dDevice;
    Microsoft::WRL::ComPtr<ID2D1DeviceContext2> m_d2dContext;
    Microsoft::WRL::ComPtr<IDWriteFactory> m_dwriteFactory;
    // Private app-only collection: does not install or register fonts in Windows.
    Microsoft::WRL::ComPtr<IDWriteFontCollection1> m_headerFontCollection;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> m_textLeft,m_textRight,m_textLogo,m_textLogoX,m_textMenu,m_textScenarioButton,m_textScenarioTitle;
    Microsoft::WRL::ComPtr<IDWriteRenderingParams> m_textRenderingParams;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_textBrush;
    Microsoft::WRL::ComPtr<ID2D1Bitmap1> m_menuBackground;
    Microsoft::WRL::ComPtr<ID2D1Bitmap1> m_menuLogo;
    Microsoft::WRL::ComPtr<ID2D1Bitmap1> m_scenarioPreview;
    bool m_scenarioPreviewArtwork=false;
    Microsoft::WRL::ComPtr<ID3D11Resource> m_wrappedBack[FrameCount];
    Microsoft::WRL::ComPtr<ID2D1Bitmap1> m_d2dTarget[FrameCount];

    OrbitX::Core::SimulationState m_state;
    float m_yaw=0.35f,m_pitch=0.18f,m_distanceKm=5200.0f,m_fov=40.0f;
    enum class AppState { MainMenu, ScenarioSelect, Parameters, VisualEffects, Modules, Graphics, Joystick, Extra, Simulation };
    AppState m_appState=AppState::MainMenu;
    int m_menuSelection=0;
    int m_hoverSelection=-1;
    bool m_solarSystemExpanded=false;
    bool m_scenarioSelected=false;
    int m_scenarioHover=-1; // 0: category, 1: Moon View, 2: Launch
    int m_graphicsHover=-1; // 0: WINDOWED, 1: FULL SCREEN, 2: VSync off, 3: VSync on, -1: neither
    bool m_mouseNavigation=false;
    bool m_vsync=false,m_tearingSupported=false;
    bool m_fullscreen=false; WINDOWPLACEMENT m_windowedPlacement{sizeof(WINDOWPLACEMENT)}; DWORD m_windowedStyle=WS_OVERLAPPEDWINDOW;
    bool m_drag=false; POINT m_dragAnchorScreen{};
};
}
