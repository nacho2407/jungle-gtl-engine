#pragma once

#include "Editor/Viewport/Viewer/ViewerViewportClient.h"

#include <functional>

struct FSkeletalMeshViewerShowFlags
{
	bool bShowSkeletalMesh = true;
	bool bShowBones = false;
	bool bShowOnlySelectedBone = false;
	bool bShowBoundingBox = false;
	bool bShowOutline = false;
};

class FSkeletalMeshViewerViewportClient : public FViewerViewportClient
{
public:
	using FBonePickHandler = std::function<bool(float LocalX, float LocalY)>;

	FSkeletalMeshViewerShowFlags& GetShowFlags() { return ShowFlags; }
	const FSkeletalMeshViewerShowFlags& GetShowFlags() const { return ShowFlags; }

	void SetBonePickHandler(FBonePickHandler InHandler);
	bool ProcessInput(FViewportInputContext& Context) override;
	void BuildViewerShowFlags(FShowFlags& OutShowFlags) const override;

private:
	bool ShouldTryBonePick(const FViewportInputContext& Context) const;
	bool IsGizmoUnderCursor(const FViewportInputContext& Context) const;

	FSkeletalMeshViewerShowFlags ShowFlags;
	FBonePickHandler BonePickHandler;
};
