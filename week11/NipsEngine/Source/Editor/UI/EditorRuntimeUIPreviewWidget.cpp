#include "Editor/UI/EditorRuntimeUIPreviewWidget.h"

#include "Editor/EditorEngine.h"
#include "Core/ResourceManager.h"
#include "Render/Resource/Texture.h"
#include "UI/RuntimeUISampleScreens.h"

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

	const char* GetWidgetTypeName(ERuntimeUIWidgetType Type)
	{
		switch (Type)
		{
		case ERuntimeUIWidgetType::Panel:
			return "Panel";
		case ERuntimeUIWidgetType::Text:
			return "Text";
		case ERuntimeUIWidgetType::Button:
			return "Button";
		case ERuntimeUIWidgetType::Image:
			return "Image";
		case ERuntimeUIWidgetType::ProgressBar:
			return "ProgressBar";
		default:
			return "Unknown";
		}
	}

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
	PreviewBackend.SetImageResolver(
		[this](const FString& ImagePath)
		{
			return ResolveRuntimeUIImage(ImagePath);
		});
	RebuildPreviewUI();
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

	const bool bLiveRuntime = PreviewSource == EPreviewSource::LiveRuntime;
	FRuntimeUISystem* UIToRender = bLiveRuntime && EditorEngine ? &EditorEngine->GetRuntimeUI() : &PreviewUI;
	if (UIToRender)
	{
		DrawScreenSelector(*UIToRender, !bLiveRuntime);
	}

	if (bLiveRuntime)
	{
		ImGui::SameLine();
		ImGui::TextDisabled("Live Runtime is read-only here.");
	}

	ImGui::Columns(2, "##RuntimeUIPreviewColumns", true);
	ImGui::SetColumnWidth(0, 720.0f);
	DrawPreviewSurface(DeltaTime);
	ImGui::NextColumn();

	if (UIToRender)
	{
		DrawWidgetSummary(*UIToRender);
	}
	DrawActionEvents();
	DrawAuthoringGuidance();

	ImGui::Columns(1);
	ImGui::End();
}

void FEditorRuntimeUIPreviewWidget::RebuildPreviewUI()
{
	PreviewUI = FRuntimeUISystem();
	RuntimeUISamples::BuildMinimalGameJamScreens(PreviewUI);
	PreviewActionEvents.clear();
	bNeedsRebuild = false;
}

