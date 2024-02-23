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
#include <ShObjIdl.h>
#include "Win32WindowEnumeration.h"
#pragma comment (lib, "dwmapi.lib")

using namespace winrt;
using namespace Windows::UI;
using namespace Windows::UI::Composition;
using namespace Windows::UI::Composition::Desktop;

#define BITS_PER_PIXEL   32
#define BYTES_PER_PIXEL  (BITS_PER_PIXEL / 8)
#define MAX_LOADSTRING 100

HWND selHwnd, hwndPSNR, hwndThreshold, hwndStart, hwndStop, hwndDiff, hwndToggle, savedX, savedY, savedW, savedH, hwndButton5, hwndDrawableCanvas, hwndPreviewCanvas, hwndProcessList;
HWND hControl, hwndUpDnEdtBdyX, hwndUpDnCtlX, hwndUpDnEdtBdyY, hwndUpDnCtlY, hwndUpDnEdtBdyW, hwndUpDnCtlW, hwndUpDnEdtBdyH, hwndUpDnCtlH;
void DisplayDiff();
HMENU hMenu;
LRESULT CALLBACK DrawCanvasProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK  PrevCanvasProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
BOOL CALLBACK EnumChildProc(HWND hwndChild, LPARAM lParam);
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
HANDLE convertedLock;
void LoadFile(HWND hwnd, bool isRefImg);
com_ptr<ID3D11SamplerState> samplerState;
RECT rcClient, rcTarget, selRect;
RECT clientRect;
POINT pt;         // x and y coordinates of cursor 
BOOL fDrawRect, isInitialLoad, isThreadWorking, isRemoverRunning, showPreview; // TRUE if bitmap rect. is dragged
int convertedX, convertedY, convertedWidth, convertedHeight, intSavedX, intSavedY, intSavedW, intSavedH;
int windowListSize;
UINT cyVScroll;
HINSTANCE hInst;
const UINT valMin = 0;          // The range of values for the Up-Down control.
const UINT valMax = GetSystemMetrics(SM_CXVIRTUALSCREEN);
LPWSTR filePath, saveFilePath;
float left, top, right, bottom;
double threshold;
cv::UMat refResized, refGrayResized, selGray, uSelMat, diff, refMat;
Gdiplus::Bitmap* refBmp = nullptr;
Gdiplus::GdiplusStartupInput gdiplusStartupInput;
ULONG_PTR           gdiplusToken;
bool showDiff = false;
bool initializedCxt;
DWORD mainThreadID;
double oldPSNR = 0.0;
LiveSplitServer server;
void SaveFileAs(HWND hwnd);
void SaveFile();
struct MonitorInfo
{
    MonitorInfo(HMONITOR monitorHandle)
    {
        MonitorHandle = monitorHandle;
        MONITORINFOEX monitorInfo = { sizeof(monitorInfo) };
        winrt::check_bool(GetMonitorInfo(MonitorHandle, &monitorInfo));
        std::wstring displayName(monitorInfo.szDevice);
        width = monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left;
        height = monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top;
        DisplayName = displayName;
    }
    MonitorInfo(HMONITOR monitorHandle, std::wstring const& displayName, long width, long height)
    {
        MonitorHandle = monitorHandle;
        DisplayName = displayName;
        width = width;
        height = height;
    }

    HMONITOR MonitorHandle;
    std::wstring DisplayName;
    long width;
    long height;

    bool operator==(const MonitorInfo& monitor) { return MonitorHandle == monitor.MonitorHandle; }
    bool operator!=(const MonitorInfo& monitor) { return !(*this == monitor); }
};
std::vector<MonitorInfo> EnumerateAllMonitors();
auto g_app = std::make_shared<App>();
auto g_windows = EnumerateWindows();
auto monitors = EnumerateAllMonitors();
void PopulateProcessList();
void RenderRefImage(HWND hwnd);
double GetPSNR();
void ResizeRefImage(int w, int h);
void Resize(int left, int top, int right, int bottom) {
    SetRect(&rcTarget, left, top, right, bottom);
}

void CalculateDim() {
    DWORD waitResult = WaitForSingleObject(
        convertedLock,    // handle to mutex
        INFINITE);  // no time-out interval
    switch (waitResult) {
        case WAIT_OBJECT_0: {
            convertedX = (rcTarget.left * (selRect.right - selRect.left) / CANVAS_WIDTH);
            convertedY = (rcTarget.top * (selRect.bottom - selRect.top) / CANVAS_HEIGHT);
            convertedWidth = ((selRect.right - selRect.left) * (rcTarget.right - rcTarget.left)) / CANVAS_WIDTH;
            convertedHeight = ((selRect.bottom - selRect.top) * (rcTarget.bottom - rcTarget.top)) / CANVAS_HEIGHT;
            SendMessage(hwndUpDnCtlX, UDM_SETPOS, 0, convertedX);
            SendMessage(hwndUpDnCtlY, UDM_SETPOS, 0, convertedY);
            SendMessage(hwndUpDnCtlW, UDM_SETPOS, 0, convertedWidth);
            SendMessage(hwndUpDnCtlH, UDM_SETPOS, 0, convertedHeight);
            
            ReleaseMutex(convertedLock);
            break;
        }
        case WAIT_ABANDONED:
            return;
    }
}

HWND CreateUpDnBuddy(HWND hwndParent, int x, int y)
{
    hControl = CreateWindowEx(WS_EX_LEFT | WS_EX_CLIENTEDGE | WS_EX_CONTEXTHELP,    //Extended window styles.
        WC_EDIT,
        NULL,
        WS_CHILDWINDOW | WS_VISIBLE | WS_BORDER    // Window styles.
        | ES_NUMBER | ES_LEFT,                     // Edit control styles.
        x, y,
        100, 25,
        hwndParent,
        NULL,
        hInst,
        NULL);
    return (hControl);
}

