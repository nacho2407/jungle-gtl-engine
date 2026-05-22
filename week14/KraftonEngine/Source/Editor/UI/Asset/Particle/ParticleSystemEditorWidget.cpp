#include "ParticleSystemEditorWidget.h"

#include "Object/Object.h"
#include "Particles/ParticleSystem.h"

#include <algorithm>
#include <cstdio>
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
		return ImGui::InvisibleButton(Label, ImVec2(ImGui::GetContentRegionAvail().x, ModuleRowHeight));
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
	if (!IsOpen() || !bPreviewPlaying)
	{
		return;
	}

	PreviewTime += DeltaTime;
}

void FParticleSystemEditorWidget::Render(float DeltaTime)
{
	(void)DeltaTime;

	UParticleSystem* ParticleSystem = GetParticleSystem();
	if (!IsOpen() || !ParticleSystem)
	{
		return;
	}

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
	RenderEmittersPanel(ImVec2(0.0f, Layout.TopHeight));
	ImGui::Dummy(ImVec2(0.0f, Style.ItemSpacing.y));
	RenderCurveEditorPanel(ImVec2(0.0f, Layout.BottomHeight));
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
	SelectedEmitterIndex = -1;
	SelectedLODIndex = 0;
	SelectedModuleIndex = -1;
	bPreviewPlaying = true;
	PreviewTime = 0.0f;
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

	if (ImGui::Button(bPreviewPlaying ? "Pause" : "Play", ImVec2(68.0f, 0.0f)))
	{
		bPreviewPlaying = !bPreviewPlaying;
	}
	ImGui::SameLine();
	if (ImGui::Button("Restart Sim", ImVec2(92.0f, 0.0f)))
	{
		PreviewTime = 0.0f;
	}
	ImGui::SameLine();
	ImGui::Text("Preview %.2fs", PreviewTime);
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

	ImGui::Text("Asset: %s", ParticleSystem->GetAssetPathFileName().c_str());
	ImGui::Text("Emitters: %d", static_cast<int32>(ParticleSystem->GetEmitters().size()));
	ImGui::Separator();
	if (ImGui::CollapsingHeader("Particle System", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::Text("Update Time FPS");
		ImGui::SameLine(180.0f);
		ImGui::TextDisabled("60.0");
		ImGui::Text("Warmup Time");
		ImGui::SameLine(180.0f);
		ImGui::TextDisabled("0.0");
	}
	if (ImGui::CollapsingHeader("Selection", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::Text("Selected Emitter");
		ImGui::SameLine(180.0f);
		ImGui::TextDisabled("%d", SelectedEmitterIndex);
		ImGui::Text("Selected LOD");
		ImGui::SameLine(180.0f);
		ImGui::TextDisabled("%d", SelectedLODIndex);
		ImGui::Text("Selected Module");
		ImGui::SameLine(180.0f);
		ImGui::TextDisabled("%d", SelectedModuleIndex);
	}
	ImGui::EndChild();
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
		ImGui::PushID(EmitterIndex);
		ImGui::BeginChild("##EmitterColumn", ImVec2(EmitterColumnWidth, 0.0f), true);

		const ImVec2 HeaderMin = ImGui::GetCursorScreenPos();
		const ImVec2 HeaderMax(HeaderMin.x + ImGui::GetContentRegionAvail().x, HeaderMin.y + EmitterHeaderHeight);
		ImDrawList* DrawList = ImGui::GetWindowDrawList();
		DrawList->AddRectFilled(HeaderMin, HeaderMax, SelectedEmitterIndex == EmitterIndex ? IM_COL32(74, 76, 83, 255) : IM_COL32(55, 56, 61, 255));
		char Header[64];
		std::snprintf(Header, sizeof(Header), "Emitter %d", EmitterIndex);
		DrawList->AddText(ImVec2(HeaderMin.x + 8.0f, HeaderMin.y + 7.0f), IM_COL32(240, 242, 245, 255), Header);
		DrawList->AddText(ImVec2(HeaderMin.x + 8.0f, HeaderMin.y + 30.0f), IM_COL32(160, 210, 255, 255), "Sprite");
		if (Emitter)
		{
			char CountLabel[32];
			std::snprintf(CountLabel, sizeof(CountLabel), "%d", Emitter->GetMaxActiveParticles());
			DrawList->AddText(ImVec2(HeaderMax.x - 36.0f, HeaderMin.y + 30.0f), IM_COL32(230, 234, 238, 255), CountLabel);
		}

		if (ImGui::InvisibleButton("##EmitterHeader", ImVec2(ImGui::GetContentRegionAvail().x, EmitterHeaderHeight)))
		{
			SelectedEmitterIndex = EmitterIndex;
			SelectedModuleIndex = -1;
		}

		if (Emitter)
		{
			const UParticleLODLevel* LODLevel = Emitter->GetLODLevel(SelectedLODIndex);
			if (LODLevel)
			{
				SelectableModuleRow("Required", SelectedEmitterIndex == EmitterIndex && SelectedModuleIndex == -1, IM_COL32(190, 190, 92, 255));
				if (ImGui::IsItemClicked())
				{
					SelectedEmitterIndex = EmitterIndex;
					SelectedModuleIndex = -1;
				}

				SelectableModuleRow("Spawn", false, IM_COL32(200, 92, 92, 255));
				SelectableModuleRow("Lifetime", false, IM_COL32(88, 92, 105, 255));
				SelectableModuleRow("Initial Size", false, IM_COL32(88, 92, 105, 255));
				SelectableModuleRow("Initial Color", false, IM_COL32(88, 140, 88, 255));

				const TArray<UParticleModule*>& Modules = LODLevel->GetModules();
				for (int32 ModuleIndex = 0; ModuleIndex < static_cast<int32>(Modules.size()); ++ModuleIndex)
				{
					char ModuleLabel[64];
					std::snprintf(ModuleLabel, sizeof(ModuleLabel), "Module %d", ModuleIndex);
					if (SelectableModuleRow(ModuleLabel, SelectedEmitterIndex == EmitterIndex && SelectedModuleIndex == ModuleIndex, IM_COL32(92, 120, 180, 255)))
					{
						SelectedEmitterIndex = EmitterIndex;
						SelectedModuleIndex = ModuleIndex;
					}
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
