#pragma once

#include "CoreMinimal.h"
#include "Renderer/RenderCommand.h"
#include <functional>
#include <memory>

class FEngine;
class FRenderer;
class FEditorEngine;
class FEditorViewportRegistry;
class FEditorUI;
class FGizmo;
class FBlitRenderer;
class FMaterial;
class FFrustum;
class FShowFlags;
class UScene;
struct FMeshData;

class FEditorViewportRenderService
{
public:
	using FBuildRenderCommands = std::function<void(
		FEngine*,
		UScene*,
		const FFrustum&,
		const FShowFlags&,
		const FVector&,
		FRenderCommandQueue&)>;

	void RenderAll(
		FEngine* Engine,
		FRenderer* Renderer,
		FEditorEngine* EditorEngine,
		FEditorViewportRegistry& ViewportRegistry,
		FEditorUI& EditorUI,
		FGizmo& Gizmo,
		FBlitRenderer& BlitRenderer,
		const std::shared_ptr<FMaterial>& WireFrameMaterial,
		FMeshData* GridMesh,
		FMaterial* GridMaterial,
		const FBuildRenderCommands& BuildRenderCommands) const;

private:
	static void ApplyWireframe(FRenderCommandQueue& Queue, FMaterial* WireMaterial);
};
