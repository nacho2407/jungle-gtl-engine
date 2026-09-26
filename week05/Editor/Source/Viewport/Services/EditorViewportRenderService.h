#pragma once

#include "CoreMinimal.h"
#include "Level/SceneRenderPacket.h"
#include "Viewport/ViewportTypes.h"
#include <functional>
#include <memory>

class FEngine;
class FRenderer;
class FEditorEngine;
class FEditorViewportRegistry;
class FEditorUI;
class FGizmo;
class FMaterial;
class FFrustum;
class FShowFlags;
class UWorld;
class ULevel;
struct FRenderMesh;

class FEditorViewportRenderService
{
public:
	using FBuildSceneRenderPacket = std::function<void(
		FEngine*,
		UWorld*,
		const FFrustum&,
		const FShowFlags&,
		FSceneRenderPacket&)>;

	// 에디터 전체 프레임 요청을 구성해 FRenderer에 전달한다.
	void RenderAll(
		FEngine* Engine,
		FRenderer* Renderer,
		FEditorEngine* EditorEngine,
		FEditorViewportRegistry& ViewportRegistry,
		FEditorUI& EditorUI,
		FGizmo& Gizmo,
		const std::shared_ptr<FMaterial>& WireFrameMaterial,
		FRenderMesh* GridMesh,
		FMaterial* GridMaterials[MAX_VIEWPORTS],
		const FBuildSceneRenderPacket& BuildSceneRenderPacket) const;
};
