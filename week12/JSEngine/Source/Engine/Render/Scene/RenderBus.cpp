#include "RenderBus.h"

void FRenderBus::Clear()
{
	for (uint32 i = 0; i < (uint32)ERenderPass::MAX; ++i)
	{
		PassQueues[i].clear();
	}

	LightInfos.clear();
	AmbientLightInfo	 = {};
	DirectionalLightInfo = {};
	ShadowLightRequests.clear();
	BoneMatrixConstantsPool.clear();
	BoneWeightHeatmapViewState = {};
    VignetteIntensity = 0.0f;
    VignetteRadius = 0.75f;
    VignetteSmoothness = 0.35f;
    VignetteColor = FColor::Black();
    CameraFadeColor = FVector4(0.0f, 0.0f, 0.0f, 1.0f);
    CameraFadeAlpha = 0.0f;
    LetterboxTargetAspect = 0.0f;
    LetterboxAmount = 0.0f;
}

void FRenderBus::AddCommand(ERenderPass Pass, const FRenderCommand& InCommand)
{
	PassQueues[(uint32)Pass].push_back(InCommand);
}

void FRenderBus::AddCommand(ERenderPass Pass, FRenderCommand&& InCommand)
{
	PassQueues[(uint32)Pass].push_back(std::move(InCommand));
}

const TArray<FRenderCommand>& FRenderBus::GetCommands(ERenderPass Pass) const
{
	return PassQueues[(uint32)Pass];
}

uint32 FRenderBus::AllocateBoneMatrixConstants()
{
	BoneMatrixConstantsPool.emplace_back();
	return static_cast<uint32>(BoneMatrixConstantsPool.size() - 1);
}

FBoneMatrixConstants* FRenderBus::GetMutableBoneMatrixConstants(uint32 Index)
{
	if (Index == InvalidBoneMatrixConstantsIndex || Index >= static_cast<uint32>(BoneMatrixConstantsPool.size()))
	{
		return nullptr;
	}

	return &BoneMatrixConstantsPool[Index];
}

const FBoneMatrixConstants* FRenderBus::GetBoneMatrixConstants(uint32 Index) const
{
	if (Index == InvalidBoneMatrixConstantsIndex || Index >= static_cast<uint32>(BoneMatrixConstantsPool.size()))
	{
		return nullptr;
	}

	return &BoneMatrixConstantsPool[Index];
}

const FBoneMatrixConstants* FRenderBus::GetBoneMatrixConstants(const FRenderCommand& Cmd) const
{
	return Cmd.bUseBoneMatrixConstants
		? GetBoneMatrixConstants(Cmd.BoneMatrixConstantsIndex)
		: nullptr;
}

void FRenderBus::SetViewProjection(const FMatrix& InView, const FMatrix& InProj, float InNearPlane, float InFarPlane)
{
	View = InView;
	Proj = InProj;
	NearPlane = InNearPlane;
	FarPlane = InFarPlane;

	const FMatrix CameraWorldMatrix = InView.GetInverse();
	CameraPosition = CameraWorldMatrix.GetOrigin();
	CameraForward = CameraWorldMatrix.GetForwardVector().GetSafeNormal();
	CameraRight = CameraWorldMatrix.GetRightVector().GetSafeNormal();
	CameraUp = CameraWorldMatrix.GetUpVector().GetSafeNormal();
}

void FRenderBus::SetRenderSettings(const EViewMode NewViewMode, const FShowFlags NewShowFlags)
{
	ViewMode = NewViewMode;
	ShowFlags = NewShowFlags;
}
