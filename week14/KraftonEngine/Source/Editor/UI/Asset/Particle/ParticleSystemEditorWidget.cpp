#include "ParticleSystemEditorWidget.h"

#include "Component/Light/DirectionalLightComponent.h"
#include "Component/Particle/ParticleSystemComponent.h"
#include "Core/Property/SoftObjectProperty.h"
#include "GameFramework/AActor.h"
#include "GameFramework/Light/DirectionalLightActor.h"
#include "Object/Object.h"
#include "Particles/Module/ParticleModule.h"
#include "Particles/Module/ParticleModuleTypeDataBase.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleSystemManager.h"
#include "Render/Scene/FScene.h"
#include "Render/Types/MinimalViewInfo.h"
#include "Runtime/Engine.h"
#include "Slate/SlateApplication.h"
#include "Viewport/Viewport.h"

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <imgui.h>
#include <utility>

namespace
{
	constexpr float MinColumnWidth = 360.0f;
	constexpr float MinViewportHeight = 220.0f;
	constexpr float MinDetailsHeight = 160.0f;
	constexpr float MinEmittersHeight = 180.0f;
	constexpr float MinCurveEditorHeight = 140.0f;
	constexpr float SplitterThickness = 6.0f;
	constexpr float ToolbarHeight = 34.0f;
	constexpr float EmitterColumnWidth = 176.0f;
	constexpr float EmitterHeaderHeight = 58.0f;
	constexpr float ModuleRowHeight = 24.0f;
	constexpr int32 NoModuleIndex = -1;
	constexpr int32 TypeDataModuleIndex = -2;

	struct FModuleRowAction
	{
		bool bSelect = false;
		bool bMoveUp = false;
		bool bMoveDown = false;
		bool bDelete = false;
	};

	float ClampFloat(float Value, float MinValue, float MaxValue)
	{
		return (std::max)(MinValue, (std::min)(Value, MaxValue));
	}

	float CalculateSplitLeadingSize(float TotalSize, float Ratio, float MinLeadingSize, float MinTrailingSize)
	{
		TotalSize = (std::max)(TotalSize, 1.0f);
		if (TotalSize >= MinLeadingSize + MinTrailingSize)
		{
			return ClampFloat(TotalSize * Ratio, MinLeadingSize, TotalSize - MinTrailingSize);
		}

		if (TotalSize > 2.0f)
		{
			return ClampFloat(TotalSize * Ratio, 1.0f, TotalSize - 1.0f);
		}

		return (std::max)(1.0f, TotalSize * 0.5f);
	}

	float CalculateSplitRatio(float LeadingSize, float TotalSize, float MinLeadingSize, float MinTrailingSize)
	{
		TotalSize = (std::max)(TotalSize, 1.0f);
		const float ClampedLeadingSize = CalculateSplitLeadingSize(TotalSize, LeadingSize / TotalSize, MinLeadingSize, MinTrailingSize);
		return ClampedLeadingSize / TotalSize;
	}

	bool DrawSplitterHandle(const char* Id, const ImVec2& Size, bool bVertical)
	{
		ImGui::InvisibleButton(Id, Size);
		const bool bHovered = ImGui::IsItemHovered();
		const bool bActive = ImGui::IsItemActive();
		if (bHovered || bActive)
		{
			ImGui::SetMouseCursor(bVertical ? ImGuiMouseCursor_ResizeEW : ImGuiMouseCursor_ResizeNS);
		}

		const ImVec2 Min = ImGui::GetItemRectMin();
		const ImVec2 Max = ImGui::GetItemRectMax();
		const ImU32 Color = ImGui::GetColorU32(bActive ? ImGuiCol_SeparatorActive : (bHovered ? ImGuiCol_SeparatorHovered : ImGuiCol_Separator));
		ImDrawList* DrawList = ImGui::GetWindowDrawList();
		if (bVertical)
		{
			const float X = (Min.x + Max.x) * 0.5f;
			DrawList->AddLine(ImVec2(X, Min.y), ImVec2(X, Max.y), Color, 2.0f);
		}
		else
		{
			const float Y = (Min.y + Max.y) * 0.5f;
			DrawList->AddLine(ImVec2(Min.x, Y), ImVec2(Max.x, Y), Color, 2.0f);
		}

		return bActive;
	}

	ImVec2 ProjectWorldAxisToViewport(const FVector& Axis, const FMinimalViewInfo& POV)
	{
		const FVector CameraRight = POV.Rotation.GetRightVector();
		const FVector CameraUp = POV.Rotation.GetUpVector();
		return ImVec2(Axis.Dot(CameraRight), -Axis.Dot(CameraUp));
	}

