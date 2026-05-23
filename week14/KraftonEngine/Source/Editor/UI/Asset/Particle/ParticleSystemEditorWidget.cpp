#include "ParticleSystemEditorWidget.h"

#include "Core/Property/SoftObjectProperty.h"
#include "Object/Object.h"
#include "Particles/Module/ParticleModule.h"
#include "Particles/Module/ParticleModuleTypeDataBase.h"
#include "Particles/ParticleSystem.h"

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <imgui.h>

namespace
{
	constexpr float MinColumnWidth = 360.0f;
	constexpr float MinViewportHeight = 220.0f;
	constexpr float MinDetailsHeight = 160.0f;
	constexpr float ToolbarHeight = 34.0f;
	constexpr float EmitterColumnWidth = 176.0f;
	constexpr float EmitterHeaderHeight = 58.0f;
	constexpr float ModuleRowHeight = 24.0f;
	constexpr int32 NoModuleIndex = -1;
	constexpr int32 TypeDataModuleIndex = -2;

	void DrawPanelHeader(const char* Label)
	{
		const ImVec2 Pos = ImGui::GetCursorScreenPos();
		const float Width = ImGui::GetContentRegionAvail().x;
		const float Height = 24.0f;
		ImDrawList* DrawList = ImGui::GetWindowDrawList();
		DrawList->AddRectFilled(Pos, ImVec2(Pos.x + Width, Pos.y + Height), IM_COL32(34, 34, 36, 255));
		DrawList->AddText(ImVec2(Pos.x + 8.0f, Pos.y + 4.0f), IM_COL32(220, 224, 232, 255), Label);
		ImGui::Dummy(ImVec2(Width, Height + 4.0f));
	}

	void DrawModuleRow(const char* Label, bool bSelected, ImU32 AccentColor)
	{
		const ImVec2 Pos = ImGui::GetCursorScreenPos();
		const float Width = ImGui::GetContentRegionAvail().x;
		ImDrawList* DrawList = ImGui::GetWindowDrawList();
		const ImU32 BackgroundColor = bSelected ? IM_COL32(78, 82, 92, 255) : IM_COL32(29, 30, 35, 255);
		DrawList->AddRectFilled(Pos, ImVec2(Pos.x + Width, Pos.y + ModuleRowHeight), BackgroundColor);
		DrawList->AddRectFilled(Pos, ImVec2(Pos.x + 4.0f, Pos.y + ModuleRowHeight), AccentColor);
		DrawList->AddText(ImVec2(Pos.x + 10.0f, Pos.y + 4.0f), IM_COL32(235, 238, 242, 255), Label);
	}

	bool SelectableModuleRow(const char* Label, bool bSelected, ImU32 AccentColor)
	{
		DrawModuleRow(Label, bSelected, AccentColor);
		return ImGui::InvisibleButton("##ModuleRow", ImVec2(ImGui::GetContentRegionAvail().x, ModuleRowHeight));
	}

	void DrawDetailRow(const char* Label, const char* Value)
	{
		ImGui::TextUnformatted(Label);
		ImGui::SameLine(180.0f);
		ImGui::TextDisabled("%s", Value ? Value : "");
	}

	void DrawDetailRowF(const char* Label, const char* Format, ...)
	{
		char Buffer[256];
		va_list Args;
		va_start(Args, Format);
		std::vsnprintf(Buffer, sizeof(Buffer), Format, Args);
		va_end(Args);
		DrawDetailRow(Label, Buffer);
	}

	const char* GetRenderTypeLabel(EParticleRenderType RenderType)
	{
		switch (RenderType)
		{
		case EParticleRenderType::Sprite: return "Sprite";
		case EParticleRenderType::Mesh: return "Mesh";
		case EParticleRenderType::Ribbon: return "Ribbon";
		case EParticleRenderType::Beam: return "Beam";
		case EParticleRenderType::GPU: return "GPU";
		default: return "Unknown";
		}
	}

	EParticleRenderType GetLODRenderType(const UParticleLODLevel* LODLevel)
	{
		const UParticleModuleTypeDataBase* TypeDataModule = LODLevel ? LODLevel->GetTypeDataModule() : nullptr;
		return TypeDataModule ? TypeDataModule->GetRenderType() : EParticleRenderType::Sprite;
	}

	const char* GetTypeDataDisplayName(const UParticleModuleTypeDataBase* TypeDataModule)
	{
		if (!TypeDataModule)
		{
			return "Sprite Data";
		}

		switch (TypeDataModule->GetRenderType())
		{
		case EParticleRenderType::Mesh: return "Mesh Data";
		case EParticleRenderType::Ribbon: return "Ribbon Data";
		case EParticleRenderType::Beam: return "Beam Data";
		case EParticleRenderType::GPU: return "GPU Sprites";
		case EParticleRenderType::Sprite: return "Sprite Data";
		default: return TypeDataModule->GetClass()->GetName();
		}
	}

