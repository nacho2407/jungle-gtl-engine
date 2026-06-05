#include "Editor/UI/Asset/UI/UIEditorWidget.h"

#include "Core/Logging/Notification.h"
#include "Editor/UI/Asset/UI/UIEditorSerializer.h"
#include "Platform/Paths.h"
#include "UI/UIManager.h"
#include "UI/UserWidget.h"

#include "ImGui/imgui.h"

#include <shellapi.h>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>

namespace
{
	void CopyToBuffer(char* Buffer, size_t BufferSize, const FString& Value)
	{
		if (!Buffer || BufferSize == 0)
		{
			return;
		}
		std::snprintf(Buffer, BufferSize, "%s", Value.c_str());
	}

	bool InputFString(const char* Label, FString& Value, size_t BufferSize = 256)
	{
		TArray<char> Buffer;
		Buffer.resize(BufferSize);
		CopyToBuffer(Buffer.data(), Buffer.size(), Value);
		if (ImGui::InputText(Label, Buffer.data(), Buffer.size()))
		{
			Value = Buffer.data();
			return true;
		}
		return false;
	}

	bool InputFStringMultiline(const char* Label, FString& Value, const ImVec2& Size, size_t BufferSize = 4096)
	{
		TArray<char> Buffer;
		Buffer.resize(BufferSize);
		CopyToBuffer(Buffer.data(), Buffer.size(), Value);
		if (ImGui::InputTextMultiline(Label, Buffer.data(), Buffer.size(), Size))
		{
			Value = Buffer.data();
			return true;
		}
		return false;
	}

	FString MakeUniqueTextId(const FUIEditorDocument& Document)
	{
		for (int32 Index = 1; Index < 10000; ++Index)
		{
			char Buffer[64];
			std::snprintf(Buffer, sizeof(Buffer), "text_%d", Index);
			const FString Candidate = Buffer;

			bool bExists = false;
			for (const FUIEditorTextElement& Element : Document.TextElements)
			{
				if (Element.Id == Candidate)
				{
					bExists = true;
					break;
				}
			}

			if (!bExists)
			{
				return Candidate;
			}
		}

		return "text";
	}

	bool SyncDraftFromNewerSource(const FUIEditorDocument& Document, FString* OutError)
	{
		if (Document.SourcePath.empty() || Document.DraftPath.empty())
		{
			return true;
		}

		try
		{
			if (!std::filesystem::exists(Document.SourcePath))
			{
				return true;
			}

			bool bShouldCopySource = !std::filesystem::exists(Document.DraftPath);
			if (!bShouldCopySource)
			{
				bShouldCopySource = std::filesystem::last_write_time(Document.SourcePath) > std::filesystem::last_write_time(Document.DraftPath);
			}

			if (bShouldCopySource)
			{
				std::filesystem::copy_file(Document.SourcePath, Document.DraftPath, std::filesystem::copy_options::overwrite_existing);
			}
		}
		catch (const std::filesystem::filesystem_error& Error)
		{
			if (OutError)
			{
				*OutError = "Failed to sync RML draft: " + FString(Error.what());
			}
			return false;
		}

		return true;
	}

	void RestoreSelectionById(const FString& SelectedId, FUIEditorDocument& Document, int32& SelectedElementIndex)
	{
		SelectedElementIndex = Document.TextElements.empty() ? -1 : 0;
		if (SelectedId.empty())
		{
			return;
		}

		for (int32 Index = 0; Index < static_cast<int32>(Document.TextElements.size()); ++Index)
		{
			if (Document.TextElements[Index].Id == SelectedId)
			{
				SelectedElementIndex = Index;
				return;
			}
		}
	}
}

void FUIEditorWidget::Open(const std::filesystem::path& InPath)
{
	HidePreview();
	Document = FUIEditorDocument {};
	SelectedElementIndex = -1;
	bDirty = false;
	AutoRefreshFrameCounter = 0;
	StatusText.clear();

	FString Error;
	if (!FUIEditorSerializer::Load(InPath, Document, &Error, true))
	{
		StatusText = "Load failed: " + Error;
		FNotificationManager::Get().AddNotification(StatusText, ENotificationType::Error, 4.0f);
		return;
	}

	SelectedElementIndex = Document.TextElements.empty() ? -1 : 0;
	bDirty = false;
	AutoRefreshFrameCounter = 0;
	bOpen = true;
	StatusText = "Loaded";
}

