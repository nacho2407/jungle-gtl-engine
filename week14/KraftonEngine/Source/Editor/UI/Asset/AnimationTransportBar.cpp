#include "AnimationTransportBar.h"

#include "Animation/AnimSingleNodeInstance.h"
#include "Component/SkeletalMeshComponent.h"
#include "Editor/UI/EditorTextureManager.h"
#include "Platform/Paths.h"

#include <imgui.h>
#include <algorithm>
#include <cmath>

// Paths.h가 끌어오는 Windows.h는 GetCurrentTime을 GetTickCount로 치환한다.
#ifdef GetCurrentTime
#undef GetCurrentTime
#endif

namespace
{
	constexpr float IconExtent  = 20.0f;
	constexpr float ButtonGap   = 2.0f;

	ID3D11ShaderResourceView* LoadToolIcon(const wchar_t* FileName)
	{
		const FString Path = FPaths::ToUtf8(FPaths::Combine(FPaths::AssetDir(), L"Editor/ToolIcons/", FileName));
		return FEditorTextureManager::Get().GetOrLoadIcon(Path);
	}

	// 아이콘 트랜스포트 버튼. bActive면 언리얼처럼 파란 하이라이트 배경을 깐다.
	bool IconButton(const char* Id, const wchar_t* FileName, const char* Fallback, bool bActive)
	{
		ImGui::PushStyleColor(ImGuiCol_Button, bActive
			? ImVec4(0.20f, 0.45f, 0.85f, 1.0f) : ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 1.0f, 1.0f, 0.15f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1.0f, 1.0f, 1.0f, 0.30f));

		bool bClicked;
		if (ID3D11ShaderResourceView* Icon = LoadToolIcon(FileName))
		{
			bClicked = ImGui::ImageButton(Id, reinterpret_cast<ImTextureID>(Icon), ImVec2(IconExtent, IconExtent));
		}
		else
		{
			bClicked = ImGui::Button(Fallback, ImVec2(IconExtent + 8.0f, IconExtent + 8.0f));
		}

		ImGui::PopStyleColor(3);
		return bClicked;
	}
}

void FAnimationTransportBar::Render(UAnimSingleNodeInstance* NodeInst,
                                    USkeletalMeshComponent* Comp,
                                    float TotalLength,
                                    int TotalFrames)
{
	const float CurrentTime = NodeInst ? NodeInst->GetCurrentTime() : 0.0f;
	const bool  bIsPlaying  = NodeInst && NodeInst->IsPlaying();
	const bool  bIsLooping  = NodeInst ? NodeInst->IsLooping() : true;
	const float PlayRate    = NodeInst ? NodeInst->GetPlayRate() : 1.0f;
	const bool  bReverse    = PlayRate < 0.0f;
	const float RateMag     = std::max(std::fabs(PlayRate), 0.01f);

	const float FrameDelta = (TotalFrames > 1) ? (TotalLength / static_cast<float>(TotalFrames - 1)) : 0.0f;

	ImGui::AlignTextToFramePadding();

	// 처음으로
	if (IconButton("##GoToFront", L"Go_To_Front_24x.png", "|<", false) && NodeInst)
	{
		NodeInst->SetCurrentTime(0.0f);
	}
	ImGui::SameLine(0.0f, ButtonGap);

	// 이전 프레임
	if (IconButton("##StepBack", L"Step_Backwards_24x.png", "<|", false) && NodeInst)
	{
		NodeInst->SetCurrentTime(std::max(CurrentTime - FrameDelta, 0.0f));
	}
	ImGui::SameLine(0.0f, ButtonGap);

	// 역재생 (PlayRate 음수). 이미 역재생 중이면 일시정지.
	if (IconButton("##PlayReverse", L"Backwards_24x.png", "<", bIsPlaying && bReverse) && Comp)
	{
		if (bIsPlaying && bReverse)
		{
			Comp->SetPlaying(false);
		}
		else
		{
			Comp->SetPlayRate(-RateMag);
			Comp->SetPlaying(true);
		}
	}
	ImGui::SameLine(0.0f, ButtonGap);

	// 정방향 재생 / 일시정지
	const bool bForwardPlaying = bIsPlaying && !bReverse;
	if (IconButton("##PlayPause",
		bForwardPlaying ? L"Pause_24x.png" : L"Play_24x.png",
		bForwardPlaying ? "||" : ">", false) && Comp)
	{
		if (bForwardPlaying)
		{
			Comp->SetPlaying(false);
		}
		else
		{
			Comp->SetPlayRate(RateMag);
			Comp->SetPlaying(true);
		}
	}
	ImGui::SameLine(0.0f, ButtonGap);

	// 다음 프레임
	if (IconButton("##StepFwd", L"Step_Forward_24x.png", "|>", false) && NodeInst)
	{
		NodeInst->SetCurrentTime(std::min(CurrentTime + FrameDelta, TotalLength));
	}
	ImGui::SameLine(0.0f, ButtonGap);

	// 끝으로
	if (IconButton("##GoToEnd", L"Go_To_End_24x.png", ">|", false) && NodeInst)
	{
		NodeInst->SetCurrentTime(TotalLength);
	}
	ImGui::SameLine(0.0f, ButtonGap);

	// 루프 토글 (ON/OFF 아이콘으로 상태 표현)
	if (IconButton("##Loop", bIsLooping ? L"Loop_24x.png" : L"Loop_24x_OFF.png",
		"Loop", false) && Comp)
	{
		Comp->SetLooping(!bIsLooping);
	}
	ImGui::SameLine(0.0f, 12.0f);

	// 현재 시간 / 전체 길이
	ImGui::AlignTextToFramePadding();
	ImGui::Text("%.3f / %.3f s", CurrentTime, TotalLength);
	ImGui::SameLine(0.0f, 12.0f);

	// 재생 속도 (방향 부호는 유지)
	static const float SpeedOptions[] = { 0.1f, 0.25f, 0.5f, 1.0f, 2.0f };
	char SpeedLabel[16];
	snprintf(SpeedLabel, sizeof(SpeedLabel), "x%.2f", static_cast<double>(RateMag));
	ImGui::SetNextItemWidth(72.0f);
	if (ImGui::BeginCombo("##Speed", SpeedLabel, ImGuiComboFlags_HeightSmall))
	{
		for (float Option : SpeedOptions)
		{
			char OptLabel[16];
			snprintf(OptLabel, sizeof(OptLabel), "x%.2f", static_cast<double>(Option));
			if (ImGui::Selectable(OptLabel, std::fabs(RateMag - Option) < 1e-3f) && Comp)
			{
				Comp->SetPlayRate(bReverse ? -Option : Option);
			}
		}
		ImGui::EndCombo();
	}
}