	const char* GetModuleDisplayName(const UParticleModule* Module)
	{
		if (!Module)
		{
			return "(null module)";
		}
		if (Cast<UParticleModuleRequired>(Module)) return "Required";
		if (Cast<UParticleModuleSpawn>(Module)) return "Spawn";
		if (Cast<UParticleModuleLifetime>(Module)) return "Lifetime";
		if (Cast<UParticleModuleLocation>(Module)) return "Initial Location";
		if (Cast<UParticleModuleVelocity>(Module)) return "Initial Velocity";
		if (Cast<UParticleModuleColor>(Module)) return "Initial Color";
		if (Cast<UParticleModuleSize>(Module)) return "Initial Size";
		return Module->GetClass()->GetName();
	}

	ImU32 GetModuleAccentColor(const UParticleModule* Module)
	{
		if (Cast<UParticleModuleRequired>(Module)) return IM_COL32(190, 190, 92, 255);
		if (Cast<UParticleModuleSpawn>(Module)) return IM_COL32(200, 92, 92, 255);
		if (Cast<UParticleModuleColor>(Module)) return IM_COL32(88, 140, 88, 255);
		if (Module && Module->IsSpawnModule()) return IM_COL32(92, 120, 180, 255);
		if (Module && Module->IsUpdateModule()) return IM_COL32(150, 95, 185, 255);
		return IM_COL32(88, 92, 105, 255);
	}

	ImU32 GetTypeDataAccentColor(EParticleRenderType RenderType)
	{
		switch (RenderType)
		{
		case EParticleRenderType::Mesh: return IM_COL32(125, 178, 105, 255);
		case EParticleRenderType::Ribbon: return IM_COL32(95, 160, 190, 255);
		case EParticleRenderType::Beam: return IM_COL32(180, 110, 185, 255);
		case EParticleRenderType::GPU: return IM_COL32(110, 135, 210, 255);
		case EParticleRenderType::Sprite: default: return IM_COL32(120, 125, 135, 255);
		}
	}

	FString GetParticleSystemTitle(const UParticleSystem* ParticleSystem, bool bDirty)
	{
		FString Title = "Particle System Editor";
		if (ParticleSystem)
		{
			const FString& AssetPath = ParticleSystem->GetAssetPathFileName();
			if (!AssetPath.empty() && AssetPath != "None")
			{
				Title += " - ";
				Title += AssetPath;
			}
		}
		if (bDirty)
		{
			Title += " *";
		}
		return Title;
	}
}

struct FParticleSystemEditorWidget::FEditorLayoutSizes
{
	float LeftWidth = 0.0f;
	float RightWidth = 0.0f;
	float TopHeight = 0.0f;
	float BottomHeight = 0.0f;
};

bool FParticleSystemEditorWidget::CanEdit(UObject* Object) const
{
	return Object && Object->IsA<UParticleSystem>();
}

void FParticleSystemEditorWidget::Open(UObject* Object)
{
	if (!CanEdit(Object))
	{
		return;
	}

	FAssetEditorWidget::Open(Object);
	ResetEditorState();
}

void FParticleSystemEditorWidget::Close()
{
	ResetEditorState();
	FAssetEditorWidget::Close();
}

void FParticleSystemEditorWidget::Tick(float DeltaTime)
{
	if (!IsOpen() || !ViewState.bPreviewPlaying)
	{
		return;
	}

	if (ViewState.bRestartPreviewRequested)
	{
		ViewState.PreviewTime = 0.0f;
		ViewState.bRestartPreviewRequested = false;
	}

	ViewState.PreviewTime += DeltaTime;
}

void FParticleSystemEditorWidget::Render(float DeltaTime)
{
	(void)DeltaTime;

	UParticleSystem* ParticleSystem = GetParticleSystem();
	if (!IsOpen() || !ParticleSystem)
	{
		return;
	}
	ValidateSelectionState(ParticleSystem);

	bool bWindowOpen = true;
	FString VisibleTitle = GetParticleSystemTitle(ParticleSystem, IsDirty());
	FString WindowTitle = VisibleTitle + "###ParticleSystemEditor";

	ImGui::SetNextWindowSize(ImVec2(1280.0f, 720.0f), ImGuiCond_Once);
	if (ConsumeFocusRequest())
	{
		ImGui::SetNextWindowFocus();
	}

	if (!ImGui::Begin(WindowTitle.c_str(), &bWindowOpen))
	{
		ImGui::End();
		if (!bWindowOpen)
		{
			Close();
		}
		return;
	}

	RenderToolbar();
	ImGui::Separator();

	const ImGuiStyle& Style = ImGui::GetStyle();
	const ImVec2 Available = ImGui::GetContentRegionAvail();
	const FEditorLayoutSizes Layout = CalculateLayoutSizes(Available);

	ImGui::BeginChild("##ParticleEditorLeftColumn", ImVec2(Layout.LeftWidth, 0.0f), ImGuiChildFlags_None);
	RenderViewportPanel(ImVec2(0.0f, Layout.TopHeight));
	ImGui::Dummy(ImVec2(0.0f, Style.ItemSpacing.y));
	RenderDetailsPanel(ImVec2(0.0f, Layout.BottomHeight));
	ImGui::EndChild();

	ImGui::SameLine();

	ImGui::BeginChild("##ParticleEditorRightColumn", ImVec2(Layout.RightWidth, 0.0f), ImGuiChildFlags_None);
	if (ViewState.bShowCurveEditor)
	{
		RenderEmittersPanel(ImVec2(0.0f, Layout.TopHeight));
		ImGui::Dummy(ImVec2(0.0f, Style.ItemSpacing.y));
		RenderCurveEditorPanel(ImVec2(0.0f, Layout.BottomHeight));
	}
	else
	{
		RenderEmittersPanel(ImVec2(0.0f, 0.0f));
	}
	ImGui::EndChild();

	ImGui::End();

	if (!bWindowOpen)
	{
		Close();
	}
}