	void DrawViewportAxisGizmo(ImDrawList* DrawList, const ImVec2& ViewportPos, const ImVec2& ViewportSize, FParticleSystemEditorViewportClient& ViewportClient)
	{
		FMinimalViewInfo POV;
		if (!ViewportClient.GetCameraView(POV))
		{
			return;
		}

		const float AxisLength = (std::max)(20.0f, (std::min)(30.0f, (std::min)(ViewportSize.x, ViewportSize.y) * 0.01f));
		const float Padding = AxisLength + 25.0f;
		const ImVec2 Origin(ViewportPos.x + Padding, ViewportPos.y + ViewportSize.y - Padding);
		const FVector CameraForward = POV.Rotation.GetForwardVector();

		struct FAxisDrawInfo
		{
			const char* Label = "";
			FVector Direction;
			ImU32 Color = 0;
			ImVec2 End;
			float Depth = 0.0f;
		};

		FAxisDrawInfo Axes[3] =
		{
			{ "X", FVector::XAxisVector, IM_COL32(255, 70, 55, 255), ImVec2(), 0.0f },
			{ "Y", FVector::YAxisVector, IM_COL32(105, 220, 70, 255), ImVec2(), 0.0f },
			{ "Z", FVector::ZAxisVector, IM_COL32(70, 135, 255, 255), ImVec2(), 0.0f },
		};

		for (FAxisDrawInfo& Axis : Axes)
		{
			const ImVec2 Projected = ProjectWorldAxisToViewport(Axis.Direction, POV);
			Axis.End = ImVec2(Origin.x + Projected.x * AxisLength, Origin.y + Projected.y * AxisLength);
			Axis.Depth = Axis.Direction.Dot(CameraForward);
		}

		std::sort(Axes, Axes + 3, [](const FAxisDrawInfo& A, const FAxisDrawInfo& B)
		{
			return A.Depth > B.Depth;
		});

		DrawList->AddCircleFilled(Origin, 3.0f, IM_COL32(22, 24, 26, 255));
		for (const FAxisDrawInfo& Axis : Axes)
		{
			DrawList->AddLine(Origin, Axis.End, Axis.Color, 2.0f);
			DrawList->AddCircleFilled(Axis.End, 2.5f, Axis.Color);

			const ImVec2 LabelDir(Axis.End.x - Origin.x, Axis.End.y - Origin.y);
			const float LabelLen = std::sqrt(LabelDir.x * LabelDir.x + LabelDir.y * LabelDir.y);
			const ImVec2 LabelOffset = LabelLen > 1.0f
				? ImVec2((LabelDir.x / LabelLen) * 15.0f, (LabelDir.y / LabelLen) * 15.0f)
				: ImVec2(4.0f, -10.0f);
			DrawList->AddText(ImVec2(Axis.End.x + LabelOffset.x - 4.0f, Axis.End.y + LabelOffset.y - 6.0f), Axis.Color, Axis.Label);
		}
	}

	void DrawPanelHeader(const char* Label, float MinWidth = 0.0f)
	{
		const ImVec2 Pos = ImGui::GetCursorScreenPos();
		const float Width = (std::max)(ImGui::GetContentRegionAvail().x, MinWidth);
		const float Height = 24.0f;
		ImDrawList* DrawList = ImGui::GetWindowDrawList();
		DrawList->AddRectFilled(Pos, ImVec2(Pos.x + Width, Pos.y + Height), IM_COL32(34, 34, 36, 255));
		DrawList->AddText(ImVec2(Pos.x + 8.0f, Pos.y + 4.0f), IM_COL32(220, 224, 232, 255), Label);
		ImGui::Dummy(ImVec2(Width, Height + 4.0f));
	}

	bool IsModuleOrderLocked(const UParticleModule* Module)
	{
		return Cast<UParticleModuleRequired>(Module) || Cast<UParticleModuleSpawn>(Module);
	}

	ImU32 GetModuleRowBackgroundColor(const UParticleModule* Module, bool bSelected)
	{
		if (Cast<UParticleModuleRequired>(Module))
		{
			return bSelected ? IM_COL32(205, 205, 112, 255) : IM_COL32(188, 190, 88, 255);
		}
		if (Cast<UParticleModuleSpawn>(Module))
		{
			return bSelected ? IM_COL32(210, 112, 112, 255) : IM_COL32(188, 92, 92, 255);
		}
		return bSelected ? IM_COL32(78, 82, 92, 255) : IM_COL32(29, 30, 35, 255);
	}

	ImU32 GetModuleRowTextColor(const UParticleModule* Module)
	{
		if (Cast<UParticleModuleRequired>(Module))
		{
			return IM_COL32(24, 25, 26, 255);
		}
		return IM_COL32(235, 238, 242, 255);
	}

	void DrawModuleRow(const char* Label, bool bSelected, const UParticleModule* Module)
	{
		const ImVec2 Pos = ImGui::GetCursorScreenPos();
		const float Width = ImGui::GetContentRegionAvail().x;
		ImDrawList* DrawList = ImGui::GetWindowDrawList();
		const ImU32 BackgroundColor = GetModuleRowBackgroundColor(Module, bSelected);
		const ImU32 TextColor = GetModuleRowTextColor(Module);
		DrawList->AddRectFilled(Pos, ImVec2(Pos.x + Width, Pos.y + ModuleRowHeight), BackgroundColor);
		DrawList->AddText(ImVec2(Pos.x + 8.0f, Pos.y + 4.0f), TextColor, Label);
	}

	bool SelectableModuleRow(const char* Label, bool bSelected, const UParticleModule* Module = nullptr)
	{
		DrawModuleRow(Label, bSelected, Module);
		return ImGui::InvisibleButton("##ModuleRow", ImVec2(ImGui::GetContentRegionAvail().x, ModuleRowHeight));
	}

	bool IsModuleDeleteLocked(const UParticleModule* Module)
	{
		return Cast<UParticleModuleRequired>(Module) || Cast<UParticleModuleSpawn>(Module);
	}

