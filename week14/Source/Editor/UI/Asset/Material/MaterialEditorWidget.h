#pragma once

#include "UI/Asset/AssetEditorWidget.h"
#include "UI/Panel/EditorPropertyRenderer.h"
#include "Viewport/Asset/StaticMeshEditorViewportClient.h"

#include "Object/FName.h"

#include <SimpleJSON/json.hpp>

#include <filesystem>

class UMaterial;
class UMaterialInterface;
class UMaterialInstance;
class UStaticMeshComponent;

class FMaterialEditorWidget : public FAssetEditorWidget
{
public:
	FMaterialEditorWidget();

	bool CanEdit(UObject* Object) const override;
	bool IsEditingObject(UObject* Object) const override;

	void Open(UObject* Object) override;
	void Close() override;
	void Tick(float DeltaTime) override;
	void Render(float DeltaTime) override;
	void CollectPreviewViewports(TArray<IEditorPreviewViewportClient*>& OutClients) const override;

	bool AllowsMultipleInstances() const override { return true; }

private:
	void RenderViewport();
	void RenderDetailsPanel(UMaterialInterface* Material);
	void RenderMaterialSettings(UMaterial* Material);
	void RenderBloomOverrides(UMaterialInstance* Instance);
	void RenderShaderParameters(UMaterialInterface* Material);
	void RenderTextureSection(UMaterialInterface* Material);
	bool SaveMaterialJson();

	void CreateMaterialInstanceAsset(UMaterial* ParentMaterial);

private:
	FStaticMeshEditorViewportClient ViewportClient;
	FEditorPropertyRenderer PropertyRenderer;

	UStaticMeshComponent* PreviewMeshComponent = nullptr;

	uint32 InstanceId = 0;
	FName PreviewWorldHandle = FName::None;
	FString WindowIdSuffix;

	std::filesystem::path MaterialPath;
	json::JSON CachedJson;
};