void FEditorRuntimeUIPreviewWidget::DrawToolbar()
{
	const char* SourceLabels[] = { "Sample GameJam UI Set", "Live Engine Runtime UI" };
	int32 SourceIndex = static_cast<int32>(PreviewSource);
	ImGui::SetNextItemWidth(210.0f);
	if (ImGui::Combo("Source", &SourceIndex, SourceLabels, 2))
	{
		PreviewSource = static_cast<EPreviewSource>(SourceIndex);
		if (PreviewSource == EPreviewSource::SampleGameJam)
		{
			bNeedsRebuild = true;
		}
	}

	ImGui::SameLine();
	if (ImGui::Button("Rebuild"))
	{
		bNeedsRebuild = true;
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

	if (bNeedsRebuild && PreviewSource == EPreviewSource::SampleGameJam)
	{
		RebuildPreviewUI();
	}
}

void FEditorRuntimeUIPreviewWidget::DrawPreviewSurface(float DeltaTime)
{
	int32 TargetWidth = 1920;
	int32 TargetHeight = 1080;
	GetPreviewResolution(ResolutionPresetIndex, CustomWidth, CustomHeight, TargetWidth, TargetHeight);

	FRuntimeUISystem* UIToRender =
		PreviewSource == EPreviewSource::LiveRuntime && EditorEngine
			? &EditorEngine->GetRuntimeUI()
			: &PreviewUI;

	if (!UIToRender)
	{
		ImGui::TextDisabled("No Runtime UI system.");
		return;
	}

	const ImVec2 Available = ImGui::GetContentRegionAvail();
	const float FitScale = std::min(
		Available.x > 0.0f ? Available.x / static_cast<float>(TargetWidth) : 1.0f,
		Available.y > 0.0f ? Available.y / static_cast<float>(TargetHeight) : 1.0f);
	const float Scale = std::max(0.05f, FitScale * PreviewZoom);
	const ImVec2 PreviewSize(
		std::max(1.0f, static_cast<float>(TargetWidth) * Scale),
		std::max(1.0f, static_cast<float>(TargetHeight) * Scale));

	ImGui::BeginChild("##RuntimeUIPreviewSurface", ImVec2(0.0f, 0.0f), false, ImGuiWindowFlags_NoScrollWithMouse);

	const ImVec2 Start = ImGui::GetCursorScreenPos();
	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	DrawList->AddRectFilled(Start, ImVec2(Start.x + PreviewSize.x, Start.y + PreviewSize.y), ImGui::GetColorU32(ImVec4(0.025f, 0.027f, 0.032f, 1.0f)), 6.0f);
	DrawList->AddRect(Start, ImVec2(Start.x + PreviewSize.x, Start.y + PreviewSize.y), ImGui::GetColorU32(ImVec4(0.25f, 0.29f, 0.35f, 1.0f)), 6.0f);

	ImGui::InvisibleButton("##RuntimeUIPreviewInputSurface", PreviewSize);
	const bool bPreviewHovered = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup);

	FRuntimeUIRenderContext Context;
	Context.RenderMode = ERuntimeUIRenderMode::PIE;
	Context.ViewportMin = FRuntimeUIVector2(Start.x, Start.y);
	Context.ViewportSize = FRuntimeUIVector2(PreviewSize.x, PreviewSize.y);
	Context.DeltaTime = DeltaTime;

	UIToRender->UpdateLayout(Context);
	if (bPreviewHovered || !UIToRender->GetPressedWidgetId().empty())
	{
		HandlePreviewInput(*UIToRender, Context, PreviewSource == EPreviewSource::SampleGameJam);
	}

	UIToRender->Render(PreviewBackend, Context);

	const TArray<FString> NewEvents = UIToRender->ConsumeActionEvents();
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

	char OverlayText[128];
	std::snprintf(OverlayText, sizeof(OverlayText), "%d x %d  |  %.2fx", TargetWidth, TargetHeight, Scale);
	DrawList->AddText(ImVec2(Start.x + 10.0f, Start.y + 8.0f), ImGui::GetColorU32(ImVec4(0.72f, 0.76f, 0.82f, 0.9f)), OverlayText);

	ImGui::EndChild();
}

void FEditorRuntimeUIPreviewWidget::DrawScreenSelector(FRuntimeUISystem& UI, bool bCanMutate)
{
	FRuntimeUICanvas* Canvas = UI.FindCanvas(FRuntimeUISystem::DefaultCanvasId);
	const FString CurrentScreen = Canvas ? Canvas->GetActiveScreenId() : FString();

	ImGui::SetNextItemWidth(180.0f);
	if (ImGui::BeginCombo("Screen", CurrentScreen.empty() ? "<none>" : CurrentScreen.c_str()))
	{
		for (const auto& ScreenPair : UI.GetScreens())
		{
			const FRuntimeUIScreen& Screen = ScreenPair.second;
			if (Screen.GetCanvasId() != FRuntimeUISystem::DefaultCanvasId)
			{
				continue;
			}

			const bool bSelected = Screen.GetId() == CurrentScreen;
			if (ImGui::Selectable(Screen.GetId().c_str(), bSelected) && bCanMutate)
			{
				UI.SetActiveScreen(FRuntimeUISystem::DefaultCanvasId, Screen.GetId());
			}
			if (bSelected)
			{
				ImGui::SetItemDefaultFocus();
			}
		}
		ImGui::EndCombo();
	}
}

