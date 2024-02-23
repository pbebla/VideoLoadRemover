//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, 
// INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. 
// IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, 
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, 
// TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH 
// THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//
//*********************************************************

#include "pch.h"
#include "SimpleCapture.h"
using namespace winrt;
using namespace winrt::Windows;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::System;
using namespace winrt::Windows::Graphics;
using namespace winrt::Windows::Graphics::Capture;
using namespace winrt::Windows::Graphics::DirectX;
using namespace winrt::Windows::Graphics::DirectX::Direct3D11;
using namespace winrt::Windows::Foundation::Numerics;
using namespace winrt::Windows::UI;
using namespace winrt::Windows::UI::Composition;
using namespace ABI::Windows::UI::Composition::Desktop;

bool SimpleCapture::InitializeShaders(winrt::com_ptr<ID3D11Device> d3dDevice) {
    std::wstring shaderFolder = L"";
#pragma region DetermineShaderPath
    if (IsDebuggerPresent()) {
#ifdef _DEBUG 
    #ifdef _WIN64
        shaderFolder = L"..\\x64\\Debug\\";
    #else
        shaderFolder = L"..\\Debug\\";
    #endif
#else
    #ifdef _WIN64
        shaderFolder = L"..\\x64\\Release\\";
    #else
        shaderFolder = L"..\\Release\\";
    #endif
#endif
    }
    if (!vertexShader.Initialize(d3dDevice, shaderFolder + L"vshaders.cso")) {
        return false;
    }
    if (!pixelShader.Initialize(d3dDevice, shaderFolder + L"PixelShader.cso")) {
        return false;
    }
    D3D11_INPUT_ELEMENT_DESC layout[] =
    {
        {"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_CLASSIFICATION::D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT,  D3D11_INPUT_CLASSIFICATION::D3D11_INPUT_PER_VERTEX_DATA, 0},
    };

    UINT numElements = ARRAYSIZE(layout);
    HRESULT hr = d3dDevice->CreateInputLayout(layout, numElements, vertexShader.GetBuffer()->GetBufferPointer(), vertexShader.GetBuffer()->GetBufferSize(), inputLayout.put());
    if (FAILED(hr)) {
        DBOUT("Error creating input layout.\n");
        return false;
    }
    return true;
}

SimpleCapture::SimpleCapture(
    IDirect3DDevice const& device,
    GraphicsCaptureItem const& item)
{
    m_item = item;
    m_device = device;
    // Set up 
    d3dDevice = GetDXGIInterfaceFromObject<ID3D11Device>(m_device);
    d3dDevice->GetImmediateContext(m_d3dContext.put());
    auto size = m_item.Size();
    m_swapChain = CreateDXGISwapChain(
        d3dDevice,
        static_cast<uint32_t>(size.Width),
        static_cast<uint32_t>(size.Height),
        static_cast<DXGI_FORMAT>(DirectXPixelFormat::B8G8R8A8UIntNormalized),
        2);
    // Create framepool, define pixel format (DXGI_FORMAT_B8G8R8A8_UNORM), and frame size. 
    m_framePool = Direct3D11CaptureFramePool::CreateFreeThreaded(
        m_device,
        DirectXPixelFormat::B8G8R8A8UIntNormalized,
        2,
        size);
    m_session = m_framePool.CreateCaptureSession(m_item);
    m_lastSize = size;
    mp_lastSize = size;
    m_frameArrived = m_framePool.FrameArrived(auto_revoke, { this, &SimpleCapture::OnFrameArrived });

    DXGI_SWAP_CHAIN_DESC1 desc = {};
    desc.Width = static_cast<uint32_t>(size.Width);
    desc.Height = static_cast<uint32_t>(size.Height);
    desc.Format = static_cast<DXGI_FORMAT>(DirectXPixelFormat::B8G8R8A8UIntNormalized);
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.BufferCount = 2;
    desc.Scaling = DXGI_SCALING_STRETCH;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    desc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
    mp_swapChain = CreateDXGISwapChain(d3dDevice, hwndPreviewCanvas, &desc);

    ////Create the Viewport
    D3D11_VIEWPORT viewport;
    ZeroMemory(&viewport, sizeof(D3D11_VIEWPORT));
    viewport.TopLeftX = 0;
    viewport.TopLeftY = 0;
    viewport.Width = (FLOAT)size.Width;
    viewport.Height = (FLOAT)size.Height;
    //Set the Viewport
    m_d3dContext->RSSetViewports(1, &viewport);

    RECT rc;
    GetClientRect(hwndPreviewCanvas, &rc);


    if (!InitializeShaders(d3dDevice)) {
        DBOUT("Error initializing buffers.\n");
        return;
    }
    if (!InitializeScene(rectX, rectY, rectR, rectB)) {
        DBOUT("Error initializing scene.\n");
        return;
    }
}

// Start sending capture frames
void SimpleCapture::StartCapture()
{
    CheckClosed();
    m_session.StartCapture();
}

ICompositionSurface SimpleCapture::CreateSurface(
    Compositor const& compositor)
{
    CheckClosed();
    return CreateCompositionSurfaceForSwapChain(compositor, m_swapChain.get());
}

// Process captured frames
void SimpleCapture::Close()
{
    auto expected = false;
    if (m_closed.compare_exchange_strong(expected, true))
    {
        m_frameArrived.revoke();
        m_framePool.Close();
        m_session.Close();
        m_swapChain = nullptr;
        mp_swapChain = nullptr;
        m_framePool = nullptr;
        m_session = nullptr;
        m_item = nullptr;
        ReleaseMutex(convertedLock);
        
    }
}

bool SimpleCapture::InitializeScene(float x = 0.0f, float y = 0.0f, float right = 0.0f, float bottom = 0.0f)
{
    rectX = x;
    rectY = y;
    rectR = right;
    rectB = bottom;
    Vertex v[] =
    {
        Vertex(x, bottom, 0.0f, 1.0f, 0.0f,0.5f),
        Vertex(x, y, 0.0f, 1.0f, 0.0f,0.5f),
        Vertex(right, y, 0.0f, 1.0f, 0.0f,0.5f),
        Vertex(x, bottom, 0.0f, 1.0f, 0.0f,0.5f),
        Vertex(right, y, 0.0f, 1.0f, 0.0f,0.5f),
        Vertex(right, bottom, 0.0f, 1.0f, 0.0f,0.5f),
    };
    D3D11_BUFFER_DESC vertexBufferDesc;
    ZeroMemory(&vertexBufferDesc, sizeof(vertexBufferDesc));
    vertexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
    vertexBufferDesc.ByteWidth = sizeof(Vertex) * ARRAYSIZE(v);
    vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vertexBufferDesc.CPUAccessFlags = 0;
    vertexBufferDesc.MiscFlags = 0;


    D3D11_SUBRESOURCE_DATA vertexBufferData;
    ZeroMemory(&vertexBufferData, sizeof(vertexBufferData));
    vertexBufferData.pSysMem = v;
    HRESULT hr = d3dDevice->CreateBuffer(&vertexBufferDesc, &vertexBufferData, &vertexBuffer);
    if (FAILED(hr)) {
        DBOUT("Error creating buffer.\n");
        return false;
    }
    return true;
}

void SimpleCapture::RenderRectangle() {
    m_d3dContext->OMSetRenderTargets(1, &renderTargetView, NULL);
    float bgcolor[] = { 0.0f, 0.0f, 1.0f, 0.0f };
    //m_d3dContext->ClearRenderTargetView(renderTargetView, bgcolor);
    m_d3dContext->IASetInputLayout(inputLayout.get());
    m_d3dContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY::D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_d3dContext->VSSetShader(vertexShader.GetShader(), NULL, 0);
    m_d3dContext->PSSetShader(pixelShader.GetShader(), NULL, 0);
    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    m_d3dContext->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);
    m_d3dContext->Draw(6, 0);
}