	FModuleRowAction EditableModuleRow(const char* Label, const UParticleModule* Module, bool bSelected, bool bCanMoveUp, bool bCanMoveDown, bool bCanDelete)
	{
		FModuleRowAction Action;

		const ImVec2 Pos = ImGui::GetCursorScreenPos();
		const float Width = ImGui::GetContentRegionAvail().x;
		const float ButtonWidth = 25.0f;
		const float ButtonGap = 2.0f;
		const float ControlsWidth = ButtonWidth * 3.0f + ButtonGap * 2.0f + 4.0f;
		const float SelectWidth = (std::max)(1.0f, Width - ControlsWidth);

		DrawModuleRow(Label, bSelected, Module);
		ImGui::SetCursorScreenPos(Pos);
		Action.bSelect = ImGui::InvisibleButton("##SelectModuleRow", ImVec2(SelectWidth, ModuleRowHeight));

		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2.0f, 1.0f));
		ImGui::SetCursorScreenPos(ImVec2(Pos.x + Width - ControlsWidth, Pos.y + 2.0f));
		ImGui::BeginDisabled(!bCanMoveUp);
		Action.bMoveUp = ImGui::SmallButton("Up");
		ImGui::EndDisabled();

		ImGui::SameLine(0.0f, ButtonGap);
		ImGui::BeginDisabled(!bCanMoveDown);
		Action.bMoveDown = ImGui::SmallButton("Dn");
		ImGui::EndDisabled();

		ImGui::SameLine(0.0f, ButtonGap);
		ImGui::BeginDisabled(!bCanDelete);
		Action.bDelete = ImGui::SmallButton("Del");
		if (!bCanDelete && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
		{
			ImGui::SetTooltip("Required and Spawn modules cannot be deleted.");
		}
		ImGui::EndDisabled();
		ImGui::PopStyleVar();

		ImGui::SetCursorScreenPos(ImVec2(Pos.x, Pos.y + ModuleRowHeight));
		return Action;
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

	bool StringContains(const FString& Value, const char* Needle)
	{
		return Needle && Value.find(Needle) != FString::npos;
	}

	bool IsColorLikeProperty(const FPropertyValue& PropertyValue)
	{
		const FString Name = PropertyValue.GetName() ? PropertyValue.GetName() : "";
		const FString DisplayName = PropertyValue.GetDisplayName() ? PropertyValue.GetDisplayName() : "";
		const FString Category = PropertyValue.GetCategory() ? PropertyValue.GetCategory() : "";
		return StringContains(Name, "Color") || StringContains(DisplayName, "Color") || StringContains(Category, "Color");
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
			return "";
		}

		switch (TypeDataModule->GetRenderType())
		{
		case EParticleRenderType::Sprite: return "";
		case EParticleRenderType::Mesh: return "Mesh Data";
		case EParticleRenderType::Ribbon: return "Ribbon Data";
		case EParticleRenderType::Beam: return "Beam Data";
		case EParticleRenderType::GPU: return "GPU Sprites";
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

static uint32 GNextParticleSystemEditorInstanceId = 0;

struct FParticleSystemEditorWidget::FEditorLayoutSizes
{
	float LeftWidth = 0.0f;
	float RightWidth = 0.0f;
	float ViewportHeight = 0.0f;
	float DetailsHeight = 0.0f;
	float EmittersHeight = 0.0f;
	float CurveEditorHeight = 0.0f;
};

FParticleSystemEditorWidget::FParticleSystemEditorWidget()
	: InstanceId(GNextParticleSystemEditorInstanceId++)
{
	const FString Id = std::to_string(InstanceId);
	PreviewWorldHandle = FName("ParticleSystemEditorPreview_" + Id);
	WindowIdSuffix = "###ParticleSystemEditor_" + Id;
}

FParticleSystemEditorWidget::~FParticleSystemEditorWidget()
{
	DestroyPreviewWorld();
}

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

	DestroyPreviewWorld();
	FAssetEditorWidget::Open(Object);
	ResetEditorState();
	CreatePreviewWorld();
}

void FParticleSystemEditorWidget::Close()
{
	ResetEditorState();
	DestroyPreviewWorld();
	FAssetEditorWidget::Close();
}

void FParticleSystemEditorWidget::Tick(float DeltaTime)
{
	if (!IsOpen())
	{
		return;
	}

	if (ViewportClient.IsRenderable())
	{
		ViewportClient.Tick(DeltaTime);
	}

	if (ViewState.bRestartPreviewRequested)
	{
		RestartPreviewSimulation();
		ViewState.bRestartPreviewRequested = false;
	}

	if (!ViewState.bPreviewPlaying)
	{
		return;
	}

	if (UWorld* PreviewWorld = ViewportClient.GetPreviewWorld())
	{
		PreviewWorld->Tick(DeltaTime, ELevelTick::LEVELTICK_All);
	}

	ViewState.PreviewTime += DeltaTime;
}

void FParticleSystemEditorWidget::CollectPreviewViewports(TArray<IEditorPreviewViewportClient*>& OutClients) const
{
	if (IsOpen())
	{
		OutClients.push_back(const_cast<FParticleSystemEditorViewportClient*>(&ViewportClient));
	}
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
	FString WindowTitle = VisibleTitle + WindowIdSuffix;
	ImGuiWindowFlags WindowFlags = ImGuiWindowFlags_None;
	if (ViewportClient.IsMouseOverViewport())
	{
		WindowFlags |= ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse;
	}

	ImGui::SetNextWindowSize(ImVec2(1280.0f, 720.0f), ImGuiCond_Once);
	if (ConsumeFocusRequest())
	{
		ImGui::SetNextWindowFocus();
	}

	if (!ImGui::Begin(WindowTitle.c_str(), &bWindowOpen, WindowFlags))
	{
		ImGui::End();
		if (!bWindowOpen)
		{
			Close();
		}
		return;
	}

	if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows))
	{
		FSlateApplication::Get().BringViewportToFront(&ViewportClient);
	}

	RenderToolbar();
	ImGui::Separator();

	const ImVec2 Available = ImGui::GetContentRegionAvail();
	const FEditorLayoutSizes Layout = CalculateLayoutSizes(Available);

	ImGui::BeginChild("##ParticleEditorLeftColumn", ImVec2(Layout.LeftWidth, 0.0f), ImGuiChildFlags_None);
	RenderViewportPanel(ImVec2(0.0f, Layout.ViewportHeight));
	const float LeftSplitTotalHeight = Layout.ViewportHeight + Layout.DetailsHeight;
	if (DrawSplitterHandle("##ParticleViewportDetailsSplitter", ImVec2(ImGui::GetContentRegionAvail().x, SplitterThickness), false))
	{
		ViewState.ViewportDetailsSplitRatio = CalculateSplitRatio(
			Layout.ViewportHeight + ImGui::GetIO().MouseDelta.y,
			LeftSplitTotalHeight,
			MinViewportHeight,
			MinDetailsHeight);
	}
	RenderDetailsPanel(ImVec2(0.0f, Layout.DetailsHeight));
	ImGui::EndChild();

	ImGui::SameLine(0.0f, 0.0f);
	const float MainSplitTotalWidth = Layout.LeftWidth + Layout.RightWidth;
	if (DrawSplitterHandle("##ParticleEditorMainSplitter", ImVec2(SplitterThickness, ImGui::GetContentRegionAvail().y), true))
	{
		ViewState.MainSplitRatio = CalculateSplitRatio(
			Layout.LeftWidth + ImGui::GetIO().MouseDelta.x,
			MainSplitTotalWidth,
			MinColumnWidth,
			MinColumnWidth);
	}

	ImGui::SameLine(0.0f, 0.0f);
	ImGui::BeginChild("##ParticleEditorRightColumn", ImVec2(Layout.RightWidth, 0.0f), ImGuiChildFlags_None);
	if (ViewState.bShowCurveEditor)
	{
		RenderEmittersPanel(ImVec2(0.0f, Layout.EmittersHeight));
		const float RightSplitTotalHeight = Layout.EmittersHeight + Layout.CurveEditorHeight;
		if (DrawSplitterHandle("##ParticleEmittersCurveSplitter", ImVec2(ImGui::GetContentRegionAvail().x, SplitterThickness), false))
		{
			ViewState.EmittersCurveSplitRatio = CalculateSplitRatio(
				Layout.EmittersHeight + ImGui::GetIO().MouseDelta.y,
				RightSplitTotalHeight,
				MinEmittersHeight,
				MinCurveEditorHeight);
		}
		RenderCurveEditorPanel(ImVec2(0.0f, Layout.CurveEditorHeight));
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
	const float AvailableWidth = (std::max)(Available.x, 1.0f);
	const float AvailableHeight = (std::max)(Available.y, 1.0f);

	FEditorLayoutSizes Layout;

	const float HorizontalSplitTotal = (std::max)(1.0f, AvailableWidth - SplitterThickness);
	Layout.LeftWidth = CalculateSplitLeadingSize(HorizontalSplitTotal, ViewState.MainSplitRatio, MinColumnWidth, MinColumnWidth);
	Layout.RightWidth = (std::max)(1.0f, HorizontalSplitTotal - Layout.LeftWidth);

	const float VerticalSplitterGap = SplitterThickness + Style.ItemSpacing.y * 2.0f;
	const float LeftSplitTotalHeight = (std::max)(1.0f, AvailableHeight - VerticalSplitterGap);
	Layout.ViewportHeight = CalculateSplitLeadingSize(LeftSplitTotalHeight, ViewState.ViewportDetailsSplitRatio, MinViewportHeight, MinDetailsHeight);
	Layout.DetailsHeight = (std::max)(1.0f, LeftSplitTotalHeight - Layout.ViewportHeight);

	if (ViewState.bShowCurveEditor)
	{
		const float RightSplitTotalHeight = (std::max)(1.0f, AvailableHeight - VerticalSplitterGap);
		Layout.EmittersHeight = CalculateSplitLeadingSize(RightSplitTotalHeight, ViewState.EmittersCurveSplitRatio, MinEmittersHeight, MinCurveEditorHeight);
		Layout.CurveEditorHeight = (std::max)(1.0f, RightSplitTotalHeight - Layout.EmittersHeight);
	}
	else
	{
		Layout.EmittersHeight = AvailableHeight;
		Layout.CurveEditorHeight = 0.0f;
	}

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
	UParticleSystem* ParticleSystem = GetParticleSystem();
	ImGui::BeginDisabled(ParticleSystem == nullptr);
	if (ImGui::Button("Save", ImVec2(62.0f, 0.0f)))
	{
		if (FParticleSystemManager::Get().Save(ParticleSystem))
		{
			ClearDirty();
		}
	}
	ImGui::SameLine();

	if (ImGui::Button("Add Emitter", ImVec2(96.0f, 0.0f)))
	{
		if (ParticleSystem->AddDefaultEmitter())
		{
			const int32 NewEmitterIndex = static_cast<int32>(ParticleSystem->GetEmitters().size()) - 1;
			SelectEmitter(NewEmitterIndex);
			MarkDirty();
			RestartPreviewSimulation();
		}
	}
	ImGui::EndDisabled();
	ImGui::SameLine();

	ImGui::BeginDisabled();
	ImGui::Button("Add LOD", ImVec2(76.0f, 0.0f));
	ImGui::SameLine();
	ImGui::Button("Bounds", ImVec2(72.0f, 0.0f));
	ImGui::EndDisabled();
	ImGui::EndChild();
}

