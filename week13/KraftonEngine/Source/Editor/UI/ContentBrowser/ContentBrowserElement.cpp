#include "ContentBrowserElement.h"
#include "Platform/Paths.h"

void ContentBrowserElement::Render(ContentBrowserContext& Context)
{
	FString Name = FPaths::ToUtf8(ContentItem.Name);
	ImGui::PushID(Name.c_str());

	bIsSelected = Context.SelectedElement == this;

	if (ImGui::Selectable("##Element", bIsSelected, 0, Context.ContentSize))
	{
		Context.SelectedElement = this;
		bIsSelected = true;
	}


	ImVec2 Min = ImGui::GetItemRectMin();
	ImVec2 Max = ImGui::GetItemRectMax();
	ImDrawList* DrawList = ImGui::GetWindowDrawList();

	ImFont* font = ImGui::GetFont();
	float fontSize = ImGui::GetFontSize();
	Max.y -= fontSize;
	Max.x -= fontSize * 0.5f;
	Min.x += fontSize * 0.5f;
	DrawList->AddImage(Icon, Min, Max);

	ImVec2 TextPos(Min.x, Max.y);
	FString Text = EllipsisText(FPaths::ToUtf8(ContentItem.Name), Context.ContentSize.x);
	DrawList->AddText(TextPos, ImGui::GetColorU32(ImGuiCol_Text), Text.c_str());
	ImGui::PopID();
}

FString ContentBrowserElement::EllipsisText(const FString& text, float maxWidth)
{
	ImFont* font = ImGui::GetFont();
	float fontSize = ImGui::GetFontSize();

	if (font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, text.c_str()).x <= maxWidth)
		return text;

	const char* ellipsis = "...";
	float ellipsisWidth = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, ellipsis).x;

	std::string result = text;

	while (!result.empty())
	{
		result.pop_back();

		float w = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, result.c_str()).x;
		if (w + ellipsisWidth <= maxWidth)
		{
			result += ellipsis;
			break;
		}
	}

	return result;
}

void DirectoryElement::Render(ContentBrowserContext& Context)
{
	ContentBrowserElement::Render(Context);
	bool bDoubleClicked = ImGui::IsItemHovered() &&	ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
	if (bDoubleClicked)
	{
		Context.CurrentPath = ContentItem.Path;
		Context.bIsNeedRefresh = true;
	}
}
