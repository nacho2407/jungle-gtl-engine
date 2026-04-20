#ifndef SYSTEM_RESOURCES_HLSL
#define SYSTEM_RESOURCES_HLSL

// ── System Textures ──
// Renderer가 패스 단위로 바인딩하는 프레임 공통 리소스.
// 슬롯 번호는 C++ ESystemTexSlot (RenderConstants.h)과 1:1 대응.

Texture2D<float>  SceneDepth  : register(t10);  // CopyResource된 Depth (R24_UNORM)
Texture2D<float4> SceneColor  : register(t11);  // CopyResource된 SceneColor (R8G8B8A8_UNORM)
// Texture2D       SceneAlbedo  : register(t12);  // (미래)
Texture2D<uint2>  StencilTex  : register(t13);  // CopyResource된 Stencil (X24_G8_UINT)

#endif // SYSTEM_RESOURCES_HLSL
