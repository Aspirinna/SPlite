#include "RendererD3D11.h"

#include <d3dcompiler.h>
#include <wincodec.h>
#include <dxgi1_2.h>
#include <algorithm>
#include <cmath>
#include <cstring>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dcomp.lib")
#pragma comment(lib, "windowscodecs.lib")

namespace splite
{

// 小工具：处理 COM 错误时的安全返回。
#define CHECK_HR(expr) do { HRESULT hr_ = (expr); if (FAILED(hr_)) return false; } while (0)
#define CHECK_HR_LOG(expr, msg) do { HRESULT hr_ = (expr); if (FAILED(hr_)) { WriteLog(msg); return false; } } while (0)

namespace
{
// 简单的调试日志：写入 %TEMP%\SPlite_renderer.log，便于排障。
void WriteLog(const char* message)
{
    wchar_t path[MAX_PATH] = {};
    GetTempPathW(MAX_PATH, path);
    wcscat_s(path, L"SPlite_renderer.log");

    FILE* f = nullptr;
    _wfopen_s(&f, path, L"a");
    if (f)
    {
        fprintf(f, "[%lu] %s\n", GetTickCount(), message);
        fclose(f);
    }
    OutputDebugStringA(message);
    OutputDebugStringA("\n");
}
}

// 给一个 2D 窗口用的简单常量缓冲：用于把像素坐标转换成 NDC。
struct Constants
{
    float viewportSize[2];
    float spriteSize[2];
    float spritePosition[2];
    float spriteScale;
    float spriteOpacity;
};

RendererD3D11::RendererD3D11() noexcept = default;

RendererD3D11::~RendererD3D11()
{
    Shutdown();
}

bool RendererD3D11::Initialize(HWND hwnd, int width, int height)
{
    WriteLog("Renderer Initialize: begin");
    if (!CreateDeviceAndSwapChain(hwnd, width, height)) { WriteLog("FAIL: CreateDeviceAndSwapChain"); return false; }
    if (!CreateRenderTarget())                            { WriteLog("FAIL: CreateRenderTarget"); return false; }
    if (!CreateShaderResources())                         { WriteLog("FAIL: CreateShaderResources"); return false; }
    if (!CreateQuadGeometry())                            { WriteLog("FAIL: CreateQuadGeometry"); return false; }
    if (!CreateFixedState())                              { WriteLog("FAIL: CreateFixedState"); return false; }

    initialized_ = true;
    WriteLog("Renderer Initialize: success");
    return true;
}

bool RendererD3D11::OnResize(int width, int height)
{
    if (!initialized_) return false;

    clientWidth_  = width;
    clientHeight_ = height;

    context_->OMSetRenderTargets(0, nullptr, nullptr);
    renderTargetView_.Reset();

    CHECK_HR(swapChain_->ResizeBuffers(2, width, height, DXGI_FORMAT_B8G8R8A8_UNORM, 0));
    return CreateRenderTarget();
}

bool RendererD3D11::LoadSprite(const std::wstring& filePath)
{
    WriteLog("LoadSprite: begin");
    if (!initialized_) { WriteLog("LoadSprite FAIL: not initialized"); return false; }

    Microsoft::WRL::ComPtr<IWICImagingFactory> factory;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
    if (FAILED(hr)) { WriteLog("LoadSprite FAIL: CoCreateInstance WIC factory"); return false; }

    Microsoft::WRL::ComPtr<IWICBitmapDecoder> decoder;
    CHECK_HR_LOG(factory->CreateDecoderFromFilename(filePath.c_str(), nullptr, GENERIC_READ,
                WICDecodeMetadataCacheOnLoad, &decoder),
                "LoadSprite FAIL: CreateDecoderFromFilename");

    Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> frame;
    CHECK_HR_LOG(decoder->GetFrame(0, &frame), "LoadSprite FAIL: GetFrame");

    Microsoft::WRL::ComPtr<IWICFormatConverter> converter;
    CHECK_HR_LOG(factory->CreateFormatConverter(&converter), "LoadSprite FAIL: CreateFormatConverter");
    CHECK_HR_LOG(converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppBGRA,
                WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeMedianCut),
                "LoadSprite FAIL: Converter Initialize");

