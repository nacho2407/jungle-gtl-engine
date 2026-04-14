#pragma once
#include <d3d11.h>
#include "Core/CoreMinimal.h"

struct FPassRenderState;
struct FRenderTargetSet;
struct FRenderResources;
class FRenderBus;

struct FRenderPassContext
{
    const FRenderBus* RenderBus;
    const FRenderTargetSet* RenderTargets;
    const FPassRenderState* RenderState;
    ID3D11Device* Device;
    ID3D11DeviceContext* DeviceContext;
    FRenderResources* RenderResources;
};

namespace PassResource
{
static const char* SceneColor = "SceneColor";
static const char* SceneNormal = "SceneNormal";
static const char* SceneWorldPos = "SceneWorldPos";
};

enum class EResourceType
{
	SRV,
	RTV,
	DSV
};

struct FResourceBinding
{
	FString Name;
    EResourceType Type;
};