HWND CreateUpDnCtl(HWND hwndParent, int x, int y, const wchar_t* label)
{
    hControl = CreateWindowEx(WS_EX_LEFT | WS_EX_LTRREADING,
        UPDOWN_CLASS,
        NULL,
        WS_CHILDWINDOW | WS_VISIBLE
        | UDS_AUTOBUDDY | UDS_SETBUDDYINT | UDS_ALIGNRIGHT | UDS_ARROWKEYS | UDS_HOTTRACK,
        0, 0,
        0, 0,         // Set to zero to automatically size to fit the buddy window.
        hwndParent,
        NULL,
        hInst,
        NULL);
    SendMessage(hControl, UDM_SETRANGE, 0, MAKELPARAM(valMax, valMin));    // Sets the controls direction 
    CreateWindow(L"Static",
        label,
        WS_VISIBLE | WS_CHILD,
        x, y - 25,
        100, 20,
        hwndParent,
        NULL,
        hInst,
        NULL);
    return (hControl);
}

void AddMenus(HWND hWnd) {
    hMenu = CreateMenu();
    HMENU hFileMenu = CreateMenu();
    HMENU hAboutMenu = CreateMenu();
    AppendMenu(hFileMenu, MF_STRING, ID_IMAGE_LOAD, L"Load Image");
    AppendMenu(hFileMenu, MF_STRING, ID_FILE_OPEN, L"Open");
    AppendMenu(hFileMenu, MF_STRING, ID_FILE_SAVE, L"Save");
    AppendMenu(hFileMenu, MF_STRING, ID_FILE_SAVE_AS, L"Save As");
    AppendMenu(hFileMenu, MF_STRING, IDM_EXIT, L"Exit");
    AppendMenu(hAboutMenu, MF_STRING, IDM_ABOUT, L"About");
    AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hFileMenu, L"File");
    AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hAboutMenu, L"About");
    SetMenu(hWnd, hMenu);
    cyVScroll = GetSystemMetrics(SM_CYVSCROLL);
    hwndUpDnEdtBdyX = CreateUpDnBuddy(hWnd, 0, 175);  // Create an buddy window (an Edit control).
    hwndUpDnCtlX = CreateUpDnCtl(hWnd, 0, 175, L"X");    // Create an Up-Down control.
    hwndUpDnEdtBdyY = CreateUpDnBuddy(hWnd, 105, 175);  // Create an buddy window (an Edit control).
    hwndUpDnCtlY = CreateUpDnCtl(hWnd, 105, 175, L"Y");    // Create an Up-Down control.
    hwndUpDnEdtBdyW = CreateUpDnBuddy(hWnd, 210, 175);  // Create an buddy window (an Edit control).
    hwndUpDnCtlW = CreateUpDnCtl(hWnd, 210, 175, L"Width");    // Create an Up-Down control.
    hwndUpDnEdtBdyH = CreateUpDnBuddy(hWnd, 315, 175);  // Create an buddy window (an Edit control).
    hwndUpDnCtlH = CreateUpDnCtl(hWnd, 315, 175, L"Height");    // Create an Up-Down control.
}

// Direct3D11CaptureFramePool requires a DispatcherQueue
auto CreateDispatcherQueueController()
{
    namespace abi = ABI::Windows::System;

    DispatcherQueueOptions options
    {
        sizeof(DispatcherQueueOptions),
        DQTYPE_THREAD_CURRENT,
        DQTAT_COM_STA
    };

    Windows::System::DispatcherQueueController controller{ nullptr };
    check_hresult(CreateDispatcherQueueController(options, reinterpret_cast<abi::IDispatcherQueueController**>(put_abi(controller))));
    return controller;
}

DesktopWindowTarget CreateDesktopWindowTarget(Compositor const& compositor, HWND window, BOOL isTopMost)
{
    namespace abi = ABI::Windows::UI::Composition::Desktop;

    auto interop = compositor.as<abi::ICompositorDesktopInterop>();
    DesktopWindowTarget target{ nullptr };
    check_hresult(interop->CreateDesktopWindowTarget(window, isTopMost, reinterpret_cast<abi::IDesktopWindowTarget**>(put_abi(target))));
    return target;
}

void MyRegisterDrawCanvasClass(HINSTANCE hInstance) {
    WNDCLASSEXW wcex_drawCanvas;
    wcex_drawCanvas.cbSize = sizeof(WNDCLASSEXW);
    wcex_drawCanvas.style = CS_HREDRAW | CS_VREDRAW;
    wcex_drawCanvas.lpfnWndProc = DrawCanvasProc;
    wcex_drawCanvas.cbClsExtra = 0;
    wcex_drawCanvas.cbWndExtra = 0;
    wcex_drawCanvas.hInstance = hInstance;
    wcex_drawCanvas.hIcon = NULL;
    wcex_drawCanvas.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex_drawCanvas.hbrBackground = (HBRUSH)GetStockObject(GRAY_BRUSH);
    wcex_drawCanvas.lpszMenuName = NULL;
    wcex_drawCanvas.lpszClassName = L"Drawable Canvas";
    wcex_drawCanvas.hIconSm = NULL;
    if (RegisterClassExW(&wcex_drawCanvas) == 0) {
        wchar_t error[40] = { 0 };
        swprintf(error, _countof(error), L"Error Registering Child %d\n", GetLastError());
        MessageBox(NULL, (LPCWSTR)error, TEXT("Error..."), MB_OK | MB_ICONERROR);
    }
}

