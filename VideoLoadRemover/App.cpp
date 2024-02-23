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
#include "App.h"
#include "SimpleCapture.h"
#pragma comment(lib,"d3d11.lib")
#pragma comment(lib, "windowsapp")

using namespace ABI::Windows::UI::Composition::Desktop;
using namespace winrt;
using namespace Windows::System;
using namespace Windows::Foundation;
using namespace Windows::UI;
using namespace Windows::UI::Composition;
using namespace Windows::Graphics::Capture;
using namespace Windows::Graphics::DirectX::Direct3D11;

void App::Initialize(
    ContainerVisual const& root)
{
    auto queue = DispatcherQueue::GetForCurrentThread();

    m_compositor = root.Compositor();
    m_root = m_compositor.CreateContainerVisual();
    m_content = m_compositor.CreateSpriteVisual();
    m_brush = m_compositor.CreateSurfaceBrush();

    m_root.RelativeSizeAdjustment({ 1, 1 });
    root.Children().InsertAtTop(m_root);
    m_content.RelativeSizeAdjustment({ 1, 1 });
    m_content.Brush(m_brush);
    m_brush.Stretch(CompositionStretch::Fill);
    m_root.Children().InsertAtTop(m_content);

    d3dDevice = CreateD3DDevice();
    auto dxgiDevice = d3dDevice.as<IDXGIDevice>();
    m_device = CreateDirect3DDevice(dxgiDevice.get());
}

void App::StartCapture(HWND hwnd)
{
    if (m_capture)
    {
        m_capture->Close();
        m_capture = nullptr;
    }

    auto item = CreateCaptureItemForWindow(hwnd);

    m_capture = std::make_unique<SimpleCapture>(m_device, item);

    auto surface = m_capture->CreateSurface(m_compositor);
    m_brush.Surface(surface);

    m_capture->StartCapture();
}

void App::StartCapture(HMONITOR hwnd)
{
    if (m_capture)
    {
        m_capture->Close();
        m_capture = nullptr;
    }

    auto item = CreateCaptureItemForMonitor(hwnd);

    m_capture = std::make_unique<SimpleCapture>(m_device, item);

    auto surface = m_capture->CreateSurface(m_compositor);
    m_brush.Surface(surface);

    m_capture->StartCapture();
}

void App::InitializeOpenCLContext() {
    if (!initializedCxt) {
        if (cv::ocl::haveOpenCL()) {
            auto m_oclCtx = cv::directx::ocl::initializeContextFromD3D11Device(d3dDevice.get());
        }
        initializedCxt = true;
    }
}

void App::ReleaseOpenCLContext() {
    //m_oclCtx.release();
}