    UINT width = 0, height = 0;
    CHECK_HR_LOG(converter->GetSize(&width, &height), "LoadSprite FAIL: GetSize");
    if (width == 0 || height == 0) { WriteLog("LoadSprite FAIL: zero size"); return false; }

    const UINT rowPitch = width * 4;
    std::vector<unsigned char> pixels(static_cast<size_t>(rowPitch) * height);
    CHECK_HR_LOG(converter->CopyPixels(nullptr, rowPitch, static_cast<UINT>(pixels.size()), pixels.data()),
                "LoadSprite FAIL: CopyPixels");

    spriteAlpha_.resize(static_cast<size_t>(width) * height);
    for (size_t pixelIndex = 0; pixelIndex < spriteAlpha_.size(); ++pixelIndex)
    {
        spriteAlpha_[pixelIndex] = pixels[pixelIndex * 4 + 3];
    }

    D3D11_TEXTURE2D_DESC texDesc{};
    texDesc.Width            = width;
    texDesc.Height           = height;
    texDesc.MipLevels        = 1;
    texDesc.ArraySize        = 1;
    texDesc.Format           = DXGI_FORMAT_B8G8R8A8_UNORM;
    texDesc.SampleDesc.Count = 1;
    texDesc.Usage            = D3D11_USAGE_DEFAULT;
    texDesc.BindFlags        = D3D11_BIND_SHADER_RESOURCE;
    texDesc.CPUAccessFlags   = 0;

    D3D11_SUBRESOURCE_DATA initData{};
    initData.pSysMem     = pixels.data();
    initData.SysMemPitch = rowPitch;

    CHECK_HR_LOG(device_->CreateTexture2D(&texDesc, &initData, &spriteTexture_),
                "LoadSprite FAIL: CreateTexture2D");
    CHECK_HR_LOG(device_->CreateShaderResourceView(spriteTexture_.Get(), nullptr, &spriteView_),
                "LoadSprite FAIL: CreateShaderResourceView");

    spriteWidth_  = width;
    spriteHeight_ = height;
    WriteLog("LoadSprite: success");
    return true;
}

void RendererD3D11::Render()
{
    if (!initialized_ || !spriteView_)
    {
        WriteLog("Render skipped: not initialized or no sprite");
        return;
    }

    // 合成窗口必须以完全透明色清屏。RGB 也清零，满足预乘 alpha 约定。
    const float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    context_->ClearRenderTargetView(renderTargetView_.Get(), clearColor);

    context_->IASetInputLayout(inputLayout_.Get());
    context_->IASetVertexBuffers(0, 1, vertexBuffer_.GetAddressOf(), &vertexStride_, &vertexOffset_);
    context_->IASetIndexBuffer(indexBuffer_.Get(), DXGI_FORMAT_R16_UINT, 0);
    context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    context_->VSSetShader(vertexShader_.Get(), nullptr, 0);
    context_->VSSetConstantBuffers(0, 1, constantBuffer_.GetAddressOf());

    context_->PSSetShader(pixelShader_.Get(), nullptr, 0);
    context_->PSSetShaderResources(0, 1, spriteView_.GetAddressOf());
    context_->PSSetSamplers(0, 1, samplerState_.GetAddressOf());

    context_->OMSetRenderTargets(1, renderTargetView_.GetAddressOf(), nullptr);
    context_->OMSetBlendState(blendState_.Get(), nullptr, 0xFFFFFFFF);

    // 所有实例共享纹理、几何和管线状态，仅更新轻量的常量缓冲。
    for (const SpriteTransform& transform : spriteTransforms_)
    {
        Constants constants{};
        constants.viewportSize[0] = static_cast<float>(clientWidth_);
        constants.viewportSize[1] = static_cast<float>(clientHeight_);
        constants.spriteSize[0]   = static_cast<float>(spriteWidth_);
        constants.spriteSize[1]   = static_cast<float>(spriteHeight_);
        constants.spritePosition[0] = transform.x;
        constants.spritePosition[1] = transform.y;
        constants.spriteScale       = transform.scale;
        constants.spriteOpacity     = transform.opacity;
        context_->UpdateSubresource(constantBuffer_.Get(), 0, nullptr, &constants, 0, 0);
        context_->DrawIndexed(6, 0, 0);
    }

    HRESULT hr = swapChain_->Present(1, 0);
    if (FAILED(hr))
    {
        WriteLog("Render Present FAIL");
    }
}

