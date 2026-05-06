#include "Editor/UI/EditorActorSequenceDetails.h"

#include "Animation/ActorSequence.h"
#include "Asset/CurveFloatAsset.h"
#include "Component/ActorComponent.h"
#include "Component/ActorSequenceComponent.h"
#include "Core/Paths.h"
#include "Core/PropertyTypes.h"
#include "Core/ResourceManager.h"
#include "Editor/EditorEngine.h"
#include "Editor/UI/EditorMainPanel.h"
#include "GameFramework/AActor.h"
#include "ImGui/imgui.h"
#include <algorithm>
#include <filesystem>

#ifdef GetCurrentTime
#undef GetCurrentTime
#endif

namespace
{
	static void DrawDetailsSeparator()
	{
		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();
	}

	static void DrawDetailsSectionLabel(const char* Label)
	{
		ImVec2 Pos = ImGui::GetCursorScreenPos();
		ImDrawList* DrawList = ImGui::GetWindowDrawList();
		const ImU32 Color = ImGui::GetColorU32(ImGuiCol_Text);
		DrawList->AddText(ImVec2(Pos.x + 0.75f, Pos.y), Color, Label);
		ImGui::TextUnformatted(Label);
	}

	static bool IsAnimatableScalarProperty(const FPropertyDescriptor& Prop)
	{
		if (!Prop.Name || !HasPropertyUsage(Prop.UsageFlags, EPropertyUsageFlags::Animatable))
		{
			return false;
		}

		return Prop.Type == EPropertyType::Float
			|| Prop.Type == EPropertyType::Vec3
			|| Prop.Type == EPropertyType::Color;
	}

	static const char* GetDefaultChannelName(EPropertyType Type)
	{
		switch (Type)
		{
		case EPropertyType::Vec3:
			return "X";
		case EPropertyType::Color:
			return "R";
		case EPropertyType::Float:
		default:
			return "Value";
		}
	}

	static EActorSequenceTrackType GetTrackType(EPropertyType Type)
	{
		switch (Type)
		{
		case EPropertyType::Vec3:
			return EActorSequenceTrackType::Vec3;
		case EPropertyType::Color:
			return EActorSequenceTrackType::Color;
		case EPropertyType::Float:
		default:
			return EActorSequenceTrackType::Float;
		}
	}

	static void GetChannelNames(EActorSequenceTrackType TrackType, TArray<const char*>& OutNames)
	{
		OutNames.clear();
		switch (TrackType)
		{
		case EActorSequenceTrackType::Vec3:
		case EActorSequenceTrackType::Transform:
			OutNames = { "X", "Y", "Z" };
			break;
		case EActorSequenceTrackType::Color:
			OutNames = { "R", "G", "B", "A" };
			break;
		case EActorSequenceTrackType::Float:
		default:
			OutNames = { "Value" };
			break;
		}
	}

	static bool HasChannel(const FActorSequenceSection& Section, const char* ChannelName)
	{
		if (!ChannelName)
		{
			return true;
		}

		for (const FActorSequenceChannel& Channel : Section.Channels)
		{
			if (Channel.ChannelName == ChannelName)
			{
				return true;
			}
		}
		return false;
	}

	static FSequenceCurvePlaybackDesc MakeDefaultPlayback(const FString& CurvePath)
	{
		FSequenceCurvePlaybackDesc Playback;
		Playback.CurveAssetPath = CurvePath;
		Playback.ApplyMode = ECurveApplyMode::Direct;
		Playback.TimeMappingMode = ECurveTimeMappingMode::NormalizedTime;
		return Playback;
	}

	static FString MakeAssetReferenceFromPath(const FString& PathText)
	{
		if (PathText.empty())
		{
			return {};
		}

		std::filesystem::path AssetPath(FPaths::ToWide(PathText));
		if (AssetPath.is_absolute())
		{
			return FPaths::ToRelativeString(AssetPath.lexically_normal().wstring());
		}
		return FPaths::Normalize(PathText);
	}