void FUIEditorWidget::Close()
{
	HidePreview();
	bOpen = false;
	bDirty = false;
	AutoRefreshFrameCounter = 0;
	SelectedElementIndex = -1;
	Document = FUIEditorDocument {};
	StatusText.clear();
}

void FUIEditorWidget::Render(float DeltaTime)
{
	(void)DeltaTime;

	if (!bOpen)
	{
		return;
	}

	const FString Title = "RML UI Editor - " + FPaths::ToUtf8(Document.SourcePath.filename().wstring());
	ImGui::SetNextWindowSize(ImVec2(820.0f, 560.0f), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin(Title.c_str(), &bOpen))
	{
		ImGui::End();
		if (!bOpen)
		{
			Close();
		}
		return;
	}

	RenderToolbar();
	ImGui::Separator();

	if (ImGui::BeginTable("##RmlUIEditorLayout", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV))
	{
		ImGui::TableSetupColumn("Elements", ImGuiTableColumnFlags_WidthFixed, 220.0f);
		ImGui::TableSetupColumn("Inspector", ImGuiTableColumnFlags_WidthStretch);

		ImGui::TableNextColumn();
		RenderElementList();

		ImGui::TableNextColumn();
		RenderInspector();

		ImGui::EndTable();
	}

	ImGui::Separator();
	RenderPreviewControls();
	RenderStatusBar();
	TickAutoRefresh();

	ImGui::End();

	if (!bOpen)
	{
		Close();
	}
}