void RendererD3D11::SetSpriteTransform(float x, float y, float scale, float opacity) noexcept
{
    spriteTransforms_ = { SpriteTransform{ x, y, scale, opacity } };
}

void RendererD3D11::SetSpriteTransforms(const std::vector<SpriteTransform>& transforms)
{
    spriteTransforms_ = transforms;
}

bool RendererD3D11::HitTest(int clientX, int clientY, unsigned char alphaThreshold) const noexcept
{
    return HitTestSprite(clientX, clientY, alphaThreshold) >= 0;
}

int RendererD3D11::HitTestSprite(int clientX, int clientY, unsigned char alphaThreshold) const noexcept
{
    if (spriteAlpha_.empty())
    {
        return -1;
    }

    for (size_t reverseIndex = spriteTransforms_.size(); reverseIndex > 0; --reverseIndex)
    {
        const SpriteTransform& transform = spriteTransforms_[reverseIndex - 1];
        const float safeScale = transform.scale > 0.0f ? transform.scale : 1.0f;
        const int spriteX = static_cast<int>((clientX - transform.x) / safeScale);
        const int spriteY = static_cast<int>((clientY - transform.y) / safeScale);

        if (spriteX < 0 || spriteY < 0 ||
            spriteX >= static_cast<int>(spriteWidth_) ||
            spriteY >= static_cast<int>(spriteHeight_))
        {
            continue;
        }

        const size_t alphaIndex = static_cast<size_t>(spriteY) * spriteWidth_ +
                                  static_cast<size_t>(spriteX);
        if (spriteAlpha_[alphaIndex] >= alphaThreshold)
        {
            return static_cast<int>(reverseIndex - 1);
        }
    }
    return -1;
}

HRGN RendererD3D11::CreateInteractionRegion(unsigned char alphaThreshold) const
{
    std::vector<RECT> rectangles;

    if (!spriteAlpha_.empty())
    {
        for (const SpriteTransform& transform : spriteTransforms_)
        {
            if (transform.scale <= 0.0f || transform.opacity <= 0.0f)
            {
                continue;
            }

            for (unsigned int y = 0; y < spriteHeight_; ++y)
            {
                unsigned int x = 0;
                while (x < spriteWidth_)
                {
                    while (x < spriteWidth_ &&
                           static_cast<float>(spriteAlpha_[static_cast<size_t>(y) * spriteWidth_ + x]) *
                               transform.opacity < alphaThreshold)
                    {
                        ++x;
                    }
                    const unsigned int runStart = x;
                    while (x < spriteWidth_ &&
                           static_cast<float>(spriteAlpha_[static_cast<size_t>(y) * spriteWidth_ + x]) *
                               transform.opacity >= alphaThreshold)
                    {
                        ++x;
                    }

                    if (runStart < x)
                    {
                        RECT rectangle = {
                            static_cast<LONG>(std::floor(transform.x + runStart * transform.scale)),
                            static_cast<LONG>(std::floor(transform.y + y * transform.scale)),
                            static_cast<LONG>(std::ceil(transform.x + x * transform.scale)),
                            static_cast<LONG>(std::ceil(transform.y + (y + 1) * transform.scale))
                        };
                        if (rectangle.right > rectangle.left && rectangle.bottom > rectangle.top)
                        {
                            rectangles.push_back(rectangle);
                        }
                    }
                }
            }
        }
    }

    if (rectangles.empty())
    {
        return CreateRectRgn(0, 0, 0, 0);
    }

    RECT bounds = rectangles.front();
    for (const RECT& rectangle : rectangles)
    {
        bounds.left   = (std::min)(bounds.left, rectangle.left);
        bounds.top    = (std::min)(bounds.top, rectangle.top);
        bounds.right  = (std::max)(bounds.right, rectangle.right);
        bounds.bottom = (std::max)(bounds.bottom, rectangle.bottom);
    }

    const DWORD regionBytes = static_cast<DWORD>(sizeof(RGNDATAHEADER) +
                                                  rectangles.size() * sizeof(RECT));
    std::vector<unsigned char> regionBuffer(regionBytes);
    auto* regionData = reinterpret_cast<RGNDATA*>(regionBuffer.data());
    regionData->rdh.dwSize   = sizeof(RGNDATAHEADER);
    regionData->rdh.iType    = RDH_RECTANGLES;
    regionData->rdh.nCount   = static_cast<DWORD>(rectangles.size());
    regionData->rdh.nRgnSize = static_cast<DWORD>(rectangles.size() * sizeof(RECT));
    regionData->rdh.rcBound  = bounds;
    std::memcpy(regionData->Buffer, rectangles.data(), rectangles.size() * sizeof(RECT));

    return ExtCreateRegion(nullptr, regionBytes, regionData);
}

