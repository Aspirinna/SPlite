// SPlite.cpp : 定义应用程序的入口点。
//

#include "framework.h"
#include "SPlite.h"
#include "graphics\RendererD3D11.h"
#include "model\PetSystem.h"
#include "data\AppConfig.h"
#include "platform\windows\StartupManager.h"
#include "platform\windows\TrayIcon.h"
#include "steam\SteamService.h"

#include <shellapi.h>
#include <chrono>
#include <string>
#include <windowsx.h>

#define MAX_LOADSTRING 100

// 全局变量:
HINSTANCE hInst;                                // 当前实例
WCHAR szTitle[MAX_LOADSTRING];                  // 标题栏文本
WCHAR szWindowClass[MAX_LOADSTRING];            // 主窗口类名
HWND g_hMainWnd = nullptr;                      // 主窗口句柄（供渲染器使用）

// 当前只有一个窗口和一个渲染器。多角色阶段会把实例管理移出入口文件。
splite::RendererD3D11 g_renderer;
splite::PetSystem g_petSystem;
splite::TrayIcon g_trayIcon;
splite::AppConfig g_config;
splite::SteamService g_steamService;
bool g_topMost = true;

constexpr UINT kMenuToggleTopMost = 1001;
constexpr UINT kMenuExit          = 1002;
constexpr UINT kMenuToggleStartup = 1003;
constexpr UINT kTrayCallback      = WM_APP + 42;
constexpr int kEmergencyExitHotKey = 1;
constexpr int kHostWindowWidth  = 560;
constexpr int kHostWindowHeight = 300;

// 从可执行文件目录向上寻找仓库内素材，避免依赖某台机器的绝对路径。
std::wstring GetAssetPath(const wchar_t* fileName)
{
    wchar_t exePath[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);

    std::wstring root(exePath);
    const size_t executableSlash = root.find_last_of(L"\\/");
    if (executableSlash != std::wstring::npos)
    {
        const std::wstring packagedPath = root.substr(0, executableSlash) +
                                          L"\\assets\\" + fileName;
        if (GetFileAttributesW(packagedPath.c_str()) != INVALID_FILE_ATTRIBUTES)
        {
            return packagedPath;
        }
    }

    for (int level = 0; level < 3; ++level)
    {
        const size_t slash = root.find_last_of(L"\\/");
        if (slash == std::wstring::npos)
        {
            return fileName;
        }
        root.resize(slash);
    }
    return root + L"\\assets\\" + fileName;
}

void ShowPetMenu(HWND hWnd, POINT screenPoint)
{
    HMENU menu = CreatePopupMenu();
    AppendMenuW(menu, MF_STRING | (g_topMost ? MF_CHECKED : 0),
                kMenuToggleTopMost, L"始终置顶");
    AppendMenuW(menu, MF_STRING | (splite::StartupManager::IsEnabled() ? MF_CHECKED : 0),
                kMenuToggleStartup, L"开机启动");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, kMenuExit, L"退出 SPlite");

    SetForegroundWindow(hWnd);
    const UINT command = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON,
                                        screenPoint.x, screenPoint.y, 0, hWnd, nullptr);
    DestroyMenu(menu);

    if (command == kMenuToggleTopMost)
    {
        g_topMost = !g_topMost;
        g_config.topMost = g_topMost;
        g_config.Save();
        SetWindowPos(hWnd, g_topMost ? HWND_TOPMOST : HWND_NOTOPMOST,
                     0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    }
    else if (command == kMenuToggleStartup)
    {
        splite::StartupManager::SetEnabled(!splite::StartupManager::IsEnabled());
    }
    else if (command == kMenuExit)
    {
        DestroyWindow(hWnd);
    }
}

