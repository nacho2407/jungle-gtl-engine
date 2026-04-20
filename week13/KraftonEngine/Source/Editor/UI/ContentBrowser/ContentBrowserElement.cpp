#include "ContentBrowserElement.h"
#include "Platform/Paths.h"

bool ContentBrowserElement::RenderSelectSpace(ContentBrowserContext& Context)
{
	FString Name = FPaths::ToUtf8(ContentItem.Name);
	ImGui::PushID(Name.c_str());

	bIsSelected = Context.SelectedElement == this;

	bool bIsClicked = ImGui::Selectable("##Element", bIsSelected, 0, Context.ContentSize);

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

	return bIsClicked;
}

void ContentBrowserElement::Render(ContentBrowserContext& Context)
{
	if (RenderSelectSpace(Context))
	{
		Context.SelectedElement = this;
		bIsSelected = true;
		OnRightClicked(Context);
	}

	bool bDoubleClicked = ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
	if (bDoubleClicked)
	{
		OnDoubleRightClicked(Context);
	}

	if (ImGui::BeginDragDropSource())
	{
		RenderSelectSpace(Context);
		ImGui::SetDragDropPayload(GetDragItemType(), &ContentItem, sizeof(ContentItem));
		OnDrag(Context);
		ImGui::EndDragDropSource();
	}
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

void DirectoryElement::OnDoubleRightClicked(ContentBrowserContext& Context)
{
	Context.CurrentPath = ContentItem.Path;
	Context.bIsNeedRefresh = true;
}

#include "Serialization/SceneSaveManager.h"
#include "Editor/EditorEngine.h"
#include "Editor/Viewport/LevelEditorViewportClient.h"
void SceneElement::OnDoubleRightClicked(ContentBrowserContext& Context)
{
	std::filesystem::path ScenePath = ContentItem.Path;
	FString FilePath = FPaths::ToUtf8(ScenePath.wstring());
	UEditorEngine* EditorEngine = Context.EditorEngine;

	EditorEngine->ClearScene();
	FWorldContext LoadCtx;
	FPerspectiveCameraData CamData;
	FSceneSaveManager::LoadSceneFromJSON(FilePath, LoadCtx, CamData);
	if (LoadCtx.World)
	{
		EditorEngine->GetWorldList().push_back(LoadCtx);
		EditorEngine->SetActiveWorld(LoadCtx.ContextHandle);
		EditorEngine->GetSelectionManager().SetWorld(LoadCtx.World);
		LoadCtx.World->WarmupPickingData(); // 씬 로드 후 메시 BVH와 월드 primitive BVH를 모두 빌드
	}
	EditorEngine->ResetViewport();

	// ResetViewport()가 카메라를 기본값으로 초기화하므로 그 이후에 복원
	if (CamData.bValid)
	{
		for (FLevelEditorViewportClient* VC : EditorEngine->GetLevelViewportClients())
		{
			if (VC->GetRenderOptions().ViewportType == ELevelViewportType::Perspective || VC->GetRenderOptions().ViewportType == ELevelViewportType::FreeOrthographic)
			{
				if (UCameraComponent* Cam = VC->GetCamera())
				{
					Cam->SetWorldLocation(CamData.Location);
					Cam->SetRelativeRotation(CamData.Rotation);
					FCameraState CS = Cam->GetCameraState();
					CS.FOV = CamData.FOV;
					CS.NearZ = CamData.NearClip;
					CS.FarZ = CamData.FarClip;
					Cam->SetCameraState(CS);
				}
				break;
			}
		}
	}
}