void MyRegisterPreviewCanvasClass(HINSTANCE hInstance) {
    WNDCLASSEXW wcex_drawCanvas;

    wcex_drawCanvas.cbSize = sizeof(WNDCLASSEX);

    wcex_drawCanvas.style = CS_HREDRAW | CS_VREDRAW;
    wcex_drawCanvas.lpfnWndProc = PrevCanvasProc;
    wcex_drawCanvas.cbClsExtra = 0;
    wcex_drawCanvas.cbWndExtra = 0;
    wcex_drawCanvas.hInstance = hInstance;
    wcex_drawCanvas.hIcon = NULL;
    wcex_drawCanvas.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex_drawCanvas.hbrBackground = (HBRUSH)GetStockObject(GRAY_BRUSH);
    wcex_drawCanvas.lpszMenuName = NULL;
    wcex_drawCanvas.lpszClassName = TEXT("Preview Canvas");
    wcex_drawCanvas.hIconSm = NULL;
    if (RegisterClassExW(&wcex_drawCanvas) == 0) {
        wchar_t error[40] = { 0 };
        swprintf(error, _countof(error), L"Error Registering Child %d\n", GetLastError());
        MessageBox(NULL, (LPCWSTR)error, TEXT("Error..."), MB_OK | MB_ICONERROR);
    }
}

int CALLBACK WinMain(
    HINSTANCE instance,
    HINSTANCE previousInstance,
    LPSTR     cmdLine,
    int       cmdShow);

LRESULT CALLBACK WndProc(
    HWND   hwnd,
    UINT   msg,
    WPARAM wParam,
    LPARAM lParam);

bool CALLBACK SetFont(HWND child, LPARAM font) {
    SendMessage(child, WM_SETFONT, font, true);
    return true;
}

auto CreateDispatcherQueueControllerForCurrentThread()
{
    namespace abi = ABI::Windows::System;

    DispatcherQueueOptions options
    {
        sizeof(DispatcherQueueOptions),
        DQTYPE_THREAD_CURRENT,
        DQTAT_COM_NONE
    };

    winrt::Windows::System::DispatcherQueueController controller{ nullptr };
    winrt::check_hresult(CreateDispatcherQueueController(options, reinterpret_cast<abi::IDispatcherQueueController**>(winrt::put_abi(controller))));
    return controller;
}

int CALLBACK WinMain(
    HINSTANCE instance,
    HINSTANCE previousInstance,
    LPSTR     cmdLine,
    int       cmdShow)
{
    //cv::setBreakOnError(true);
	// Init COM
    winrt::init_apartment();
    hInst = instance;
    isInitialLoad = true;
    mainThreadID = GetCurrentThreadId();
    isRemoverRunning = false;
    showPreview = true;
    initializedCxt = false;
    server = LiveSplitServer();
    // Create the window
    WNDCLASSEX wcex = {};
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = instance;
    wcex.hIcon = LoadIcon(instance, MAKEINTRESOURCE(IDI_ICON1));
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = MAKEINTRESOURCEW(IDC_HPSIXTHGENLOADREMOVERTOOL);
    wcex.lpszClassName = L"Video Load Remover";
    wcex.hIconSm = LoadIcon(wcex.hInstance, IDI_APPLICATION);
    WINRT_VERIFY(RegisterClassEx(&wcex));
    MyRegisterDrawCanvasClass(instance);
    MyRegisterPreviewCanvasClass(instance);
    Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);
    isThreadWorking = true;
    HWND hwnd = CreateWindow(
        L"Video Load Remover",
        L"Video Load Remover",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        990,
        540,
        NULL,
        NULL,
        instance,
        NULL);
    WINRT_VERIFY(hwnd);
    ShowWindow(hwnd, cmdShow);
    UpdateWindow(hwnd);

    // Create combo box
    hwndProcessList = CreateWindowW(
        WC_COMBOBOX,
        TEXT(""),
        CBS_DROPDOWNLIST | CBS_HASSTRINGS | WS_VSCROLL | WS_CHILD | WS_OVERLAPPED | WS_VISIBLE,
        200,
        0,
        400,
        200,
        hwnd,
        NULL,
        instance,
        NULL);
    WINRT_VERIFY(hwndProcessList);
    PopulateProcessList();
    EnumChildWindows(hwnd, (WNDENUMPROC)SetFont, (LPARAM)GetStockObject(DEFAULT_GUI_FONT));
    // Create a DispatcherQueue for our thread
    auto controller = CreateDispatcherQueueControllerForCurrentThread();
    // Initialize Composition
    auto compositor = Compositor();
    auto target = CreateDesktopWindowTarget(compositor, hwndDrawableCanvas, true);
    auto root = compositor.CreateContainerVisual();
    root.RelativeSizeAdjustment({ 1.0f, 1.0f });
    target.Root(root);

    // Enqueue our capture work on the dispatcher
    auto queue = controller.DispatcherQueue();
    auto success = queue.TryEnqueue([=]() -> void
        {
            g_app->Initialize(root);
        });
    WINRT_VERIFY(success);
    //SendMessage(comboBoxHwnd, CB_SETCURSEL, 0, 0);

    // Message pump
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
        if (msg.message == WM_SETPSNR) {
            WCHAR buf[10];
            StringCbPrintfW(buf, sizeof(buf), TEXT("%f"), oldPSNR);
            //DBOUT(oldPSNR << "\n");
            SetWindowText(hwndPSNR, buf);
            RedrawWindow(hwndPSNR, NULL, NULL, RDW_ERASE);
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}

