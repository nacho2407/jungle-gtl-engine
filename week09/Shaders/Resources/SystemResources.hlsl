#ifndef SYSTEM_RESOURCES_HLSL
#define SYSTEM_RESOURCES_HLSL

#include "BindingSlots.hlsli"

Texture2D<float>  SceneDepth : REGISTER_T(SLOT_TEX_SCENE_DEPTH);
Texture2D<float4> SceneColor : REGISTER_T(SLOT_TEX_SCENE_COLOR);
Texture2D<uint2>  StencilTex : REGISTER_T(SLOT_TEX_STENCIL);

#endif
