#pragma once

#include "Editor/UI/EditorWidget.h"
#include "Render/Types/RenderConstants.h"

class EditorShadowMapDebugWidget : public FEditorWidget
{
public:
	virtual void Render(float DeltaTime) override;

private:
	// 0=CSM(t21), 1=SpotAtlas(t22), 2=PointCube(t23)
	int32 SelectedTab = 0;

	// CSM: 선택된 cascade index
	int32 CSMCascadeIndex = 0;

	// Spot Atlas: 선택된 page index
	int32 SpotPageIndex = 0;

	// Point Cube: 선택된 cube index + face index
	int32 PointCubeIndex = 0;
	int32 PointFaceIndex = 0;
};
