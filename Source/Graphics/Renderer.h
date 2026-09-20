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
private:
    static constexpr UINT FrameCount=2;
    struct Vertex { DirectX::XMFLOAT3 pos, normal; DirectX::XMFLOAT2 uv; };
    struct SceneCB { DirectX::XMFLOAT4X4 mvp; DirectX::XMFLOAT4X4 model; DirectX::XMFLOAT4 sunDir; };
    bool InitDevice(); bool InitAssets(); bool CreateSphere(); bool LoadTextureWIC(const std::wstring& path);
    bool CreateScenePipeline(); bool CreateDepth(); bool CreateConstantBuffer(); bool InitVectorText(); bool LoadBundledHeaderFont(); bool CreateHudBackBufferTargets(); bool LoadMenuBackground(); bool LoadMenuLogo(); bool LoadScenarioPreview();
    void WaitForGPU(); void MoveToNextFrame(); void UpdateCB(); void DrawHudText();
    void SetError(const std::string& s);

    HWND m_hwnd{}; UINT m_width{},m_height{}; std::wstring m_root; std::string m_error;
    Microsoft::WRL::ComPtr<IDXGIFactory6> m_factory; Microsoft::WRL::ComPtr<ID3D12Device> m_device;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_queue; Microsoft::WRL::ComPtr<IDXGISwapChain3> m_swap;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_rtvHeap,m_srvHeap,m_dsvHeap;
    UINT m_rtvStride{}; Microsoft::WRL::ComPtr<ID3D12Resource> m_back[FrameCount],m_depth;
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> m_alloc[FrameCount]; Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_cmd;
    Microsoft::WRL::ComPtr<ID3D12Fence> m_fence; UINT64 m_fenceValue[FrameCount]{}; HANDLE m_fenceEvent{}; UINT m_frame{};
    Microsoft::WRL::ComPtr<ID3D12RootSignature> m_sceneRoot; Microsoft::WRL::ComPtr<ID3D12PipelineState> m_scenePSO;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_vb,m_ib,m_cb,m_texture; D3D12_VERTEX_BUFFER_VIEW m_vbv{}; D3D12_INDEX_BUFFER_VIEW m_ibv{}; UINT m_indexCount{}; UINT8* m_cbPtr{};

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
    bool m_mouseNavigation=false;
    bool m_fullscreen=false; WINDOWPLACEMENT m_windowedPlacement{sizeof(WINDOWPLACEMENT)}; DWORD m_windowedStyle=WS_OVERLAPPEDWINDOW;
    bool m_drag=false; POINT m_dragAnchorScreen{};
};
}