LRESULT CALLBACK WndProc(
    HWND   hwnd,
    UINT   msg,
    WPARAM wParam,
    LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
    {
        convertedLock = CreateMutex(
            NULL,              // default security attributes
            FALSE,             // initially not owned
            NULL);             // unnamed mutex

        hwndDrawableCanvas = CreateWindowExW(
            WS_EX_CLIENTEDGE,
            L"Drawable Canvas",
            TEXT(""),
            WS_CHILD | SS_BITMAP | WS_BORDER | WS_VISIBLE | WS_EX_LAYERED,
            5, 200, CANVAS_WIDTH, CANVAS_HEIGHT,
            hwnd,
            NULL,
            GetModuleHandle(NULL),
            NULL);

        hwndPreviewCanvas = CreateWindowExW(
            WS_EX_CLIENTEDGE,
            TEXT("Preview Canvas"),
            L"",
            WS_CHILDWINDOW | SS_BITMAP | WS_BORDER | WS_VISIBLE | WS_EX_LAYERED | SS_NOTIFY,
            490, 200, CANVAS_WIDTH, CANVAS_HEIGHT,
            hwnd,
            (HMENU)ID_PREVIEW,
            GetModuleHandle(NULL),
            NULL);

        AddMenus(hwnd);
        hwndPSNR = CreateWindowW(
            L"STATIC",
            TEXT(""),
            WS_VISIBLE | WS_CHILD,
            900,
            150,
            70,
            20,
            hwnd,
            (HMENU)IDC_PSNR,
            (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE),
            NULL);

        hwndThreshold = CreateWindowW(
            L"EDIT",
            TEXT(""),
            WS_VISIBLE | WS_CHILD | WS_BORDER,
            900,
            175,
            70,
            20,
            hwnd,
            (HMENU)IDC_PSNR,
            (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE),
            NULL);

        hwndStart = CreateWindowW(
            L"BUTTON",  // Predefined class; Unicode assumed 
            L"Start",      // Button text 
            WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,  // Styles 
            0,         // x position 
            25,         // y position 
            150,        // Button width
            20,        // Button height
            hwnd,     // Parent window
            (HMENU)ID_START,       // No menu.
            (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE),
            NULL);      // Pointer not needed.

        hwndStop = CreateWindowW(
            L"BUTTON",  // Predefined class; Unicode assumed 
            L"Stop",      // Button text 
            WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,  // Styles 
            0,         // x position 
            50,         // y position 
            150,        // Button width
            20,        // Button height
            hwnd,     // Parent window
            (HMENU)ID_STOP,       // No menu.
            (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE),
            NULL);      // Pointer not needed.

        hwndDiff = CreateWindowW(
            L"BUTTON",  // Predefined class; Unicode assumed 
            L"Show Diff",      // Button text 
            WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,  // Styles 
            0,         // x position 
            75,         // y position 
            150,        // Button width
            20,        // Button height
            hwnd,     // Parent window
            (HMENU)ID_PRINT,       // No menu.
            (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE),
            NULL);      // Pointer not needed.

        hwndToggle = CreateWindowW(
            L"BUTTON",  // Predefined class; Unicode assumed 
            L"Disable Preview",      // Button text 
            WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,  // Styles 
            0,         // x position 
            100,         // y position 
            150,        // Button width
            20,        // Button height
            hwnd,     // Parent window
            (HMENU)ID_TOGGLE,       // No menu.
            (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE),
            NULL);      // Pointer not needed.

        savedX = CreateWindowW(
            L"STATIC",  // Predefined class; Unicode assumed 
            L"",      // Button text 
            WS_VISIBLE | WS_CHILD,  // Styles 
            0,         // x position 
            125,         // y position 
            100,        // Button width
            20,        // Button height
            hwnd,     // Parent window
            NULL,       // No menu.
            (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE),
            NULL);      // Pointer not needed.

        savedY = CreateWindowW(
            L"STATIC",  // Predefined class; Unicode assumed 
            L"",      // Button text 
            WS_VISIBLE | WS_CHILD,  // Styles 
            105,         // x position 
            125,         // y position 
            100,        // Button width
            20,        // Button height
            hwnd,     // Parent window
            NULL,       // No menu.
            (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE),
            NULL);      // Pointer not needed.

        savedW = CreateWindowW(
            L"STATIC",  // Predefined class; Unicode assumed 
            L"",      // Button text 
            WS_VISIBLE | WS_CHILD,  // Styles 
            210,         // x position 
            125,         // y position 
            100,        // Button width
            20,        // Button height
            hwnd,     // Parent window
            NULL,       // No menu.
            (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE),
            NULL);      // Pointer not needed.

        savedH = CreateWindowW(
            L"STATIC",  // Predefined class; Unicode assumed 
            L"",      // Button text 
            WS_VISIBLE | WS_CHILD,  // Styles 
            315,         // x position 
            125,         // y position 
            100,        // Button width
            20,        // Button height
            hwnd,     // Parent window
            NULL,       // No menu.
            (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE),
            NULL);      // Pointer not needed.

        //EnableWindow(hwndStop, false);
        
        hwndButton5 = CreateWindowW(
            L"BUTTON",  // Predefined class; Unicode assumed 
            L"Refresh",      // Button text 
            WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,  // Styles 
            605,         // x position 
            0,         // y position 
            75,        // Button width
            20,        // Button height
            hwnd,     // Parent window
            (HMENU)ID_HWNDBUTTON5,       // No menu.
            (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE),
            NULL);

        EnableWindow(hwndStop, false);
        EnableWindow(hwndDiff, false);
    }
        break;
    case WM_DESTROY: {
        if (initializedCxt && refMat.empty()) {
            g_app->ReleaseOpenCLContext();
        }
        isThreadWorking = false;
        isRemoverRunning = false;
        server.close_pipe();
        refResized.release();
        selGray.release();
        refGrayResized.release();
        uSelMat.release();
        diff.release();
        refMat.release();
        PostQuitMessage(0);
    };
        break;
    case WM_COMMAND:
    {
        //DBOUT("WM_COMMAND\n");
        int wmId = LOWORD(wParam);
        int hWmID = HIWORD(wParam);
        if (hWmID == CBN_SELCHANGE)
        {
            if (isInitialLoad) {
                if (!refMat.empty()) {
                    g_app->InitializeOpenCLContext();
                }
                isInitialLoad = false;
            }
            auto index = SendMessage((HWND)lParam, CB_GETCURSEL, 0, 0);
            if (index < windowListSize) {
                auto& window = g_windows[index];
                selHwnd = window.Hwnd();
                GetClientRect(selHwnd, &selRect);
                g_app->StartCapture(window.Hwnd());
            }
            else {
                auto& window = monitors[index - windowListSize];
                DBOUT(index - windowListSize << "\n");
                selHwnd = NULL;
                selRect = RECT{ 0, 0, window.width, window.height };
                g_app->StartCapture(window.MonitorHandle);
            }
            if (!IsRectEmpty(&rcTarget)) {
                CalculateDim();
                g_app->m_capture->InitializeScene(left, top, right, bottom);
                if (intSavedX != NULL) {
                    int range = 5;
                    if (std::abs(intSavedX - convertedX) <= range && std::abs(intSavedY - convertedY) <= range && std::abs(intSavedW - convertedWidth) <= range && std::abs(intSavedH - convertedHeight) <= range) {
                        convertedX = intSavedX;
                        convertedY = intSavedY;
                        convertedWidth = intSavedW;
                        convertedHeight = intSavedH;
                        SendMessage(hwndUpDnCtlX, UDM_SETPOS, 0, convertedX);
                        SendMessage(hwndUpDnCtlY, UDM_SETPOS, 0, convertedY);
                        SendMessage(hwndUpDnCtlW, UDM_SETPOS, 0, convertedWidth);
                        SendMessage(hwndUpDnCtlH, UDM_SETPOS, 0, convertedHeight);
                    }
                }
            }
        }
        switch (wmId) {
        case ID_HWNDBUTTON5:
            PopulateProcessList();
            //SendMessage(hwndProcessList, CB_SETCURSEL, (WPARAM)2, (LPARAM)0);
            break;
        case ID_FILE_OPEN:
            /*if (isInitialLoad) {
                wchar_t error[60] = { 0 };
                swprintf(error, _countof(error), L"Please select a window before loading an reference image.\n");
                MessageBox(NULL, (LPCWSTR)error, TEXT("Error..."), MB_OK | MB_ICONERROR);
                break;
            }*/
            LoadFile(hwnd, false);
            break;
        case ID_IMAGE_LOAD:
            LoadFile(hwnd, true);
            break;
        case ID_FILE_SAVE:
            SaveFile();
            break;
        case ID_FILE_SAVE_AS:
            SaveFileAs(hwnd);
            break;
        case ID_TOGGLE:
            showPreview = !showPreview;
            if (showPreview) {
                SetWindowText(hwndToggle, L"Disable Preview");
            }
            else {
                SetWindowText(hwndToggle, L"Enable Preview");
            }
            break;
        case IDM_ABOUT:
            DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hwnd, About);
            DBOUT(GetLastError() << std::endl);
            break;
        case IDM_EXIT:
            DestroyWindow(hwnd);
            break;
        case ID_START:
            if (!server.open_pipe()) {
                DBOUT("Couldn't open Livesplit pipe.\n");
                break;
            }
            if (!IsRectEmpty(&rcTarget)) {
                TCHAR buff[MAX_LOADSTRING];
                GetWindowText(hwndThreshold, buff, MAX_LOADSTRING);
                threshold = _tstof(buff);
                isRemoverRunning = true;
                EnableWindow(hwndStart, false);
                EnableWindow(hwndStop, true);
                EnableWindow(hwndThreshold, false);
            }
            break;
        case ID_STOP:
            if (isRemoverRunning) {
                isRemoverRunning = false;
                server.send_to_ls("unpausegametime\r\n");
                server.set_isLoading(false);
                server.close_pipe();
                EnableWindow(hwndStop, false);
                EnableWindow(hwndStart, true);
                EnableWindow(hwndThreshold, true);
            }
            break;
        case ID_PRINT:
            //if (!selMat.empty() && !diff.empty()) {
                //cv::imwrite("test.png", diff);
                //cv::imshow("Diff", diff);
            //}
            if (!IsRectEmpty(&rcTarget) && !refMat.empty()) {
                showDiff = !showDiff;
                if (showDiff) {
                    SetWindowText(hwndDiff, L"Hide Diff");
                }
                else {
                    SetWindowText(hwndDiff, L"Show Diff");
                }
            }
            break;
        }
    }
        break;
    case WM_NOTIFY:
    {
        UINT nCode = ((LPNMHDR)lParam)->code;
        switch (nCode) {
        case UDN_DELTAPOS:
            LPNMUPDOWN lpnmud = (LPNMUPDOWN)lParam;
            if (!IsRectEmpty(&rcTarget)) {
                DWORD waitResult = WaitForSingleObject(
                    convertedLock,    // handle to mutex
                    INFINITE);  // no time-out interval
                switch (waitResult) {
                    case WAIT_OBJECT_0: {
                        LONG l = rcTarget.left;
                        LONG t = rcTarget.top;
                        LONG r = rcTarget.right;
                        LONG b = rcTarget.bottom;
                        int newPos = lpnmud->iPos + lpnmud->iDelta;
                        if (lpnmud->hdr.hwndFrom == hwndUpDnCtlX) {
                            convertedX = newPos;
                            l = CANVAS_WIDTH * convertedX / (selRect.right - selRect.left);
                            r = l + (CANVAS_WIDTH * convertedWidth / (selRect.right - selRect.left));
                        }
                        else if (lpnmud->hdr.hwndFrom == hwndUpDnCtlY) {
                            convertedY = newPos;
                            t = CANVAS_HEIGHT * convertedY / (selRect.bottom - selRect.top);
                            b = t + (CANVAS_HEIGHT * convertedHeight / (selRect.bottom - selRect.top));
                        }
                        else if (lpnmud->hdr.hwndFrom == hwndUpDnCtlW) {
                            convertedWidth = newPos;
                            r = l + (CANVAS_WIDTH * convertedWidth / (selRect.right - selRect.left));
                        }
                        else {
                            convertedHeight = newPos;
                            b = t + (CANVAS_HEIGHT * convertedHeight / (selRect.bottom - selRect.top));
                        }
                        Resize(l, t, r, b);
                        InvalidateRect(hwnd, NULL, false);
                        DBOUT("abs:" << l << " " << r << " " << t << " " << b << "\n");
                        left = (((l) * 2.0) / CANVAS_WIDTH) + (-1.0);
                        right = (((r) * 2.0) / CANVAS_WIDTH) + (-1.0);
                        top = (((t) * -2.0) / CANVAS_HEIGHT) + (1.0);
                        bottom = (((b) * -2.0) / CANVAS_HEIGHT) + (1.0);
                        DBOUT("normal:" << left << " " << right << " " << top << " " << bottom << "\n");
                        ReleaseMutex(convertedLock);
                        g_app->m_capture->InitializeScene(left, top, right, bottom);
                        
                        break;
                    }
                    case WAIT_ABANDONED:
                        break;
                }
            }
            //mainDraw();
            break;
        }
        break;
    }
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        Gdiplus::Graphics gf(hdc);
        gf.DrawImage(refBmp, 605, 25, 145 * CANVAS_WIDTH / CANVAS_HEIGHT, 145);
        EndPaint(hwnd, &ps);
        ReleaseDC(hwnd, hdc);
        break;
    }
    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
        break;
    }

    return 0;
}

