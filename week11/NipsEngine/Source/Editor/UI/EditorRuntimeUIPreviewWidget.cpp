#include "Editor/UI/EditorRuntimeUIPreviewWidget.h"

#include "Editor/EditorEngine.h"
#include "Core/Paths.h"
#include "Engine/Input/InputSystem.h"
#include "Runtime/ViewportRect.h"

#include "ImGui/imgui.h"

#include <algorithm>
#include <cctype>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <commdlg.h>
#include <cstdio>
#include <filesystem>
#include <utility>
#include <Windows.h>

#pragma comment(lib, "Comdlg32.lib")

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

	FString ToLower(FString Value)
	{
		std::transform(Value.begin(), Value.end(), Value.begin(),
			[](unsigned char Ch) { return static_cast<char>(std::tolower(Ch)); });
		return Value;
	}

	bool HasParentDirectoryReference(const std::filesystem::path& Path)
	{
		for (const std::filesystem::path& Part : Path)
		{
			if (Part == std::filesystem::path(L".."))
			{
				return true;
			}
		}
		return false;
	}

	bool NormalizeRmlPath(const FString& InPath, FString& OutPath)
	{
		OutPath.clear();
		if (InPath.empty())
		{
			return false;
		}

		std::filesystem::path Path(FPaths::ToWide(InPath));
		const std::filesystem::path Root = std::filesystem::path(FPaths::RootDir()).lexically_normal();
		const std::filesystem::path UiRoot = (Root / L"Asset" / L"UI").lexically_normal();
		if (!Path.is_absolute())
		{
			Path = Root / Path;
		}
		Path = Path.lexically_normal();

		const FString Extension = ToLower(FPaths::ToUtf8(Path.extension().wstring()));
		if (Extension != ".rml")
		{
			return false;
		}

		const std::filesystem::path RelativeToUi = Path.lexically_relative(UiRoot);
		if (RelativeToUi.empty() || HasParentDirectoryReference(RelativeToUi))
		{
			return false;
		}

		const std::filesystem::path RelativeToRoot = Path.lexically_relative(Root);
		if (RelativeToRoot.empty() || HasParentDirectoryReference(RelativeToRoot))
		{
			return false;
		}

		OutPath = FPaths::Normalize(FPaths::ToUtf8(RelativeToRoot.generic_wstring()));
		return true;
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
	ImGui::SetNextWindowSize(ImVec2(1120.0f, 760.0f), ImGuiCond_Once);
	if (!ImGui::Begin("Runtime UI Preview"))
	{
		ImGui::End();
		return;
	}

	DrawToolbar();
	ImGui::Separator();

	if (ImGui::BeginTable(
		"##RmlRuntimeUIPreviewLayout",
		2,
		ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp))
	{
		ImGui::TableSetupColumn("Preview", ImGuiTableColumnFlags_WidthStretch, 1.0f);
		ImGui::TableSetupColumn("Info", ImGuiTableColumnFlags_WidthFixed, 320.0f);
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		DrawPreviewSurface(DeltaTime);
		ImGui::TableSetColumnIndex(1);
		DrawDocumentInfo();
		DrawActionEvents();
		DrawAuthoringGuidance();
		ImGui::EndTable();
	}
	ImGui::End();
}

void FEditorRuntimeUIPreviewWidget::DrawToolbar()
{
	ImGui::BeginChild("##RmlRuntimeUIPreviewToolbar", ImVec2(0.0f, 82.0f), false, ImGuiWindowFlags_NoScrollbar);
	const float ButtonWidth = 72.0f;
	const float PathWidth = std::max(180.0f, ImGui::GetContentRegionAvail().x - (ButtonWidth * 3.0f) - (ImGui::GetStyle().ItemSpacing.x * 4.0f));
	ImGui::SetNextItemWidth(PathWidth);
	if (ImGui::InputText("RML", PreviewDocumentPathBuffer, IM_ARRAYSIZE(PreviewDocumentPathBuffer), ImGuiInputTextFlags_EnterReturnsTrue))
	{
		RefreshPreviewDocument();
	}
	AcceptRmlDragDropTarget();

	ImGui::SameLine();
	if (ImGui::Button("Load", ImVec2(ButtonWidth, 0.0f)))
	{
		FString PickedPath;
		if (OpenRmlFileDialog(PickedPath) && SetPreviewDocumentPath(PickedPath))
		{
			RefreshPreviewDocument();
		}
	}

	ImGui::SameLine();
	if (ImGui::Button("Reload", ImVec2(ButtonWidth, 0.0f)))
	{
		RefreshPreviewDocument();
	}

	ImGui::SameLine();
	ImGui::Checkbox("Input", &bEnableInteraction);

	const int32 PresetCount = static_cast<int32>(sizeof(PreviewResolutionPresets) / sizeof(PreviewResolutionPresets[0]));
	const char* CurrentPreset = PreviewResolutionPresets[std::clamp(ResolutionPresetIndex, 0, PresetCount - 1)].Label;
	ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4.0f);
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
	ImGui::SameLine();
	ImGui::Checkbox("Guide", &bShowGuidance);
	ImGui::EndChild();
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

	ImGui::BeginChild("##RmlRuntimeUIPreviewSurface", ImVec2(0.0f, 0.0f), true, ImGuiWindowFlags_NoScrollWithMouse);

	const ImVec2 ChildAvail = ImGui::GetContentRegionAvail();
	const ImVec2 Start(
		ImGui::GetCursorScreenPos().x + std::max(0.0f, (ChildAvail.x - PreviewSize.x) * 0.5f),
		ImGui::GetCursorScreenPos().y + std::max(0.0f, (ChildAvail.y - PreviewSize.y) * 0.5f));
	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	DrawList->AddRectFilled(Start, ImVec2(Start.x + PreviewSize.x, Start.y + PreviewSize.y),
		ImGui::GetColorU32(ImVec4(0.025f, 0.027f, 0.032f, 1.0f)), 6.0f);
	DrawList->AddRect(Start, ImVec2(Start.x + PreviewSize.x, Start.y + PreviewSize.y),
		ImGui::GetColorU32(ImVec4(0.25f, 0.29f, 0.35f, 1.0f)), 6.0f);

	ImGui::SetCursorScreenPos(Start);
	ImGui::InvisibleButton("##RmlRuntimeUIPreviewInputSurface", PreviewSize);
	AcceptRmlDragDropTarget();
	const bool bPreviewHovered = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup);

	FRuntimeUIRenderContext Context;
	Context.RenderMode = ERuntimeUIRenderMode::PIE;
	Context.ViewportMin = FRuntimeUIVector2(Start.x, Start.y);
	Context.ViewportSize = FRuntimeUIVector2(PreviewSize.x, PreviewSize.y);
	Context.LayoutSize = FRuntimeUIVector2(static_cast<float>(TargetWidth), static_cast<float>(TargetHeight));
	Context.DeltaTime = DeltaTime;
	Context.bPreviewDocumentOnly = true;

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
		if (EditorEngine->PumpPIERmlUiInput(PreviewRect, TargetWidth, TargetHeight, true))
		{
			InputSystem::Get().SetGuiMouseCapture(true);
			InputSystem::Get().SetGuiViewportMouseBlock(true);
		}
	}

	const TArray<FString> NewEvents = EditorEngine->PollRmlUIPreviewActionEvents();
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