void SimpleCapture::SetRectBox(int x, int y, int w, int h) {
    if (refMat.empty()) {
        if (!initializedCxt) {
            if (cv::ocl::haveOpenCL()) {
                auto m_oclCtx = cv::directx::ocl::initializeContextFromD3D11Device(d3dDevice.get());
            }
            initializedCxt = true;
        }
    }
    box = {
        (UINT)x,
        (UINT)y,
        0,
        (UINT)(x + w),
        (UINT)(y + h),
        1
    };
}

void SimpleCapture::OnFrameArrived(
    Direct3D11CaptureFramePool const& sender,
    winrt::Windows::Foundation::IInspectable const&)
{
    renderTargetView = nullptr;
    prevBuffer = nullptr;
    backBuffer = nullptr;
    tempBuffer = nullptr;
    auto size = m_lastSize;
    auto newSize = false;
    //int cX, cY, cW, cH;
    auto frame = sender.TryGetNextFrame();
    auto frameContentSize = frame.ContentSize();
    if (frameContentSize.Width != m_lastSize.Width ||
        frameContentSize.Height != m_lastSize.Height)
    {
        // The thing we have been capturing has changed size.
        // We need to resize our swap chain first, then blit the pixels.
        // After we do that, retire the frame and then recreate our frame pool.
        newSize = true;
        m_lastSize = frameContentSize;
        //DBOUT("test\n");
        m_swapChain->ResizeBuffers(
            2,
            static_cast<uint32_t>(m_lastSize.Width),
            static_cast<uint32_t>(m_lastSize.Height),
            static_cast<DXGI_FORMAT>(DirectXPixelFormat::B8G8R8A8UIntNormalized),
            0);
    }
    auto frameSurface = GetDXGIInterfaceFromObject<ID3D11Texture2D>(frame.Surface());
    check_hresult(m_swapChain->GetBuffer(0, guid_of<ID3D11Texture2D>(), backBuffer.put_void()));
    m_d3dContext->CopyResource(backBuffer.get(), frameSurface.get());
    check_hresult(d3dDevice->CreateRenderTargetView(backBuffer.get(), NULL, &renderTargetView));
    RenderRectangle();
    DXGI_PRESENT_PARAMETERS presentParameters = { 0 };
    m_swapChain->Present1(1, 0, &presentParameters);
    if (convertedWidth > 0 && convertedHeight > 0) {
        DWORD waitResult = WaitForSingleObject(convertedLock, INFINITE);
        switch (waitResult) {
            case WAIT_OBJECT_0: {
                SetRectBox(convertedX, convertedY, convertedWidth, convertedHeight);
                if (mp_lastSize.Width != convertedWidth ||
                    mp_lastSize.Height != convertedHeight) {
                    mp_lastSize = { convertedWidth, convertedHeight };
                    check_hresult(mp_swapChain->ResizeBuffers(
                        2,
                        static_cast<uint32_t>(mp_lastSize.Width),
                        static_cast<uint32_t>(mp_lastSize.Height),
                        static_cast<DXGI_FORMAT>(DirectXPixelFormat::B8G8R8A8UIntNormalized),
                        0));
                }
                check_hresult(mp_swapChain->GetBuffer(0, guid_of<ID3D11Texture2D>(), prevBuffer.put_void()));
                DXGI_PRESENT_PARAMETERS presentParameters = { 0 };
                tempBuffer = prevBuffer;
                if (!isInitialLoad && convertedWidth > 0 && convertedHeight > 0) {
                    //DBOUT(box.left << " " << box.top << " " << box.right << " " << box.bottom << "\n");
                    //DBOUT(rcTarget.left << " " << rcTarget.top << " " << rcTarget.right << " " << rcTarget.bottom << "\n");
                    ResizeRefImage(convertedWidth, convertedHeight);
                    GrabMat(frameSurface);
                    auto ret = GetPSNR();
                    uSelMat.release();
                }
                if (showDiff) {
                    try {
                        cv::directx::convertToD3D11Texture2D(diff, prevBuffer.get());
                    }
                    catch (cv::Exception& e)
                    {
                        DBOUT("ERROR: " << e.msg.c_str() << std::endl);
                    }
                }
                else {

                    try {
                        m_d3dContext->CopySubresourceRegion(prevBuffer.get(), 0, 0, 0, 0, frameSurface.get(), 0, &box);
                    }
                    catch (cv::Exception& e)
                    {
                        DBOUT("ERROR: " << e.msg.c_str() << std::endl);
                    }
                }
                if (showPreview) {
                    try {
                        mp_swapChain->Present1(1, 0, &presentParameters);
                    }
                    catch (cv::Exception& e)
                    {
                        DBOUT("ERROR: " << e.msg.c_str() << std::endl);
                    }
                }
                
                
                ReleaseMutex(convertedLock);
                break;
            }
            case WAIT_ABANDONED:
                break;
        }
    }
    
    if (newSize)
    {
        m_framePool.Recreate(
            m_device,
            DirectXPixelFormat::B8G8R8A8UIntNormalized,
            2,
            m_lastSize);
    }
    renderTargetView->Release();
}

