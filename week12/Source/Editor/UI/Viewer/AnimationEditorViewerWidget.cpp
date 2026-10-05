#include "AnimationEditorViewerWidget.h"

#include "Animation/AnimNotify.h"
#include "Editor/Viewer/AnimationEditorViewer.h"
#include "Editor/Viewport/FSceneViewport.h"
#include "Editor/EditorEngine.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Component/SkeletalMeshComponent.h"
#include "Core/Paths.h"
#include "Core/Reflection/ReflectionRegistry.h"
#include "Core/ResourceManager.h"
#include "Engine/Core/EditorResourcePaths.h"
#include "GameFramework/PrimitiveActors.h"
#include "imgui.h"
#include "Object/Class.h"
#include "WICTextureLoader.h"

#include <algorithm>
#include <cstdio>

namespace
{
	FString GetBaseFileNameWithoutExtension(const FString& Path)
	{
		if (Path.empty())
		{
			return "Viewer";
		}

		const size_t SlashPos = Path.find_last_of("/\\");
		const size_t NameBegin = SlashPos == FString::npos ? 0 : SlashPos + 1;
		FString Name = Path.substr(NameBegin);

		const size_t DotPos = Name.find_last_of('.');
		if (DotPos != FString::npos && DotPos > 0)
		{
			Name = Name.substr(0, DotPos);
		}

		return Name.empty() ? "Viewer" : Name;
	}

	FString GetViewerAssetLabel(FEditorViewer* Viewer)
	{
		return Viewer ? GetBaseFileNameWithoutExtension(Viewer->GetFileName()) : FString("Viewer");
	}

	FAnimationEditorViewer* AsAnimationViewer(FEditorViewer* Viewer)
	{
		return Viewer && Viewer->GetTabKind() == EEditorTabKind::AnimSequenceViewer
			? static_cast<FAnimationEditorViewer*>(Viewer)
			: nullptr;
	}

	const FAnimationEditorViewer* AsAnimationViewer(const FEditorViewer* Viewer)
	{
		return Viewer && Viewer->GetTabKind() == EEditorTabKind::AnimSequenceViewer
			? static_cast<const FAnimationEditorViewer*>(Viewer)
			: nullptr;
	}

	void GetSelectableAnimNotifyClasses(TArray<UClass*>& OutClasses)
	{
		OutClasses.clear();
		FReflectionRegistry::Get().GetClassesDerivedFrom(UAnimNotify::StaticClass(), OutClasses);
		OutClasses.erase(
			std::remove_if(
				OutClasses.begin(),
				OutClasses.end(),
				[](UClass* Class)
				{
					return !Class ||
						Class == UAnimNotify::StaticClass() ||
						Class == UAnimNotifyState::StaticClass() ||
						Class->HasAnyClassFlags(CF_Abstract);
				}),
			OutClasses.end());

		std::sort(
			OutClasses.begin(),
			OutClasses.end(),
			[](const UClass* A, const UClass* B)
			{
				return FString(A->GetDisplayName()) < FString(B->GetDisplayName());
			});
	}

	FString GetAnimNotifyClassDisplayName(const FString& ClassName)
	{
		if (UClass* Class = FReflectionRegistry::Get().FindClass(ClassName))
		{
			return Class->GetDisplayName();
		}
		return ClassName.empty() ? FString("None / Name Only") : ClassName;
	}

