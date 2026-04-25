#ifndef SYSTEM_RESOURCES_HLSL
#define SYSTEM_RESOURCES_HLSL

// ── System Textures ── (t16+)
// Renderer가 패스 단위로 바인딩하는 프레임 공통 리소스.
// 슬롯 번호는 C++ ESystemTexSlot (RenderConstants.h)과 1:1 대응.
// t0~t3: 머티리얼 | t8~t10: 라이팅 SB | t16+: 시스템

Texture2D<float>  SceneDepthTexture    : register(t16);  // CopyResource된 Depth (R24_UNORM)
Texture2D<float4> SceneColorTexture    : register(t17);  // CopyResource된 SceneColor (R8G8B8A8_UNORM)
Texture2D<float4> GBufferNormalTexture : register(t18);  // GBuffer World Normal (R16G16B16A16_FLOAT)
Texture2D<uint2>  StencilTexture       : register(t19);  // CopyResource된 Stencil (X24_G8_UINT)
Texture2D<float4> CullingHeatmapTexture : register(t20); // Tile Culling Heatmap (R8G8B8A8_UNORM)
Texture2D<float>  SpotLightAtlasTexture : register(t22); // Spotlight atlas (D32_FLOAT)

#endif // SYSTEM_RESOURCES_HLSL