UParticleSystem* FParticleSystemEditorWidget::GetParticleSystem() const
{
	if (!EditedObject || !EditedObject->IsA<UParticleSystem>())
	{
		return nullptr;
	}

	return static_cast<UParticleSystem*>(EditedObject);
}

void FParticleSystemEditorWidget::ResetEditorState()
{
	ViewState = FEditorViewState{};
}

void FParticleSystemEditorWidget::ValidateSelectionState(const UParticleSystem* ParticleSystem)
{
	if (!ParticleSystem)
	{
		SelectParticleSystem();
		return;
	}

	const TArray<UParticleEmitter*>& Emitters = ParticleSystem->GetEmitters();
	if (Emitters.empty())
	{
		SelectParticleSystem();
		return;
	}

	FEditorSelectionState& Selection = ViewState.Selection;
	if (Selection.EmitterIndex < 0 || Selection.EmitterIndex >= static_cast<int32>(Emitters.size()))
	{
		if (Selection.Kind == ESelectionKind::ParticleSystem)
		{
			return;
		}
		SelectEmitter(0);
		return;
	}

	const UParticleEmitter* Emitter = Emitters[Selection.EmitterIndex];
	const int32 LODCount = Emitter ? static_cast<int32>(Emitter->GetLODLevels().size()) : 0;
	if (LODCount <= 0)
	{
		Selection.LODIndex = 0;
		if (Selection.Kind == ESelectionKind::LOD || Selection.Kind == ESelectionKind::Module)
		{
			SelectEmitter(Selection.EmitterIndex);
		}
		return;
	}

	if (Selection.LODIndex < 0 || Selection.LODIndex >= LODCount)
	{
		Selection.LODIndex = 0;
	}

	const UParticleLODLevel* LODLevel = Emitter->GetLODLevel(Selection.LODIndex);
	const int32 ModuleCount = LODLevel ? static_cast<int32>(LODLevel->GetModules().size()) : 0;
	if (Selection.Kind == ESelectionKind::Module)
	{
		const bool bValidTypeDataSelection = Selection.ModuleIndex == TypeDataModuleIndex && LODLevel && LODLevel->GetTypeDataModule();
		const bool bValidModuleSelection = Selection.ModuleIndex >= 0 && Selection.ModuleIndex < ModuleCount;
		if (!bValidTypeDataSelection && !bValidModuleSelection)
		{
			SelectEmitter(Selection.EmitterIndex);
		}
	}
}

void FParticleSystemEditorWidget::SelectParticleSystem()
{
	ViewState.Selection = FEditorSelectionState{};
}

void FParticleSystemEditorWidget::SelectEmitter(int32 EmitterIndex)
{
	ViewState.Selection.Kind = ESelectionKind::Emitter;
	ViewState.Selection.EmitterIndex = EmitterIndex;
	ViewState.Selection.LODIndex = 0;
	ViewState.Selection.ModuleIndex = NoModuleIndex;
}

void FParticleSystemEditorWidget::SelectLOD(int32 EmitterIndex, int32 LODIndex)
{
	ViewState.Selection.Kind = ESelectionKind::LOD;
	ViewState.Selection.EmitterIndex = EmitterIndex;
	ViewState.Selection.LODIndex = LODIndex;
	ViewState.Selection.ModuleIndex = NoModuleIndex;
}

void FParticleSystemEditorWidget::SelectModule(int32 EmitterIndex, int32 LODIndex, int32 ModuleIndex)
{
	ViewState.Selection.Kind = ESelectionKind::Module;
	ViewState.Selection.EmitterIndex = EmitterIndex;
	ViewState.Selection.LODIndex = LODIndex;
	ViewState.Selection.ModuleIndex = ModuleIndex;
}

bool FParticleSystemEditorWidget::IsEmitterSelected(int32 EmitterIndex) const
{
	const FEditorSelectionState& Selection = ViewState.Selection;
	return Selection.EmitterIndex == EmitterIndex &&
		(Selection.Kind == ESelectionKind::Emitter || Selection.Kind == ESelectionKind::LOD || Selection.Kind == ESelectionKind::Module);
}

bool FParticleSystemEditorWidget::IsLODSelected(int32 EmitterIndex, int32 LODIndex) const
{
	const FEditorSelectionState& Selection = ViewState.Selection;
	return Selection.EmitterIndex == EmitterIndex &&
		Selection.LODIndex == LODIndex &&
		(Selection.Kind == ESelectionKind::LOD || Selection.Kind == ESelectionKind::Module);
}