void RendererD3D11::SaveBackBufferToPng(const wchar_t* filePath)
{
    if (!swapChain_) return;

    Microsoft::WRL::ComPtr<ID3D11Texture2D> backBuffer;
    if (FAILED(swapChain_->GetBuffer(0, IID_PPV_ARGS(&backBuffer)))) return;

    D3D11_TEXTURE2D_DESC desc{};
    backBuffer->GetDesc(&desc);

    D3D11_TEXTURE2D_DESC stagingDesc = desc;
    stagingDesc.Usage          = D3D11_USAGE_STAGING;
    stagingDesc.BindFlags      = 0;
    stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    stagingDesc.MiscFlags      = 0;

    Microsoft::WRL::ComPtr<ID3D11Texture2D> staging;
    if (FAILED(device_->CreateTexture2D(&stagingDesc, nullptr, &staging))) return;

    context_->CopyResource(staging.Get(), backBuffer.Get());

    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (FAILED(context_->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped))) return;

    const UINT width  = desc.Width;
    const UINT height = desc.Height;
    const UINT pitch  = mapped.RowPitch;

    Microsoft::WRL::ComPtr<IWICImagingFactory> factory;
    if (SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory))))
    {
        Microsoft::WRL::ComPtr<IWICStream> stream;
        if (SUCCEEDED(factory->CreateStream(&stream)) &&
            SUCCEEDED(stream->InitializeFromFilename(filePath, GENERIC_WRITE)))
        {
            Microsoft::WRL::ComPtr<IWICBitmapEncoder> encoder;
            if (SUCCEEDED(factory->CreateEncoder(GUID_ContainerFormatPng, nullptr, &encoder)) &&
                SUCCEEDED(encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache)))
            {
                Microsoft::WRL::ComPtr<IWICBitmapFrameEncode> frame;
                Microsoft::WRL::ComPtr<IPropertyBag2> props;
                if (SUCCEEDED(encoder->CreateNewFrame(&frame, &props)) &&
                    SUCCEEDED(frame->Initialize(props.Get())))
                {
                    frame->SetSize(width, height);
                    WICPixelFormatGUID format = GUID_WICPixelFormat32bppBGRA;
                    frame->SetPixelFormat(&format);
                    frame->WritePixels(height, pitch, pitch * height, static_cast<BYTE*>(mapped.pData));
                    frame->Commit();
                }
                encoder->Commit();
            }
        }
    }

    context_->Unmap(staging.Get(), 0);
    WriteLog("SaveBackBufferToPng done");
}

void RendererD3D11::Shutdown()
{
    spriteView_.Reset();
    spriteTexture_.Reset();
    spriteAlpha_.clear();
    blendState_.Reset();
    samplerState_.Reset();
    constantBuffer_.Reset();
    indexBuffer_.Reset();
    vertexBuffer_.Reset();
    inputLayout_.Reset();
    pixelShader_.Reset();
    vertexShader_.Reset();
    renderTargetView_.Reset();
    compositionVisual_.Reset();
    compositionTarget_.Reset();
    compositionDevice_.Reset();
    swapChain_.Reset();
    context_.Reset();
    device_.Reset();
    initialized_ = false;
}

