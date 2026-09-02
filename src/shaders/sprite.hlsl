// ============================================================
// SPlite - 2D 精灵顶点着色器 + 像素着色器
// 职责：把像素坐标的四边形顶点转换成 NDC，并采样带 alpha 的精灵纹理。
// ============================================================

// 常量缓冲：与 C++ 端 Constants 结构体一一对应（注意 16 字节对齐）。
cbuffer Constants : register(b0)
{
    float2 viewportSize;   // 窗口客户区像素宽高
    float2 spriteSize;     // 精灵像素宽高
    float2 spritePosition; // 精灵左上角位置
    float  spriteScale;    // 统一缩放
    float  spriteOpacity;  // 整体透明度
};

struct VSInput
{
    float3 pos : POSITION;  // 像素坐标位置
    float2 uv  : TEXCOORD0; // 纹理坐标
};

struct VSOutput
{
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD0;
};

// 顶点着色器：像素坐标 -> NDC 坐标
VSOutput VSMain(VSInput input)
{
    VSOutput output;

    // NDC 范围是 [-1, 1]。像素中心(0,0) 在视口左上角。
    float2 ndc;
    float2 pixelPosition = input.pos.xy * spriteScale + spritePosition;
    ndc.x = (pixelPosition.x / viewportSize.x) * 2.0f - 1.0f;
    ndc.y = 1.0f - (pixelPosition.y / viewportSize.y) * 2.0f;

    output.pos = float4(ndc, 0.0f, 1.0f);
    output.uv  = input.uv;
    return output;
}

// 像素着色器：采样精灵纹理
Texture2D spriteTexture : register(t0);
SamplerState spriteSampler : register(s0);

float4 PSMain(VSOutput input) : SV_TARGET
{
    // DirectComposition 使用预乘 alpha：RGB 必须先乘以透明度。
    float4 color = spriteTexture.Sample(spriteSampler, input.uv);
    color.a *= spriteOpacity;
    return float4(color.rgb * color.a, color.a);
}