void FEditorRuntimeUIPreviewWidget::DrawDocumentInfo() const
{
	int32 TargetWidth = 1920;
	int32 TargetHeight = 1080;
	GetPreviewResolution(ResolutionPresetIndex, CustomWidth, CustomHeight, TargetWidth, TargetHeight);

	ImGui::Text("Preview");
	ImGui::Separator();
	ImGui::TextDisabled("Document");
	ImGui::TextWrapped("%s", PreviewDocumentPathBuffer);
	ImGui::TextDisabled("Screen Id");
	ImGui::TextWrapped("%s", PreviewScreenIdBuffer);
	ImGui::TextDisabled("Layout");
	ImGui::Text("%d x %d", TargetWidth, TargetHeight);
	ImGui::TextDisabled("Status");
	ImGui::TextUnformatted(bPreviewDocumentLoaded ? "Loaded" : "Not loaded");
	ImGui::Spacing();
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
	FString Path;
	if (!NormalizeRmlPath(PreviewDocumentPathBuffer, Path))
	{
		bPreviewDocumentLoaded = false;
		return false;
	}
	strncpy_s(PreviewDocumentPathBuffer, Path.c_str(), _TRUNCATE);
	bPreviewDocumentLoaded = EditorEngine->LoadRmlUIDocument(ScreenId, Path);
	if (bPreviewDocumentLoaded)
	{
		EditorEngine->ShowRmlUIScreen(ScreenId);
	}
	return bPreviewDocumentLoaded;
}

bool FEditorRuntimeUIPreviewWidget::OpenRmlFileDialog(FString& OutPath) const
{
	OutPath.clear();

	WCHAR FileBuffer[MAX_PATH] = {};
	OPENFILENAMEW DialogDesc = {};
	DialogDesc.lStructSize = sizeof(DialogDesc);
	DialogDesc.hwndOwner = ImGui::GetMainViewport()
		? static_cast<HWND>(ImGui::GetMainViewport()->PlatformHandleRaw)
		: nullptr;
	DialogDesc.lpstrFilter = L"RML Files (*.rml)\0*.rml\0All Files (*.*)\0*.*\0";
	DialogDesc.lpstrFile = FileBuffer;
	DialogDesc.nMaxFile = MAX_PATH;
	DialogDesc.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

	const std::filesystem::path InitialDir = (std::filesystem::path(FPaths::RootDir()) / L"Asset" / L"UI").lexically_normal();
	const std::wstring InitialDirText = InitialDir.wstring();
	DialogDesc.lpstrInitialDir = InitialDirText.c_str();

	const std::filesystem::path PrevCwd = std::filesystem::current_path();
	const BOOL bPicked = GetOpenFileNameW(&DialogDesc);
	std::error_code RestoreEc;
	std::filesystem::current_path(PrevCwd, RestoreEc);
	if (!bPicked)
	{
		return false;
	}

	return NormalizeRmlPath(FPaths::ToUtf8(std::wstring(FileBuffer)), OutPath);
}

bool FEditorRuntimeUIPreviewWidget::SetPreviewDocumentPath(const FString& Path)
{
	FString NormalizedPath;
	if (!NormalizeRmlPath(Path, NormalizedPath))
	{
		return false;
	}

	strncpy_s(PreviewDocumentPathBuffer, NormalizedPath.c_str(), _TRUNCATE);
	return true;
}

bool FEditorRuntimeUIPreviewWidget::AcceptRmlDragDropTarget()
{
	if (!ImGui::BeginDragDropTarget())
	{
		return false;
	}

	bool bAccepted = false;
	const ImGuiPayload* Payload = ImGui::AcceptDragDropPayload("RMLContentItem");
	if (!Payload)
	{
		Payload = ImGui::AcceptDragDropPayload("ContentBrowserPath");
	}
	if (Payload && Payload->Data && Payload->DataSize > 0)
	{
		const FString Path(static_cast<const char*>(Payload->Data));
		if (SetPreviewDocumentPath(Path))
		{
			RefreshPreviewDocument();
			bAccepted = true;
		}
	}

	ImGui::EndDragDropTarget();
	return bAccepted;
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