	static void AddMissingChannels(
		FActorSequenceSection& Section,
		EActorSequenceTrackType TrackType,
		const FString& DefaultCurvePath)
	{
		TArray<const char*> ChannelNames;
		GetChannelNames(TrackType, ChannelNames);
		if (ChannelNames.size() <= 1)
		{
			return;
		}

		const FSequenceCurvePlaybackDesc TemplatePlayback = Section.Channels.empty()
			? MakeDefaultPlayback(DefaultCurvePath)
			: Section.Channels.front().Playback;

		for (const char* ChannelName : ChannelNames)
		{
			if (HasChannel(Section, ChannelName))
			{
				continue;
			}

			FActorSequenceChannel NewChannel;
			NewChannel.ChannelName = ChannelName;
			NewChannel.Playback = TemplatePlayback;
			Section.Channels.push_back(NewChannel);
		}
	}

	static FString MakeTrackDisplayName(
		const FActorSequenceTrack& Track,
		const FActorSequenceChannel& Channel)
	{
		if (Track.PropertyPath.empty())
		{
			return "<Unbound>";
		}

		if (Channel.ChannelName.empty() || Channel.ChannelName == "Value")
		{
			return Track.PropertyPath;
		}

		return Track.PropertyPath + "." + Channel.ChannelName;
	}
}

void FEditorActorSequenceDetails::Initialize(UEditorEngine* InEditorEngine, bool* InUndoCaptureFlag)
{
	EditorEngine = InEditorEngine;
	UndoCaptureFlag = InUndoCaptureFlag;
}

void FEditorActorSequenceDetails::CollectAnimatableScalarProperties(
	UActorComponent* Component,
	TArray<FPropertyDescriptor>& OutProps) const
{
	OutProps.clear();
	if (!Component)
	{
		return;
	}

	TArray<FPropertyDescriptor> Props;
	Component->GetEditableProperties(Props);
	for (const FPropertyDescriptor& Prop : Props)
	{
		if (IsAnimatableScalarProperty(Prop))
		{
			OutProps.push_back(Prop);
		}
	}
}

UActorComponent* FEditorActorSequenceDetails::ResolveTrackComponent(
	UActorSequenceComponent* SequenceComp,
	const FActorSequenceBinding& Binding) const
{
	AActor* Owner = SequenceComp ? SequenceComp->GetOwner() : nullptr;
	if (!Owner)
	{
		return nullptr;
	}

	for (UActorComponent* Component : Owner->GetComponents())
	{
		if (!Component)
		{
			continue;
		}
		if (Binding.Binding.TargetObjectGuid.IsValid()
			&& Component->GetPersistentGuid() == Binding.Binding.TargetObjectGuid)
		{
			return Component;
		}
	}

	for (UActorComponent* Component : Owner->GetComponents())
	{
		if (Component && Component->GetName() == Binding.Binding.TargetObjectName)
		{
			return Component;
		}
	}

	return nullptr;
}

FString FEditorActorSequenceDetails::MakeComponentLabel(AActor* Owner, UActorComponent* Component) const
{
	if (!Component)
	{
		return "None";
	}

	FString Label = Component->GetFName().ToString();
	if (Label.empty())
	{
		Label = Component->GetTypeInfo() ? Component->GetTypeInfo()->name : "Component";
	}

	if (Owner && Component == Owner->GetRootComponent())
	{
		return "[Root] " + Label;
	}
	return Label;
}

void FEditorActorSequenceDetails::MarkEdited(
	UActorSequenceComponent* SequenceComp,
	const char* UndoLabel)
{
	if (!SequenceComp)
	{
		return;
	}

	if (EditorEngine && UndoCaptureFlag && !*UndoCaptureFlag)
	{
		EditorEngine->GetUndoSystem().CaptureSnapshot(UndoLabel ? UndoLabel : "Edit Actor Sequence");
		*UndoCaptureFlag = true;
	}

	SequenceComp->MarkSequenceDirty();
	SequenceComp->PostEditChangeProperty({ "Sequence", EPropertyChangeType::ValueSet });

	if (EditorEngine)
	{
		EditorEngine->GetMainPanel().GetSceneWidget().MarkSceneDirty();
	}
}