void FParticleSystemEditorWidget::RenderViewportPanel(const ImVec2& Size)
{
	ImGui::BeginChild("##ParticleViewportPanel", Size, ImGuiChildFlags_Borders);
	DrawPanelHeader("Viewport");

	ImVec2 ViewportSize = ImGui::GetContentRegionAvail();
	if (ViewportSize.x <= 0.0f || ViewportSize.y <= 0.0f)
	{
		ImGui::EndChild();
		return;
	}

	ImVec2 ViewportPos = ImGui::GetCursorScreenPos();
	ViewportClient.SetViewportRect(ViewportPos.x, ViewportPos.y, ViewportSize.x, ViewportSize.y);

	FViewport* VP = ViewportClient.GetViewport();
	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	if (VP)
	{
		VP->RequestResize(static_cast<uint32>((std::max)(ViewportSize.x, 1.0f)), static_cast<uint32>((std::max)(ViewportSize.y, 1.0f)));
		ViewportClient.NotifyViewportResized(static_cast<int32>(ViewportSize.x), static_cast<int32>(ViewportSize.y));

		if (VP->GetSRV())
		{
			ImGui::Image((ImTextureID)VP->GetSRV(), ViewportSize);
		}
		else
		{
			ImGui::Dummy(ViewportSize);
		}
		FSlateApplication::Get().SetViewportImGuiHovered(&ViewportClient, ImGui::IsItemHovered());
	}
	else
	{
		ImGui::InvisibleButton("##ParticlePreviewViewport", ViewportSize);
		const ImVec2 CanvasMax(ViewportPos.x + ViewportSize.x, ViewportPos.y + ViewportSize.y);
		DrawList->AddRectFilled(ViewportPos, CanvasMax, IM_COL32(75, 77, 77, 255));
	}

	DrawList->AddRectFilled(ImVec2(ViewportPos.x + 8.0f, ViewportPos.y + 8.0f), ImVec2(ViewportPos.x + 50.0f, ViewportPos.y + 30.0f), IM_COL32(26, 27, 30, 190), 8.0f);
	DrawList->AddText(ImVec2(ViewportPos.x + 18.0f, ViewportPos.y + 11.0f), IM_COL32(230, 234, 240, 255), "View");
	DrawList->AddRectFilled(ImVec2(ViewportPos.x + 58.0f, ViewportPos.y + 8.0f), ImVec2(ViewportPos.x + 100.0f, ViewportPos.y + 30.0f), IM_COL32(26, 27, 30, 190), 8.0f);
	DrawList->AddText(ImVec2(ViewportPos.x + 68.0f, ViewportPos.y + 11.0f), IM_COL32(230, 234, 240, 255), "Time");
	DrawViewportAxisGizmo(DrawList, ViewportPos, ViewportSize, ViewportClient);

	ImGui::EndChild();
}

