#pragma once

// 模块：Direct3D 11 渲染器
// 职责：负责 D3D11 设备、交换链、着色器和精灵贴图的创建与绘制。
// 设计：当前只提供一个“普通窗口内的 2D 精灵渲染”能力，
//       后续阶段会在此模块基础上扩展透明、多角色共享资源等特性。
// 依赖方向：graphics 只依赖 common/系统库，不依赖具体的“桌宠”逻辑。

#include <windows.h>
#include <d3d11.h>
#include <wrl/client.h>
#include <string>
#include <vector>

namespace splite
{

// 顶点：一个带位置 + 纹理坐标的 2D 顶点。
// 它与输入布局 D3D11_INPUT_ELEMENT_DESC 一一对应，二者必须保持匹配。
struct Vertex
{
    float pos[3];   // x, y, z（NDC 坐标，-1..1）
    float uv[2];    // u, v（纹理坐标，0..1）
};

// D3D11 渲染器。
// 对外暴露最少的接口：
//   Initialize -> 创建设备和交换链
//   OnResize   -> 窗口大小变化时重建后备缓冲区
//   LoadSprite -> 从 PNG 文件加载一张精灵贴图
//   Render     -> 绘制一帧
class RendererD3D11
{
public:
    RendererD3D11() noexcept;
    ~RendererD3D11();

    RendererD3D11(const RendererD3D11&) = delete;
    RendererD3D11& operator=(const RendererD3D11&) = delete;

    // 初始化 D3D11 设备、交换链、着色器、混合状态。
    // 返回 false 表示初始化失败，调用方应中止启动。
    bool Initialize(HWND hwnd, int width, int height);

    // 窗口客户区尺寸变化时需要重新创建后备缓冲区。
    // 返回 false 表示重建失败（通常意味着设备丢失，后续再处理）。
    bool OnResize(int width, int height);

    // 从磁盘读取一张 PNG 精灵贴图（保留 alpha 通道）。
    bool LoadSprite(const std::wstring& filePath);

    // 将当前精灵绘制到屏幕，并提交到交换链。
    void Render();

    // 调试：把当前后备缓冲区内容读取并保存为 PNG，用于验证渲染结果。
    void SaveBackBufferToPng(const wchar_t* filePath);

    // 释放所有 GPU 资源。析构函数也会调用，可重复调用。
    void Shutdown();

private:
    // -- 初始化分解步骤 --
    bool CreateDeviceAndSwapChain(HWND hwnd, int width, int height);
    bool CreateRenderTarget();
    bool CreateShaderResources();
    bool CreateQuadGeometry();
    bool CreateFixedState();

    // 从磁盘编译一段 HLSL 文件并返回字节码。
    bool CompileShader(const wchar_t* filePath,
                       const char* entryPoint,
                       const char* target,
                       ID3DBlob** outBlob);

    // -- 常用 GPU 对象 --
    Microsoft::WRL::ComPtr<ID3D11Device>        device_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context_;
    Microsoft::WRL::ComPtr<IDXGISwapChain>      swapChain_;

    Microsoft::WRL::ComPtr<ID3D11RenderTargetView>      renderTargetView_;
    Microsoft::WRL::ComPtr<ID3D11VertexShader>          vertexShader_;
    Microsoft::WRL::ComPtr<ID3D11PixelShader>           pixelShader_;
    Microsoft::WRL::ComPtr<ID3D11InputLayout>           inputLayout_;

    Microsoft::WRL::ComPtr<ID3D11Buffer>               vertexBuffer_;
    Microsoft::WRL::ComPtr<ID3D11Buffer>               indexBuffer_;
    Microsoft::WRL::ComPtr<ID3D11Buffer>               constantBuffer_;
    Microsoft::WRL::ComPtr<ID3D11SamplerState>         samplerState_;
    Microsoft::WRL::ComPtr<ID3D11BlendState>           blendState_;

    Microsoft::WRL::ComPtr<ID3D11Texture2D>            spriteTexture_;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>   spriteView_;

    // -- 当前窗口/视口状态 --
    int  clientWidth_  = 0;
    int  clientHeight_ = 0;

    // 精灵的原始像素尺寸，用于计算显示位置。
    unsigned int spriteWidth_  = 0;
    unsigned int spriteHeight_ = 0;

    // 顶点缓冲布局信息（供 IASetVertexBuffers 使用）。
    UINT vertexStride_ = sizeof(Vertex);
    UINT vertexOffset_ = 0;

    bool initialized_ = false;
};

} // namespace splite