bool FParticleSystemEditorWidget::IsModuleSelected(int32 EmitterIndex, int32 LODIndex, int32 ModuleIndex) const
{
	const FEditorSelectionState& Selection = ViewState.Selection;
	return Selection.Kind == ESelectionKind::Module &&
		Selection.EmitterIndex == EmitterIndex &&
		Selection.LODIndex == LODIndex &&
		Selection.ModuleIndex == ModuleIndex;
}

const char* FParticleSystemEditorWidget::GetSelectionKindLabel() const
{
	switch (ViewState.Selection.Kind)
	{
	case ESelectionKind::ParticleSystem: return "Particle System";
	case ESelectionKind::Emitter: return "Emitter";
	case ESelectionKind::LOD: return "LOD";
	case ESelectionKind::Module: return "Module";
	default: return "Unknown";
	}
}

int32 FParticleSystemEditorWidget::GetDisplayLODIndex(const UParticleEmitter* Emitter) const
{
	if (!Emitter || Emitter->GetLODLevels().empty())
	{
		return 0;
	}

	const int32 LODCount = static_cast<int32>(Emitter->GetLODLevels().size());
	return (std::max)(0, (std::min)(ViewState.Selection.LODIndex, LODCount - 1));
}

const UParticleEmitter* FParticleSystemEditorWidget::GetSelectedEmitter(const UParticleSystem* ParticleSystem) const
{
	if (!ParticleSystem)
	{
		return nullptr;
	}

	const TArray<UParticleEmitter*>& Emitters = ParticleSystem->GetEmitters();
	const int32 EmitterIndex = ViewState.Selection.EmitterIndex;
	if (EmitterIndex < 0 || EmitterIndex >= static_cast<int32>(Emitters.size()))
	{
		return nullptr;
	}

	return Emitters[EmitterIndex];
}

const UParticleLODLevel* FParticleSystemEditorWidget::GetSelectedLODLevel(const UParticleSystem* ParticleSystem) const
{
	const UParticleEmitter* Emitter = GetSelectedEmitter(ParticleSystem);
	if (!Emitter)
	{
		return nullptr;
	}

	return Emitter->GetLODLevel(GetDisplayLODIndex(Emitter));
}

const UParticleModule* FParticleSystemEditorWidget::GetSelectedModule(const UParticleSystem* ParticleSystem) const
{
	if (ViewState.Selection.Kind != ESelectionKind::Module)
	{
		return nullptr;
	}

	const UParticleLODLevel* LODLevel = GetSelectedLODLevel(ParticleSystem);
	if (!LODLevel)
	{
		return nullptr;
	}

	if (ViewState.Selection.ModuleIndex == TypeDataModuleIndex)
	{
		return LODLevel->GetTypeDataModule();
	}

	const TArray<UParticleModule*>& Modules = LODLevel->GetModules();
	const int32 ModuleIndex = ViewState.Selection.ModuleIndex;
	if (ModuleIndex < 0 || ModuleIndex >= static_cast<int32>(Modules.size()))
	{
		return nullptr;
	}

	return Modules[ModuleIndex];
}

FParticleSystemEditorWidget::FEditorLayoutSizes FParticleSystemEditorWidget::CalculateLayoutSizes(const ImVec2& Available) const
{
	const ImGuiStyle& Style = ImGui::GetStyle();
	const float Gap = Style.ItemSpacing.x;
	const float AvailableWidth = (std::max)(Available.x, 1.0f);
	const float AvailableHeight = (std::max)(Available.y, 1.0f);

	FEditorLayoutSizes Layout;
	if (AvailableWidth >= MinColumnWidth * 2.0f + Gap)
	{
		Layout.LeftWidth = AvailableWidth * 0.52f;
		const float MaxLeftWidth = AvailableWidth - MinColumnWidth - Gap;
		Layout.LeftWidth = (std::max)(MinColumnWidth, (std::min)(Layout.LeftWidth, MaxLeftWidth));
	}
	else
	{
		Layout.LeftWidth = (std::max)(1.0f, (AvailableWidth - Gap) * 0.5f);
	}

	Layout.RightWidth = (std::max)(1.0f, AvailableWidth - Layout.LeftWidth - Gap);

	const float VerticalGap = Style.ItemSpacing.y * 2.0f;
	const float UsableHeight = (std::max)(1.0f, AvailableHeight - VerticalGap);
	if (UsableHeight >= MinViewportHeight + MinDetailsHeight)
	{
		Layout.TopHeight = UsableHeight * 0.58f;
		const float MaxTopHeight = UsableHeight - MinDetailsHeight;
		Layout.TopHeight = (std::max)(MinViewportHeight, (std::min)(Layout.TopHeight, MaxTopHeight));
	}
	else
	{
		Layout.TopHeight = (std::max)(1.0f, UsableHeight * 0.58f);
	}
	Layout.BottomHeight = (std::max)(1.0f, UsableHeight - Layout.TopHeight);

	return Layout;
}

