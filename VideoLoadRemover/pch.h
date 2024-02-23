#pragma once

#include <Unknwn.h>
#include <inspectable.h>
#include <dshow.h>
#include "Resource.h"
// WinRT
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.System.h>
#include <winrt/Windows.UI.h>
#include <winrt/Windows.UI.Composition.h>
#include <winrt/Windows.UI.Composition.Desktop.h>
#include <winrt/Windows.UI.Popups.h>
#include <winrt/Windows.Graphics.Capture.h>
#include <winrt/Windows.Graphics.DirectX.h>
#include <winrt/Windows.Graphics.DirectX.Direct3d11.h>
#include <windows.ui.composition.interop.h>
#include <DispatcherQueue.h>

// STL
#include <atomic>
#include <memory>
#include <tchar.h>

// D3D
#include <d3d11_4.h>
#include <dxgi1_6.h>
#include <d2d1_3.h>
#include <wincodec.h>

// Helpers
#include "composition.interop.h"
#include "d3dHelpers.h"
#include "direct3d11.interop.h"
#include "capture.interop.h"
#include <fstream>
#include <sstream>
#include <string>
#include "Vertex.h"
#include "Shaders.h"
#include <opencv2/opencv.hpp>
#include <opencv2/core/ocl.hpp>
#include <opencv2/core/directx.hpp>
#include <gdiplus.h>
#include "LiveSplitServer.h"
#pragma comment(lib,"gdiplus.lib")

#define DBOUT( s )            \
{                             \
   std::wostringstream os_;    \
   os_ << s;                   \
   OutputDebugStringW( os_.str().c_str() );  \
}
#define CANVAS_WIDTH 480
#define CANVAS_HEIGHT 270
extern int convertedX, convertedY, convertedWidth, convertedHeight;
extern HWND hwndPreviewCanvas, hwndPSNR;
extern HANDLE convertedLock;
extern DWORD mainThreadID;
extern cv::UMat selGray, uSelMat, diff, refMat;
extern RECT rcTarget;
extern BOOL isThreadWorking, isInitialLoad, showPreview;
extern bool showDiff, initializedCxt;
extern void ResizeRefImage(int w, int h);
extern double GetPSNR();
extern void DisplayDiff();