	bool DrawAnimNotifyClassCombo(const char* Label, FString& InOutClassName)
	{
		TArray<UClass*> NotifyClasses;
		GetSelectableAnimNotifyClasses(NotifyClasses);

		bool bChanged = false;
		const FString Preview = GetAnimNotifyClassDisplayName(InOutClassName);
		ImGui::SetNextItemWidth(220.0f);
		if (ImGui::BeginCombo(Label, Preview.c_str()))
		{
			const bool bNoneSelected = InOutClassName.empty();
			if (ImGui::Selectable("None / Name Only", bNoneSelected))
			{
				InOutClassName.clear();
				bChanged = true;
			}
			if (bNoneSelected)
			{
				ImGui::SetItemDefaultFocus();
			}

			if (!NotifyClasses.empty())
			{
				ImGui::Separator();
			}

			for (UClass* Class : NotifyClasses)
			{
				const FString ClassName = Class->GetName();
				const bool bSelected = ClassName == InOutClassName;
				const FString ItemLabel = FString(Class->GetDisplayName()) + "##" + ClassName;
				if (ImGui::Selectable(ItemLabel.c_str(), bSelected))
				{
					InOutClassName = ClassName;
					bChanged = true;
				}
				if (bSelected)
				{
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}

		return bChanged;
	}
}

void FAnimationEditorViewerWidget::RenderContent(float DeltaTime)
{
	(void)DeltaTime;

	FAnimationEditorViewer* AnimationViewer = Viewer && Viewer->GetTabKind() == EEditorTabKind::AnimSequenceViewer
		? static_cast<FAnimationEditorViewer*>(Viewer)
		: nullptr;
	if (!AnimationViewer)
	{
		FEditorViewerWidget::RenderContent(DeltaTime);
		return;
	}

	FSceneViewport& SceneViewport = AnimationViewer->GetViewport();
	ID3D11ShaderResourceView* SRV = SceneViewport.GetOutSRV();
	if (!SRV)
	{
		ImGui::TextDisabled("Viewer render target is not ready.");
		return;
	}

	ASkeletalMeshActor* ViewTarget = AnimationViewer->GetViewTarget();
	USkeletalMeshComponent* SkelMeshComp = ViewTarget ? ViewTarget->GetSkeletalMeshComponent() : nullptr;
	USkeletalMesh* SkeletalMesh = SkelMeshComp ? SkelMeshComp->GetSkeletalMesh() : nullptr;

	UAnimSequence* Sequence = AnimationViewer->GetAnimSequence();
	if (CachedAnimSequence != Sequence)
	{
		CachedAnimSequence = Sequence;
		SelectedAnimTrackIndex = -1;
		SelectedAnimNotifyIndex = -1;
		DraggingAnimNotifyIndex = -1;
		SelectedAnimNotifyNameBufferIndex = -1;
		SelectedAnimNotifyNameBuffer[0] = '\0';
		bAnimNotifyDragDirty = false;
	}

	const float TimelineHeight = 350.0f;
	ImVec2 WorkSize = ImGui::GetContentRegionAvail();
	const float ViewAreaHeight = std::max(180.0f, WorkSize.y - TimelineHeight - ImGui::GetStyle().ItemSpacing.y);
	const float DetailsWidth = std::clamp(RightPanelWidth, 350.0f, std::max(350.0f, WorkSize.x * 0.4f));
	const float ViewWidth = std::max(220.0f, WorkSize.x - DetailsWidth - ImGui::GetStyle().ItemSpacing.x);

	RenderViewportPanel(SceneViewport, SRV, ImVec2(ViewWidth, ViewAreaHeight));
	RenderDefaultViewportToolbar();

	ImGui::SameLine();
	ImGui::BeginChild("AnimSequenceDetailsPanel", ImVec2(DetailsWidth, ViewAreaHeight), true);
	RenderAnimSequenceDetails(Sequence, SkeletalMesh);
	ImGui::EndChild();

	const float TimelineListWidth = 350.0f;
	const float SpacingX = ImGui::GetStyle().ItemSpacing.x;
	const float TimelineWidth = std::max(260.0f, WorkSize.x - TimelineListWidth - SpacingX);

	ImGui::BeginChild("AnimSequenceTimelinePanel", ImVec2(TimelineWidth, 0.0f), true);
	RenderAnimSequenceToolbar(Sequence);
	RenderAnimSequenceTimeline(Sequence);
	ImGui::EndChild();

	ImGui::SameLine();

	ImGui::BeginChild("AnimSequenceListPanel", ImVec2(TimelineListWidth, 0.0f), true);
	RenderAnimSequenceList(Sequence);
	ImGui::EndChild();
}

void FAnimationEditorViewerWidget::SyncPreviewMeshPathBuffer()
{
	if (!Viewer)
	{
		PreviewMeshPathBufferSource.clear();
		PreviewMeshPathBuffer[0] = '\0';
		return;
	}

	const FString& Path = AsAnimationViewer(Viewer)->GetPreviewMeshPath();
	if (PreviewMeshPathBufferSource == Path)
	{
		return;
	}

	PreviewMeshPathBufferSource = Path;
	snprintf(PreviewMeshPathBuffer, sizeof(PreviewMeshPathBuffer), "%s", Path.c_str());
}

void FAnimationEditorViewerWidget::LoadAnimSequenceToolbarIcons()
{
	if (bAnimSequenceToolbarIconsLoadAttempted)
	{
		return;
	}

	bAnimSequenceToolbarIconsLoadAttempted = true;
	if (!EditorEngine)
	{
		return;
	}

	ID3D11Device* Device = EditorEngine->GetRenderer().GetFD3DDevice().GetDevice();
	if (!Device)
	{
		return;
	}

	const std::wstring IconDir = FEditorResourcePaths::ToolIconsAbsoluteDir();
	auto LoadIcon = [&](const wchar_t* FileName, TComPtr<ID3D11ShaderResourceView>& OutIcon)
	{
		const std::wstring IconPath = IconDir + FileName;
		DirectX::CreateWICTextureFromFile(Device, IconPath.c_str(), nullptr, OutIcon.GetAddressOf());
	};

	LoadIcon(L"PlayControlsPlayForward.png", AnimSequencePlayIcon);
	LoadIcon(L"PlayControlsPause.png", AnimSequencePauseIcon);
	LoadIcon(L"PlayControlsPlayReverse.png", AnimSequenceReverseIcon);
	LoadIcon(L"PlayControlsToFront.png", AnimSequenceToFrontIcon);
	LoadIcon(L"PlayControlsToEnd.png", AnimSequenceToEndIcon);
	LoadIcon(L"PlayControlsLooping.png", AnimSequenceLoopingIcon);
	LoadIcon(L"PlayControlsNoLooping.png", AnimSequenceNoLoopingIcon);
	LoadIcon(L"PlayControlsToNexting.png", AnimSequenceToNextingIcon);
	LoadIcon(L"PlayControlsToPreviousing.png", AnimSequenceToPreviousingIcon);
}

bool FAnimationEditorViewerWidget::DrawAnimSequenceIconButton(
	const char* Id,
	ID3D11ShaderResourceView* Icon,
	const char* Tooltip,
	const ImVec2& Size)
{
	ImGui::PushID(Id);
	const bool bClicked = ImGui::InvisibleButton("##IconButton", Size);
	const ImVec2 Min = ImGui::GetItemRectMin();
	const ImVec2 Max = ImGui::GetItemRectMax();
	const bool bHovered = ImGui::IsItemHovered();
	const bool bActive = ImGui::IsItemActive();
	ImDrawList* DrawList = ImGui::GetWindowDrawList();

	const ImU32 BgColor = ImGui::GetColorU32(
		bActive ? ImVec4(0.16f, 0.19f, 0.24f, 1.0f) :
		bHovered ? ImVec4(0.13f, 0.16f, 0.20f, 1.0f) :
				   ImVec4(0.10f, 0.12f, 0.15f, 1.0f));
	const ImU32 BorderColor = ImGui::GetColorU32(
		bHovered ? ImVec4(0.38f, 0.45f, 0.56f, 1.0f) : ImVec4(0.22f, 0.26f, 0.32f, 1.0f));
	DrawList->AddRectFilled(Min, Max, BgColor, 5.0f);
	DrawList->AddRect(Min, Max, BorderColor, 5.0f);

	if (Icon)
	{
		const float Padding = std::max(5.0f, Size.x * 0.18f);
		DrawList->AddImage(
			reinterpret_cast<ImTextureID>(Icon),
			ImVec2(Min.x + Padding, Min.y + Padding),
			ImVec2(Max.x - Padding, Max.y - Padding));
	}
	else
	{
		const ImVec2 TextSize = ImGui::CalcTextSize("?");
		DrawList->AddText(
			ImVec2(Min.x + (Size.x - TextSize.x) * 0.5f, Min.y + (Size.y - TextSize.y) * 0.5f),
			ImGui::GetColorU32(ImVec4(0.72f, 0.76f, 0.84f, 1.0f)),
			"?");
	}

	if (bHovered && Tooltip)
	{
		ImGui::SetTooltip("%s", Tooltip);
	}

	ImGui::PopID();
	return bClicked;
}

bool FAnimationEditorViewerWidget::SaveAnimSequenceAsset(UAnimSequence* Sequence)
{
	if (!Sequence)
	{
		return false;
	}

	FString SavePath = Sequence->GetAssetPath();
	if (SavePath.empty() && Viewer)
	{
		SavePath = Viewer->GetFileName();
	}

	if (SavePath.empty())
	{
		return false;
	}

	return FResourceManager::Get().SaveAnimSequence(SavePath, Sequence);
}

void FAnimationEditorViewerWidget::RenderAnimSequenceToolbar(UAnimSequence* Sequence)
{
	constexpr ImGuiWindowFlags ToolbarFlags =
		ImGuiWindowFlags_NoScrollbar |
		ImGuiWindowFlags_NoScrollWithMouse;
	ImGui::BeginChild("AnimSequenceToolbar", ImVec2(0.0f, 56.0f), false, ToolbarFlags);
	ImGui::SetCursorPos(ImVec2(8.0f, 8.0f));

	if (!Sequence || !Viewer)
	{
		ImGui::TextDisabled("Animation sequence is not loaded.");
		ImGui::EndChild();
		return;
	}

	LoadAnimSequenceToolbarIcons();

	const bool bPlaying = AsAnimationViewer(Viewer)->IsAnimationPlaying();
	const float PlayRate = AsAnimationViewer(Viewer)->GetAnimationPlayRate();
	const bool bReversePlaying = bPlaying && PlayRate < 0.0f;
	const bool bForwardPlaying = bPlaying && PlayRate >= 0.0f;
	const ImVec2 ButtonSize(38.0f, 38.0f);

	if (DrawAnimSequenceIconButton("ToFront", AnimSequenceToFrontIcon.Get(), "To Front", ButtonSize))
	{
		AsAnimationViewer(Viewer)->SetAnimationPlaying(false);
		AsAnimationViewer(Viewer)->SetAnimationTime(0.0f);
	}
	ImGui::SameLine();
	if (DrawAnimSequenceIconButton(
		"Reverse",
		bReversePlaying ? AnimSequencePauseIcon.Get() : AnimSequenceReverseIcon.Get(),
		bReversePlaying ? "Pause" : "Reverse",
		ButtonSize))
	{
		if (bReversePlaying)
		{
			AsAnimationViewer(Viewer)->SetAnimationPlaying(false);
		}
		else
		{
			const float ReverseRate = PlayRate == 0.0f ? -1.0f : -std::abs(PlayRate);
			AsAnimationViewer(Viewer)->SetAnimationPlayRate(ReverseRate);
			AsAnimationViewer(Viewer)->SetAnimationPlaying(true);
		}
	}
	ImGui::SameLine();
	if (DrawAnimSequenceIconButton(
		"Play",
		bForwardPlaying ? AnimSequencePauseIcon.Get() : AnimSequencePlayIcon.Get(),
		bForwardPlaying ? "Pause" : "Play",
		ButtonSize))
	{
		if (bForwardPlaying)
		{
			AsAnimationViewer(Viewer)->SetAnimationPlaying(false);
		}
		else
		{
			const float ForwardRate = PlayRate == 0.0f ? 1.0f : std::abs(PlayRate);
			AsAnimationViewer(Viewer)->SetAnimationPlayRate(ForwardRate);
			AsAnimationViewer(Viewer)->SetAnimationPlaying(true);
		}
	}

	ImGui::SameLine();
	if (DrawAnimSequenceIconButton("ToEnd", AnimSequenceToEndIcon.Get(), "To End", ButtonSize))
	{
		AsAnimationViewer(Viewer)->SetAnimationPlaying(false);
		AsAnimationViewer(Viewer)->SetAnimationTime(std::max(0.0f, AsAnimationViewer(Viewer)->GetAnimationLength()));
	}
	ImGui::SameLine(0.0f, 14.0f);
	const bool bLooping = AsAnimationViewer(Viewer)->IsAnimationLooping();
	if (DrawAnimSequenceIconButton(
		"Loop",
		bLooping ? AnimSequenceLoopingIcon.Get() : AnimSequenceNoLoopingIcon.Get(),
		bLooping ? "Looping" : "No Looping",
		ButtonSize))
	{
		AsAnimationViewer(Viewer)->SetAnimationLooping(!bLooping);
	}

	ImGui::SameLine(0.0f, 18.0f);
	const float CurrentTime = AsAnimationViewer(Viewer)->GetAnimationCurrentTime();
	const float Length = std::max(0.0f, AsAnimationViewer(Viewer)->GetAnimationLength());
	ImGui::Text("%.3f / %.3f sec", CurrentTime, Length);

	ImGui::EndChild();
}

void FAnimationEditorViewerWidget::RenderAnimSequenceTimeline(UAnimSequence* Sequence)
{
	if (!Sequence)
	{
		ImGui::SameLine();
		ImGui::TextDisabled("No sequence");
		return;
	}

	const float Length = std::max(0.0f, Sequence->GetPlayLength());
	const UAnimDataModel* DataModel = Sequence->GetDataModel();
	const float FrameRate = DataModel ? DataModel->GetFrameRate().AsDecimal() : 0.0f;
	const int32 FrameCount = DataModel ? DataModel->GetNumberOfFrames() : 0;

	ImVec2 CanvasSize = ImGui::GetContentRegionAvail();
	CanvasSize.y = std::max(CanvasSize.y, 170.0f);
	CanvasSize.x = std::max(CanvasSize.x, 1.0f);
	ImGui::InvisibleButton("##AnimTimelineCanvas", CanvasSize);

	const ImVec2 Min = ImGui::GetItemRectMin();
	const ImVec2 Max = ImGui::GetItemRectMax();
	const bool bHovered = ImGui::IsItemHovered();
	const bool bActive = ImGui::IsItemActive();
	ImDrawList* DrawList = ImGui::GetWindowDrawList();

	const ImU32 BgColor = ImGui::GetColorU32(ImVec4(0.075f, 0.082f, 0.100f, 1.0f));
	const ImU32 TrackColor = ImGui::GetColorU32(ImVec4(0.105f, 0.116f, 0.140f, 1.0f));
	const ImU32 LineColor = ImGui::GetColorU32(ImVec4(0.32f, 0.35f, 0.42f, 1.0f));
	const ImU32 MajorLineColor = ImGui::GetColorU32(ImVec4(0.46f, 0.50f, 0.58f, 1.0f));
	const ImU32 TextColor = ImGui::GetColorU32(ImVec4(0.72f, 0.76f, 0.84f, 1.0f));
	const ImU32 PlayheadColor = ImGui::GetColorU32(ImVec4(0.95f, 0.28f, 0.24f, 1.0f));
	const ImU32 NotifyColor = ImGui::GetColorU32(ImVec4(0.95f, 0.70f, 0.22f, 1.0f));
	const ImU32 NotifySelectedColor = ImGui::GetColorU32(ImVec4(1.0f, 0.84f, 0.36f, 1.0f));
	const ImU32 NotifyBodyColor = ImGui::GetColorU32(ImVec4(0.95f, 0.70f, 0.22f, 0.24f));

	DrawList->AddRectFilled(Min, Max, BgColor, 4.0f);
	const float RulerY = Min.y + 28.0f;
	const float TrackTop = Min.y + 54.0f;
	const float TrackBottom = Max.y - 18.0f;
	const float NotifyBarTop = TrackTop + 14.0f;
	const float NotifyBarBottom = TrackBottom - 14.0f;
	DrawList->AddRectFilled(ImVec2(Min.x + 8.0f, TrackTop), ImVec2(Max.x - 8.0f, TrackBottom), TrackColor, 4.0f);

	const float TrackMinX = Min.x + 12.0f;
	const float TrackMaxX = Max.x - 12.0f;
	const float TrackWidth = std::max(1.0f, TrackMaxX - TrackMinX);
	auto TimeToX = [&](float Time)
	{
		return TrackMinX + (Length > 0.0f ? std::clamp(Time / Length, 0.0f, 1.0f) : 0.0f) * TrackWidth;
	};
	auto XToTime = [&](float X)
	{
		return Length > 0.0f ? std::clamp((X - TrackMinX) / TrackWidth, 0.0f, 1.0f) * Length : 0.0f;
	};
	const ImVec2 MousePos = ImGui::GetIO().MousePos;

	if (bHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
	{
		PendingAnimNotifyTimeToAdd = XToTime(MousePos.x);
		ImGui::OpenPopup("AnimTimelineContextMenu");
	}

	if (ImGui::BeginPopup("AnimTimelineContextMenu"))
	{
		ImGui::Text("Add Notify at %.3f sec", PendingAnimNotifyTimeToAdd);
		ImGui::Separator();

		if (ImGui::Selectable("Name Only Notify##AddAnimNotify_NameOnly"))
		{
			Sequence->AddNotify(PendingAnimNotifyTimeToAdd, FName("AnimNotify"), 0.0f, "");
			SaveAnimSequenceAsset(Sequence);

			const TArray<FAnimNotifyStateEvent>& AddedNotifies = Sequence->GetNotifies();
			for (int32 NotifyIndex = 0; NotifyIndex < static_cast<int32>(AddedNotifies.size()); ++NotifyIndex)
			{
				const FAnimNotifyStateEvent& AddedNotify = AddedNotifies[NotifyIndex];
				if (AddedNotify.NotifyClassName.empty() &&
					std::abs(AddedNotify.TriggerTime - PendingAnimNotifyTimeToAdd) <= 0.001f)
				{
					SelectedAnimNotifyIndex = NotifyIndex;
				}
			}

			if (Viewer)
			{
				AsAnimationViewer(Viewer)->SetAnimationTime(PendingAnimNotifyTimeToAdd);
			}
		}

		if (ImGui::Selectable("Name Only Notify State##AddAnimNotifyState_NameOnly"))
		{
			const float Duration = std::clamp(
				AnimNotifyDurationToAdd,
				0.01f,
				std::max(0.01f, Length - PendingAnimNotifyTimeToAdd));
			Sequence->AddNotify(PendingAnimNotifyTimeToAdd, FName("AnimNotifyState"), Duration, "");
			SaveAnimSequenceAsset(Sequence);

			const TArray<FAnimNotifyStateEvent>& AddedNotifies = Sequence->GetNotifies();
			for (int32 NotifyIndex = 0; NotifyIndex < static_cast<int32>(AddedNotifies.size()); ++NotifyIndex)
			{
				const FAnimNotifyStateEvent& AddedNotify = AddedNotifies[NotifyIndex];
				if (AddedNotify.NotifyClassName.empty() &&
					std::abs(AddedNotify.TriggerTime - PendingAnimNotifyTimeToAdd) <= 0.001f)
				{
					SelectedAnimNotifyIndex = NotifyIndex;
				}
			}

			if (Viewer)
			{
				AsAnimationViewer(Viewer)->SetAnimationTime(PendingAnimNotifyTimeToAdd);
			}
		}

		TArray<UClass*> NotifyClasses;
		GetSelectableAnimNotifyClasses(NotifyClasses);
		if (NotifyClasses.empty())
		{
			ImGui::Separator();
			ImGui::TextDisabled("No AnimNotify classes are registered.");
		}
		else
		{
			ImGui::Separator();
			for (UClass* Class : NotifyClasses)
			{
				const FString ClassName = Class->GetName();
				const FString NotifyName = Class->GetDisplayName();
				const FString ItemLabel = NotifyName + "##AddAnimNotify_" + ClassName;
				if (ImGui::Selectable(ItemLabel.c_str()))
				{
					const bool bIsStateNotify = Class->IsChildOf(UAnimNotifyState::StaticClass());
					const float Duration = bIsStateNotify
						? std::clamp(AnimNotifyDurationToAdd, 0.01f, std::max(0.01f, Length - PendingAnimNotifyTimeToAdd))
						: 0.0f;
					Sequence->AddNotify(PendingAnimNotifyTimeToAdd, FName(NotifyName), Duration, ClassName);
					SaveAnimSequenceAsset(Sequence);

					const TArray<FAnimNotifyStateEvent>& AddedNotifies = Sequence->GetNotifies();
					for (int32 NotifyIndex = 0; NotifyIndex < static_cast<int32>(AddedNotifies.size()); ++NotifyIndex)
					{
						const FAnimNotifyStateEvent& AddedNotify = AddedNotifies[NotifyIndex];
						if (AddedNotify.NotifyClassName == ClassName &&
							std::abs(AddedNotify.TriggerTime - PendingAnimNotifyTimeToAdd) <= 0.001f)
						{
							SelectedAnimNotifyIndex = NotifyIndex;
						}
					}

					if (Viewer)
					{
						AsAnimationViewer(Viewer)->SetAnimationTime(PendingAnimNotifyTimeToAdd);
					}
				}
			}
		}

		ImGui::EndPopup();
	}

	const int32 DesiredTicks = std::max(2, static_cast<int32>(TrackWidth / 90.0f));
	for (int32 Tick = 0; Tick <= DesiredTicks; ++Tick)
	{
		const float Alpha = static_cast<float>(Tick) / static_cast<float>(DesiredTicks);
		const float X = TrackMinX + Alpha * TrackWidth;
		const float Time = Alpha * Length;
		const bool bMajor = (Tick % 2) == 0;
		DrawList->AddLine(ImVec2(X, RulerY), ImVec2(X, TrackBottom), bMajor ? MajorLineColor : LineColor, bMajor ? 1.2f : 1.0f);
		char Label[32];
		snprintf(Label, sizeof(Label), "%.2f", Time);
		DrawList->AddText(ImVec2(X + 3.0f, Min.y + 8.0f), TextColor, Label);
	}

	if (FrameRate > 0.0f && FrameCount > 0)
	{
		const int32 FrameStep = std::max(1, static_cast<int32>(FrameCount / std::max(1.0f, TrackWidth / 18.0f)));
		for (int32 Frame = 0; Frame <= FrameCount; Frame += FrameStep)
		{
			const float Time = static_cast<float>(Frame) / FrameRate;
			const float X = TimeToX(Time);
			DrawList->AddLine(ImVec2(X, RulerY + 14.0f), ImVec2(X, RulerY + 20.0f), LineColor, 1.0f);
		}
	}

	const TArray<FAnimNotifyStateEvent>& Notifies = Sequence->GetNotifies();
	int32 HoveredAnimNotifyIndex = -1;
	int32 HoveredAnimNotifyMode = 0;
	float HoveredPriority = FLT_MAX;

	for (int32 NotifyIndex = 0; NotifyIndex < static_cast<int32>(Notifies.size()); ++NotifyIndex)
	{
		const FAnimNotifyStateEvent& Notify = Notifies[NotifyIndex];
		const float NotifyStartTime = std::clamp(Notify.TriggerTime, 0.0f, Length);
		const float NotifyDuration = std::clamp(Notify.Duration, 0.0f, std::max(0.0f, Length - NotifyStartTime));
		const float NotifyEndTime = NotifyStartTime + NotifyDuration;
		const float NotifyStartX = TimeToX(NotifyStartTime);
		const float NotifyEndX = TimeToX(NotifyEndTime);
		const bool bSelected = SelectedAnimNotifyIndex == NotifyIndex || DraggingAnimNotifyIndex == NotifyIndex;
		const ImU32 Color = bSelected ? NotifySelectedColor : NotifyColor;

		if (NotifyDuration > 0.0001f)
		{
			DrawList->AddRectFilled(
				ImVec2(NotifyStartX, NotifyBarTop),
				ImVec2(std::max(NotifyStartX + 2.0f, NotifyEndX), NotifyBarBottom),
				NotifyBodyColor,
				3.0f);
			DrawList->AddRect(
				ImVec2(NotifyStartX, NotifyBarTop),
				ImVec2(std::max(NotifyStartX + 2.0f, NotifyEndX), NotifyBarBottom),
				Color,
				3.0f,
				0,
				bSelected ? 2.0f : 1.2f);
			DrawList->AddLine(ImVec2(NotifyStartX, TrackTop - 8.0f), ImVec2(NotifyStartX, TrackBottom), Color, bSelected ? 2.2f : 1.4f);
			DrawList->AddLine(ImVec2(NotifyEndX, TrackTop), ImVec2(NotifyEndX, TrackBottom), Color, bSelected ? 2.2f : 1.4f);

			const bool bYInBar = MousePos.y >= NotifyBarTop - 6.0f && MousePos.y <= NotifyBarBottom + 6.0f;
			const float StartDistance = std::abs(MousePos.x - NotifyStartX);
			const float EndDistance = std::abs(MousePos.x - NotifyEndX);
			const bool bBodyHit = bYInBar && MousePos.x >= std::min(NotifyStartX, NotifyEndX) && MousePos.x <= std::max(NotifyStartX, NotifyEndX);

			if (bHovered && bYInBar && StartDistance <= 7.0f && StartDistance < HoveredPriority)
			{
				HoveredAnimNotifyIndex = NotifyIndex;
				HoveredAnimNotifyMode = 2;
				HoveredPriority = StartDistance;
			}
			if (bHovered && bYInBar && EndDistance <= 7.0f && EndDistance < HoveredPriority)
			{
				HoveredAnimNotifyIndex = NotifyIndex;
				HoveredAnimNotifyMode = 3;
				HoveredPriority = EndDistance;
			}
			if (bHovered && bBodyHit && HoveredPriority == FLT_MAX)
			{
				HoveredAnimNotifyIndex = NotifyIndex;
				HoveredAnimNotifyMode = 1;
				HoveredPriority = 9999.0f;
			}
		}
		else
		{
			const float MarkerHalfWidth = bSelected ? 8.0f : 6.0f;
			const float MarkerHeight = bSelected ? 14.0f : 12.0f;
			const float LineThickness = bSelected ? 2.4f : 1.6f;

			DrawList->AddLine(ImVec2(NotifyStartX, TrackTop - 8.0f), ImVec2(NotifyStartX, TrackBottom), Color, LineThickness);
			DrawList->AddTriangleFilled(
				ImVec2(NotifyStartX, TrackTop - MarkerHeight),
				ImVec2(NotifyStartX - MarkerHalfWidth, TrackTop - 2.0f),
				ImVec2(NotifyStartX + MarkerHalfWidth, TrackTop - 2.0f),
				Color);

			const float DistanceToMouse = std::abs(MousePos.x - NotifyStartX);
			if (bHovered &&
				DistanceToMouse <= 8.0f &&
				MousePos.y >= TrackTop - 18.0f && MousePos.y <= TrackBottom &&
				DistanceToMouse < HoveredPriority)
			{
				HoveredAnimNotifyIndex = NotifyIndex;
				HoveredAnimNotifyMode = 1;
				HoveredPriority = DistanceToMouse;
			}
		}
	}

	if (HoveredAnimNotifyIndex >= 0 && HoveredAnimNotifyIndex < static_cast<int32>(Notifies.size()))
	{
		const FAnimNotifyStateEvent& HoveredNotify = Notifies[HoveredAnimNotifyIndex];
		const char* ModeText = HoveredAnimNotifyMode == 2 ? "Drag start" : HoveredAnimNotifyMode == 3 ? "Drag end" : "Drag to move";
		ImGui::SetTooltip("Notify: %s\nStart: %.3f sec\nDuration: %.3f sec\nEnd: %.3f sec\n%s",
			HoveredNotify.NotifyName.ToString().c_str(),
			HoveredNotify.TriggerTime,
			HoveredNotify.Duration,
			HoveredNotify.GetEndTime(),
			ModeText);
	}

	if (bHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
	{
		if (HoveredAnimNotifyIndex >= 0)
		{
			SelectedAnimNotifyIndex = HoveredAnimNotifyIndex;
			DraggingAnimNotifyIndex = HoveredAnimNotifyIndex;
			AnimNotifyDragMode = HoveredAnimNotifyMode;
			AnimNotifyDragGrabOffset = XToTime(MousePos.x) - Notifies[HoveredAnimNotifyIndex].TriggerTime;
			bAnimNotifyDragDirty = false;
			if (Viewer)
			{
				AsAnimationViewer(Viewer)->SetAnimationPlaying(false);
				AsAnimationViewer(Viewer)->SetAnimationTime(Notifies[HoveredAnimNotifyIndex].TriggerTime);
			}
		}
		else if (Viewer)
		{
			AsAnimationViewer(Viewer)->SetAnimationTime(XToTime(MousePos.x));
		}
	}

	if (DraggingAnimNotifyIndex >= 0 && DraggingAnimNotifyIndex < static_cast<int32>(Notifies.size()) && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
	{
		const FAnimNotifyStateEvent& Notify = Notifies[DraggingAnimNotifyIndex];
		const float MouseTime = XToTime(MousePos.x);
		bool bChanged = false;
		float PreviewTime = Notify.TriggerTime;

		if (AnimNotifyDragMode == 2)
		{
			const float OldEndTime = Notify.GetEndTime();
			const float NewStartTime = std::clamp(MouseTime, 0.0f, OldEndTime);
			bChanged = Sequence->SetNotifyTimeRange(DraggingAnimNotifyIndex, NewStartTime, OldEndTime - NewStartTime);
			PreviewTime = NewStartTime;
		}
		else if (AnimNotifyDragMode == 3)
		{
			const float NewEndTime = std::clamp(MouseTime, Notify.TriggerTime, Length);
			bChanged = Sequence->SetNotifyDuration(DraggingAnimNotifyIndex, NewEndTime - Notify.TriggerTime);
			PreviewTime = NewEndTime;
		}
		else
		{
			const float NewTriggerTime = std::clamp(MouseTime - AnimNotifyDragGrabOffset, 0.0f, std::max(0.0f, Length - Notify.Duration));
			bChanged = Sequence->SetNotifyTriggerTime(DraggingAnimNotifyIndex, NewTriggerTime);
			PreviewTime = NewTriggerTime;
		}

		if (bChanged)
		{
			bAnimNotifyDragDirty = true;
			if (Viewer)
			{
				AsAnimationViewer(Viewer)->SetAnimationTime(PreviewTime);
			}
		}
	}
	else if (bActive && ImGui::IsMouseDragging(ImGuiMouseButton_Left) && Viewer)
	{
		AsAnimationViewer(Viewer)->SetAnimationTime(XToTime(MousePos.x));
	}

	if (DraggingAnimNotifyIndex >= 0 && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
	{
		int32 NewNotifyIndex = DraggingAnimNotifyIndex;
		const TArray<FAnimNotifyStateEvent>& UpdatedNotifies = Sequence->GetNotifies();
		if (DraggingAnimNotifyIndex < static_cast<int32>(UpdatedNotifies.size()))
		{
			const float FinalTriggerTime = UpdatedNotifies[DraggingAnimNotifyIndex].TriggerTime;
			if (bAnimNotifyDragDirty && Sequence->MoveNotifyAt(DraggingAnimNotifyIndex, FinalTriggerTime, &NewNotifyIndex))
			{
				SaveAnimSequenceAsset(Sequence);
			}
		}

		SelectedAnimNotifyIndex = NewNotifyIndex;
		DraggingAnimNotifyIndex = -1;
		AnimNotifyDragMode = 0;
		AnimNotifyDragGrabOffset = 0.0f;
		bAnimNotifyDragDirty = false;
	}

	const float CurrentTime = Viewer ? AsAnimationViewer(Viewer)->GetAnimationCurrentTime() : 0.0f;
	const float PlayheadX = TimeToX(CurrentTime);
	DrawList->AddLine(ImVec2(PlayheadX, Min.y + 4.0f), ImVec2(PlayheadX, Max.y - 4.0f), PlayheadColor, 2.0f);
	DrawList->AddTriangleFilled(
		ImVec2(PlayheadX, Min.y + 4.0f),
		ImVec2(PlayheadX - 6.0f, Min.y + 16.0f),
		ImVec2(PlayheadX + 6.0f, Min.y + 16.0f),
		PlayheadColor);
}

void FAnimationEditorViewerWidget::RenderAnimSequenceList(UAnimSequence* Sequence)
{
	(void)Sequence;

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::TextDisabled("Other Anim Sequences");

	const FString CurrentPath = Viewer ? FPaths::Normalize(Viewer->GetFileName()) : FString();
	const TArray<FString> Paths = FResourceManager::Get().GetAnimSequencePaths();
	int32 VisibleCount = 0;
	const float ListHeight = std::max(72.0f, ImGui::GetContentRegionAvail().y);
	if (ImGui::BeginChild("OtherAnimSequenceList", ImVec2(0.0f, ListHeight), false))
	{
		for (const FString& Path : Paths)
		{
			const FString NormalizedPath = FPaths::Normalize(Path);
			if (!CurrentPath.empty() && NormalizedPath == CurrentPath)
			{
				continue;
			}

			const FString Name = GetBaseFileNameWithoutExtension(Path);
			if (ImGui::Selectable(Name.c_str(), false))
			{
				if (EditorEngine && Viewer)
				{
					EditorEngine->GetMainPanel().ChangeViewerTarget(Viewer, NormalizedPath);
				}
			}
			++VisibleCount;
		}

		if (VisibleCount == 0)
		{
			ImGui::TextDisabled("No other .animseq files.");
		}
	}
	ImGui::EndChild();
}

void FAnimationEditorViewerWidget::RenderAnimSequenceDetails(UAnimSequence* Sequence, USkeletalMesh* PreviewMesh)
{
	ImGui::Text("Animation");
	ImGui::Separator();

	if (!Sequence)
	{
		ImGui::TextDisabled("No animation sequence.");
		return;
	}

	const UAnimDataModel* DataModel = Sequence->GetDataModel();
	ImGui::TextWrapped("%s", GetViewerAssetLabel(Viewer).c_str());
	ImGui::Text("Length: %.3f sec", Sequence->GetPlayLength());
	if (DataModel)
	{
		ImGui::Text("Sample Rate: %.3f", DataModel->GetFrameRate().AsDecimal());
		ImGui::Text("Frames: %d", DataModel->GetNumberOfFrames());
		ImGui::Text("Tracks: %d", static_cast<int32>(DataModel->GetBoneAnimationTracks().size()));
	}

	ImGui::Spacing();
	ImGui::TextDisabled("Preview Mesh");
	SyncPreviewMeshPathBuffer();
	if (PreviewMesh)
	{
		ImGui::TextWrapped("Loaded: %s", PreviewMesh->GetAssetPathFileName().c_str());
	}
	else
	{
		ImGui::TextWrapped("No compatible skeletal mesh is currently previewing it.");
	}
	ImGui::SetNextItemWidth(-1.0f);
	ImGui::InputText("##PreviewMeshPath", PreviewMeshPathBuffer, sizeof(PreviewMeshPathBuffer));
	if (ImGui::Button("Load Preview Mesh"))
	{
		PreviewMeshPathBufferSource = PreviewMeshPathBuffer;
		AsAnimationViewer(Viewer)->SetAnimationSequencePreviewMesh(PreviewMeshPathBufferSource);
	}

	ImGui::Spacing();
	ImGui::Separator();
	if (ImGui::CollapsingHeader("Notifies", ImGuiTreeNodeFlags_DefaultOpen))
	{
		const TArray<FAnimNotifyStateEvent>& Notifies = Sequence->GetNotifies();
		if (SelectedAnimNotifyIndex >= static_cast<int32>(Notifies.size()))
		{
			SelectedAnimNotifyIndex = -1;
			SelectedAnimNotifyNameBufferIndex = -1;
			SelectedAnimNotifyNameBuffer[0] = '\0';
		}

		if (Notifies.empty())
		{
			ImGui::TextDisabled("No notifies. Right-click the timeline to add one.");
		}
		else
		{
			for (int32 NotifyIndex = 0; NotifyIndex < static_cast<int32>(Notifies.size()); ++NotifyIndex)
			{
				const FAnimNotifyStateEvent& Notify = Notifies[NotifyIndex];
				char Label[256];
				snprintf(
					Label,
					sizeof(Label),
					"%.3f - %.3f  %s  (%s)##AnimNotify%d",
					Notify.TriggerTime,
					Notify.GetEndTime(),
					Notify.NotifyName.ToString().c_str(),
					GetAnimNotifyClassDisplayName(Notify.NotifyClassName).c_str(),
					NotifyIndex);

				ImGui::PushID(NotifyIndex);
				if (ImGui::SmallButton("Delete"))
				{
					const int32 DeletedIndex = NotifyIndex;
					if (Sequence->RemoveNotifyAt(DeletedIndex))
					{
						SaveAnimSequenceAsset(Sequence);
						if (SelectedAnimNotifyIndex == DeletedIndex)
						{
							SelectedAnimNotifyIndex = -1;
						}
						else if (SelectedAnimNotifyIndex > DeletedIndex)
						{
							--SelectedAnimNotifyIndex;
						}

						if (DraggingAnimNotifyIndex == DeletedIndex)
						{
							DraggingAnimNotifyIndex = -1;
							AnimNotifyDragMode = 0;
							AnimNotifyDragGrabOffset = 0.0f;
							bAnimNotifyDragDirty = false;
						}
						else if (DraggingAnimNotifyIndex > DeletedIndex)
						{
							--DraggingAnimNotifyIndex;
						}

						SelectedAnimNotifyNameBufferIndex = -1;
						SelectedAnimNotifyNameBuffer[0] = '\0';
						ImGui::PopID();
						break;
					}
				}
				ImGui::SameLine();

				if (ImGui::Selectable(Label, SelectedAnimNotifyIndex == NotifyIndex))
				{
					SelectedAnimNotifyIndex = NotifyIndex;
					SelectedAnimNotifyNameBufferIndex = -1;
					if (Viewer)
					{
						AsAnimationViewer(Viewer)->SetAnimationTime(Notify.TriggerTime);
					}
				}
				ImGui::PopID();
			}
		}

		const TArray<FAnimNotifyStateEvent>& CurrentNotifies = Sequence->GetNotifies();
		if (SelectedAnimNotifyIndex >= 0 && SelectedAnimNotifyIndex < static_cast<int32>(CurrentNotifies.size()))
		{
			ImGui::Spacing();
			ImGui::Separator();
			const FAnimNotifyStateEvent& SelectedNotify = CurrentNotifies[SelectedAnimNotifyIndex];
			ImGui::Text("Selected: %s", SelectedNotify.NotifyName.ToString().c_str());
			FString SelectedNotifyClassName = SelectedNotify.NotifyClassName;
			if (DrawAnimNotifyClassCombo("Class", SelectedNotifyClassName))
			{
				if (Sequence->SetNotifyClassName(SelectedAnimNotifyIndex, SelectedNotifyClassName))
				{
					SaveAnimSequenceAsset(Sequence);
				}
			}

			if (SelectedAnimNotifyNameBufferIndex != SelectedAnimNotifyIndex)
			{
				snprintf(
					SelectedAnimNotifyNameBuffer,
					sizeof(SelectedAnimNotifyNameBuffer),
					"%s",
					SelectedNotify.NotifyName.ToString().c_str());
				SelectedAnimNotifyNameBufferIndex = SelectedAnimNotifyIndex;
			}

			ImGui::SetNextItemWidth(180.0f);
			if (ImGui::InputText("Name", SelectedAnimNotifyNameBuffer, sizeof(SelectedAnimNotifyNameBuffer)))
			{
				if (SelectedAnimNotifyNameBuffer[0] != '\0' && Sequence->SetNotifyName(SelectedAnimNotifyIndex, FName(FString(SelectedAnimNotifyNameBuffer))))
				{
					SaveAnimSequenceAsset(Sequence);
				}
			}

			float StartTime = Sequence->GetNotifies()[SelectedAnimNotifyIndex].TriggerTime;
			float Duration = Sequence->GetNotifies()[SelectedAnimNotifyIndex].Duration;
			ImGui::SetNextItemWidth(110.0f);
			if (ImGui::InputFloat("Start", &StartTime, 0.01f, 0.1f, "%.3f"))
			{
				if (Sequence->SetNotifyTriggerTime(SelectedAnimNotifyIndex, StartTime))
				{
					int32 NewNotifyIndex = SelectedAnimNotifyIndex;
					const float FinalTriggerTime = Sequence->GetNotifies()[SelectedAnimNotifyIndex].TriggerTime;
					Sequence->MoveNotifyAt(SelectedAnimNotifyIndex, FinalTriggerTime, &NewNotifyIndex);
					SelectedAnimNotifyIndex = NewNotifyIndex;
					SelectedAnimNotifyNameBufferIndex = -1;
					SaveAnimSequenceAsset(Sequence);
					if (Viewer)
					{
						AsAnimationViewer(Viewer)->SetAnimationTime(std::clamp(StartTime, 0.0f, Sequence->GetPlayLength()));
					}
				}
			}
			ImGui::SetNextItemWidth(110.0f);
			if (ImGui::InputFloat("Duration", &Duration, 0.01f, 0.1f, "%.3f"))
			{
				if (Sequence->SetNotifyDuration(SelectedAnimNotifyIndex, Duration))
				{
					SaveAnimSequenceAsset(Sequence);
				}
			}
			ImGui::Text("End: %.3f", Sequence->GetNotifies()[SelectedAnimNotifyIndex].GetEndTime());
			ImGui::TextDisabled("Duration 0.0 is treated as an instant notify.");
		}
	}

	ImGui::Spacing();
	ImGui::Separator();
	if (ImGui::CollapsingHeader("Source", ImGuiTreeNodeFlags_DefaultOpen))
	{
		if (!Sequence->GetSourceFilePath().empty())
		{
			ImGui::TextWrapped("File: %s", Sequence->GetSourceFilePath().c_str());
		}
		if (!Sequence->GetSourceStackName().empty())
		{
			ImGui::TextWrapped("Stack: %s", Sequence->GetSourceStackName().c_str());
		}
		if (!Sequence->GetAssetPath().empty())
		{
			ImGui::TextWrapped("Asset: %s", Sequence->GetAssetPath().c_str());
		}
	}

	if (DataModel && ImGui::CollapsingHeader("Tracks"))
	{
		const TArray<FBoneAnimationTrack>& Tracks = DataModel->GetBoneAnimationTracks();
		for (int32 TrackIndex = 0; TrackIndex < static_cast<int32>(Tracks.size()); ++TrackIndex)
		{
			const FBoneAnimationTrack& Track = Tracks[TrackIndex];
			if (ImGui::Selectable(Track.Name.ToString().c_str(), SelectedAnimTrackIndex == TrackIndex))
			{
				SelectedAnimTrackIndex = TrackIndex;
			}
		}
	}
}

void FAnimationEditorViewerWidget::RenderAnimSequenceLeftPanel(UAnimSequence* Sequence, USkeletalMeshComponent* SkelMeshComp)
{
	ImGui::Text("Animation Sequence");
	ImGui::Separator();

	if (!Sequence)
	{
		ImGui::TextWrapped("Could not load this animation sequence.");
		return;
	}

	const UAnimDataModel* DataModel = Sequence->GetDataModel();
	ImGui::TextWrapped("%s", GetViewerAssetLabel(Viewer).c_str());
	ImGui::Spacing();
	ImGui::Text("Length: %.3f sec", Sequence->GetPlayLength());
	if (DataModel)
	{
		ImGui::Text("Sample Rate: %.3f", DataModel->GetFrameRate().AsDecimal());
		ImGui::Text("Frames: %d", DataModel->GetNumberOfFrames());
		ImGui::Text("Tracks: %d", static_cast<int32>(DataModel->GetBoneAnimationTracks().size()));
	}

	ImGui::Spacing();
	ImGui::TextDisabled("Preview Mesh");
	SyncPreviewMeshPathBuffer();
	if (SkelMeshComp && SkelMeshComp->GetSkeletalMesh())
	{
		ImGui::TextWrapped("Loaded: %s", SkelMeshComp->GetSkeletalMesh()->GetAssetPathFileName().c_str());
	}
	else
	{
		ImGui::TextWrapped("No preview mesh. Set a skeletal FBX path, or reimport the animseq with PreviewMeshPath.");
	}
	ImGui::SetNextItemWidth(-1.0f);
	ImGui::InputText("##PreviewMeshPath", PreviewMeshPathBuffer, sizeof(PreviewMeshPathBuffer));
	if (ImGui::Button("Load Preview Mesh"))
	{
		PreviewMeshPathBufferSource = PreviewMeshPathBuffer;
		AsAnimationViewer(Viewer)->SetAnimationSequencePreviewMesh(PreviewMeshPathBufferSource);
	}

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::TextDisabled("Playback");

	const bool bPlaying = AsAnimationViewer(Viewer)->IsAnimationPlaying();
	if (ImGui::Button(bPlaying ? "Pause" : "Play"))
	{
		AsAnimationViewer(Viewer)->SetAnimationPlaying(!bPlaying);
	}
	ImGui::SameLine();
	if (ImGui::Button("Restart"))
	{
		AsAnimationViewer(Viewer)->RestartAnimation();
	}
	ImGui::SameLine();
	if (ImGui::Button("Stop"))
	{
		AsAnimationViewer(Viewer)->SetAnimationPlaying(false);
		AsAnimationViewer(Viewer)->SetAnimationTime(0.0f);
	}

	bool bLooping = AsAnimationViewer(Viewer)->IsAnimationLooping();
	if (ImGui::Checkbox("Loop", &bLooping))
	{
		AsAnimationViewer(Viewer)->SetAnimationLooping(bLooping);
	}

	float PlayRate = AsAnimationViewer(Viewer)->GetAnimationPlayRate();
	ImGui::SetNextItemWidth(-1.0f);
	if (ImGui::DragFloat("Play Rate", &PlayRate, 0.01f, -4.0f, 4.0f, "%.2f"))
	{
		AsAnimationViewer(Viewer)->SetAnimationPlayRate(PlayRate);
	}

	float CurrentTime = AsAnimationViewer(Viewer)->GetAnimationCurrentTime();
	const float Length = std::max(0.0f, AsAnimationViewer(Viewer)->GetAnimationLength());
	ImGui::SetNextItemWidth(-1.0f);
	if (ImGui::SliderFloat("Time", &CurrentTime, 0.0f, std::max(Length, 0.001f), "%.3f"))
	{
		AsAnimationViewer(Viewer)->SetAnimationTime(CurrentTime);
	}

	if (DataModel && DataModel->GetFrameRate().AsDecimal() > 0.0f)
	{
		const float Frame = CurrentTime * DataModel->GetFrameRate().AsDecimal();
		ImGui::Text("Frame: %.1f / %d", Frame, DataModel->GetNumberOfFrames());
	}

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::TextDisabled("Tracks");
	if (!DataModel || DataModel->GetBoneAnimationTracks().empty())
	{
		ImGui::TextDisabled("No bone tracks.");
		return;
	}

	const TArray<FBoneAnimationTrack>& Tracks = DataModel->GetBoneAnimationTracks();
	if (SelectedAnimTrackIndex >= static_cast<int32>(Tracks.size()))
	{
		SelectedAnimTrackIndex = -1;
	}
	for (int32 TrackIndex = 0; TrackIndex < static_cast<int32>(Tracks.size()); ++TrackIndex)
	{
		ImGuiTreeNodeFlags Flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_SpanAvailWidth;
		if (SelectedAnimTrackIndex == TrackIndex)
		{
			Flags |= ImGuiTreeNodeFlags_Selected;
		}
		const bool bTrackOpen = ImGui::TreeNodeEx((void*)(intptr_t)TrackIndex, Flags, "%s", Tracks[TrackIndex].Name.ToString().c_str());
		if (ImGui::IsItemClicked())
		{
			SelectedAnimTrackIndex = TrackIndex;
		}
		if (bTrackOpen)
		{
			ImGui::TreePop();
		}
	}
}

void FAnimationEditorViewerWidget::RenderAnimSequenceRightPanel(UAnimSequence* Sequence, USkeletalMesh* PreviewMesh)
{
	ImGui::Text("Details");
	ImGui::Separator();

	if (!Sequence)
	{
		ImGui::TextDisabled("No animation sequence.");
		return;
	}

	const UAnimDataModel* DataModel = Sequence->GetDataModel();
	ImGui::TextDisabled("Source");
	if (!Sequence->GetSourceFilePath().empty())
	{
		ImGui::TextWrapped("File: %s", Sequence->GetSourceFilePath().c_str());
	}
	if (!Sequence->GetSourceStackName().empty())
	{
		ImGui::TextWrapped("Stack: %s", Sequence->GetSourceStackName().c_str());
	}
	if (!Sequence->GetAssetPath().empty())
	{
		ImGui::TextWrapped("Asset: %s", Sequence->GetAssetPath().c_str());
	}

	ImGui::Spacing();
	ImGui::TextDisabled("Preview Skeleton");
	if (PreviewMesh && PreviewMesh->HasValidMeshData())
	{
		ImGui::Text("Bones: %d", static_cast<int32>(PreviewMesh->GetBones().size()));
		ImGui::Text("Sections: %d", static_cast<int32>(PreviewMesh->GetSections().size()));
		ImGui::Text("Vertices: %d", static_cast<int32>(PreviewMesh->GetVertices().size()));
	}
	else
	{
		ImGui::TextWrapped("The sequence is loaded, but no compatible skeletal mesh is currently previewing it.");
	}

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::TextDisabled("Selected Track");
	if (!DataModel || SelectedAnimTrackIndex < 0 || SelectedAnimTrackIndex >= static_cast<int32>(DataModel->GetBoneAnimationTracks().size()))
	{
		ImGui::TextDisabled("Select a track on the left.");
		return;
	}

	const FBoneAnimationTrack& Track = DataModel->GetBoneAnimationTracks()[SelectedAnimTrackIndex];
	const FRawAnimSequenceTrack& RawTrack = Track.InternalTrack;
	ImGui::TextWrapped("Bone: %s", Track.Name.ToString().c_str());
	ImGui::Text("Position Keys: %d", static_cast<int32>(RawTrack.PosKeys.size()));
	ImGui::Text("Rotation Keys: %d", static_cast<int32>(RawTrack.RotKeys.size()));
	ImGui::Text("Scale Keys: %d", static_cast<int32>(RawTrack.ScaleKeys.size()));

	if (!RawTrack.PosKeys.empty())
	{
		const FVector3f& Key = RawTrack.PosKeys.front();
		ImGui::Text("First Pos: %.3f, %.3f, %.3f", Key.X, Key.Y, Key.Z);
	}
	if (!RawTrack.ScaleKeys.empty())
	{
		const FVector3f& Key = RawTrack.ScaleKeys.front();
		ImGui::Text("First Scale: %.3f, %.3f, %.3f", Key.X, Key.Y, Key.Z);
	}
}