LRESULT CALLBACK DrawCanvasProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message)
    {
    case WM_LBUTTONDOWN: {
        GetClientRect(hWnd, &clientRect);
        pt.x = (LONG)LOWORD(lParam);
        pt.y = (LONG)HIWORD(lParam);
        fDrawRect = true;
    }
        break;
    case WM_MOUSEMOVE:
    {
        if ((wParam && MK_LBUTTON) && fDrawRect && !isInitialLoad)
        {
            if (!IsRectEmpty(&rcTarget))
            {
                InvalidateRect(hWnd, NULL, true);
            }
            
            if ((pt.x < (LONG)LOWORD(lParam)) &&
                (pt.y > (LONG) HIWORD(lParam)))
            {
                left = pt.x;
                top = HIWORD(lParam);
                right = LOWORD(lParam);
                bottom = pt.y;
            }
            else if ((pt.x > (LONG)LOWORD(lParam)) &&
                (pt.y > (LONG)HIWORD(lParam)))
            {
                left = LOWORD(lParam);
                top = HIWORD(lParam);
                right = pt.x;
                bottom = pt.y;
            }
            else if ((pt.x > (LONG)LOWORD(lParam)) &&
                (pt.y < (LONG)HIWORD(lParam)))
            {
                left = LOWORD(lParam);
                top = pt.y;
                right = pt.x;
                bottom = HIWORD(lParam);
            }
            else
            {
                left = pt.x;
                top = pt.y;
                right = LOWORD(lParam);
                bottom = HIWORD(lParam);
            }
            Resize(left, top, right, bottom);
            CalculateDim();
            left = (((left) * 2.0f) / CANVAS_WIDTH) + (-1.0f);
            right = (((right) * 2.0f) / CANVAS_WIDTH) + (-1.0f);
            top = (((top) * -2.0f) / CANVAS_HEIGHT) + (1.0f);
            bottom = (((bottom) * -2.0f) / CANVAS_HEIGHT) + (1.0f);
            g_app->m_capture->InitializeScene(left, top, right, bottom);
            //mainDraw();
        }
    }
    break;
    case WM_LBUTTONUP: {
        fDrawRect = false;
        DBOUT(left << " " << top << " " << bottom << " " << right << "\n");
        DBOUT("rect: " << rcTarget.left << " " << rcTarget.top << " " << rcTarget.bottom << " " << rcTarget.right << "\n");
    }
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return (LRESULT)NULL;
}

