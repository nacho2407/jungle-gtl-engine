#pragma once

#include "Viewport/EditorViewportClient.h"

#include <functional>

// Viewer-local display options. These do not inherit the Level Editor show flags.
struct FSkeletalViewerShowFlags
{
    bool bShowSkeletalMesh = true;
    bool bShowBones = false;
    bool bShowOnlySelectedBone = false;
    bool bShowSelectedBoneWeight = false;
    bool bShowBoundingBox = false;
    bool bShowOutline = false;
};

enum class ESkeletalMeshViewerMode
{
    BindPose,
    Animation
};

class FSkeletalMeshViewportClient : public FEditorViewportClient
{
public:
	using FBonePickHandler = std::function<bool(float LocalX, float LocalY)>;

	FSkeletalViewerShowFlags&       GetShowFlags()       { return ShowFlags; }
	const FSkeletalViewerShowFlags& GetShowFlags() const { return ShowFlags; }
	void SetBonePickHandler(FBonePickHandler InHandler);
	void SetBonePickingEnabled(bool bEnabled) { bBonePickingEnabled = bEnabled; }
	bool IsBonePickingEnabled() const { return bBonePickingEnabled; }

	bool ProcessInput(FViewportInputContext& Context) override;

private:
	bool ShouldTryBonePick(const FViewportInputContext& Context) const;
	bool IsGizmoUnderCursor(const FViewportInputContext& Context) const;

	FSkeletalViewerShowFlags ShowFlags;
	FBonePickHandler BonePickHandler;
	bool bBonePickingEnabled = true;
};