void FParticleSystemEditorWidget::CreatePreviewWorld()
{
	if (!GEngine || !GetParticleSystem())
	{
		return;
	}

	FWorldContext& WorldContext = GEngine->CreateWorldContext(EWorldType::EditorPreview, PreviewWorldHandle);
	WorldContext.World->SetWorldType(EWorldType::EditorPreview);
	WorldContext.World->InitWorld();

	PreviewActor = WorldContext.World->SpawnActor<AActor>();
	PreviewParticleComponent = PreviewActor->AddComponent<UParticleSystemComponent>();
	PreviewParticleComponent->SetTemplate(GetParticleSystem());
	PreviewParticleComponent->Activate();
	PreviewActor->SetRootComponent(PreviewParticleComponent);
	PreviewActor->SetActorLocation(FVector(0.0f, 0.0f, 0.0f));

	ADirectionalLightActor* LightActor = WorldContext.World->SpawnActor<ADirectionalLightActor>();
	LightActor->InitDefaultComponents();
	LightActor->SetActorRotation(FVector(0.0f, 45.0f, -45.0f));
	if (UDirectionalLightComponent* LightComp = LightActor->GetComponentByClass<UDirectionalLightComponent>())
	{
		LightComp->SetShadowBias(0.0f);
		LightComp->PushToScene();
	}

	ViewportClient.Initialize(GEngine->GetRenderer().GetFD3DDevice().GetDevice(), 512, 512);
	ViewportClient.SetPreviewWorld(WorldContext.World);
	ViewportClient.SetPreviewActor(PreviewActor);
	ViewportClient.SetPreviewParticleComponent(PreviewParticleComponent);
	ViewportClient.ResetCameraToPreviewBounds();

	WorldContext.World->SetEditorPOVProvider(&ViewportClient);
	WorldContext.World->BeginPlay();

	FSlateApplication::Get().RegisterViewport(&ViewportClient);
}

void FParticleSystemEditorWidget::DestroyPreviewWorld()
{
	UWorld* PreviewWorld = ViewportClient.GetPreviewWorld();
	if (PreviewWorld)
	{
		PreviewWorld->SetEditorPOVProvider(nullptr);
	}

	FSlateApplication::Get().UnregisterViewport(&ViewportClient);
	ViewportClient.Release();
	PreviewActor = nullptr;
	PreviewParticleComponent = nullptr;

	if (PreviewWorld)
	{
		FScene& PreviewScene = PreviewWorld->GetScene();
		if (GEngine)
		{
			GEngine->GetRenderer().GetResources().ReleaseShadowResourcesForScene(&PreviewScene);
		}

		if (GEngine && PreviewWorldHandle.IsValid())
		{
			GEngine->DestroyWorldContext(PreviewWorldHandle);
		}
	}
}

void FParticleSystemEditorWidget::RestartPreviewSimulation()
{
	ViewState.PreviewTime = 0.0f;

	if (!PreviewParticleComponent)
	{
		return;
	}

	PreviewParticleComponent->ResetSystem();
	PreviewParticleComponent->Activate();
	PreviewParticleComponent->MarkRenderStateDirty();
}

void FParticleSystemEditorWidget::RenderDetailsPanel(const ImVec2& Size)
{
	ImGui::BeginChild("##ParticleDetailsPanel", Size, ImGuiChildFlags_Borders);
	DrawPanelHeader("Details");

	UParticleSystem* ParticleSystem = GetParticleSystem();
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

	UParticleModule* SelectedModule = const_cast<UParticleModule*>(GetSelectedModule(ParticleSystem));
	if (SelectedModule && ImGui::CollapsingHeader("Module", ImGuiTreeNodeFlags_DefaultOpen))
	{
		const UParticleModuleTypeDataBase* TypeDataModule = Cast<UParticleModuleTypeDataBase>(SelectedModule);
		DrawDetailRow("Name", TypeDataModule ? GetTypeDataDisplayName(TypeDataModule) : GetModuleDisplayName(SelectedModule));
		DrawDetailRow("Class", SelectedModule->GetClass()->GetName());
		DrawDetailRow("Spawn Module", SelectedModule->IsSpawnModule() ? "true" : "false");
		DrawDetailRow("Update Module", SelectedModule->IsUpdateModule() ? "true" : "false");
		DrawDetailRow("Source", ViewState.Selection.ModuleIndex == TypeDataModuleIndex ? "LOD TypeDataModule" : "LOD Modules[]");
		ImGui::Separator();
		if (RenderObjectProperties(SelectedModule))
		{
			ApplyEditedObjectSideEffects(SelectedModule);
			MarkDirty();
			RestartPreviewSimulation();
		}
	}
	ImGui::EndChild();
}

