#pragma once

class SimpleCapture;

class App
{
public:
    App() {}
    ~App() {}

    void Initialize(
        winrt::Windows::UI::Composition::ContainerVisual const& root);

    void StartCapture(HWND hwnd);
    void StartCapture(HMONITOR hwnd);
    std::unique_ptr<SimpleCapture> m_capture{ nullptr };
    winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice m_device{ nullptr };
    winrt::com_ptr<ID3D11Device> d3dDevice;
    void InitializeOpenCLContext();
    void ReleaseOpenCLContext();
private:
    winrt::Windows::UI::Composition::Compositor m_compositor{ nullptr };
    winrt::Windows::UI::Composition::ContainerVisual m_root{ nullptr };
    winrt::Windows::UI::Composition::SpriteVisual m_content{ nullptr };
    winrt::Windows::UI::Composition::CompositionSurfaceBrush m_brush{ nullptr };
};