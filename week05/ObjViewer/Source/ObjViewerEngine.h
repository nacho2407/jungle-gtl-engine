#pragma once

#include "Core/Engine.h"

class AStaticMeshActor;
class FObjViewerShell;
class FObjViewerViewportClient;
class FWindowsWindow;
class UStaticMesh;
class USceneComponent;

enum class EObjImportAxis : uint8_t
{
	PosX,
	NegX,
	PosY,
	NegY,
	PosZ,
	NegZ
};

enum class EObjImportPreset : uint8_t
{
	Auto,
	Custom,
	Blender,
	Maya,
	ThreeDSMax,
	Unreal,
	Unity
};

struct FObjImportSummary
{
	FString ImportSource = "Unknown";
	EObjImportPreset SourcePreset = EObjImportPreset::Auto;
	EObjImportAxis ForwardAxis = EObjImportAxis::PosX;
	EObjImportAxis UpAxis = EObjImportAxis::PosZ;
	bool bReplaceCurrentModel = true;
	bool bCenterToOrigin = true;
	bool bPlaceOnGround = false;
	bool bFrameCameraAfterImport = true;
	float UniformScale = 1.0f;
};

struct FObjViewerModelState
{
	bool bLoaded = false;

	FString SourceFilePath;
	FString FileName;

	uint64 FileSizeBytes = 0;
	AStaticMeshActor* DisplayActor = nullptr;
	UStaticMesh* Mesh = nullptr;

	int32 VertexCount = 0;
	int32 IndexCount = 0;
	int32 TriangleCount = 0;
	int32 SectionCount = 0;
	bool bHasUV = false;

	FVector BoundsCenter = FVector::ZeroVector;
	float BoundsRadius = 0.0f;
	FVector BoundsExtent = FVector::ZeroVector;

	FObjImportSummary LastImportSummary;
};

class FObjViewerEngine : public FEngine
{
public:
	FObjViewerEngine();
	~FObjViewerEngine() override;

	UScene* GetActiveScene() const override;
	UWorld* GetActiveWorld() const override;
	const FWorldContext* GetActiveWorldContext() const override;

	bool LoadModelFromFile(const FString& FilePath, const FObjImportSummary& ImportOptions);
	bool LoadModelFromFile(const FString& FilePath, const FString& ImportSource = "Unknown");
	bool ReloadLoadedModel();
	void ClearLoadedModel();
	void FrameLoadedModel();
	void ResetViewerCamera();

	bool HasLoadedModel() const { return ModelState.bLoaded && ModelState.Mesh != nullptr; }
	const FObjViewerModelState& GetModelState() const { return ModelState; }
	FObjViewerShell& GetShell() const;

protected:
	void BindHost(FWindowsWindow* InMainWindow) override;
	bool InitializeWorlds(int32 Width, int32 Height) override;
	bool InitializeMode() override;
	std::unique_ptr<IViewportClient> CreateViewportClient() override;
	void TickWorlds(float DeltaTime) override;
	void RenderFrame() override;

private:
	void InitializeViewerCamera() const;
	void UpdateLoadedModelState(
		const FString& FilePath,
		const FObjImportSummary& ImportOptions,
		UStaticMesh* Mesh,
		AStaticMeshActor* DisplayActor);

	FWorldContext* ViewerWorldContext = nullptr;
	FWindowsWindow* MainWindow = nullptr;
	FObjViewerModelState ModelState;
	std::unique_ptr<FObjViewerShell> ViewerShell;
};
