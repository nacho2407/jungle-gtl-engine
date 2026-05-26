#pragma once

#include "Editor/UI/Panel/EditorPropertyRenderer.h"
#include "Editor/Viewport/Asset/ParticleSystemEditorViewportClient.h"
#include "Editor/UI/Asset/AssetEditorWidget.h"
#include "Object/FName.h"

struct ImVec2;
struct FPropertyValue;
struct FRawDistributionFloat;
struct FRawDistributionVector;
struct FFloatCurve;
class AActor;
class UObject;
class UParticleEmitter;
class UParticleLODLevel;
class UParticleModule;
class UParticleSystem;
class UParticleSystemComponent;

class FParticleSystemEditorWidget : public FAssetEditorWidget
{
public:
	FParticleSystemEditorWidget();
	~FParticleSystemEditorWidget() override;

	bool CanEdit(UObject* Object) const override;
	bool AllowsMultipleInstances() const override { return true; }
	void Open(UObject* Object) override;
	void Close() override;
	void Tick(float DeltaTime) override;
	void CollectPreviewViewports(TArray<IEditorPreviewViewportClient*>& OutClients) const override;
	void Render(float DeltaTime) override;

private:
	enum class ESelectionKind
	{
		ParticleSystem,
		Emitter,
		LOD,
		Module,
	};

	struct FEditorLayoutSizes;
	struct FEditorSelectionState
	{
		ESelectionKind Kind = ESelectionKind::ParticleSystem;
		int32 EmitterIndex = -1;
		int32 LODIndex = 0;
		int32 ModuleIndex = -1;
	};

	struct FEditorViewState
	{
		FEditorSelectionState Selection;
		bool bShowCurveEditor = true;
		bool bPreviewPlaying = true;
		bool bRestartPreviewRequested = false;
		int32 SoloEmitterIndex = -1;
		float PreviewTime = 0.0f;
		float PreviewAnimSpeed = 1.0f;
		float MainSplitRatio = 0.52f;
		float ViewportDetailsSplitRatio = 0.58f;
		float EmittersCurveSplitRatio = 0.58f;
	};

	enum class ETangentHandle
	{
		None,
		Arrive,
		Leave,
	};

	enum
	{
		MaxCurveEditorTracks = 16
	};

	struct FCurveEditorSelection
	{
		UObject* OwnerObject = nullptr;
		FFloatCurve* Curves[MaxCurveEditorTracks] = {};
		FString CurveLabels[MaxCurveEditorTracks];
		int32 CurveCount = 0;
		FString Label;
		int32 ActiveCurveIndex = 0;
	};

	struct FCurveEditorState
	{
		int32 SelectedKeyIndex = -1;
		bool bDraggingSelectedKey = false;
		ETangentHandle DraggingTangentHandle = ETangentHandle::None;
		bool bPanningView = false;
		bool bSuppressNextCanvasContextMenu = false;
		float PendingContextTime = 0.0f;
		float PendingContextValue = 0.0f;
		float ViewMinTime = 0.0f;
		float ViewMaxTime = 1.0f;
		float ViewMinValue = -1.0f;
		float ViewMaxValue = 1.0f;
	};

	UParticleSystem* GetParticleSystem() const;
	void ResetEditorState();
	void ValidateSelectionState(const UParticleSystem* ParticleSystem);
	void SelectParticleSystem();
	void SelectEmitter(int32 EmitterIndex);
	void SelectLOD(int32 EmitterIndex, int32 LODIndex);
	void SelectModule(int32 EmitterIndex, int32 LODIndex, int32 ModuleIndex);
	bool SelectLODByIndex(int32 LODIndex);
	bool SelectAdjacentLOD(int32 Direction);
	bool SelectExtremeLOD(bool bLowest);
	bool AddLODToSystem(bool bInsertAfterCurrent);
	bool RegenerateLowestLOD(bool bDuplicateHighest);
	bool DeleteSelectedLOD();
	bool IsEmitterSelected(int32 EmitterIndex) const;
	bool IsLODSelected(int32 EmitterIndex, int32 LODIndex) const;
	bool IsModuleSelected(int32 EmitterIndex, int32 LODIndex, int32 ModuleIndex) const;
	const char* GetSelectionKindLabel() const;
	int32 GetCurrentSystemLODIndex(const UParticleSystem* ParticleSystem) const;
	const UParticleEmitter* GetSelectedEmitter(const UParticleSystem* ParticleSystem) const;
	const UParticleLODLevel* GetSelectedLODLevel(const UParticleSystem* ParticleSystem) const;
	const UParticleModule* GetSelectedModule(const UParticleSystem* ParticleSystem) const;
	FEditorLayoutSizes CalculateLayoutSizes(const ImVec2& Available) const;
	void RenderToolbar();
	void RenderViewportPanel(const ImVec2& Size);
	void RenderDetailsPanel(const ImVec2& Size);
	void RenderEmittersPanel(const ImVec2& Size);
	void RenderCurveEditorPanel(const ImVec2& Size);
	bool RenderObjectProperties(UObject* Object, bool bReadOnly);
	void ApplyEditedObjectSideEffects(UObject* Object);
	void CreatePreviewWorld();
	void DestroyPreviewWorld();
	void RestartPreviewSimulation();
	void RefreshParticleSystemComponents();
	void ClearSelectedCurve();
	void SelectDistributionCurve(UObject* OwnerObject, FRawDistributionFloat* Distribution, const FString& Label, int32 CurveIndex);
	void SelectDistributionCurve(UObject* OwnerObject, FRawDistributionVector* Distribution, const FString& Label, int32 CurveIndex);
	bool SelectModuleDistributionCurves(UParticleModule* Module);
	bool AppendDistributionCurve(UObject* OwnerObject, FRawDistributionFloat* Distribution, const FString& Label);
	bool AppendDistributionCurve(UObject* OwnerObject, FRawDistributionVector* Distribution, const FString& Label);
	void RemoveSelectedCurve(int32 CurveIndex);
	int32 GetSelectedCurveCount() const;
	FFloatCurve* GetSelectedCurve(int32 CurveIndex) const;
	FFloatCurve* GetActiveSelectedCurve() const;
	void FitCurveViewToSelectedCurves();
	bool RenderSelectedCurveEditor(const ImVec2& Size);

private:
	FEditorViewState ViewState;
	FCurveEditorSelection CurveSelection;
	FCurveEditorState CurveEditorState;
	FEditorPropertyRenderer PropertyRenderer;
	FParticleSystemEditorViewportClient ViewportClient;
	AActor* PreviewActor = nullptr;
	UParticleSystemComponent* PreviewParticleComponent = nullptr;
	uint32 InstanceId = 0;
	FName PreviewWorldHandle = FName::None;
	FString WindowIdSuffix;
};