bool FParticleSystemEditorWidget::RenderObjectProperties(UObject* Object)
{
	if (!Object)
	{
		ImGui::TextDisabled("No object properties.");
		return false;
	}

	TArray<FPropertyValue> Properties;
	Object->GetEditableProperties(Properties);
	if (Properties.empty())
	{
		ImGui::TextDisabled("No editable properties.");
		return false;
	}

	bool bAnyChanged = false;
	if (ImGui::BeginTable("##ParticleObjectProperties", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerV))
	{
		ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 180.0f);
		ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

		for (int32 Index = 0; Index < static_cast<int32>(Properties.size()); ++Index)
		{
			FPropertyValue& PropertyValue = Properties[Index];
			ImGui::PushID(Index);
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::AlignTextToFramePadding();
			ImGui::TextUnformatted(PropertyValue.GetDisplayName());
			ImGui::TableSetColumnIndex(1);
			ImGui::SetNextItemWidth(-1.0f);

			const bool bChanged = RenderPropertyValueEditor(PropertyValue);
			if (bChanged)
			{
				bAnyChanged = true;
				if (PropertyValue.Property)
				{
					Object->PostEditProperty(PropertyValue.Property->Name);
				}
			}
			ImGui::PopID();
		}
		ImGui::EndTable();
	}

	return bAnyChanged;
}

bool FParticleSystemEditorWidget::RenderPropertyValueEditor(FPropertyValue& PropertyValue)
{
	void* ValuePtr = PropertyValue.GetValuePtr();
	if (!ValuePtr)
	{
		return false;
	}

	bool bChanged = false;
	const bool bReadOnly = PropertyValue.Property && (PropertyValue.Property->Flags & PF_ReadOnly) != 0;
	if (bReadOnly)
	{
		ImGui::BeginDisabled();
	}

	switch (PropertyValue.GetType())
	{
	case EPropertyType::Bool:
		bChanged = ImGui::Checkbox("##Value", static_cast<bool*>(ValuePtr));
		break;
	case EPropertyType::ByteBool:
	{
		uint8* Value = static_cast<uint8*>(ValuePtr);
		bool bValue = *Value != 0;
		if (ImGui::Checkbox("##Value", &bValue))
		{
			*Value = bValue ? 1 : 0;
			bChanged = true;
		}
		break;
	}
	case EPropertyType::Int:
	{
		int32* Value = static_cast<int32*>(ValuePtr);
		const float Min = PropertyValue.GetMin();
		const float Max = PropertyValue.GetMax();
		if (Min != 0.0f || Max != 0.0f)
		{
			bChanged = ImGui::DragInt("##Value", Value, PropertyValue.GetSpeed(), static_cast<int32>(Min), static_cast<int32>(Max));
		}
		else
		{
			bChanged = ImGui::DragInt("##Value", Value, PropertyValue.GetSpeed());
		}
		break;
	}
	case EPropertyType::Float:
	{
		float* Value = static_cast<float*>(ValuePtr);
		const float Min = PropertyValue.GetMin();
		const float Max = PropertyValue.GetMax();
		if (Min != 0.0f || Max != 0.0f)
		{
			bChanged = ImGui::DragFloat("##Value", Value, PropertyValue.GetSpeed(), Min, Max, "%.4f");
		}
		else
		{
			bChanged = ImGui::DragFloat("##Value", Value, PropertyValue.GetSpeed());
		}
		break;
	}
	case EPropertyType::Vec3:
	{
		float* Value = static_cast<float*>(ValuePtr);
		bChanged = ImGui::DragFloat3("##Value", Value, PropertyValue.GetSpeed());
		break;
	}
	case EPropertyType::Vec4:
	case EPropertyType::Color4:
	{
		float* Value = static_cast<float*>(ValuePtr);
		if (PropertyValue.GetType() == EPropertyType::Color4 || IsColorLikeProperty(PropertyValue))
		{
			bChanged = ImGui::ColorEdit4("##Value", Value);
		}
		else
		{
			bChanged = ImGui::DragFloat4("##Value", Value, PropertyValue.GetSpeed());
		}
		break;
	}
	case EPropertyType::String:
	{
		FString* Value = static_cast<FString*>(ValuePtr);
		char Buffer[256];
		strncpy_s(Buffer, sizeof(Buffer), Value->c_str(), _TRUNCATE);
		if (ImGui::InputText("##Value", Buffer, sizeof(Buffer)))
		{
			*Value = Buffer;
			bChanged = true;
		}
		break;
	}
	case EPropertyType::Name:
	{
		FName* Value = static_cast<FName*>(ValuePtr);
		const FString CurrentValue = Value->ToString();
		char Buffer[256];
		strncpy_s(Buffer, sizeof(Buffer), CurrentValue.c_str(), _TRUNCATE);
		if (ImGui::InputText("##Value", Buffer, sizeof(Buffer)))
		{
			*Value = FName(FString(Buffer));
			bChanged = true;
		}
		break;
	}
	case EPropertyType::Enum:
	{
		const FEnum* EnumType = PropertyValue.GetEnumType();
		if (EnumType && EnumType->GetNames() && EnumType->GetCount() > 0)
		{
			int32 Value = 0;
			std::memcpy(&Value, ValuePtr, EnumType->GetSize());
			const char* Preview = (Value >= 0 && static_cast<uint32>(Value) < EnumType->GetCount())
				? EnumType->GetNames()[Value]
				: "Unknown";
			if (ImGui::BeginCombo("##Value", Preview))
			{
				for (uint32 EnumIndex = 0; EnumIndex < EnumType->GetCount(); ++EnumIndex)
				{
					const bool bSelected = Value == static_cast<int32>(EnumIndex);
					if (ImGui::Selectable(EnumType->GetNames()[EnumIndex], bSelected))
					{
						const int32 NewValue = static_cast<int32>(EnumIndex);
						std::memcpy(ValuePtr, &NewValue, EnumType->GetSize());
						bChanged = true;
					}
					if (bSelected)
					{
						ImGui::SetItemDefaultFocus();
					}
				}
				ImGui::EndCombo();
			}
		}
		else
		{
			ImGui::TextDisabled("(unknown enum)");
		}
		break;
	}
	case EPropertyType::SoftObjectRef:
	{
		const FSoftObjectProperty* SoftObjectProperty = PropertyValue.Property ? PropertyValue.Property->AsSoftObjectProperty() : nullptr;
		const FString Path = SoftObjectProperty ? SoftObjectProperty->GetPathFromValuePtr(ValuePtr) : FString("None");
		char Buffer[512];
		strncpy_s(Buffer, sizeof(Buffer), Path.c_str(), _TRUNCATE);
		if (SoftObjectProperty && ImGui::InputText("##Value", Buffer, sizeof(Buffer)))
		{
			SoftObjectProperty->SetPathFromValuePtr(ValuePtr, FString(Buffer));
			bChanged = true;
		}
		break;
	}
	case EPropertyType::ObjectRef:
	{
		UObject* ObjectValue = *static_cast<UObject**>(ValuePtr);
		ImGui::TextDisabled("%s", ObjectValue ? ObjectValue->GetName().c_str() : "None");
		break;
	}
	case EPropertyType::Array:
		ImGui::TextDisabled("(array)");
		break;
	case EPropertyType::Struct:
		ImGui::TextDisabled("(struct)");
		break;
	case EPropertyType::ClassRef:
		ImGui::TextDisabled("(class)");
		break;
	case EPropertyType::Rotator:
		ImGui::TextDisabled("(rotator)");
		break;
	default:
		ImGui::TextDisabled("(unsupported)");
		break;
	}

	if (bReadOnly)
	{
		ImGui::EndDisabled();
	}

	return bChanged && !bReadOnly;
}