// 此代码模块中包含的函数的前向声明:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    // 同一用户会话只允许一个实例，避免重复创建多个桌面覆盖窗口。
    HANDLE singleInstanceMutex = CreateMutexW(nullptr, TRUE, L"Local\\SPlite.SingleInstance");
    if (singleInstanceMutex && GetLastError() == ERROR_ALREADY_EXISTS)
    {
        CloseHandle(singleInstanceMutex);
        return 0;
    }

    g_config = splite::AppConfig::Load();
    g_topMost = g_config.topMost;

    // 初始化 COM。WIC 图片解码器依赖 COM 对象。
    HRESULT hrCom = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const bool comInitialized = SUCCEEDED(hrCom);

    // 初始化全局字符串
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_SPLITE, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // 执行应用程序初始化:
    if (!InitInstance (hInstance, nCmdShow))
    {
        if (singleInstanceMutex) CloseHandle(singleInstanceMutex);
        return FALSE;
    }

    g_trayIcon.Initialize(g_hMainWnd, hInstance, kTrayCallback);
    g_steamService.Initialize();

    // 初始化 D3D11 渲染器，并加载一张带 alpha 的测试精灵。
    // 注意：实际产品中这里会换成资源管理器统一加载角色素材。
    {
        RECT rc = {};
        GetClientRect(g_hMainWnd, &rc);
        const int clientWidth  = rc.right - rc.left;
        const int clientHeight = rc.bottom - rc.top;

        const bool rendererReady = g_renderer.Initialize(g_hMainWnd, clientWidth, clientHeight) &&
                                   g_renderer.LoadSprite(GetAssetPath(L"sprite_test.png"));
        if (rendererReady)
        {
            // 两个实例共享同一 PNG/GPU 纹理，用于验证多角色调度和独立拖动。
            const float groundY = static_cast<float>(clientHeight - 256);
            g_petSystem.AddPet(L"pet-main", 280.0f, groundY);
            g_petSystem.AddPet(L"pet-companion", 30.0f, groundY, 0.82f);
            g_renderer.SetSpriteTransforms(g_petSystem.BuildRenderTransforms());
        }
        else
        {
            MessageBoxW(g_hMainWnd,
                        L"图形系统或角色素材加载失败。请查看临时目录中的 SPlite_renderer.log。",
                        L"SPlite 启动失败", MB_OK | MB_ICONERROR);
            DestroyWindow(g_hMainWnd);
        }
    }

    MSG msg = {};
    bool running = true;
    auto previousFrame = std::chrono::steady_clock::now();
    while (running)
    {
        // 泵消息：一次处理完所有已排队消息，避免阻塞。
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
            {
                running = false;
                break;
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        const auto currentFrame = std::chrono::steady_clock::now();
        const float deltaSeconds = std::chrono::duration<float>(currentFrame - previousFrame).count();
        previousFrame = currentFrame;

        g_petSystem.Update(deltaSeconds);
        g_steamService.Update();
        g_renderer.SetSpriteTransforms(g_petSystem.BuildRenderTransforms());

        // 渲染一帧。动画更新时间与消息处理相互独立。
        g_renderer.Render();
    }

    if (comInitialized)
    {
        CoUninitialize();
    }
    g_steamService.Shutdown();

    if (singleInstanceMutex)
    {
        ReleaseMutex(singleInstanceMutex);
        CloseHandle(singleInstanceMutex);
    }

    return (int) msg.wParam;
}

//
//  函数: MyRegisterClass()
//
//  目标: 注册窗口类。
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_SPLITE));
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    // DirectComposition 负责背景透明，因此不使用 GDI 背景刷和传统菜单栏。
    wcex.hbrBackground  = nullptr;
    wcex.lpszMenuName   = nullptr;
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

//
//  函数: InitInstance(HINSTANCE, int)
//
//  目标: 保存实例句柄并创建主窗口
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   hInst = hInstance;

   RECT workArea = {};
   SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0);
   // 当前使用紧凑宿主窗口，确保任何输入问题都只局限在宠物附近，
   // 不允许透明渲染窗口覆盖整个桌面工作区。
   const int x = workArea.right - kHostWindowWidth - 24;
   const int y = workArea.bottom - kHostWindowHeight - 24;

   // 工具窗口不会出现在任务栏；NOREDIRECTIONBITMAP 让 DirectComposition
   // 直接提供窗口内容；TOPMOST 让宠物保持在普通应用上方。
   // Debug 版本保留任务栏入口，便于图形捕获、调试和异常退出；
   // Release 版本使用真正的无任务栏桌宠窗口。
