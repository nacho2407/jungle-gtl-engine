#pragma once
#include "Viewport/EditorViewportClient.h"

// Skeletal Mesh Viewer 전용 토글. 글로벌 FShowFlags와 분리해 본 색/두께 같은
// 후속 옵션을 자유롭게 추가할 수 있게 한다.
struct FSkeletalViewerShowFlags
{
	bool bShowSkeletalMesh     = true;
	bool bShowBones            = false;
	// bShowBones가 true일 때만 의미. true면 Viewer::SelectedBoneIndex의 본 한 개만 그림.
	bool bShowOnlySelectedBone = false;
};

class FSkeletalMeshViewportClient : public FEditorViewportClient
{
public:
	FSkeletalViewerShowFlags&       GetShowFlags()       { return ShowFlags; }
	const FSkeletalViewerShowFlags& GetShowFlags() const { return ShowFlags; }

private:
	FSkeletalViewerShowFlags ShowFlags;
};
