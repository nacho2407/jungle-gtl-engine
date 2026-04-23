// Shader include: Common/Resources/SystemResources.hlsl
// Role: shared shader code or editor/material entry.
// Slots: declared locally or in included common resources.

#ifndef SYSTEM_RESOURCES_HLSL
#define SYSTEM_RESOURCES_HLSL

Texture2D<float>  SceneDepth  : register(t10);
Texture2D<float4> SceneColor  : register(t11);
Texture2D<uint2>  StencilTex  : register(t13);

#endif