bool RendererD3D11::CreateDeviceAndSwapChain(HWND hwnd, int width, int height)
{
    clientWidth_  = width;
    clientHeight_ = height;

    const D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0 };
    D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;

    UINT deviceFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#if defined(_DEBUG)
    deviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
    CHECK_HR(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, deviceFlags,
                levels, ARRAYSIZE(levels), D3D11_SDK_VERSION,
                &device_, &featureLevel, &context_));

    Microsoft::WRL::ComPtr<IDXGIDevice> dxgiDevice;
    CHECK_HR(device_.As(&dxgiDevice));

    Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
    CHECK_HR(dxgiDevice->GetAdapter(&adapter));

    Microsoft::WRL::ComPtr<IDXGIFactory2> factory;
    CHECK_HR(adapter->GetParent(IID_PPV_ARGS(&factory)));

    DXGI_SWAP_CHAIN_DESC1 scd{};
    scd.Width       = static_cast<UINT>(width);
    scd.Height      = static_cast<UINT>(height);
    scd.Format      = DXGI_FORMAT_B8G8R8A8_UNORM;
    scd.Stereo      = FALSE;
    scd.SampleDesc.Count = 1;
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.BufferCount = 2;
    scd.Scaling     = DXGI_SCALING_STRETCH;
    scd.SwapEffect  = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    scd.AlphaMode   = DXGI_ALPHA_MODE_PREMULTIPLIED;

    Microsoft::WRL::ComPtr<IDXGISwapChain1> compositionSwapChain;
    CHECK_HR(factory->CreateSwapChainForComposition(device_.Get(), &scd, nullptr,
                                                      &compositionSwapChain));
    CHECK_HR(compositionSwapChain.As(&swapChain_));

    CHECK_HR(DCompositionCreateDevice(dxgiDevice.Get(),
                                      __uuidof(IDCompositionDevice),
                                      reinterpret_cast<void**>(compositionDevice_.GetAddressOf())));
    CHECK_HR(compositionDevice_->CreateTargetForHwnd(hwnd, TRUE, &compositionTarget_));
    CHECK_HR(compositionDevice_->CreateVisual(&compositionVisual_));
    CHECK_HR(compositionVisual_->SetContent(swapChain_.Get()));
    CHECK_HR(compositionTarget_->SetRoot(compositionVisual_.Get()));
    CHECK_HR(compositionDevice_->Commit());
    return true;
}

bool RendererD3D11::CreateRenderTarget()
{
    Microsoft::WRL::ComPtr<ID3D11Texture2D> backBuffer;
    CHECK_HR(swapChain_->GetBuffer(0, IID_PPV_ARGS(&backBuffer)));
    CHECK_HR(device_->CreateRenderTargetView(backBuffer.Get(), nullptr, &renderTargetView_));

    D3D11_VIEWPORT vp{};
    vp.Width    = static_cast<float>(clientWidth_);
    vp.Height   = static_cast<float>(clientHeight_);
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    context_->RSSetViewports(1, &vp);
    return true;
}

bool RendererD3D11::CreateShaderResources()
{
    wchar_t exePath[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    std::wstring root(exePath);
    const size_t lastSlash = root.find_last_of(L"\\/");
    if (lastSlash != std::wstring::npos) root.resize(lastSlash);

    // 安装包把着色器放在 exe 旁的 shaders 目录；开发构建则从仓库读取。
    std::wstring shaderPath = root + L"\\shaders\\sprite.hlsl";
    if (GetFileAttributesW(shaderPath.c_str()) == INVALID_FILE_ATTRIBUTES)
    {
        root = root.substr(0, root.find_last_of(L"\\/"));
        root = root.substr(0, root.find_last_of(L"\\/"));
        shaderPath = root + L"\\src\\shaders\\sprite.hlsl";
    }

    Microsoft::WRL::ComPtr<ID3DBlob> vsBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> psBlob;
    if (!CompileShader(shaderPath.c_str(), "VSMain", "vs_5_0", &vsBlob)) return false;
    if (!CompileShader(shaderPath.c_str(), "PSMain", "ps_5_0", &psBlob)) return false;

    CHECK_HR(device_->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &vertexShader_));
    CHECK_HR(device_->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &pixelShader_));

    D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex, pos), D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(Vertex, uv), D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    CHECK_HR(device_->CreateInputLayout(layout, ARRAYSIZE(layout),
                vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &inputLayout_));

    D3D11_BUFFER_DESC cbDesc{};
    cbDesc.ByteWidth = sizeof(Constants);
    cbDesc.Usage     = D3D11_USAGE_DEFAULT;
    cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    CHECK_HR(device_->CreateBuffer(&cbDesc, nullptr, &constantBuffer_));
    return true;
}

