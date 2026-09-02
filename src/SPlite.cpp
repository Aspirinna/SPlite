// SPlite.cpp : 定义应用程序的入口点。
//

#include "framework.h"
#include "SPlite.h"
#include "graphics\RendererD3D11.h"

#include <shellapi.h>
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
bool g_topMost = true;

constexpr int kPetWindowWidth  = 256;
constexpr int kPetWindowHeight = 256;
constexpr UINT kMenuToggleTopMost = 1001;
constexpr UINT kMenuExit          = 1002;

// 从可执行文件目录向上寻找仓库内素材，避免依赖某台机器的绝对路径。
std::wstring GetAssetPath(const wchar_t* fileName)
{
    wchar_t exePath[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);

    std::wstring root(exePath);
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
        return FALSE;
    }

    // 初始化 D3D11 渲染器，并加载一张带 alpha 的测试精灵。
    // 注意：实际产品中这里会换成资源管理器统一加载角色素材。
    {
        RECT rc = {};
        GetClientRect(g_hMainWnd, &rc);
        const int clientWidth  = rc.right - rc.left;
        const int clientHeight = rc.bottom - rc.top;

        if (g_renderer.Initialize(g_hMainWnd, clientWidth, clientHeight))
        {
            g_renderer.LoadSprite(GetAssetPath(L"sprite_test.png"));
        }
        else
        {
            // 渲染器失败的场景需在后续阶段完善（如弹出提示、降级为纯 Win32）。
        }
    }

    MSG msg = {};
    bool running = true;
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

        // 渲染一帧。这里会在消息空闲时持续刷新，保证动画连续。
        g_renderer.Render();
    }

    if (comInitialized)
    {
        CoUninitialize();
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
   const int x = workArea.right - kPetWindowWidth - 32;
   const int y = workArea.bottom - kPetWindowHeight - 32;

   // 工具窗口不会出现在任务栏；NOREDIRECTIONBITMAP 让 DirectComposition
   // 直接提供窗口内容；TOPMOST 让宠物保持在普通应用上方。
   const DWORD exStyle = WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOREDIRECTIONBITMAP;
   HWND hWnd = CreateWindowExW(exStyle, szWindowClass, szTitle, WS_POPUP,
      x, y, kPetWindowWidth, kPetWindowHeight,
      nullptr, nullptr, hInstance, nullptr);

   if (!hWnd)
   {
      return FALSE;
   }

   // 保存主窗口句柄，供 wWinMain 后续初始化渲染器使用。
   g_hMainWnd = hWnd;

   ShowWindow(hWnd, nCmdShow);
   UpdateWindow(hWnd);

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
        // 把实体像素上的左键按下转换为标题栏拖动，实现无边框窗口拖拽。
        ReleaseCapture();
        SendMessageW(hWnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
        return 0;
    case WM_RBUTTONUP:
        {
            POINT point = {};
            GetCursorPos(&point);

            HMENU menu = CreatePopupMenu();
            AppendMenuW(menu, MF_STRING | (g_topMost ? MF_CHECKED : 0),
                        kMenuToggleTopMost, L"始终置顶");
            AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
            AppendMenuW(menu, MF_STRING, kMenuExit, L"退出 SPlite");
            SetForegroundWindow(hWnd);
            const UINT command = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON,
                                                point.x, point.y, 0, hWnd, nullptr);
            DestroyMenu(menu);

            if (command == kMenuToggleTopMost)
            {
                g_topMost = !g_topMost;
                SetWindowPos(hWnd, g_topMost ? HWND_TOPMOST : HWND_NOTOPMOST,
                             0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
            }
            else if (command == kMenuExit)
            {
                DestroyWindow(hWnd);
            }
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