bool FEditorActorSequenceDetails::AddTrack(
	UActorSequenceComponent* SequenceComp,
	bool bAddAllChannels)
{
	UActorSequence* Sequence = SequenceComp ? SequenceComp->GetSequence() : nullptr;
	AActor* Owner = SequenceComp ? SequenceComp->GetOwner() : nullptr;
	if (!Sequence || !Owner)
	{
		return false;
	}

	UActorComponent* DefaultComponent = nullptr;
	TArray<FPropertyDescriptor> DefaultProperties;

	for (UActorComponent* Component : Owner->GetComponents())
	{
		TArray<FPropertyDescriptor> CandidateProperties;
		CollectAnimatableScalarProperties(Component, CandidateProperties);
		if (!CandidateProperties.empty())
		{
			DefaultComponent = Component;
			DefaultProperties = CandidateProperties;
			break;
		}
	}

	FActorSequenceBinding Binding;
	Binding.Binding.BindingGuid = FGuid::NewGuid();

	if (DefaultComponent)
	{
		DefaultComponent->EnsurePersistentGuid();
		Binding.Binding.TargetObjectGuid = DefaultComponent->GetPersistentGuid();
		Binding.Binding.TargetObjectName = DefaultComponent->GetName();
	}

	FActorSequenceTrack Track;
	Track.TrackGuid = FGuid::NewGuid();
	if (!DefaultProperties.empty())
	{
		Track.PropertyPath = DefaultProperties.front().Name;
		Track.TrackType = GetTrackType(DefaultProperties.front().Type);
	}

	FActorSequenceSection Section;
	Section.SectionGuid = FGuid::NewGuid();
	Section.Duration = Sequence->Duration > 0.0f ? Sequence->Duration : 1.0f;
	Section.PlayRate = 1.0f;

	TArray<FString> CurvePaths = FResourceManager::Get().GetCurvePaths();
	const FString DefaultCurvePath = CurvePaths.empty() ? FString() : CurvePaths.front();

	FActorSequenceChannel Channel;
	Channel.ChannelName = DefaultProperties.empty()
		? "Value"
		: GetDefaultChannelName(DefaultProperties.front().Type);
	Channel.Playback = MakeDefaultPlayback(DefaultCurvePath);

	Section.Channels.push_back(Channel);
	if (bAddAllChannels)
	{
		AddMissingChannels(Section, Track.TrackType, DefaultCurvePath);
	}

	Track.Sections.push_back(Section);
	Binding.Tracks.push_back(Track);
	Sequence->Bindings.push_back(Binding);
	MarkEdited(SequenceComp, bAddAllChannels ? "Add Actor Sequence Channels" : "Add Actor Sequence Track");
	return true;
}

bool FEditorActorSequenceDetails::AddMissingChannelsToSequence(UActorSequenceComponent* SequenceComp)
{
	UActorSequence* Sequence = SequenceComp ? SequenceComp->GetSequence() : nullptr;
	if (!Sequence)
	{
		return false;
	}

	TArray<FString> CurvePaths = FResourceManager::Get().GetCurvePaths();
	const FString DefaultCurvePath = CurvePaths.empty() ? FString() : CurvePaths.front();
	bool bChanged = false;

	for (FActorSequenceBinding& Binding : Sequence->Bindings)
	{
		for (FActorSequenceTrack& Track : Binding.Tracks)
		{
			for (FActorSequenceSection& Section : Track.Sections)
			{
				const int32 PreviousCount = static_cast<int32>(Section.Channels.size());
				AddMissingChannels(Section, Track.TrackType, DefaultCurvePath);
				bChanged |= PreviousCount != static_cast<int32>(Section.Channels.size());
			}
		}
	}

	if (bChanged)
	{
		MarkEdited(SequenceComp, "Add Actor Sequence Channels");
	}
	return bChanged;
}