void FParticleSystemEditorWidget::ApplyEditedObjectSideEffects(UObject* Object)
{
	if (!Object)
	{
		return;
	}

	if (UParticleModuleRequired* RequiredModule = Cast<UParticleModuleRequired>(Object))
	{
		RequiredModule->Material = nullptr;
		if (UParticleEmitter* Emitter = const_cast<UParticleEmitter*>(GetSelectedEmitter(GetParticleSystem())))
		{
			Emitter->SetEmitterDuration(RequiredModule->EmitterDuration);
			Emitter->SetLooping(RequiredModule->bLooping);
		}
	}

	if (UParticleModuleTypeDataMesh* MeshTypeData = Cast<UParticleModuleTypeDataMesh>(Object))
	{
		MeshTypeData->Mesh = nullptr;
	}
}

void FParticleSystemEditorWidget::RenderEmittersPanel(const ImVec2& Size)
{
	ImGui::BeginChild("##ParticleEmittersPanel", Size, ImGuiChildFlags_Borders, ImGuiWindowFlags_HorizontalScrollbar);

	UParticleSystem* ParticleSystem = GetParticleSystem();
	const ImGuiStyle& Style = ImGui::GetStyle();
	const TArray<UParticleEmitter*>* EmittersPtr = ParticleSystem ? &ParticleSystem->GetEmitters() : nullptr;
	const int32 EmitterCount = EmittersPtr ? static_cast<int32>(EmittersPtr->size()) : 0;
	const float EmitterColumnsWidth = EmitterCount > 0
		? EmitterColumnWidth * static_cast<float>(EmitterCount) + Style.ItemSpacing.x * static_cast<float>((std::max)(0, EmitterCount - 1))
		: 0.0f;
	DrawPanelHeader("Emitters", EmitterColumnsWidth);

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

	auto CalculateEmitterColumnHeight = [&](UParticleEmitter* Emitter)
	{
		float Height = EmitterHeaderHeight + Style.ItemSpacing.y;
		if (!Emitter)
		{
			return Height + ImGui::GetTextLineHeightWithSpacing();
		}

		const TArray<UParticleLODLevel*>& LODLevels = Emitter->GetLODLevels();
		Height += LODLevels.empty() ? ImGui::GetTextLineHeightWithSpacing() : ImGui::GetFrameHeightWithSpacing();
		Height += 3.0f + Style.ItemSpacing.y;

		UParticleLODLevel* LODLevel = Emitter->GetLODLevel(GetDisplayLODIndex(Emitter));
		if (LODLevel)
		{
			Height += ImGui::GetFrameHeightWithSpacing();
			Height += 3.0f + Style.ItemSpacing.y;
			Height += ModuleRowHeight + Style.ItemSpacing.y;
			Height += static_cast<float>(LODLevel->GetModules().size()) * ModuleRowHeight;
		}

		return Height + Style.ItemSpacing.y;
	};

	float EmitterColumnHeight = 1.0f;
	for (UParticleEmitter* Emitter : Emitters)
	{
		EmitterColumnHeight = (std::max)(EmitterColumnHeight, CalculateEmitterColumnHeight(Emitter));
	}

	for (int32 EmitterIndex = 0; EmitterIndex < static_cast<int32>(Emitters.size()); ++EmitterIndex)
	{
		UParticleEmitter* Emitter = Emitters[EmitterIndex];
		const int32 DisplayLODIndex = GetDisplayLODIndex(Emitter);
		UParticleLODLevel* LODLevel = Emitter ? Emitter->GetLODLevel(DisplayLODIndex) : nullptr;
		const EParticleRenderType RenderType = GetLODRenderType(LODLevel);
		ImGui::PushID(EmitterIndex);
		ImGui::BeginChild("##EmitterColumn", ImVec2(EmitterColumnWidth, EmitterColumnHeight), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

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
				if (ImGui::SmallButton("+ Module"))
				{
					ImGui::OpenPopup("##AddParticleModulePopup");
				}
				if (ImGui::BeginPopup("##AddParticleModulePopup"))
				{
					auto AddModule = [&](UParticleModule* NewModule)
					{
						if (!NewModule)
						{
							return;
						}
						TArray<UParticleModule*>& MutableModules = LODLevel->GetMutableModules();
						MutableModules.push_back(NewModule);
						SelectModule(EmitterIndex, DisplayLODIndex, static_cast<int32>(MutableModules.size()) - 1);
						MarkDirty();
						RestartPreviewSimulation();
					};

					if (ImGui::MenuItem("Lifetime"))
					{
						AddModule(UObjectManager::Get().CreateObject<UParticleModuleLifetime>());
					}
					if (ImGui::MenuItem("Initial Location"))
					{
						AddModule(UObjectManager::Get().CreateObject<UParticleModuleLocation>());
					}
					if (ImGui::MenuItem("Initial Velocity"))
					{
						AddModule(UObjectManager::Get().CreateObject<UParticleModuleVelocity>());
					}
					if (ImGui::MenuItem("Initial Color"))
					{
						AddModule(UObjectManager::Get().CreateObject<UParticleModuleColor>());
					}
					if (ImGui::MenuItem("Initial Size"))
					{
						AddModule(UObjectManager::Get().CreateObject<UParticleModuleSize>());
					}
					ImGui::Separator();
					ImGui::BeginDisabled();
					ImGui::MenuItem("Required", nullptr, false, false);
					ImGui::MenuItem("Spawn", nullptr, false, false);
					ImGui::EndDisabled();
					ImGui::EndPopup();
				}
				ImGui::Dummy(ImVec2(0.0f, 3.0f));

				const UParticleModuleTypeDataBase* TypeDataModule = LODLevel->GetTypeDataModule();
				const char* TypeDataLabel = TypeDataModule ? GetTypeDataDisplayName(TypeDataModule) : GetRenderTypeLabel(RenderType);
				if (TypeDataModule)
				{
					ImGui::PushID(TypeDataModuleIndex);
					if (SelectableModuleRow(TypeDataLabel, IsModuleSelected(EmitterIndex, DisplayLODIndex, TypeDataModuleIndex), TypeDataModule))
					{
						SelectModule(EmitterIndex, DisplayLODIndex, TypeDataModuleIndex);
					}
					ImGui::PopID();
				}
				else
				{
					ImGui::PushID(TypeDataModuleIndex);
					if (SelectableModuleRow(TypeDataLabel, IsLODSelected(EmitterIndex, DisplayLODIndex)))
					{
						SelectLOD(EmitterIndex, DisplayLODIndex);
					}
					ImGui::PopID();
				}

				TArray<UParticleModule*>& Modules = LODLevel->GetMutableModules();
				bool bModuleListMutated = false;
				for (int32 ModuleIndex = 0; ModuleIndex < static_cast<int32>(Modules.size()); ++ModuleIndex)
				{
					UParticleModule* Module = Modules[ModuleIndex];
					const bool bDeleteLocked = IsModuleDeleteLocked(Module);
					const bool bOrderLocked = IsModuleOrderLocked(Module);
					const UParticleModule* PreviousModule = ModuleIndex > 0 ? Modules[ModuleIndex - 1] : nullptr;
					const UParticleModule* NextModule = ModuleIndex + 1 < static_cast<int32>(Modules.size()) ? Modules[ModuleIndex + 1] : nullptr;
					const bool bCanMoveUp = !bOrderLocked && ModuleIndex > 0 && !IsModuleOrderLocked(PreviousModule);
					const bool bCanMoveDown = !bOrderLocked && ModuleIndex + 1 < static_cast<int32>(Modules.size()) && !IsModuleOrderLocked(NextModule);
					ImGui::PushID(ModuleIndex);
					const FModuleRowAction Action = EditableModuleRow(
						GetModuleDisplayName(Module),
						Module,
						IsModuleSelected(EmitterIndex, DisplayLODIndex, ModuleIndex),
						bCanMoveUp,
						bCanMoveDown,
						!bDeleteLocked);
					if (Action.bSelect)
					{
						SelectModule(EmitterIndex, DisplayLODIndex, ModuleIndex);
					}
					ImGui::PopID();

					if (Action.bMoveUp && bCanMoveUp)
					{
						std::swap(Modules[ModuleIndex], Modules[ModuleIndex - 1]);
						SelectModule(EmitterIndex, DisplayLODIndex, ModuleIndex - 1);
						MarkDirty();
						RestartPreviewSimulation();
						bModuleListMutated = true;
					}
					else if (Action.bMoveDown && bCanMoveDown)
					{
						std::swap(Modules[ModuleIndex], Modules[ModuleIndex + 1]);
						SelectModule(EmitterIndex, DisplayLODIndex, ModuleIndex + 1);
						MarkDirty();
						RestartPreviewSimulation();
						bModuleListMutated = true;
					}
					else if (Action.bDelete && !bDeleteLocked)
					{
						UParticleModule* RemovedModule = Modules[ModuleIndex];
						Modules.erase(Modules.begin() + ModuleIndex);
						UObjectManager::Get().DestroyObject(RemovedModule);
						if (Modules.empty())
						{
							SelectLOD(EmitterIndex, DisplayLODIndex);
						}
						else
						{
							const int32 NewModuleIndex = (std::min)(ModuleIndex, static_cast<int32>(Modules.size()) - 1);
							SelectModule(EmitterIndex, DisplayLODIndex, NewModuleIndex);
						}
						MarkDirty();
						RestartPreviewSimulation();
						bModuleListMutated = true;
					}

					if (bModuleListMutated)
					{
						break;
					}
				}
			}
		}

		ImGui::EndChild();
		ImGui::PopID();
		if (EmitterIndex + 1 < static_cast<int32>(Emitters.size()))
		{
			ImGui::SameLine();
		}
	}

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