#if defined(_DEBUG)
   DWORD exStyle = WS_EX_APPWINDOW | WS_EX_NOREDIRECTIONBITMAP;
#else
   DWORD exStyle = WS_EX_TOOLWINDOW | WS_EX_NOREDIRECTIONBITMAP | WS_EX_NOACTIVATE;
#endif
   if (g_topMost)
   {
       exStyle |= WS_EX_TOPMOST;
   }
   HWND hWnd = CreateWindowExW(exStyle, szWindowClass, szTitle, WS_POPUP,
      x, y, kHostWindowWidth, kHostWindowHeight,
      nullptr, nullptr, hInstance, nullptr);

   if (!hWnd)
   {
      return FALSE;
   }

   // 保存主窗口句柄，供 wWinMain 后续初始化渲染器使用。
   g_hMainWnd = hWnd;

   ShowWindow(hWnd, nCmdShow);
   UpdateWindow(hWnd);
   RegisterHotKey(hWnd, kEmergencyExitHotKey,
                  MOD_CONTROL | MOD_SHIFT | MOD_NOREPEAT, VK_F12);

   return TRUE;
}

//
//  函数: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  目标: 处理主窗口的消息。
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_HOTKEY:
        if (wParam == kEmergencyExitHotKey)
        {
            DestroyWindow(hWnd);
        }
        return 0;
    case WM_NCHITTEST:
        {
            POINT point = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            ScreenToClient(hWnd, &point);
            if (!g_renderer.HitTest(point.x, point.y))
            {
                // 透明像素不拦截鼠标，下方窗口仍然可以正常操作。
                return HTTRANSPARENT;
            }
            return HTCLIENT;
        }
    case WM_LBUTTONDOWN:
        {
            const int x = GET_X_LPARAM(lParam);
            const int y = GET_Y_LPARAM(lParam);
            const int petIndex = g_renderer.HitTestSprite(x, y);
            if (g_petSystem.BeginDrag(petIndex, x, y))
            {
                SetCapture(hWnd);
            }
        }
        return 0;
    case WM_MOUSEMOVE:
        if (g_petSystem.IsDragging())
        {
            RECT client = {};
            GetClientRect(hWnd, &client);
            g_petSystem.DragTo(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam),
                               client.right, client.bottom);
        }
        return 0;
    case WM_LBUTTONUP:
        if (g_petSystem.IsDragging())
        {
            g_petSystem.EndDrag();
            ReleaseCapture();
        }
        return 0;
    case WM_RBUTTONUP:
        {
            POINT point = {};
            GetCursorPos(&point);
            ShowPetMenu(hWnd, point);
        }
        return 0;
    case kTrayCallback:
        if (LOWORD(lParam) == WM_RBUTTONUP || LOWORD(lParam) == WM_CONTEXTMENU)
        {
            POINT point = {};
            GetCursorPos(&point);
            ShowPetMenu(hWnd, point);
        }
        return 0;
    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            BeginPaint(hWnd, &ps);
            // 实际绘制由主消息循环调用 g_renderer.Render() 完成。
            // 这里只用于正确提交 WM_PAINT 的无效区域。
            EndPaint(hWnd, &ps);
        }
        break;
    case WM_SIZE:
        {
            // 客户区尺寸变化时，让渲染器重建后备缓冲区。
            const int w = LOWORD(lParam);
            const int h = HIWORD(lParam);
            if (w > 0 && h > 0)
            {
                g_renderer.OnResize(w, h);
                InvalidateRect(hWnd, nullptr, FALSE);
            }
        }
        break;
    case WM_DESTROY:
        UnregisterHotKey(hWnd, kEmergencyExitHotKey);
        g_trayIcon.Shutdown();
        g_renderer.Shutdown();
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// “关于”框的消息处理程序。
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