void FEditorActorSequenceDetails::Render(UActorSequenceComponent* SequenceComp, float DeltaTime)
{
	UActorSequence* Sequence = SequenceComp ? SequenceComp->GetSequence() : nullptr;
	if (!Sequence)
	{
		return;
	}

	DrawDetailsSeparator();
	DrawDetailsSectionLabel("Actor Sequence");

	bool bSequenceChanged = false;
	bSequenceChanged |= ImGui::DragFloat("Duration##ActorSequence", &Sequence->Duration, 0.01f, 0.0f, 100000.0f);
	bSequenceChanged |= ImGui::Checkbox("Loop##ActorSequence", &Sequence->bLoop);
	if (bSequenceChanged)
	{
		Sequence->Duration = std::max(0.0f, Sequence->Duration);
		MarkEdited(SequenceComp, "Edit Actor Sequence");
	}

	UActorSequencePlayer* PreviewPlayer = SequenceComp->GetPreviewSequencePlayer();
	if (PreviewPlayer && PreviewPlayer->IsPlaying())
	{
		SequenceComp->ExecutePreviewTick(DeltaTime);
	}

	ImGui::Spacing();
	ImGui::TextUnformatted("Preview");
	if (ImGui::Button("Play Preview"))
	{
		SequenceComp->PlayPreview();
	}
	ImGui::SameLine();
	if (ImGui::Button("Pause"))
	{
		SequenceComp->PausePreview();
	}
	ImGui::SameLine();
	if (ImGui::Button("Stop"))
	{
		SequenceComp->StopPreview();
	}

	if (PreviewPlayer)
	{
		float PreviewTime = PreviewPlayer->GetCurrentTime();
		const float PreviewMax = std::max(Sequence->Duration, 0.0f);
		if (ImGui::SliderFloat("Scrub", &PreviewTime, 0.0f, PreviewMax))
		{
			SequenceComp->SetPreviewTime(PreviewTime);
		}
	}

	ImGui::Spacing();
	int32 TrackDisplayCount = 0;
	for (const FActorSequenceBinding& Binding : Sequence->Bindings)
	{
		for (const FActorSequenceTrack& Track : Binding.Tracks)
		{
			for (const FActorSequenceSection& Section : Track.Sections)
			{
				TrackDisplayCount += static_cast<int32>(Section.Channels.size());
			}
		}
	}
	ImGui::Text("Tracks: %d", TrackDisplayCount);

	if (ImGui::Button("Add Track"))
	{
		AddTrack(SequenceComp, false);
	}
	ImGui::SameLine();
	if (ImGui::Button("Add All Channels"))
	{
		AddTrack(SequenceComp, true);
	}
	ImGui::SameLine();
	if (ImGui::Button("Add Missing Channels"))
	{
		AddMissingChannelsToSequence(SequenceComp);
	}

	int32 TrackIndex = 0;
	for (int32 BindingIndex = 0; BindingIndex < static_cast<int32>(Sequence->Bindings.size()); ++BindingIndex)
	{
		FActorSequenceBinding& Binding = Sequence->Bindings[BindingIndex];
		for (int32 InnerTrackIndex = 0; InnerTrackIndex < static_cast<int32>(Binding.Tracks.size()); ++InnerTrackIndex)
		{
			FActorSequenceTrack& Track = Binding.Tracks[InnerTrackIndex];
			for (int32 SectionIndex = 0; SectionIndex < static_cast<int32>(Track.Sections.size()); ++SectionIndex)
			{
				FActorSequenceSection& Section = Track.Sections[SectionIndex];
				for (int32 ChannelIndex = 0; ChannelIndex < static_cast<int32>(Section.Channels.size()); ++ChannelIndex)
				{
					if (RenderTrack(
						SequenceComp,
						Binding,
						Track,
						Section,
						Section.Channels[ChannelIndex],
						TrackIndex))
					{
						Section.Channels.erase(Section.Channels.begin() + ChannelIndex);
						if (Section.Channels.empty())
						{
							Track.Sections.erase(Track.Sections.begin() + SectionIndex);
						}
						if (Track.Sections.empty())
						{
							Binding.Tracks.erase(Binding.Tracks.begin() + InnerTrackIndex);
						}
						if (Binding.Tracks.empty())
						{
							Sequence->Bindings.erase(Sequence->Bindings.begin() + BindingIndex);
						}
						MarkEdited(SequenceComp, "Remove Actor Sequence Track");
						return;
					}
					++TrackIndex;
				}
			}
		}
	}

	if ((ImGui::IsItemDeactivatedAfterEdit() || !ImGui::IsAnyItemActive()) && UndoCaptureFlag)
	{
		*UndoCaptureFlag = false;
	}
}