void FEditorRuntimeUIPreviewWidget::DrawWidgetSummary(const FRuntimeUISystem& UI) const
{
	ImGui::Text("Widgets");
	ImGui::Separator();
	ImGui::TextDisabled("Count: %d", static_cast<int32>(UI.GetWidgets().size()));

	if (ImGui::BeginChild("##RuntimeUIPreviewWidgetList", ImVec2(0.0f, 210.0f), true))
	{
		for (const auto& WidgetPair : UI.GetWidgets())
		{
			const FRuntimeUIWidget& Widget = WidgetPair.second;
			ImGui::Text("%s", Widget.GetId().c_str());
			ImGui::SameLine();
			ImGui::TextDisabled("(%s)", GetWidgetTypeName(Widget.GetType()));
		}
	}
	ImGui::EndChild();
}

void FEditorRuntimeUIPreviewWidget::DrawActionEvents()
{
	ImGui::Spacing();
	ImGui::Text("Action Events");
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
	ImGui::Text("Authoring Rule");
	ImGui::Separator();
	ImGui::TextWrapped("StateMachine decides which UI set is shown. UI scripts should expose Build(UI), and both Runtime and Preview should pass their own UI facade into that Build function.");
	ImGui::Spacing();
	ImGui::TextDisabled("Example");
	ImGui::TextWrapped("TitleUI.Build(Engine.API.UI) at runtime");
	ImGui::TextWrapped("TitleUI.Build(PreviewUI) inside this panel");
}

void FEditorRuntimeUIPreviewWidget::HandlePreviewInput(FRuntimeUISystem& UI, const FRuntimeUIRenderContext& Context, bool bCanMutate)
{
	if (!bEnableInteraction || !bCanMutate)
	{
		return;
	}

	const ImGuiIO& IO = ImGui::GetIO();
	FRuntimeUIInputEvent Event;
	Event.ScreenPosition = FRuntimeUIVector2(IO.MousePos.x, IO.MousePos.y);

	Event.Type = ERuntimeUIInputEventType::MouseMove;
	UI.HandleInput(Event);

	if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
	{
		Event.Type = ERuntimeUIInputEventType::MouseButtonDown;
		Event.MouseButton = ERuntimeUIMouseButton::Left;
		UI.HandleInput(Event);
	}
	if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
	{
		Event.Type = ERuntimeUIInputEventType::MouseButtonUp;
		Event.MouseButton = ERuntimeUIMouseButton::Left;
		UI.HandleInput(Event);
	}
	if (IO.MouseWheel != 0.0f)
	{
		Event.Type = ERuntimeUIInputEventType::MouseWheel;
		Event.MouseButton = ERuntimeUIMouseButton::None;
		Event.WheelDelta = IO.MouseWheel;
		UI.HandleInput(Event);
	}

	(void)Context;
}

FRuntimeUIResolvedImage FEditorRuntimeUIPreviewWidget::ResolveRuntimeUIImage(const FString& ImagePath) const
{
	UTexture* Texture = FResourceManager::Get().LoadTexture(ImagePath);
	if (!Texture || !Texture->GetSRV())
	{
		return {};
	}

	FRuntimeUIResolvedImage Result;
	Result.TextureId = Texture->GetSRV();
	Result.Width = 1.0f;
	Result.Height = 1.0f;

	ID3D11Resource* Resource = nullptr;
	Texture->GetSRV()->GetResource(&Resource);
	if (Resource)
	{
		ID3D11Texture2D* Texture2D = nullptr;
		if (SUCCEEDED(Resource->QueryInterface(__uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&Texture2D))) && Texture2D)
		{
			D3D11_TEXTURE2D_DESC Desc = {};
			Texture2D->GetDesc(&Desc);
			Result.Width = static_cast<float>(Desc.Width);
			Result.Height = static_cast<float>(Desc.Height);
			Texture2D->Release();
		}
		Resource->Release();
	}

	return Result;
}