void FParticleSystemEditorWidget::RenderToolbar()
{
	ImGui::BeginChild("##ParticleEditorToolbar", ImVec2(0.0f, ToolbarHeight), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
	ImGui::AlignTextToFramePadding();
	ImGui::TextDisabled("Particle System");
	ImGui::SameLine(0.0f, 14.0f);

	if (ImGui::Button(ViewState.bPreviewPlaying ? "Pause" : "Play", ImVec2(68.0f, 0.0f)))
	{
		ViewState.bPreviewPlaying = !ViewState.bPreviewPlaying;
	}
	ImGui::SameLine();
	if (ImGui::Button("Restart Sim", ImVec2(92.0f, 0.0f)))
	{
		ViewState.bRestartPreviewRequested = true;
		ViewState.PreviewTime = 0.0f;
	}
	ImGui::SameLine();
	ImGui::Text("Preview %.2fs", ViewState.PreviewTime);
	ImGui::SameLine(0.0f, 18.0f);
	ImGui::Checkbox("Curve", &ViewState.bShowCurveEditor);
	ImGui::SameLine(0.0f, 18.0f);
	ImGui::BeginDisabled();
	ImGui::Button("Save", ImVec2(62.0f, 0.0f));
	ImGui::SameLine();
	ImGui::Button("Add Emitter", ImVec2(96.0f, 0.0f));
	ImGui::SameLine();
	ImGui::Button("Add LOD", ImVec2(76.0f, 0.0f));
	ImGui::SameLine();
	ImGui::Button("Bounds", ImVec2(72.0f, 0.0f));
	ImGui::EndDisabled();
	ImGui::EndChild();
}

void FParticleSystemEditorWidget::RenderViewportPanel(const ImVec2& Size) const
{
	ImGui::BeginChild("##ParticleViewportPanel", Size, ImGuiChildFlags_Borders);
	DrawPanelHeader("Viewport");

	const ImVec2 CanvasSize = ImGui::GetContentRegionAvail();
	const ImVec2 CanvasMin = ImGui::GetCursorScreenPos();
	const ImVec2 CanvasMax(CanvasMin.x + CanvasSize.x, CanvasMin.y + CanvasSize.y);
	ImGui::InvisibleButton("##ParticlePreviewViewport", CanvasSize);

	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	DrawList->AddRectFilled(CanvasMin, CanvasMax, IM_COL32(75, 77, 77, 255));

	const float GridStep = 48.0f;
	for (float X = CanvasMin.x; X < CanvasMax.x; X += GridStep)
	{
		DrawList->AddLine(ImVec2(X, CanvasMin.y), ImVec2(X, CanvasMax.y), IM_COL32(92, 94, 94, 110));
	}
	for (float Y = CanvasMin.y; Y < CanvasMax.y; Y += GridStep)
	{
		DrawList->AddLine(ImVec2(CanvasMin.x, Y), ImVec2(CanvasMax.x, Y), IM_COL32(92, 94, 94, 110));
	}

	const ImVec2 Center((CanvasMin.x + CanvasMax.x) * 0.5f, (CanvasMin.y + CanvasMax.y) * 0.5f);
	DrawList->AddCircle(Center, 34.0f, IM_COL32(0, 112, 255, 255), 36, 2.0f);
	DrawList->AddLine(ImVec2(Center.x - 54.0f, Center.y), ImVec2(Center.x + 54.0f, Center.y), IM_COL32(0, 112, 255, 255), 2.0f);
	DrawList->AddLine(ImVec2(Center.x, Center.y - 54.0f), ImVec2(Center.x, Center.y + 54.0f), IM_COL32(0, 112, 255, 255), 2.0f);

	DrawList->AddText(ImVec2(CanvasMin.x + 12.0f, CanvasMin.y + 10.0f), IM_COL32(230, 234, 240, 255), "View");
	DrawList->AddText(ImVec2(CanvasMin.x + 58.0f, CanvasMin.y + 10.0f), IM_COL32(230, 234, 240, 255), "Time");
	DrawList->AddText(ImVec2(CanvasMin.x + 12.0f, CanvasMax.y - 28.0f), IM_COL32(255, 80, 50, 255), "X");
	DrawList->AddText(ImVec2(CanvasMin.x + 42.0f, CanvasMax.y - 28.0f), IM_COL32(90, 220, 90, 255), "Y");
	DrawList->AddText(ImVec2(CanvasMin.x + 28.0f, CanvasMax.y - 54.0f), IM_COL32(80, 130, 255, 255), "Z");
	ImGui::EndChild();
}

void FParticleSystemEditorWidget::RenderDetailsPanel(const ImVec2& Size) const
{
	ImGui::BeginChild("##ParticleDetailsPanel", Size, ImGuiChildFlags_Borders);
	DrawPanelHeader("Details");

	const UParticleSystem* ParticleSystem = GetParticleSystem();
	if (!ParticleSystem)
	{
		ImGui::TextDisabled("No particle system selected.");
		ImGui::EndChild();
		return;
	}

	if (ImGui::CollapsingHeader("Particle System", ImGuiTreeNodeFlags_DefaultOpen))
	{
		DrawDetailRow("Asset", ParticleSystem->GetAssetPathFileName().c_str());
		DrawDetailRowF("Emitters", "%d", static_cast<int32>(ParticleSystem->GetEmitters().size()));
	}
	if (ImGui::CollapsingHeader("Selection", ImGuiTreeNodeFlags_DefaultOpen))
	{
		const FEditorSelectionState& Selection = ViewState.Selection;
		DrawDetailRow("Selection Type", GetSelectionKindLabel());
		DrawDetailRowF("Selected Emitter", "%d", Selection.EmitterIndex);
		DrawDetailRowF("Selected LOD", "%d", Selection.LODIndex);
		DrawDetailRowF("Selected Module", "%d", Selection.ModuleIndex);
	}

	const UParticleEmitter* SelectedEmitter = GetSelectedEmitter(ParticleSystem);
	if (SelectedEmitter && ImGui::CollapsingHeader("Emitter", ImGuiTreeNodeFlags_DefaultOpen))
	{
		DrawDetailRowF("Max Active Particles", "%d", SelectedEmitter->GetMaxActiveParticles());
		DrawDetailRowF("Emitter Duration", "%.3f", SelectedEmitter->GetEmitterDuration());
		DrawDetailRow("Looping", SelectedEmitter->IsLooping() ? "true" : "false");
		DrawDetailRowF("LOD Count", "%d", static_cast<int32>(SelectedEmitter->GetLODLevels().size()));
	}

	const UParticleLODLevel* SelectedLOD = GetSelectedLODLevel(ParticleSystem);
	if (SelectedLOD && ImGui::CollapsingHeader("LOD", ImGuiTreeNodeFlags_DefaultOpen))
	{
		const UParticleModuleTypeDataBase* TypeDataModule = SelectedLOD->GetTypeDataModule();
		DrawDetailRowF("Level", "%d", SelectedLOD->GetLevel());
		DrawDetailRow("Enabled", SelectedLOD->IsEnabled() ? "true" : "false");
		DrawDetailRow("Render Type", GetRenderTypeLabel(GetLODRenderType(SelectedLOD)));
		DrawDetailRow("Type Data", GetTypeDataDisplayName(TypeDataModule));
		DrawDetailRowF("Payload Size", "%d", TypeDataModule ? TypeDataModule->GetParticlePayloadSize() : 0);
		DrawDetailRowF("Module Count", "%d", static_cast<int32>(SelectedLOD->GetModules().size()));
	}

	const UParticleModule* SelectedModule = GetSelectedModule(ParticleSystem);
	if (SelectedModule && ImGui::CollapsingHeader("Module", ImGuiTreeNodeFlags_DefaultOpen))
	{
		const UParticleModuleTypeDataBase* TypeDataModule = Cast<UParticleModuleTypeDataBase>(SelectedModule);
		DrawDetailRow("Name", TypeDataModule ? GetTypeDataDisplayName(TypeDataModule) : GetModuleDisplayName(SelectedModule));
		DrawDetailRow("Class", SelectedModule->GetClass()->GetName());
		DrawDetailRow("Spawn Module", SelectedModule->IsSpawnModule() ? "true" : "false");
		DrawDetailRow("Update Module", SelectedModule->IsUpdateModule() ? "true" : "false");
		DrawDetailRow("Source", ViewState.Selection.ModuleIndex == TypeDataModuleIndex ? "LOD TypeDataModule" : "LOD Modules[]");
		ImGui::Separator();
		RenderObjectProperties(SelectedModule);
	}
	ImGui::EndChild();
}

void FParticleSystemEditorWidget::RenderObjectProperties(const UObject* Object) const
{
	if (!Object)
	{
		ImGui::TextDisabled("No object properties.");
		return;
	}

	TArray<FPropertyValue> Properties;
	const_cast<UObject*>(Object)->GetEditableProperties(Properties);
	if (Properties.empty())
	{
		ImGui::TextDisabled("No editable properties.");
		return;
	}

	for (int32 Index = 0; Index < static_cast<int32>(Properties.size()); ++Index)
	{
		ImGui::PushID(Index);
		RenderPropertyValueReadOnly(Properties[Index]);
		ImGui::PopID();
	}
}

void FParticleSystemEditorWidget::RenderPropertyValueReadOnly(const FPropertyValue& PropertyValue) const
{
	void* ValuePtr = PropertyValue.GetValuePtr();
	if (!ValuePtr)
	{
		return;
	}

	switch (PropertyValue.GetType())
	{
	case EPropertyType::Bool:
		DrawDetailRow(PropertyValue.GetDisplayName(), *static_cast<bool*>(ValuePtr) ? "true" : "false");
		break;
	case EPropertyType::ByteBool:
		DrawDetailRow(PropertyValue.GetDisplayName(), *static_cast<uint8_t*>(ValuePtr) ? "true" : "false");
		break;
	case EPropertyType::Int:
		DrawDetailRowF(PropertyValue.GetDisplayName(), "%d", *static_cast<int32*>(ValuePtr));
		break;
	case EPropertyType::Float:
		DrawDetailRowF(PropertyValue.GetDisplayName(), "%.3f", *static_cast<float*>(ValuePtr));
		break;
	case EPropertyType::Vec3:
	{
		const FVector& Value = *static_cast<FVector*>(ValuePtr);
		DrawDetailRowF(PropertyValue.GetDisplayName(), "%.3f, %.3f, %.3f", Value.X, Value.Y, Value.Z);
		break;
	}
	case EPropertyType::Vec4:
	case EPropertyType::Color4:
	{
		const FVector4& Value = *static_cast<FVector4*>(ValuePtr);
		DrawDetailRowF(PropertyValue.GetDisplayName(), "%.3f, %.3f, %.3f, %.3f", Value.X, Value.Y, Value.Z, Value.W);
		break;
	}
	case EPropertyType::String:
		DrawDetailRow(PropertyValue.GetDisplayName(), static_cast<FString*>(ValuePtr)->c_str());
		break;
	case EPropertyType::Name:
	{
		const FString Value = static_cast<FName*>(ValuePtr)->ToString();
		DrawDetailRow(PropertyValue.GetDisplayName(), Value.c_str());
		break;
	}
	case EPropertyType::Enum:
	{
		const FEnum* EnumType = PropertyValue.GetEnumType();
		int32 Value = 0;
		if (EnumType)
		{
			std::memcpy(&Value, ValuePtr, EnumType->GetSize());
		}
		const char* Label = (EnumType && Value >= 0 && static_cast<uint32>(Value) < EnumType->GetCount())
			? EnumType->GetNames()[Value]
			: "(unknown)";
		DrawDetailRow(PropertyValue.GetDisplayName(), Label);
		break;
	}
	case EPropertyType::SoftObjectRef:
	{
		const FSoftObjectProperty* SoftObjectProperty = PropertyValue.Property ? PropertyValue.Property->AsSoftObjectProperty() : nullptr;
		const FString Path = SoftObjectProperty ? SoftObjectProperty->GetPathFromValuePtr(ValuePtr) : FString("None");
		DrawDetailRow(PropertyValue.GetDisplayName(), Path.c_str());
		break;
	}
	case EPropertyType::ObjectRef:
	{
		UObject* ObjectValue = *static_cast<UObject**>(ValuePtr);
		DrawDetailRow(PropertyValue.GetDisplayName(), ObjectValue ? ObjectValue->GetName().c_str() : "None");
		break;
	}
	case EPropertyType::Array:
		DrawDetailRow(PropertyValue.GetDisplayName(), "(array)");
		break;
	case EPropertyType::Struct:
		DrawDetailRow(PropertyValue.GetDisplayName(), "(struct)");
		break;
	case EPropertyType::ClassRef:
		DrawDetailRow(PropertyValue.GetDisplayName(), "(class)");
		break;
	case EPropertyType::Rotator:
		DrawDetailRow(PropertyValue.GetDisplayName(), "(rotator)");
		break;
	default:
		DrawDetailRow(PropertyValue.GetDisplayName(), "(unsupported)");
		break;
	}
}

void FParticleSystemEditorWidget::RenderEmittersPanel(const ImVec2& Size)
{
	ImGui::BeginChild("##ParticleEmittersPanel", Size, ImGuiChildFlags_Borders, ImGuiWindowFlags_HorizontalScrollbar);
	DrawPanelHeader("Emitters");

	UParticleSystem* ParticleSystem = GetParticleSystem();
	if (!ParticleSystem)
	{
		ImGui::TextDisabled("No particle system selected.");
		ImGui::EndChild();
		return;
	}

	const TArray<UParticleEmitter*>& Emitters = ParticleSystem->GetEmitters();
	if (Emitters.empty())
	{
		ImGui::TextDisabled("No emitters.");
		ImGui::EndChild();
		return;
	}

	for (int32 EmitterIndex = 0; EmitterIndex < static_cast<int32>(Emitters.size()); ++EmitterIndex)
	{
		UParticleEmitter* Emitter = Emitters[EmitterIndex];
		const int32 DisplayLODIndex = GetDisplayLODIndex(Emitter);
		const UParticleLODLevel* LODLevel = Emitter ? Emitter->GetLODLevel(DisplayLODIndex) : nullptr;
		const EParticleRenderType RenderType = GetLODRenderType(LODLevel);
		ImGui::PushID(EmitterIndex);
		ImGui::BeginChild("##EmitterColumn", ImVec2(EmitterColumnWidth, 0.0f), true);

		const ImVec2 HeaderMin = ImGui::GetCursorScreenPos();
		const ImVec2 HeaderMax(HeaderMin.x + ImGui::GetContentRegionAvail().x, HeaderMin.y + EmitterHeaderHeight);
		ImDrawList* DrawList = ImGui::GetWindowDrawList();
		DrawList->AddRectFilled(HeaderMin, HeaderMax, IsEmitterSelected(EmitterIndex) ? IM_COL32(74, 76, 83, 255) : IM_COL32(55, 56, 61, 255));
		char Header[64];
		std::snprintf(Header, sizeof(Header), "Emitter %d", EmitterIndex);
		DrawList->AddText(ImVec2(HeaderMin.x + 8.0f, HeaderMin.y + 7.0f), IM_COL32(240, 242, 245, 255), Header);
		DrawList->AddText(ImVec2(HeaderMin.x + 8.0f, HeaderMin.y + 30.0f), IM_COL32(160, 210, 255, 255), GetRenderTypeLabel(RenderType));
		if (Emitter)
		{
			char CountLabel[32];
			std::snprintf(CountLabel, sizeof(CountLabel), "%d", Emitter->GetMaxActiveParticles());
			DrawList->AddText(ImVec2(HeaderMax.x - 36.0f, HeaderMin.y + 30.0f), IM_COL32(230, 234, 238, 255), CountLabel);
		}

		if (ImGui::InvisibleButton("##EmitterHeader", ImVec2(ImGui::GetContentRegionAvail().x, EmitterHeaderHeight)))
		{
			SelectEmitter(EmitterIndex);
		}

		if (Emitter)
		{
			const TArray<UParticleLODLevel*>& LODLevels = Emitter->GetLODLevels();
			if (!LODLevels.empty())
			{
				for (int32 LODIndex = 0; LODIndex < static_cast<int32>(LODLevels.size()); ++LODIndex)
				{
					const UParticleLODLevel* LOD = LODLevels[LODIndex];
					const bool bSelectedLOD = IsLODSelected(EmitterIndex, LODIndex);
					const bool bEnabledLOD = LOD && LOD->IsEnabled();
					char LODLabel[24];
					std::snprintf(LODLabel, sizeof(LODLabel), "LOD %d", LOD ? LOD->GetLevel() : LODIndex);
					ImGui::PushID(LODIndex);
					ImGui::PushStyleColor(ImGuiCol_Button, bSelectedLOD ? ImVec4(0.38f, 0.42f, 0.52f, 1.0f) : ImVec4(0.20f, 0.21f, 0.24f, 1.0f));
					ImGui::PushStyleColor(ImGuiCol_Text, bEnabledLOD ? ImVec4(0.86f, 0.88f, 0.92f, 1.0f) : ImVec4(0.48f, 0.50f, 0.54f, 1.0f));
					if (ImGui::SmallButton(LODLabel))
					{
						SelectLOD(EmitterIndex, LODIndex);
					}
					ImGui::PopStyleColor(2);
					ImGui::PopID();
					if (LODIndex + 1 < static_cast<int32>(LODLevels.size()))
					{
						ImGui::SameLine();
					}
				}
				ImGui::Dummy(ImVec2(0.0f, 3.0f));
			}
			else
			{
				ImGui::TextDisabled("No LOD levels.");
			}

			if (LODLevel)
			{
				const UParticleModuleTypeDataBase* TypeDataModule = LODLevel->GetTypeDataModule();
				if (TypeDataModule)
				{
					ImGui::PushID(TypeDataModuleIndex);
					if (SelectableModuleRow(GetTypeDataDisplayName(TypeDataModule), IsModuleSelected(EmitterIndex, DisplayLODIndex, TypeDataModuleIndex), GetTypeDataAccentColor(RenderType)))
					{
						SelectModule(EmitterIndex, DisplayLODIndex, TypeDataModuleIndex);
					}
					ImGui::PopID();
				}

				const TArray<UParticleModule*>& Modules = LODLevel->GetModules();
				for (int32 ModuleIndex = 0; ModuleIndex < static_cast<int32>(Modules.size()); ++ModuleIndex)
				{
					const UParticleModule* Module = Modules[ModuleIndex];
					ImGui::PushID(ModuleIndex);
					if (SelectableModuleRow(GetModuleDisplayName(Module), IsModuleSelected(EmitterIndex, DisplayLODIndex, ModuleIndex), GetModuleAccentColor(Module)))
					{
						SelectModule(EmitterIndex, DisplayLODIndex, ModuleIndex);
					}
					ImGui::PopID();
				}
			}
		}

		ImGui::EndChild();
		ImGui::PopID();
		ImGui::SameLine();
	}

	ImGui::NewLine();
	ImGui::EndChild();
}

void FParticleSystemEditorWidget::RenderCurveEditorPanel(const ImVec2& Size) const
{
	ImGui::BeginChild("##ParticleCurveEditorPanel", Size, ImGuiChildFlags_Borders);
	DrawPanelHeader("Curve Editor");

	const ImVec2 CanvasSize = ImGui::GetContentRegionAvail();
	const ImVec2 CanvasMin = ImGui::GetCursorScreenPos();
	const ImVec2 CanvasMax(CanvasMin.x + CanvasSize.x, CanvasMin.y + CanvasSize.y);
	ImGui::InvisibleButton("##ParticleCurveEditorCanvas", CanvasSize);

	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	DrawList->AddRectFilled(CanvasMin, CanvasMax, IM_COL32(52, 52, 52, 255));
	for (int32 i = 0; i <= 8; ++i)
	{
		const float X = CanvasMin.x + CanvasSize.x * (static_cast<float>(i) / 8.0f);
		DrawList->AddLine(ImVec2(X, CanvasMin.y), ImVec2(X, CanvasMax.y), IM_COL32(140, 140, 140, 170));
	}
	for (int32 i = 0; i <= 4; ++i)
	{
		const float Y = CanvasMin.y + CanvasSize.y * (static_cast<float>(i) / 4.0f);
		DrawList->AddLine(ImVec2(CanvasMin.x, Y), ImVec2(CanvasMax.x, Y), IM_COL32(140, 140, 140, 170));
	}
	DrawList->AddText(ImVec2(CanvasMin.x + 10.0f, CanvasMin.y + 8.0f), IM_COL32(225, 230, 235, 255), "Curve data will be connected after module properties are defined.");
	ImGui::EndChild();
}