bool RendererD3D11::CreateQuadGeometry()
{
    // 当前测试角色与窗口均为 256 像素。顶点使用客户区像素坐标，
    // 后续多角色批处理阶段会把位置和尺寸移入实例数据。
    const float width  = 256.0f;
    const float height = 256.0f;

    Vertex vertices[] = {
        { { 0.0f,  height, 0.0f }, { 0.0f, 1.0f } },
        { { width, height, 0.0f }, { 1.0f, 1.0f } },
        { { width, 0.0f,  0.0f }, { 1.0f, 0.0f } },
        { { 0.0f,  0.0f,  0.0f }, { 0.0f, 0.0f } },
    };
    const unsigned short indices[] = { 0, 1, 2, 0, 2, 3 };

    D3D11_BUFFER_DESC vbDesc{};
    vbDesc.ByteWidth = sizeof(vertices);
    vbDesc.Usage     = D3D11_USAGE_IMMUTABLE;
    vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA vbData{};
    vbData.pSysMem = vertices;
    CHECK_HR(device_->CreateBuffer(&vbDesc, &vbData, &vertexBuffer_));

    D3D11_BUFFER_DESC ibDesc{};
    ibDesc.ByteWidth = sizeof(indices);
    ibDesc.Usage     = D3D11_USAGE_IMMUTABLE;
    ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    D3D11_SUBRESOURCE_DATA ibData{};
    ibData.pSysMem = indices;
    CHECK_HR(device_->CreateBuffer(&ibDesc, &ibData, &indexBuffer_));
    return true;
}

bool RendererD3D11::CreateFixedState()
{
    D3D11_SAMPLER_DESC sDesc{};
    sDesc.Filter         = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sDesc.AddressU       = D3D11_TEXTURE_ADDRESS_CLAMP;
    sDesc.AddressV       = D3D11_TEXTURE_ADDRESS_CLAMP;
    sDesc.AddressW       = D3D11_TEXTURE_ADDRESS_CLAMP;
    sDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    CHECK_HR(device_->CreateSamplerState(&sDesc, &samplerState_));

    D3D11_BLEND_DESC bDesc{};
    bDesc.RenderTarget[0].BlendEnable           = TRUE;
    // DirectComposition 要求交换链内容采用预乘 alpha。
    bDesc.RenderTarget[0].SrcBlend              = D3D11_BLEND_ONE;
    bDesc.RenderTarget[0].DestBlend             = D3D11_BLEND_INV_SRC_ALPHA;
    bDesc.RenderTarget[0].BlendOp               = D3D11_BLEND_OP_ADD;
    bDesc.RenderTarget[0].SrcBlendAlpha         = D3D11_BLEND_ONE;
    bDesc.RenderTarget[0].DestBlendAlpha        = D3D11_BLEND_ZERO;
    bDesc.RenderTarget[0].BlendOpAlpha          = D3D11_BLEND_OP_ADD;
    bDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    CHECK_HR(device_->CreateBlendState(&bDesc, &blendState_));
    return true;
}

bool RendererD3D11::CompileShader(const wchar_t* filePath, const char* entryPoint, const char* target, ID3DBlob** outBlob)
{
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
    HRESULT hr = D3DCompileFromFile(filePath, nullptr, nullptr, entryPoint, target,
                                    D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, 0,
                                    outBlob, &errorBlob);
    if (FAILED(hr))
    {
        if (errorBlob)
        {
            OutputDebugStringA(static_cast<const char*>(errorBlob->GetBufferPointer()));
            WriteLog("CompileShader FAIL (see debug output)");
        }
        else
        {
            WriteLog("CompileShader FAIL with no blob");
        }
        return false;
    }
    return true;
}

} // namespace splite
