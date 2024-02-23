#pragma once
class SimpleCapture
{
public:
    bool InitializeShaders(winrt::com_ptr<ID3D11Device> d3dDevice);
    SimpleCapture(
        winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice const& device,
        winrt::Windows::Graphics::Capture::GraphicsCaptureItem const& item);
    ~SimpleCapture() { Close(); }

    void StartCapture();
    winrt::Windows::UI::Composition::ICompositionSurface CreateSurface(
        winrt::Windows::UI::Composition::Compositor const& compositor);

    void Close();
    void SetRectBox(int, int, int, int);
    bool InitializeScene(float x, float y, float right, float bottom);
    void GrabMat(winrt::com_ptr<ID3D11Texture2D> frameSurface);
    void RenderRectangle();
private:
    void OnFrameArrived(
        winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool const& sender,
        winrt::Windows::Foundation::IInspectable const& args);

    void CheckClosed()
    {
        if (m_closed.load() == true)
        {
            throw winrt::hresult_error(RO_E_CLOSED);
        }
    }

private:
    winrt::Windows::Graphics::Capture::GraphicsCaptureItem m_item{ nullptr };
    winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool m_framePool{ nullptr };
    winrt::Windows::Graphics::Capture::GraphicsCaptureSession m_session{ nullptr };
    winrt::Windows::Graphics::SizeInt32 m_lastSize;
    winrt::Windows::Graphics::SizeInt32 mp_lastSize;

    winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice m_device{ nullptr };
    winrt::com_ptr<IDXGISwapChain1> m_swapChain{ nullptr };
    winrt::com_ptr<ID3D11DeviceContext> m_d3dContext{ nullptr };

    std::atomic<bool> m_closed = false;
    winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool::FrameArrived_revoker m_frameArrived;
    winrt::com_ptr<ID3D11InputLayout> inputLayout;
    winrt::com_ptr<ID3D11Device> d3dDevice;
    VertexShader vertexShader;
    PixelShader pixelShader;
    ID3D11Buffer* vertexBuffer;
    float rectX, rectY, rectR, rectB;
    D3D11_BOX box;
    winrt::com_ptr<IDXGISwapChain1> mp_swapChain{ nullptr };
    winrt::com_ptr<ID3D11Texture2D> backBuffer;
    winrt::com_ptr<ID3D11Texture2D> prevBuffer;
    winrt::com_ptr<ID3D11Texture2D> tempBuffer;
    ID3D11RenderTargetView* renderTargetView;
    ID3D11Multithread* lock;
};