#define ID_Edit1  200
BOOL CALLBACK EnumChildProc(HWND hwndChild, LPARAM lParam)
{
    RECT editSize;
    int idChild;
    idChild = GetWindowLong(hwndChild, GWL_ID);
    LPRECT rcParent;
    rcParent = (LPRECT)lParam;
    GetClientRect(hwndChild, &editSize);
    //Calculate the change ratio
    double cxRate = rcParent->right * 1.0 / CANVAS_WIDTH; //884 is width of client area
    double cyRate = rcParent->bottom * 1.0 / CANVAS_HEIGHT; //641 is height of client area

    double newRight = editSize.left * cxRate;
    double newTop = editSize.top * cyRate;
    double newWidth = editSize.right * cxRate;
    double newHeight = editSize.bottom * cyRate;

    MoveWindow(hwndChild, newRight, newTop, newWidth, newHeight, TRUE);

    // Make sure the child window is visible. 
    ShowWindow(hwndChild, SW_SHOW);
    return TRUE;
}

LRESULT CALLBACK  PrevCanvasProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE:
        break;
    case WM_CLOSE:
        DestroyWindow(hwnd);
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    }
    return DefWindowProc(hwnd, message, wParam, lParam);
}

std::vector<MonitorInfo> EnumerateAllMonitors()
{
    std::vector<MonitorInfo> monitors;
    EnumDisplayMonitors(nullptr, nullptr, [](HMONITOR hmon, HDC, LPRECT, LPARAM lparam)
        {
            auto& monitors = *reinterpret_cast<std::vector<MonitorInfo>*>(lparam);
            monitors.push_back(MonitorInfo(hmon));

            return TRUE;
        }, reinterpret_cast<LPARAM>(&monitors));
    return monitors;
}

void PopulateProcessList() {
    SendMessageW(hwndProcessList, (UINT)CB_RESETCONTENT, 0, 0);
    g_windows = EnumerateWindows();
    RECT tempRect;
    windowListSize = 0;
    // Populate combo box
    for (auto& window : g_windows)
    {
        GetClientRect(window.Hwnd(), &tempRect);
        if (tempRect.right > 0 && tempRect.bottom > 0) {
            SendMessage(hwndProcessList, CB_ADDSTRING, 0, (LPARAM)window.Title().c_str());
            windowListSize++;
        }
    }
    int cMonitors = GetSystemMetrics(SM_CMONITORS);
    monitors = EnumerateAllMonitors();
    // Add new monitors
    for (auto& mon : monitors)
    {
        winrt::check_hresult(static_cast<const int32_t>(SendMessageW(hwndProcessList, CB_ADDSTRING, 0, (LPARAM)mon.DisplayName.c_str())));
    }
}

