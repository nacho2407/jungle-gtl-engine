#include "Editor/UI/EditorViewportWidget.h"

#include "Editor/EditorEngine.h"
#include "Editor/Viewport/LevelEditorViewportClient.h"
#include "Viewport/Viewport.h"
#include "ImGui/imgui.h"

void FEditorViewportWidget::SetIndex(int32 InIndex)
{
	Index = InIndex;
	if (Index == 0)
	{
		WindowName = "Viewport";
	}
	else
	{
		WindowName = "Viewport " + std::to_string(Index + 1);
	}
}

void FEditorViewportWidget::Render(float DeltaTime)
{
	(void)DeltaTime;

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
	ImGui::Begin(WindowName.c_str(), nullptr, ImGuiWindowFlags_MenuBar);

	// 메뉴 바에 Split/Merge 토글 버튼
	if (ImGui::BeginMenuBar())
	{
		// 오른쪽 정렬을 위해 커서를 이동
		float ButtonWidth = ImGui::CalcTextSize("Split").x + ImGui::GetStyle().FramePadding.x * 2.0f;
		ImGui::SetCursorPosX(ImGui::GetWindowContentRegionMax().x - ButtonWidth);

		bool bIsSplit = EditorEngine ? EditorEngine->IsSplitViewport() : false;
		const char* ButtonLabel = bIsSplit ? "Merge" : "Split";

		if (ImGui::Button(ButtonLabel))
		{
			if (EditorEngine)
			{
				EditorEngine->ToggleViewportSplit();
			}
		}
		ImGui::EndMenuBar();
	}

	// 뷰포트 패널 위에서 마우스 클릭 시 활성 뷰포트 전환
	if (ViewportClient && EditorEngine)
	{
		if (ImGui::IsWindowHovered() && (ImGui::IsMouseClicked(0) || ImGui::IsMouseClicked(1)))
		{
			if (EditorEngine->GetActiveViewport() != ViewportClient)
			{
				EditorEngine->SetActiveViewport(ViewportClient);
			}
		}
	}

	ImVec2 Size = ImGui::GetContentRegionAvail();

	if (ViewportClient)
	{
		FViewport* VP = ViewportClient->GetViewport();
		if (VP && VP->GetSRV())
		{
			// 패널 리사이즈 감지 → 지연 요청 (다음 프레임 RenderViewport 직전에 적용)
			if (Size.x > 0 && Size.y > 0)
			{
				uint32 NewWidth = static_cast<uint32>(Size.x);
				uint32 NewHeight = static_cast<uint32>(Size.y);

				if (NewWidth != VP->GetWidth() || NewHeight != VP->GetHeight())
				{
					VP->RequestResize(NewWidth, NewHeight);
				}
			}

			// 현재 RT의 SRV를 표시 (리사이즈는 다음 프레임에 적용됨)
			ImGui::Image(
				(ImTextureID)VP->GetSRV(),
				Size
			);
		}
	}

	ImGui::End();
	ImGui::PopStyleVar();
}
