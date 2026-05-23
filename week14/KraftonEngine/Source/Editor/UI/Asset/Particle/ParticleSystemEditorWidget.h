#pragma once

#include "Editor/UI/Asset/AssetEditorWidget.h"

struct ImVec2;
struct FPropertyValue;
class UObject;
class UParticleEmitter;
class UParticleLODLevel;
class UParticleModule;
class UParticleSystem;

class FParticleSystemEditorWidget : public FAssetEditorWidget
{
public:
	FParticleSystemEditorWidget() = default;

	bool CanEdit(UObject* Object) const override;
	void Open(UObject* Object) override;
	void Close() override;
	void Tick(float DeltaTime) override;
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
		float PreviewTime = 0.0f;
	};

	UParticleSystem* GetParticleSystem() const;
	void ResetEditorState();
	void ValidateSelectionState(const UParticleSystem* ParticleSystem);
	void SelectParticleSystem();
	void SelectEmitter(int32 EmitterIndex);
	void SelectLOD(int32 EmitterIndex, int32 LODIndex);
	void SelectModule(int32 EmitterIndex, int32 LODIndex, int32 ModuleIndex);
	bool IsEmitterSelected(int32 EmitterIndex) const;
	bool IsLODSelected(int32 EmitterIndex, int32 LODIndex) const;
	bool IsModuleSelected(int32 EmitterIndex, int32 LODIndex, int32 ModuleIndex) const;
	const char* GetSelectionKindLabel() const;
	int32 GetDisplayLODIndex(const UParticleEmitter* Emitter) const;
	const UParticleEmitter* GetSelectedEmitter(const UParticleSystem* ParticleSystem) const;
	const UParticleLODLevel* GetSelectedLODLevel(const UParticleSystem* ParticleSystem) const;
	const UParticleModule* GetSelectedModule(const UParticleSystem* ParticleSystem) const;
	FEditorLayoutSizes CalculateLayoutSizes(const ImVec2& Available) const;
	void RenderToolbar();
	void RenderViewportPanel(const ImVec2& Size) const;
	void RenderDetailsPanel(const ImVec2& Size) const;
	void RenderEmittersPanel(const ImVec2& Size);
	void RenderCurveEditorPanel(const ImVec2& Size) const;
	void RenderObjectProperties(const UObject* Object) const;
	void RenderPropertyValueReadOnly(const FPropertyValue& PropertyValue) const;

private:
	FEditorViewState ViewState;
};
