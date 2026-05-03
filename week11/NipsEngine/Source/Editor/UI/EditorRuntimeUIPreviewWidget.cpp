#include "Editor/UI/EditorRuntimeUIPreviewWidget.h"

#include "Editor/EditorEngine.h"
#include "Engine/Input/InputSystem.h"
#include "Runtime/ViewportRect.h"

#include "ImGui/imgui.h"

#include <algorithm>
#include <cstdio>
#include <utility>

namespace
{
	struct FPreviewResolutionPreset
	{
		const char* Label;
		int32 Width;
		int32 Height;
	};

	constexpr FPreviewResolutionPreset PreviewResolutionPresets[] =
	{
		{ "1920 x 1080", 1920, 1080 },
		{ "1600 x 900", 1600, 900 },
		{ "1280 x 720", 1280, 720 },
		{ "1024 x 768", 1024, 768 },
		{ "800 x 600", 800, 600 },
		{ "Custom", 0, 0 },
	};

	void GetPreviewResolution(int32 PresetIndex, int32 CustomWidth, int32 CustomHeight, int32& OutWidth, int32& OutHeight)
	{
		const int32 PresetCount = static_cast<int32>(sizeof(PreviewResolutionPresets) / sizeof(PreviewResolutionPresets[0]));
		PresetIndex = std::clamp(PresetIndex, 0, PresetCount - 1);
		const FPreviewResolutionPreset& Preset = PreviewResolutionPresets[PresetIndex];
		if (Preset.Width > 0 && Preset.Height > 0)
		{
			OutWidth = Preset.Width;
			OutHeight = Preset.Height;
			return;
		}

		OutWidth = std::max(320, CustomWidth);
		OutHeight = std::max(180, CustomHeight);
	}
}

void FEditorRuntimeUIPreviewWidget::Initialize(UEditorEngine* InEditorEngine)
{
	FEditorWidget::Initialize(InEditorEngine);
}

void FEditorRuntimeUIPreviewWidget::SetRmlRenderQueue(std::function<void(const FRuntimeUIRenderContext&)> InQueueCallback)
{
	QueueRmlRenderContext = std::move(InQueueCallback);
}

void FEditorRuntimeUIPreviewWidget::Render(float DeltaTime)
{
	ImGui::SetNextWindowSize(ImVec2(980.0f, 680.0f), ImGuiCond_Once);
	if (!ImGui::Begin("Runtime UI Preview"))
	{
		ImGui::End();
		return;
	}

	DrawToolbar();
	ImGui::Separator();

	ImGui::Columns(2, "##RmlRuntimeUIPreviewColumns", true);
	ImGui::SetColumnWidth(0, 720.0f);
	DrawPreviewSurface(DeltaTime);
	ImGui::NextColumn();

	DrawActionEvents();
	DrawAuthoringGuidance();

	ImGui::Columns(1);
	ImGui::End();
}

void FEditorRuntimeUIPreviewWidget::DrawToolbar()
{
	ImGui::SetNextItemWidth(360.0f);
	if (ImGui::InputText("RML", PreviewDocumentPathBuffer, IM_ARRAYSIZE(PreviewDocumentPathBuffer), ImGuiInputTextFlags_EnterReturnsTrue))
	{
		RefreshPreviewDocument();
	}

	ImGui::SameLine();
	if (ImGui::Button("Load"))
	{
		RefreshPreviewDocument();
	}

	ImGui::SameLine();
	if (ImGui::Button("Reload"))
	{
		RefreshPreviewDocument();
	}

	ImGui::SameLine();
	ImGui::Checkbox("Interaction", &bEnableInteraction);

	ImGui::SameLine();
	ImGui::Checkbox("Guide", &bShowGuidance);

	const int32 PresetCount = static_cast<int32>(sizeof(PreviewResolutionPresets) / sizeof(PreviewResolutionPresets[0]));
	const char* CurrentPreset = PreviewResolutionPresets[std::clamp(ResolutionPresetIndex, 0, PresetCount - 1)].Label;
	ImGui::SetNextItemWidth(140.0f);
	if (ImGui::BeginCombo("Resolution", CurrentPreset))
	{
		for (int32 i = 0; i < PresetCount; ++i)
		{
			const bool bSelected = ResolutionPresetIndex == i;
			if (ImGui::Selectable(PreviewResolutionPresets[i].Label, bSelected))
			{
				ResolutionPresetIndex = i;
			}
			if (bSelected)
			{
				ImGui::SetItemDefaultFocus();
			}
		}
		ImGui::EndCombo();
	}

	if (ResolutionPresetIndex == PresetCount - 1)
	{
		ImGui::SameLine();
		ImGui::SetNextItemWidth(78.0f);
		ImGui::DragInt("W", &CustomWidth, 8.0f, 320, 7680);
		ImGui::SameLine();
		ImGui::SetNextItemWidth(78.0f);
		ImGui::DragInt("H", &CustomHeight, 8.0f, 180, 4320);
	}

	ImGui::SameLine();
	ImGui::SetNextItemWidth(120.0f);
	ImGui::SliderFloat("Zoom", &PreviewZoom, 0.25f, 1.5f, "%.2fx");
}