void SimpleCapture::GrabMat(winrt::com_ptr<ID3D11Texture2D> frameSurface) {
    if (prevBuffer.get() != nullptr && frameSurface.get() != nullptr) {
        try {
            D3D11_TEXTURE2D_DESC desc, desc2;
            ZeroMemory(&desc, sizeof(desc));
            prevBuffer->GetDesc(&desc2);
            desc.Width = desc2.Width;
            desc.Height = desc2.Height;
            desc.MipLevels = 1;
            desc.ArraySize = 1;
            desc.Format = desc2.Format;
            desc.SampleDesc.Count = 1;
            desc.Usage = desc2.Usage;
            desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED;
            desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
            winrt::com_ptr<ID3D11Texture2D> temp = nullptr;
            d3dDevice->CreateTexture2D(&desc2, NULL, temp.put());
            m_d3dContext->CopySubresourceRegion(temp.get(), 0, 0, 0, 0, frameSurface.get(), 0, &box);
            cv::directx::convertFromD3D11Texture2D(temp.get(), uSelMat);
            //cv::directx::convertFromD3D11Texture2D(prevBuffer.get(), uSelMat);
        }
        catch (std::exception& e) {
            DBOUT("convert to mat " << e.what() << std::endl);
        }
        try {
            cv::cvtColor(uSelMat, selGray, cv::COLOR_BGR2GRAY);
        }
        catch (std::exception& e) {
            DBOUT("cvtColor 839 " << e.what() << "\n");
        }
    }
}

/* Works with Mat

With UMat:
-works if select window and region before ref
-works if select window before ref then region
-doesn't work if select ref then window then region*/