void FUIEditorWidget::RenderToolbar()
{
	if (ImGui::Button("Add Text"))
	{
		AddTextElement();
	}

	ImGui::SameLine();
	if (ImGui::Button("Save"))
	{
		Save(true);
	}

	ImGui::SameLine();
	if (ImGui::Button("Refresh"))
	{
		RefreshPreview(true);
	}

	ImGui::SameLine();
	if (ImGui::Button("Open External"))
	{
		ShellExecuteW(nullptr, L"open", Document.DraftPath.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
	}

	ImGui::SameLine();
	ImGui::TextDisabled("%s", bDirty ? "Dirty" : "Saved");
}

void FUIEditorWidget::RenderElementList()
{
	ImGui::TextUnformatted("Elements");
	ImGui::Separator();

	ImGui::BeginChild("##RmlUIEditorElementsScroll", ImVec2(0.0f, 0.0f), true, ImGuiWindowFlags_AlwaysVerticalScrollbar);
	if (Document.TextElements.empty())
	{
		ImGui::TextDisabled("No editable text elements.");
		ImGui::EndChild();
		return;
	}

	for (int32 Index = 0; Index < static_cast<int32>(Document.TextElements.size()); ++Index)
	{
		const FUIEditorTextElement& Element = Document.TextElements[Index];
		const FString Label = Element.Id.empty() ? FString("(empty id)") : Element.Id;
		if (ImGui::Selectable(Label.c_str(), SelectedElementIndex == Index))
		{
			SelectedElementIndex = Index;
		}
	}
	ImGui::EndChild();
}

void FUIEditorWidget::RenderInspector()
{
	ImGui::TextUnformatted("Inspector");
	ImGui::Separator();

	if (SelectedElementIndex < 0 || SelectedElementIndex >= static_cast<int32>(Document.TextElements.size()))
	{
		ImGui::TextDisabled("No element selected.");
		return;
	}

	FUIEditorTextElement& Element = Document.TextElements[SelectedElementIndex];

	if (ImGui::Button(Element.bCanEditText ? "Delete Text Element" : "Delete Element"))
	{
		DeleteSelectedTextElement();
		return;
	}

	ImGui::Separator();

	ImGui::BeginDisabled();
	InputFString("Id", Element.Id);
	ImGui::EndDisabled();

	if (!Element.ClassName.empty())
	{
		ImGui::BeginDisabled();
		InputFString("Class", Element.ClassName);
		ImGui::EndDisabled();
	}

	ImGui::BeginDisabled(!Element.bCanEditText);
	const bool bTextChanged = InputFStringMultiline("Text", Element.Text, ImVec2(-1.0f, 86.0f));
	ImGui::EndDisabled();
	if (bTextChanged)
	{
		Element.bTextDirty = true;
		MarkDirty();
	}

	const char* LayoutLabels[] = { "px", "%" };
	int LayoutMode = Element.bUsePercentLayout ? 1 : 0;
	if (ImGui::Combo("Layout Unit", &LayoutMode, LayoutLabels, IM_ARRAYSIZE(LayoutLabels)))
	{
		Element.bUsePercentLayout = LayoutMode == 1;
		if (Element.bUsePercentLayout)
		{
			Element.X = std::clamp(Element.X, 0.0f, 100.0f);
			Element.Y = std::clamp(Element.Y, 0.0f, 100.0f);
			Element.Width = std::clamp(Element.Width, 1.0f, 100.0f);
			Element.Height = std::clamp(Element.Height, 1.0f, 100.0f);
		}
		Element.bXDirty = true;
		Element.bYDirty = true;
		Element.bWidthDirty = true;
		Element.bHeightDirty = true;
		Element.bStyleDirty = true;
		MarkDirty();
	}

	const float PositionSpeed = Element.bUsePercentLayout ? 0.1f : 1.0f;
	const float SizeSpeed = Element.bUsePercentLayout ? 0.1f : 1.0f;
	const float PositionMax = Element.bUsePercentLayout ? 100.0f : 4096.0f;
	const float SizeMax = Element.bUsePercentLayout ? 100.0f : 4096.0f;
	const char* ValueFormat = Element.bUsePercentLayout ? "%.2f" : "%.1f";

	if (ImGui::DragFloat("X", &Element.X, PositionSpeed, 0.0f, PositionMax, ValueFormat))
	{
		Element.bXDirty = true;
		Element.bStyleDirty = true;
		MarkDirty();
	}
	if (ImGui::DragFloat("Y", &Element.Y, PositionSpeed, 0.0f, PositionMax, ValueFormat))
	{
		Element.bYDirty = true;
		Element.bStyleDirty = true;
		MarkDirty();
	}
	if (ImGui::DragFloat("Width", &Element.Width, SizeSpeed, 1.0f, SizeMax, ValueFormat))
	{
		Element.bWidthDirty = true;
		Element.bStyleDirty = true;
		MarkDirty();
	}
	if (ImGui::DragFloat("Height", &Element.Height, SizeSpeed, 1.0f, SizeMax, ValueFormat))
	{
		Element.bHeightDirty = true;
		Element.bStyleDirty = true;
		MarkDirty();
	}
	ImGui::BeginDisabled(!Element.bCanEditText);
	if (ImGui::DragFloat("Font Size", &Element.FontSize, 1.0f, 1.0f, 200.0f))
	{
		Element.bFontSizeDirty = true;
		Element.bStyleDirty = true;
		MarkDirty();
	}

	const char* WeightLabels[] = { "normal", "bold", "black" };
	const char* WeightValues[] = { "normal", "bold", "900" };
	int CurrentWeight = 0;
	if (Element.FontWeight == "bold")
	{
		CurrentWeight = 1;
	}
	else if (Element.FontWeight == "900" || Element.FontWeight == "black")
	{
		CurrentWeight = 2;
	}

	if (ImGui::Combo("Font Weight", &CurrentWeight, WeightLabels, IM_ARRAYSIZE(WeightLabels)))
	{
		Element.FontWeight = WeightValues[CurrentWeight];
		Element.bFontWeightDirty = true;
		Element.bStyleDirty = true;
		MarkDirty();
	}

	const char* AlignLabels[] = { "left", "center", "right" };
	const char* AlignValues[] = { "left", "center", "right" };
	int CurrentAlign = 0;
	if (Element.TextAlign == "center")
	{
		CurrentAlign = 1;
	}
	else if (Element.TextAlign == "right")
	{
		CurrentAlign = 2;
	}

	if (ImGui::Combo("Text Align", &CurrentAlign, AlignLabels, IM_ARRAYSIZE(AlignLabels)))
	{
		Element.TextAlign = AlignValues[CurrentAlign];
		Element.bTextAlignDirty = true;
		Element.bStyleDirty = true;
		MarkDirty();
	}

	if (ImGui::ColorEdit4("Color", Element.Color, ImGuiColorEditFlags_AlphaPreviewHalf))
	{
		Element.bColorDirty = true;
		Element.bStyleDirty = true;
		MarkDirty();
	}
	ImGui::EndDisabled();
}

void FUIEditorWidget::RenderPreviewControls()
{
	if (PreviewWidget && PreviewWidget->IsInViewport())
	{
		if (ImGui::Button("Hide Preview"))
		{
			HidePreview();
		}
	}
	else
	{
		if (ImGui::Button("Show Preview"))
		{
			ShowPreview();
		}
	}

	ImGui::SameLine();
	ImGui::TextDisabled("Preview uses the game viewport.");

	ImGui::SameLine();
	ImGui::Checkbox("Auto Refresh", &bAutoRefresh);

	ImGui::SameLine();
	ImGui::SetNextItemWidth(96.0f);
	if (ImGui::DragInt("Frames", &AutoRefreshIntervalFrames, 1.0f, 1, 600))
	{
		if (AutoRefreshIntervalFrames < 1)
		{
			AutoRefreshIntervalFrames = 1;
		}
		AutoRefreshFrameCounter = 0;
	}
}

void FUIEditorWidget::RenderStatusBar()
{
	const FString PathText = FPaths::ToUtf8(Document.SourcePath.wstring());
	const FString DraftPathText = FPaths::ToUtf8(Document.DraftPath.wstring());
	ImGui::TextDisabled("Source: %s", PathText.c_str());
	ImGui::TextDisabled("Draft: %s", DraftPathText.c_str());
	if (!StatusText.empty())
	{
		ImGui::TextDisabled("Status: %s", StatusText.c_str());
	}
}

void FUIEditorWidget::AddTextElement()
{
	FUIEditorTextElement Element;
	Element.TagName = "div";
	Element.Id = MakeUniqueTextId(Document);
	Element.Text = "New Text";
	Element.X = 120.0f;
	Element.Y = 80.0f + static_cast<float>(Document.TextElements.size()) * 44.0f;
	Element.bTextDirty = true;
	Element.bStyleDirty = true;
	Element.bPendingInsert = true;

	Document.TextElements.push_back(std::move(Element));
	SelectedElementIndex = static_cast<int32>(Document.TextElements.size()) - 1;
	MarkDirty();
	StatusText = "Text element added.";
}

void FUIEditorWidget::DeleteSelectedTextElement()
{
	if (SelectedElementIndex < 0 || SelectedElementIndex >= static_cast<int32>(Document.TextElements.size()))
	{
		return;
	}

	const FUIEditorTextElement& Element = Document.TextElements[SelectedElementIndex];
	const FString DeletedId = Element.Id;
	if (!Element.bPendingInsert && Element.ElementRange.IsValid())
	{
		Document.DeletedElementRanges.push_back(Element.ElementRange);
	}

	Document.TextElements.erase(Document.TextElements.begin() + SelectedElementIndex);
	if (Document.TextElements.empty())
	{
		SelectedElementIndex = -1;
	}
	else if (SelectedElementIndex >= static_cast<int32>(Document.TextElements.size()))
	{
		SelectedElementIndex = static_cast<int32>(Document.TextElements.size()) - 1;
	}

	MarkDirty();
	StatusText = "Text element deleted: " + DeletedId;
}

bool FUIEditorWidget::Save(bool bShowNotification)
{
	FString Error;
	if (!Validate(&Error))
	{
		StatusText = "Save failed: " + Error;
		if (bShowNotification)
		{
			FNotificationManager::Get().AddNotification(StatusText, ENotificationType::Error, 4.0f);
		}
		return false;
	}

	if (!FUIEditorSerializer::Commit(Document, &Error))
	{
		StatusText = "Save failed: " + Error;
		if (bShowNotification)
		{
			FNotificationManager::Get().AddNotification(StatusText, ENotificationType::Error, 4.0f);
		}
		return false;
	}

	Document.bDirty = false;
	bDirty = false;
	StatusText = "Saved";
	if (bShowNotification)
	{
		FNotificationManager::Get().AddNotification("RML saved to source.", ENotificationType::Success, 2.0f);
	}
	return true;
}

void FUIEditorWidget::RefreshPreview(bool bShowNotification)
{
	FString Error;
	bool bReloadedEditorDocument = false;
	if (bDirty)
	{
		if (!Validate(&Error))
		{
			StatusText = "Refresh failed: " + Error;
			if (bShowNotification)
			{
				FNotificationManager::Get().AddNotification(StatusText, ENotificationType::Error, 4.0f);
			}
			return;
		}

		if (!FUIEditorSerializer::SaveDraft(Document, &Error))
		{
			StatusText = "Refresh failed: " + Error;
			if (bShowNotification)
			{
				FNotificationManager::Get().AddNotification(StatusText, ENotificationType::Error, 4.0f);
			}
			return;
		}
	}
	else
	{
		const std::filesystem::path SourcePath = Document.SourcePath;
		const FString SelectedId =
			SelectedElementIndex >= 0 && SelectedElementIndex < static_cast<int32>(Document.TextElements.size())
				? Document.TextElements[SelectedElementIndex].Id
				: FString {};

		if (!SyncDraftFromNewerSource(Document, &Error) || !FUIEditorSerializer::Load(SourcePath, Document, &Error))
		{
			StatusText = "Refresh failed: " + Error;
			if (bShowNotification)
			{
				FNotificationManager::Get().AddNotification(StatusText, ENotificationType::Error, 4.0f);
			}
			return;
		}

		RestoreSelectionById(SelectedId, Document, SelectedElementIndex);
		bReloadedEditorDocument = true;
	}

	if (!Validate(&Error))
	{
		StatusText = "Refresh failed: " + Error;
		if (bShowNotification)
		{
			FNotificationManager::Get().AddNotification(StatusText, ENotificationType::Error, 4.0f);
		}
		return;
	}

	Document.bDirty = false;
	bDirty = false;

	UUIManager::Get().ClearRmlCaches();
	int32 ReloadedCount = UUIManager::Get().ReloadDocumentsByPath(GetDraftPathString());
	const FString SourcePathString = GetDocumentPathString();
	if (SourcePathString != GetDraftPathString())
	{
		ReloadedCount += UUIManager::Get().ReloadDocumentsByPath(SourcePathString);
	}
	if (ReloadedCount > 0)
	{
		char Buffer[96];
		std::snprintf(Buffer, sizeof(Buffer), "%s refreshed: %d widget(s).", bReloadedEditorDocument ? "Disk" : "Draft", ReloadedCount);
		StatusText = Buffer;
		if (bShowNotification)
		{
			FNotificationManager::Get().AddNotification(StatusText, ENotificationType::Success, 2.5f);
		}
		return;
	}

	StatusText = "Draft saved. No matching preview widget to reload.";
	if (bShowNotification)
	{
		FNotificationManager::Get().AddNotification(StatusText, ENotificationType::Info, 3.0f);
	}
}

void FUIEditorWidget::TickAutoRefresh()
{
	if (!bAutoRefresh || !bDirty)
	{
		AutoRefreshFrameCounter = 0;
		return;
	}

	++AutoRefreshFrameCounter;
	if (AutoRefreshFrameCounter < AutoRefreshIntervalFrames)
	{
		return;
	}

	AutoRefreshFrameCounter = 0;
	RefreshPreview(false);
}

void FUIEditorWidget::ShowPreview()
{
	FString Error;
	if (!Validate(&Error))
	{
		StatusText = "Preview failed: " + Error;
		FNotificationManager::Get().AddNotification(StatusText, ENotificationType::Error, 4.0f);
		return;
	}

	if (!FUIEditorSerializer::SaveDraft(Document, &Error))
	{
		StatusText = "Preview failed: " + Error;
		FNotificationManager::Get().AddNotification(StatusText, ENotificationType::Error, 4.0f);
		return;
	}

	Document.bDirty = false;
	bDirty = false;

	if (!PreviewWidget)
	{
		PreviewWidget = UUIManager::Get().CreateWidget(nullptr, GetDraftPathString());
		if (PreviewWidget)
		{
			PreviewWidget->SetWantsMouse(false);
		}
	}

	if (PreviewWidget)
	{
		PreviewWidget->AddToViewport(10000);
		StatusText = "Preview shown.";
	}
}

void FUIEditorWidget::HidePreview()
{
	if (PreviewWidget && PreviewWidget->IsInViewport())
	{
		PreviewWidget->RemoveFromParent();
	}
	PreviewWidget = nullptr;
}

void FUIEditorWidget::MarkDirty()
{
	Document.bDirty = true;
	bDirty = true;
	AutoRefreshFrameCounter = 0;
}

bool FUIEditorWidget::Validate(FString* OutError) const
{
	for (size_t Index = 0; Index < Document.TextElements.size(); ++Index)
	{
		const FUIEditorTextElement& Element = Document.TextElements[Index];
		if (Element.Id.empty())
		{
			if (OutError) *OutError = "Element id cannot be empty.";
			return false;
		}
		if (Element.Width < 1.0f || Element.Height < 1.0f)
		{
			if (OutError) *OutError = "Width and height must be at least 1.";
			return false;
		}

		for (size_t OtherIndex = Index + 1; OtherIndex < Document.TextElements.size(); ++OtherIndex)
		{
			if (Element.Id == Document.TextElements[OtherIndex].Id)
			{
				if (OutError) *OutError = "Element ids must be unique.";
				return false;
			}
		}
	}

	return true;
}

FString FUIEditorWidget::GetDocumentPathString() const
{
	return FPaths::ToUtf8(Document.SourcePath.wstring());
}

FString FUIEditorWidget::GetDraftPathString() const
{
	return FPaths::ToUtf8(Document.DraftPath.wstring());
}