bool FEditorActorSequenceDetails::RenderTrack(
	UActorSequenceComponent* SequenceComp,
	FActorSequenceBinding& Binding,
	FActorSequenceTrack& Track,
	FActorSequenceSection& Section,
	FActorSequenceChannel& Channel,
	int32 TrackIndex)
{
	UActorSequence* Sequence = SequenceComp ? SequenceComp->GetSequence() : nullptr;
	AActor* Owner = SequenceComp ? SequenceComp->GetOwner() : nullptr;
	if (!Sequence || !Owner)
	{
		return false;
	}

	ImGui::PushID(TrackIndex);
	const FString Header = "Track " + std::to_string(TrackIndex) + ": " + MakeTrackDisplayName(Track, Channel);
	const bool bOpen = ImGui::TreeNodeEx(
		"TrackHeader",
		ImGuiTreeNodeFlags_DefaultOpen,
		"%s",
		Header.c_str());

	ImGui::SameLine();
	if (ImGui::SmallButton("Remove"))
	{
		if (bOpen)
		{
			ImGui::TreePop();
		}
		ImGui::PopID();
		return true;
	}

	if (!bOpen)
	{
		ImGui::PopID();
		return false;
	}

	UActorComponent* CurrentComponent = ResolveTrackComponent(SequenceComp, Binding);
	const FString CurrentComponentLabel = MakeComponentLabel(Owner, CurrentComponent);
	if (ImGui::BeginCombo("Target Component", CurrentComponentLabel.c_str()))
	{
		for (UActorComponent* Component : Owner->GetComponents())
		{
			TArray<FPropertyDescriptor> AnimatableProps;
			CollectAnimatableScalarProperties(Component, AnimatableProps);
			if (AnimatableProps.empty())
			{
				continue;
			}

			const bool bSelected = (Component == CurrentComponent);
			const FString Label = MakeComponentLabel(Owner, Component);
			if (ImGui::Selectable(Label.c_str(), bSelected))
			{
				Component->EnsurePersistentGuid();
				Binding.Binding.TargetObjectGuid = Component->GetPersistentGuid();
				Binding.Binding.TargetObjectName = Component->GetName();

				const bool bCurrentPropertyValid =
					std::any_of(
						AnimatableProps.begin(),
						AnimatableProps.end(),
						[&Track](const FPropertyDescriptor& Prop)
						{
							return Prop.Name && Track.PropertyPath == Prop.Name;
						});

				if (!bCurrentPropertyValid)
				{
					Track.PropertyPath = AnimatableProps.front().Name;
					Track.TrackType = GetTrackType(AnimatableProps.front().Type);
					Channel.ChannelName = GetDefaultChannelName(AnimatableProps.front().Type);
				}
				MarkEdited(SequenceComp, "Edit Actor Sequence Track");
			}
			if (bSelected)
			{
				ImGui::SetItemDefaultFocus();
			}
		}
		ImGui::EndCombo();
	}

	TArray<FPropertyDescriptor> AnimatableProps;
	CollectAnimatableScalarProperties(CurrentComponent, AnimatableProps);
	const FString CurrentProperty =
		Track.PropertyPath.empty() ? "<None>" : Track.PropertyPath;
	if (ImGui::BeginCombo("Target Property", CurrentProperty.c_str()))
	{
		for (const FPropertyDescriptor& Prop : AnimatableProps)
		{
			if (!Prop.Name)
			{
				continue;
			}

			const bool bSelected = (Track.PropertyPath == Prop.Name);
			if (ImGui::Selectable(Prop.Name, bSelected))
			{
				Track.PropertyPath = Prop.Name;
				Track.TrackType = GetTrackType(Prop.Type);
				Channel.ChannelName = GetDefaultChannelName(Prop.Type);
				MarkEdited(SequenceComp, "Edit Actor Sequence Track");
			}
			if (bSelected)
			{
				ImGui::SetItemDefaultFocus();
			}
		}
		ImGui::EndCombo();
	}

	TArray<const char*> ChannelNames;
	GetChannelNames(Track.TrackType, ChannelNames);
	const FString CurrentChannel = Channel.ChannelName.empty() ? "<None>" : Channel.ChannelName;
	if (ImGui::BeginCombo("Channel", CurrentChannel.c_str()))
	{
		for (const char* ChannelName : ChannelNames)
		{
			const bool bSelected = Channel.ChannelName == ChannelName;
			if (ImGui::Selectable(ChannelName, bSelected))
			{
				Channel.ChannelName = ChannelName;
				MarkEdited(SequenceComp, "Edit Actor Sequence Track");
			}
			if (bSelected)
			{
				ImGui::SetItemDefaultFocus();
			}
		}
		ImGui::EndCombo();
	}

	TArray<FString> CurvePaths = FResourceManager::Get().GetCurvePaths();
	const FString CurrentCurve = Channel.Playback.CurveAssetPath.empty()
		? "<None>"
		: Channel.Playback.CurveAssetPath;
	if (ImGui::BeginCombo("Curve", CurrentCurve.c_str()))
	{
		const bool bNoneSelected = Channel.Playback.CurveAssetPath.empty();
		if (ImGui::Selectable("None", bNoneSelected))
		{
			Channel.Playback.CurveAssetPath.clear();
			Channel.Playback.Curve = nullptr;
			MarkEdited(SequenceComp, "Edit Actor Sequence Track");
		}
		if (bNoneSelected)
		{
			ImGui::SetItemDefaultFocus();
		}

		for (const FString& CurvePath : CurvePaths)
		{
			const bool bSelected = (Channel.Playback.CurveAssetPath == CurvePath);
			if (ImGui::Selectable(CurvePath.c_str(), bSelected))
			{
				Channel.Playback.CurveAssetPath = CurvePath;
				Channel.Playback.Curve = nullptr;
				MarkEdited(SequenceComp, "Edit Actor Sequence Track");
			}
			if (bSelected)
			{
				ImGui::SetItemDefaultFocus();
			}
		}
		ImGui::EndCombo();
	}

	if (ImGui::BeginDragDropTarget())
	{
		if (const ImGuiPayload* Payload = ImGui::AcceptDragDropPayload("CurveContentItem"))
		{
			const char* PayloadPath = static_cast<const char*>(Payload->Data);
			if (PayloadPath && PayloadPath[0] != '\0')
			{
				Channel.Playback.CurveAssetPath = MakeAssetReferenceFromPath(PayloadPath);
				Channel.Playback.Curve = nullptr;
				MarkEdited(SequenceComp, "Edit Actor Sequence Track");
			}
		}
		ImGui::EndDragDropTarget();
	}

	ImGui::BeginDisabled(!EditorEngine || Channel.Playback.CurveAssetPath.empty());
	if (ImGui::Button("Open Curve Editor"))
	{
		EditorEngine->GetMainPanel().OpenCurveAsset(Channel.Playback.CurveAssetPath);
	}
	ImGui::EndDisabled();

	bool bPlaybackChanged = false;
	bPlaybackChanged |= ImGui::DragFloat("Start Time", &Section.StartTime, 0.01f, 0.0f, 100000.0f);
	bPlaybackChanged |= ImGui::DragFloat("Duration", &Section.Duration, 0.01f, 0.0f, 100000.0f);
	bPlaybackChanged |= ImGui::DragFloat("Play Rate", &Section.PlayRate, 0.01f, -100.0f, 100.0f);
	bPlaybackChanged |= ImGui::Checkbox("Track Loop", &Section.bLoop);

	static const char* ApplyModeNames[] = { "Direct", "Add", "Multiply" };
	int ApplyModeIndex = static_cast<int>(Channel.Playback.ApplyMode);
	if (ImGui::Combo("Apply Mode", &ApplyModeIndex, ApplyModeNames, IM_ARRAYSIZE(ApplyModeNames)))
	{
		Channel.Playback.ApplyMode = static_cast<ECurveApplyMode>(ApplyModeIndex);
		bPlaybackChanged = true;
	}

	static const char* TimeMappingNames[] = { "Curve Time", "Normalized Time" };
	int TimeMappingIndex = static_cast<int>(Channel.Playback.TimeMappingMode);
	if (ImGui::Combo("Time Mapping", &TimeMappingIndex, TimeMappingNames, IM_ARRAYSIZE(TimeMappingNames)))
	{
		Channel.Playback.TimeMappingMode = static_cast<ECurveTimeMappingMode>(TimeMappingIndex);
		bPlaybackChanged = true;
	}

	if (bPlaybackChanged)
	{
		Section.StartTime = std::max(0.0f, Section.StartTime);
		Section.Duration = std::max(0.0f, Section.Duration);
		MarkEdited(SequenceComp, "Edit Actor Sequence Track");
	}

	ImGui::TreePop();
	ImGui::PopID();
	return false;
}
