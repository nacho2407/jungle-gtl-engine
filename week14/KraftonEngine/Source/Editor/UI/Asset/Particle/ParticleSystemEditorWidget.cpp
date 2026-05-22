#include "ParticleSystemEditorWidget.h"

#include "Object/Object.h"
#include "Particles/ParticleSystem.h"

#include <algorithm>
#include <cstdio>
#include <imgui.h>

namespace
{
	constexpr float MinLeftColumnWidth = 420.0f;
	constexpr float MinPanelHeight = 180.0f;

	void DrawPanelHeader(const char* Label)
	{
		ImGui::TextUnformatted(Label);
		ImGui::Separator();
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
	const float LeftWidth = (std::max)(MinLeftColumnWidth, Available.x * 0.46f);
	const float RightWidth = (std::max)(MinLeftColumnWidth, Available.x - LeftWidth - Style.ItemSpacing.x);
	const float TopHeight = (std::max)(MinPanelHeight, Available.y * 0.58f);
	const float BottomHeight = (std::max)(MinPanelHeight, Available.y - TopHeight - Style.ItemSpacing.y);

	ImGui::BeginChild("##ParticleEditorLeftColumn", ImVec2(LeftWidth, 0.0f), ImGuiChildFlags_None);
	RenderViewportPanel(ImVec2(0.0f, TopHeight));
	RenderDetailsPanel(ImVec2(0.0f, BottomHeight));
	ImGui::EndChild();

	ImGui::SameLine();

	ImGui::BeginChild("##ParticleEditorRightColumn", ImVec2(RightWidth, 0.0f), ImGuiChildFlags_None);
	RenderEmittersPanel(ImVec2(0.0f, TopHeight));
	RenderCurveEditorPanel(ImVec2(0.0f, BottomHeight));
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

void FParticleSystemEditorWidget::RenderToolbar()
{
	if (ImGui::Button(bPreviewPlaying ? "Pause" : "Play"))
	{
		bPreviewPlaying = !bPreviewPlaying;
	}
	ImGui::SameLine();
	if (ImGui::Button("Restart Sim"))
	{
		PreviewTime = 0.0f;
	}
	ImGui::SameLine();
	ImGui::Text("Preview %.2fs", PreviewTime);
	ImGui::SameLine();
	ImGui::BeginDisabled();
	ImGui::Button("Save");
	ImGui::SameLine();
	ImGui::Button("Add Emitter");
	ImGui::SameLine();
	ImGui::Button("Add LOD");
	ImGui::EndDisabled();
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
	DrawList->AddRectFilled(CanvasMin, CanvasMax, IM_COL32(70, 70, 70, 255));
	DrawList->AddText(ImVec2(CanvasMin.x + 12.0f, CanvasMin.y + 12.0f), IM_COL32(220, 224, 232, 255), "Particle preview viewport");
	DrawList->AddText(ImVec2(CanvasMin.x + 12.0f, CanvasMin.y + 34.0f), IM_COL32(160, 166, 176, 255), "Preview world/component hookup comes later.");
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
	ImGui::Text("Selected Emitter: %d", SelectedEmitterIndex);
	ImGui::Text("Selected LOD: %d", SelectedLODIndex);
	ImGui::Text("Selected Module: %d", SelectedModuleIndex);
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
		ImGui::BeginGroup();

		char Header[64];
		std::snprintf(Header, sizeof(Header), "Emitter %d", EmitterIndex);
		if (ImGui::Selectable(Header, SelectedEmitterIndex == EmitterIndex, 0, ImVec2(150.0f, 24.0f)))
		{
			SelectedEmitterIndex = EmitterIndex;
			SelectedModuleIndex = -1;
		}

		if (Emitter)
		{
			ImGui::TextDisabled("Max: %d", Emitter->GetMaxActiveParticles());
			ImGui::TextDisabled("Duration: %.2f", Emitter->GetEmitterDuration());

			const UParticleLODLevel* LODLevel = Emitter->GetLODLevel(SelectedLODIndex);
			if (LODLevel)
			{
				ImGui::Selectable("Required", false, ImGuiSelectableFlags_Disabled, ImVec2(150.0f, 22.0f));
				const TArray<UParticleModule*>& Modules = LODLevel->GetModules();
				for (int32 ModuleIndex = 0; ModuleIndex < static_cast<int32>(Modules.size()); ++ModuleIndex)
				{
					char ModuleLabel[64];
					std::snprintf(ModuleLabel, sizeof(ModuleLabel), "Module %d", ModuleIndex);
					if (ImGui::Selectable(ModuleLabel, SelectedEmitterIndex == EmitterIndex && SelectedModuleIndex == ModuleIndex, 0, ImVec2(150.0f, 22.0f)))
					{
						SelectedEmitterIndex = EmitterIndex;
						SelectedModuleIndex = ModuleIndex;
					}
				}
			}
		}

		ImGui::EndGroup();
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
	ImGui::TextDisabled("Curve editor is optional and will be connected after module data is defined.");
	ImGui::EndChild();
}