double GetPSNR() {
    double PSNR = -1.0;
    if (!refGrayResized.empty() && convertedWidth > 0 && convertedHeight > 0) {
        try {
            //DBOUT("refGrayResized Mat " << refGrayResized.cols << " " << refGrayResized.rows << std::endl);
            //DBOUT("selGray Mat " << selGray.cols << " " << selGray.rows << std::endl);
            PSNR = cv::PSNR(selGray, refGrayResized);
        }
        catch (std::exception& e) {
            DBOUT("PSNR 847 " << e.what() << "\n");
        }
        if (isRemoverRunning) {
            if (PSNR >= threshold && !server.get_isLoading()) {
                server.send_to_ls("pausegametime\r\n");
                server.set_isLoading(true);
            }
            else if (PSNR < threshold && server.get_isLoading()) {
                server.send_to_ls("unpausegametime\r\n");
                server.set_isLoading(false);
            }
        }
        if (oldPSNR != PSNR) {
            oldPSNR = PSNR;
            if (!PostThreadMessage(mainThreadID, WM_SETPSNR, NULL, NULL)) {
                DBOUT(GetLastError() << "\n");
            }
        }
        //RedrawWindow(hwndPSNR, NULL, NULL, RDW_ERASE);
        //DBOUT(PSNR << "\n");
        if (showDiff && PSNR >= 0.0) {
            DisplayDiff();
        }
    }
    return PSNR;
}

void LoadFile(HWND hwnd, bool isRefImg) {
    IFileOpenDialog* pFileOpen;
    HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_ALL,
        IID_IFileOpenDialog, reinterpret_cast<void**>(&pFileOpen));
    if (SUCCEEDED(hr))
    {
        if (isRefImg) {
            COMDLG_FILTERSPEC rgSpec[] =
            {
                {L"*.jpg;*.jpeg;*.png;*.bmp;*.gif", L"*.jpg;*.jpeg;*.png;*.bmp;*.gif"},
            };
            pFileOpen->SetFileTypes(ARRAYSIZE(rgSpec), rgSpec);
        }
        else {
            COMDLG_FILTERSPEC rgSpec[] =
            {
                {L".dat", L"*.dat"},
            };
            pFileOpen->SetFileTypes(ARRAYSIZE(rgSpec), rgSpec);
        }
        hr = pFileOpen->Show(NULL);
        if (SUCCEEDED(hr))
        {
            IShellItem* pItem;
            hr = pFileOpen->GetResult(&pItem);
            if (SUCCEEDED(hr))
            {
                if (isRefImg) {
                    hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &filePath);
                    if (SUCCEEDED(hr))
                    {
                        RenderRefImage(hwnd);
                    }
                }
                else {
                    hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &saveFilePath);
                    if (SUCCEEDED(hr))
                    {
                        DWORD waitResult = WaitForSingleObject(
                            convertedLock,    // handle to mutex
                            INFINITE);  // no time-out interval
                        switch (waitResult) {
                        case WAIT_OBJECT_0: {
                            size_t convertedChars = 0;
                            std::wstring line;
                            std::wifstream ss(saveFilePath, std::wifstream::in);
                            std::getline(ss, line);
                            threshold = stod(line);
                            std::getline(ss, line);
                            rcTarget.left = stol(line);
                            std::getline(ss, line);
                            rcTarget.top = stol(line);
                            std::getline(ss, line);
                            rcTarget.right = stol(line);
                            std::getline(ss, line);
                            rcTarget.bottom = stol(line);
                            std::getline(ss, line);
                            filePath = new wchar_t[line.size()+1];
                            wcsncpy_s((wchar_t*)filePath, line.size()+1, line.c_str(), line.size());
                            std::getline(ss, line);
                            convertedX = stoi(line);
                            std::getline(ss, line);
                            convertedY = stoi(line);
                            std::getline(ss, line);
                            convertedWidth = stoi(line);
                            std::getline(ss, line);
                            convertedHeight = stoi(line);
                            ss.close();
                            Resize(rcTarget.left, rcTarget.top, rcTarget.right, rcTarget.bottom);
                            RenderRefImage(hwnd);
                            WCHAR buf[40];
                            swprintf(buf, sizeof(buf), L"%f", threshold);
                            SetWindowText(hwndThreshold, std::to_wstring(threshold).c_str());
                            SetWindowText(savedX, std::to_wstring(convertedX).c_str());
                            SetWindowText(savedY, std::to_wstring(convertedY).c_str());
                            SetWindowText(savedW, std::to_wstring(convertedWidth).c_str());
                            SetWindowText(savedH, std::to_wstring(convertedHeight).c_str());
                            intSavedX = convertedX;
                            intSavedY = convertedY;
                            intSavedW = convertedWidth;
                            intSavedH = convertedHeight;
                            //mainDraw();
                            SendMessage(hwndUpDnCtlX, UDM_SETPOS, 0, convertedX);
                            SendMessage(hwndUpDnCtlY, UDM_SETPOS, 0, convertedY);
                            SendMessage(hwndUpDnCtlW, UDM_SETPOS, 0, convertedWidth);
                            SendMessage(hwndUpDnCtlH, UDM_SETPOS, 0, convertedHeight);
                            ReleaseMutex(convertedLock);
                            left = (((rcTarget.left) * 2.0f) / CANVAS_WIDTH) + (-1.0f);
                            right = (((rcTarget.right) * 2.0f) / CANVAS_WIDTH) + (-1.0f);
                            top = (((rcTarget.top) * -2.0f) / CANVAS_HEIGHT) + (1.0f);
                            bottom = (((rcTarget.bottom) * -2.0f) / CANVAS_HEIGHT) + (1.0f);
                            break;
                        }
                        case WAIT_ABANDONED:
                            return;
                        }
                        
                    }
                }
                pItem->Release();
            }
        }
        pFileOpen->Release();
    }
}

