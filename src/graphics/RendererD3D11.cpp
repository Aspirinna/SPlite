#include "RendererD3D11.h"

#include <d3dcompiler.h>
#include <wincodec.h>
#include <dxgi.h>
#include <cstring>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxgi.lib")
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
    float pad[4];
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

    const float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
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

    Constants constants{};
    constants.viewportSize[0] = static_cast<float>(clientWidth_);
    constants.viewportSize[1] = static_cast<float>(clientHeight_);
    constants.spriteSize[0]   = static_cast<float>(spriteWidth_);
    constants.spriteSize[1]   = static_cast<float>(spriteHeight_);

    context_->UpdateSubresource(constantBuffer_.Get(), 0, nullptr, &constants, 0, 0);

    context_->OMSetRenderTargets(1, renderTargetView_.GetAddressOf(), nullptr);
    context_->OMSetBlendState(blendState_.Get(), nullptr, 0xFFFFFFFF);

    context_->DrawIndexed(6, 0, 0);

    HRESULT hr = swapChain_->Present(1, 0);
    if (FAILED(hr))
    {
        WriteLog("Render Present FAIL");
    }
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
    blendState_.Reset();
    samplerState_.Reset();
    constantBuffer_.Reset();
    indexBuffer_.Reset();
    vertexBuffer_.Reset();
    inputLayout_.Reset();
    pixelShader_.Reset();
    vertexShader_.Reset();
    renderTargetView_.Reset();
    swapChain_.Reset();
    context_.Reset();
    device_.Reset();
    initialized_ = false;
}

bool RendererD3D11::CreateDeviceAndSwapChain(HWND hwnd, int width, int height)
{
    clientWidth_  = width;
    clientHeight_ = height;

    DXGI_SWAP_CHAIN_DESC scd{};
    scd.BufferDesc.Width  = width;
    scd.BufferDesc.Height = height;
    scd.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    scd.BufferDesc.RefreshRate.Numerator   = 60;
    scd.BufferDesc.RefreshRate.Denominator = 1;
    scd.SampleDesc.Count   = 1;
    scd.BufferUsage        = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.BufferCount        = 2;
    scd.OutputWindow       = hwnd;
    scd.Windowed           = TRUE;
    scd.SwapEffect         = DXGI_SWAP_EFFECT_FLIP_DISCARD;

    const D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0 };
    D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;

    CHECK_HR(D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
                levels, ARRAYSIZE(levels), D3D11_SDK_VERSION,
                &scd, &swapChain_, &device_, &featureLevel, &context_));
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
    root = root.substr(0, root.find_last_of(L"\\/"));
    root = root.substr(0, root.find_last_of(L"\\/"));
    const std::wstring shaderPath = root + L"\\src\\shaders\\sprite.hlsl";

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
    const float halfW = 128.0f;
    const float halfH = 128.0f;

    Vertex vertices[] = {
        { { -halfW, -halfH, 0.0f }, { 0.0f, 1.0f } },
        { {  halfW, -halfH, 0.0f }, { 1.0f, 1.0f } },
        { {  halfW,  halfH, 0.0f }, { 1.0f, 0.0f } },
        { { -halfW,  halfH, 0.0f }, { 0.0f, 0.0f } },
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
    bDesc.RenderTarget[0].SrcBlend              = D3D11_BLEND_SRC_ALPHA;
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