void FEditorRuntimeUIPreviewWidget::DrawPreviewSurface(float DeltaTime)
{
	if (!EditorEngine)
	{
		ImGui::TextDisabled("EditorEngine is not ready.");
		return;
	}

	if (!bPreviewDocumentLoaded)
	{
		LoadPreviewDocument();
	}

	int32 TargetWidth = 1920;
	int32 TargetHeight = 1080;
	GetPreviewResolution(ResolutionPresetIndex, CustomWidth, CustomHeight, TargetWidth, TargetHeight);

	const ImVec2 Available = ImGui::GetContentRegionAvail();
	const float FitScale = std::min(
		Available.x > 0.0f ? Available.x / static_cast<float>(TargetWidth) : 1.0f,
		Available.y > 0.0f ? Available.y / static_cast<float>(TargetHeight) : 1.0f);
	const float Scale = std::max(0.05f, FitScale * PreviewZoom);
	const ImVec2 PreviewSize(
		std::max(1.0f, static_cast<float>(TargetWidth) * Scale),
		std::max(1.0f, static_cast<float>(TargetHeight) * Scale));

	ImGui::BeginChild("##RmlRuntimeUIPreviewSurface", ImVec2(0.0f, 0.0f), false, ImGuiWindowFlags_NoScrollWithMouse);

	const ImVec2 Start = ImGui::GetCursorScreenPos();
	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	DrawList->AddRectFilled(Start, ImVec2(Start.x + PreviewSize.x, Start.y + PreviewSize.y),
		ImGui::GetColorU32(ImVec4(0.025f, 0.027f, 0.032f, 1.0f)), 6.0f);
	DrawList->AddRect(Start, ImVec2(Start.x + PreviewSize.x, Start.y + PreviewSize.y),
		ImGui::GetColorU32(ImVec4(0.25f, 0.29f, 0.35f, 1.0f)), 6.0f);

	ImGui::InvisibleButton("##RmlRuntimeUIPreviewInputSurface", PreviewSize);
	const bool bPreviewHovered = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup);

	FRuntimeUIRenderContext Context;
	Context.RenderMode = ERuntimeUIRenderMode::PIE;
	Context.ViewportMin = FRuntimeUIVector2(Start.x, Start.y);
	Context.ViewportSize = FRuntimeUIVector2(PreviewSize.x, PreviewSize.y);
	Context.DeltaTime = DeltaTime;

	if (QueueRmlRenderContext)
	{
		QueueRmlRenderContext(Context);
	}

	if (bEnableInteraction && bPreviewHovered)
	{
		FViewportRect PreviewRect(
			static_cast<int32>(Start.x),
			static_cast<int32>(Start.y),
			static_cast<int32>(PreviewSize.x),
			static_cast<int32>(PreviewSize.y));
		if (EditorEngine->PumpPIERmlUiInput(PreviewRect))
		{
			InputSystem::Get().SetGuiMouseCapture(true);
			InputSystem::Get().SetGuiViewportMouseBlock(true);
		}
	}

	const TArray<FString> NewEvents = EditorEngine->PollRmlUIActionEvents();
	for (const FString& Event : NewEvents)
	{
		if (!Event.empty())
		{
			PreviewActionEvents.push_back(Event);
		}
	}
	while (PreviewActionEvents.size() > 12)
	{
		PreviewActionEvents.erase(PreviewActionEvents.begin());
	}

	char OverlayText[192];
	std::snprintf(OverlayText, sizeof(OverlayText), "%d x %d | %.2fx | %s",
		TargetWidth, TargetHeight, Scale, bPreviewDocumentLoaded ? "RML loaded" : "RML missing");
	DrawList->AddText(ImVec2(Start.x + 10.0f, Start.y + 8.0f),
		ImGui::GetColorU32(ImVec4(0.72f, 0.76f, 0.82f, 0.9f)), OverlayText);

	ImGui::EndChild();
}

void FEditorRuntimeUIPreviewWidget::DrawActionEvents()
{
	ImGui::Text("RmlUi Action Events");
	ImGui::Separator();
	if (PreviewActionEvents.empty())
	{
		ImGui::TextDisabled("No events yet.");
		return;
	}

	for (auto It = PreviewActionEvents.rbegin(); It != PreviewActionEvents.rend(); ++It)
	{
		ImGui::BulletText("%s", It->c_str());
	}
}

void FEditorRuntimeUIPreviewWidget::DrawAuthoringGuidance() const
{
	if (!bShowGuidance)
	{
		return;
	}

	ImGui::Spacing();
	ImGui::Text("RML Preview Rule");
	ImGui::Separator();
	ImGui::TextWrapped("Load an .rml document under Asset/UI. Linked .rcss files are resolved by the RmlUi file interface.");
	ImGui::Spacing();
	ImGui::TextDisabled("Action event");
	ImGui::TextWrapped("<button id=\"StartButton\" data-action=\"StartGame\">START</button>");
	ImGui::Spacing();
	ImGui::TextDisabled("Lua runtime");
	ImGui::TextWrapped("Engine.API.UI.LoadDocument(\"Title\", \"Asset/UI/Title/Title.rml\")");
	ImGui::TextWrapped("Engine.API.UI.ShowDocument(\"Title\")");
}

bool FEditorRuntimeUIPreviewWidget::LoadPreviewDocument()
{
	if (!EditorEngine)
	{
		return false;
	}

	const FString ScreenId = PreviewScreenIdBuffer;
	const FString Path = PreviewDocumentPathBuffer;
	bPreviewDocumentLoaded = EditorEngine->LoadRmlUIDocument(ScreenId, Path);
	if (bPreviewDocumentLoaded)
	{
		EditorEngine->ShowRmlUIScreen(ScreenId);
	}
	return bPreviewDocumentLoaded;
}

void FEditorRuntimeUIPreviewWidget::RefreshPreviewDocument()
{
	if (!EditorEngine)
	{
		bPreviewDocumentLoaded = false;
		return;
	}

	EditorEngine->UnloadRmlUIDocument(PreviewScreenIdBuffer);
	bPreviewDocumentLoaded = false;
	PreviewActionEvents.clear();
	LoadPreviewDocument();
}