void RenderRefImage(HWND hwnd) {
    g_app->InitializeOpenCLContext();
    int size = WideCharToMultiByte(CP_UTF8, 0, filePath, -1, NULL, 0, NULL, NULL);
    char* chars = new char[size];

    if (WideCharToMultiByte(CP_UTF8, 0, filePath, -1, chars, size, NULL, NULL) == size) {
        try {
            cv::imread(cv::String(chars)).copyTo(refMat);
            cv::cvtColor(refMat, refMat, cv::COLOR_BGR2BGRA);
        }
        catch (std::exception& e) {
            DBOUT("render ref " << e.what() << std::endl);
        }
        if (refBmp != nullptr) {
            delete refBmp;
            refBmp = nullptr;
        }
        if (!diff.empty()) {
            diff.release();
        }
        refBmp = new Gdiplus::Bitmap(filePath);
        EnableWindow(hwndDiff, true);
        InvalidateRect(hwnd, NULL, true);
    }
    delete[] chars;
}

void ResizeRefImage(int w, int h) {
    if (!refMat.empty()) {
        try {
            cv::resize(refMat, refResized, cv::Size(w, h), cv::INTER_NEAREST);
        }
        catch (std::exception& e) {
            DBOUT("cv resize 1317 " << e.what() << std::endl);
        }
        try {
            cv::cvtColor(refResized, refGrayResized, cv::COLOR_BGR2GRAY);
        }
        catch (std::exception& e) {
            DBOUT("cv cvtColor 1324 " << e.what() << std::endl);
        }
    }
}

void DisplayDiff() {
    try {
        cv::absdiff(uSelMat, refResized, diff);
    }
    catch (std::exception& e) {
        DBOUT("absdiff " << e.what() << "\n");
    }
}

void SaveFileAs(HWND hwnd) {
    TCHAR buff[MAX_LOADSTRING];
    GetWindowText(hwndThreshold, buff, MAX_LOADSTRING);
    threshold = _tstof(buff);
    if (IsRectEmpty(&rcTarget) || threshold == NULL || threshold == 0.0 || refMat.empty()) {
        MessageBox(hwnd, L"Need green box, threshold value, and a reference image.", TEXT("Error..."), MB_OK | MB_ICONERROR);
        return;
    }
    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED |
        COINIT_DISABLE_OLE1DDE);
    IFileSaveDialog* pFileSaveAs;
    hr = CoCreateInstance(CLSID_FileSaveDialog, NULL, CLSCTX_ALL,
        IID_IFileSaveDialog, reinterpret_cast<void**>(&pFileSaveAs));
    if (SUCCEEDED(hr)) {
        COMDLG_FILTERSPEC rgSpec[] =
        {
            {L".dat", L"*.dat;"},
        };
        hr = pFileSaveAs->SetFileTypes(ARRAYSIZE(rgSpec), rgSpec);
        hr = pFileSaveAs->SetDefaultExtension(L".dat");
        hr = pFileSaveAs->Show(NULL);
        if (SUCCEEDED(hr)) {
            IShellItem* pItem;
            hr = pFileSaveAs->GetResult(&pItem);
            if (SUCCEEDED(hr)) {
                hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &saveFilePath);
                if (SUCCEEDED(hr)) {
                    std::wofstream outData(saveFilePath);
                    if (outData.is_open()) {
                        DBOUT("filePath: " << filePath << "\n");
                        TCHAR buff[MAX_LOADSTRING];
                        GetWindowText(hwndThreshold, buff, MAX_LOADSTRING);
                        threshold = _tstof(buff);
                        outData << threshold << std::endl;
                        outData << rcTarget.left << std::endl;
                        outData << rcTarget.top << std::endl;
                        outData << rcTarget.right << std::endl;
                        outData << rcTarget.bottom << std::endl;
                        outData << filePath << std::endl;
                        outData << convertedX << std::endl;
                        outData << convertedY << std::endl;
                        outData << convertedWidth << std::endl;
                        outData << convertedHeight << std::endl;
                        outData.close();
                    }
                    SetWindowText(hwndThreshold, std::to_wstring(threshold).c_str());
                    SetWindowText(savedX, std::to_wstring(convertedX).c_str());
                    SetWindowText(savedY, std::to_wstring(convertedY).c_str());
                    SetWindowText(savedW, std::to_wstring(convertedWidth).c_str());
                    SetWindowText(savedH, std::to_wstring(convertedHeight).c_str());
                }
                pItem->Release();
            }
        }
        pFileSaveAs->Release();
    }
}

void SaveFile() {
    if (saveFilePath != NULL) {
        std::ofstream outData(saveFilePath);
        if (outData.is_open()) {
            TCHAR buff[MAX_LOADSTRING];
            GetWindowText(hwndThreshold, buff, MAX_LOADSTRING);
            threshold = _tstof(buff);
            outData << threshold << std::endl;
            outData << rcTarget.left << std::endl;
            outData << rcTarget.top << std::endl;
            outData << rcTarget.right << std::endl;
            outData << rcTarget.bottom << std::endl;
            outData << filePath << std::endl;
            outData << convertedX << std::endl;
            outData << convertedY << std::endl;
            outData << convertedWidth << std::endl;
            outData << convertedHeight << std::endl;
            outData.close();
        }
        SetWindowText(hwndThreshold, std::to_wstring(threshold).c_str());
        SetWindowText(savedX, std::to_wstring(convertedX).c_str());
        SetWindowText(savedY, std::to_wstring(convertedY).c_str());
        SetWindowText(savedW, std::to_wstring(convertedWidth).c_str());
        SetWindowText(savedH, std::to_wstring(convertedHeight).c_str());
    }
}

INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}