#pragma once

#include "ParticleEditorViewerWidget.h"

#include "Core/Reflection/ReflectionRegistry.h"
#include "Core/Paths.h"
#include "Core/ResourceManager.h"
#include "Editor/Asset/EditorAssetService.h"
#include "Editor/EditorEngine.h"
#include "Editor/UI/EditorMainPanel.h"
#include "Editor/UI/EditorMainPanelViewportToolbarHelpers.h"
#include "Editor/Viewer/EditorViewer.h"
#include "Editor/Viewer/ParticleEditorViewer.h"
#include "Editor/Viewport/FSceneViewport.h"
#include "Engine/Core/EditorResourcePaths.h"
#include "Asset/CurveColorAsset.h"
#include "Asset/CurveFloatAsset.h"
#include "Asset/CurveVectorAsset.h"
#include "Object/Class.h"
#include "Object/Property.h"
#include "Particle/ParticleAsset.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "WICTextureLoader.h"

#include <algorithm>
#include <cctype>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <Windows.h>
#include <commdlg.h>

namespace ParticleEditorInternal
{
constexpr const char* ParticleModuleDragPayload = "ParticleModule";
constexpr const char* ParticleEmitterDragPayload = "ParticleEmitter";
constexpr float EmitterNodeWidth = 198.0f;
constexpr float EmitterSeparatorGap = 10.0f;
constexpr int32 MaxParticleDragSelectionCount = 64;
constexpr ImU32 ParticleSelectionOutlineColor = IM_COL32(240, 219, 79, 255);

struct FParticleModuleDragPayload
{
	int32 EmitterIndex = -1;
	int32 LODIndex = -1;
	int32 ModuleIndex = -1;
	int32 Count = 0;
	int32 ModuleIndices[MaxParticleDragSelectionCount] = {};
};

struct FParticleEmitterDragPayload
{
	int32 EmitterIndex = -1;
	int32 Count = 0;
	int32 EmitterIndices[MaxParticleDragSelectionCount] = {};
};

struct FParticlePropertyRenderContext
{
	FParticleEditorViewer* Viewer = nullptr;
	UObject* Object = nullptr;
	UEditorEngine* EditorEngine = nullptr;
	bool* bUndoCaptured = nullptr;
};

struct FParticleModuleAddress
{
	EParticleEditorSelectionType Type = EParticleEditorSelectionType::None;
	int32 EmitterIndex = -1;
	int32 LODIndex = -1;
	int32 ModuleIndex = -1;
};

struct FParticleModuleMultiSelectionView
{
	TArray<int32>* ModuleIndices = nullptr;
	int32* EmitterIndex = nullptr;
	int32* LODIndex = nullptr;
};

struct FParticleCurveSourceView
{
	EParticleEditorSelectionType* Type = nullptr;
	int32* EmitterIndex = nullptr;
	int32* LODIndex = nullptr;
	int32* ModuleIndex = nullptr;
};

struct FParticleModuleRowContext
{
	FParticleEditorViewer* Viewer = nullptr;
	FParticleCurveSourceView CurveSource;
	FParticleModuleMultiSelectionView MultiSelection;
};

struct FParticleModuleRowDesc
{
	const char* Label = nullptr;
	FParticleModuleAddress Address;
	ImU32 BackgroundColor = 0;
};

// Viewer & Window Helpers ────────────────────────────────────────────────────
FParticleEditorViewer* AsParticleViewer(FEditorViewer* Viewer);
HWND ResolveSaveDialogOwnerWindow(const UEditorEngine* EditorEngine);
bool OpenParticleSaveFileDialog(HWND OwnerWindow, const FParticleEditorViewer* Viewer, FString& OutFilePath);

// Selection & Shortcut Helpers ───────────────────────────────────────────────
const char* GetSelectionLabel(EParticleEditorSelectionType Type);
const char* GetObjectLabel(const UObject* Object);
bool HasDeletableSelectedEmitter(FParticleEditorViewer* Viewer);
bool HasDeletableSelectedModule(FParticleEditorViewer* Viewer);
bool IsAnyPopupOpen();
void HandleParticleEditorShortcuts(FParticleEditorViewer* Viewer);

// Module Class Menu ──────────────────────────────────────────────────────────
void GetParticleModuleClasses(TArray<UClass*>& OutClasses);
void GetParticleTypeDataModuleClasses(TArray<UClass*>& OutClasses);
bool DrawParticleModuleClassMenu(FParticleEditorViewer* Viewer);

// Toolbar & Common Buttons ───────────────────────────────────────────────────
void DrawViewModeMenuItems(FParticleEditorViewer* Viewer);
bool DrawPopupButton(const char* Label, const char* PopupId);
bool DrawRoundedToolbarButton(const char* Id, const char* Label, const char* Tooltip, const ImVec2& Size);
bool DrawCascadeGraphButton(const char* Id, const ImVec2& Size, bool bActive);
bool DrawCurrentLODToolbarInput(FParticleEditorViewer* Viewer, ID3D11ShaderResourceView* Icon, const ImVec2& IconSize, const ImVec2& Size);

// Curve Toolbar & Grid Helpers ───────────────────────────────────────────────
float ChooseParticleCurveGridStep(float PixelsPerUnit, float TargetPixels);
bool DrawParticleCurveToolbarButton(const char* Id, ID3D11ShaderResourceView* Icon, const char* Label, bool bActive, bool bEnabled = true);
void DrawParticleCurveToolbarSeparator(const char* Id);

// Asset Combo Helpers ────────────────────────────────────────────────────────
bool DrawSearchableAssetPathCombo(const char* Label, const FString& Current, const TArray<FString>& Options, FString& OutSelectedPath);
bool PassesAssetSearchFilter(const FString& Path, const char* Filter);
void PushAssetComboStyle();
void PopAssetComboStyle();

// Panel & Details UI ─────────────────────────────────────────────────────────
void DrawParticlePanelTitle(const char* Title, const char* Subtitle);
void DrawParticleDetailsSection(const char* Title);
void DrawParticleDetailsText(const char* Label, const char* Value);

// Reflection Property Rendering ──────────────────────────────────────────────
const char* GetPropertyDisplayName(const FProperty& Property);
FString MakeParticlePropertyWidgetLabel(const FProperty& Property);
bool IsParticleGraphReferenceProperty(const FProperty& Property);
void CollectParticleEditableProperties(UObject* Object, TArray<const FProperty*>& OutProperties);
bool RenderParticleReflectionProperties(FParticlePropertyRenderContext& Context);
bool RenderParticlePropertyWidget(FParticlePropertyRenderContext& Context, const FProperty& Property);
bool RenderParticlePropertyValueWidget(FParticlePropertyRenderContext& Context, const FProperty& Property, void* ValuePtr, const char* Label);
bool RenderParticleObjectPtrWidget(const FProperty& Property, void* ValuePtr, const char* Label, UEditorEngine* EditorEngine);
bool RenderParticleSoftObjectPtrWidget(const FProperty& Property, void* ValuePtr, const char* Label, UEditorEngine* EditorEngine);
bool RenderParticleArrayPropertyWidget(FParticlePropertyRenderContext& Context, const FProperty& Property, void* ValuePtr);
bool RenderParticleStructPropertyWidget(FParticlePropertyRenderContext& Context, const FProperty& Property, void* ValuePtr, const char* Label);

// Particle Graph Resolve & Selection Helpers ─────────────────────────────────
UParticleLODLevel* ResolveParticleLOD(FParticleEditorViewer* Viewer, int32 EmitterIndex, int32 LODIndex);
UParticleModule* ResolveParticleModule(FParticleEditorViewer* Viewer, EParticleEditorSelectionType Type, int32 EmitterIndex, int32 LODIndex, int32 ModuleIndex);
void SelectParticleModuleTarget(FParticleEditorViewer* Viewer, EParticleEditorSelectionType Type, int32 EmitterIndex, int32 LODIndex, int32 ModuleIndex);

// Multi-Selection Helpers ────────────────────────────────────────────────────
bool ContainsIndex(const TArray<int32>& Indices, int32 Index);
void ToggleIndex(TArray<int32>& Indices, int32 Index);
void SeedEmitterMultiSelectionFromViewer(FParticleEditorViewer* Viewer, TArray<int32>& EmitterIndices);
void SeedModuleMultiSelectionFromViewer(FParticleEditorViewer* Viewer, TArray<int32>& ModuleIndices, int32& MultiEmitterIndex, int32& MultiLODIndex, int32 EmitterIndex, int32 LODIndex);
void ClearModuleMultiSelection(TArray<int32>& ModuleIndices, int32& EmitterIndex, int32& LODIndex);
void SetModuleMultiSelectionContext(TArray<int32>& ModuleIndices, int32& MultiEmitterIndex, int32& MultiLODIndex, int32 EmitterIndex, int32 LODIndex);

// Drag & Drop Payload Helpers ────────────────────────────────────────────────
void BuildModulePayload(FParticleModuleDragPayload& Payload, const FParticleModuleAddress& Address, const FParticleModuleMultiSelectionView& MultiSelection);
void BuildEmitterPayload(FParticleEmitterDragPayload& Payload, int32 EmitterIndex, const TArray<int32>& MultiSelectedEmitterIndices);
TArray<int32> GetPayloadModuleIndices(const FParticleModuleDragPayload& Payload);
TArray<int32> GetPayloadEmitterIndices(const FParticleEmitterDragPayload& Payload);
void ApplyModulePayloadToEmitter(FParticleEditorViewer* Viewer, const FParticleModuleDragPayload& Payload, int32 TargetEmitterIndex);

// Emitter Graph Rendering & Drag-Drop ────────────────────────────────────────
void DrawEmitterPreview(const ImVec2& Size, int32 EmitterIndex, bool bSelected);
void HandleModuleContextMenu(FParticleEditorViewer* Viewer, int32 EmitterIndex, int32 LODIndex, int32 ModuleIndex);
void HandleModuleDragDropTarget(FParticleEditorViewer* Viewer, const FParticleModuleAddress& Address);
void DrawSelectableModuleRow(const FParticleModuleRowContext& Context, const FParticleModuleRowDesc& Row);

// LOD & Numeric Input Helpers ────────────────────────────────────────────────
int32 GetSelectedEmitterLODCount(FParticleEditorViewer* Viewer);
bool IsUnsignedIntegerText(const char* Text);
int FilterUnsignedIntegerInput(ImGuiInputTextCallbackData* Data);
} // namespace ParticleEditorInternal
