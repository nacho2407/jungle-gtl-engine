#include "AnimGraphInstance.h"

#include "Core/ResourceManager.h"
#include "Component/SkeletalMeshComponent.h"
#include "Core/Logging/Log.h"

#include <algorithm>
#include <cmath>

namespace
{
	constexpr const char* FloatParametersKey = "FloatParameters";
	constexpr const char* BoolParametersKey = "BoolParameters";
	constexpr const char* IntParametersKey = "IntParameters";
	constexpr const char* ParameterNameKey = "Name";
	constexpr const char* ParameterValueKey = "Value";

	template <typename TValue>
	void SerializeParameterMap(FArchive& Ar, const char* Key, TMap<FString, TValue>& Parameters);
}

void UAnimGraphInstance::Serialize(FArchive& Ar)
{
	UAnimInstance::Serialize(Ar);

	SerializeParameterMap(Ar, FloatParametersKey, FloatParameters);
	SerializeParameterMap(Ar, BoolParametersKey, BoolParameters);
	SerializeParameterMap(Ar, IntParametersKey, IntParameters);
}

void UAnimGraphInstance::SetGraphAsset(UAnimGraphAsset* InAsset)
{
	GraphAsset = InAsset;
	CurrentTime = 0.0f;
	PreviousTime = 0.0f;
	SequenceCacheMap.clear();
	StateMachineCacheMap.clear();
	LoggedNodeWarnings.clear();
	bLoggedMissingGraph = false;

	if (!GraphAsset)
	{
		UE_LOG_WARNING("[AnimGraphInstance] SetGraphAsset(nullptr)");
		return;
	}
}

void UAnimGraphInstance::CopyRuntimeParametersFrom(const UAnimGraphInstance* SourceInstance)
{
	if (!SourceInstance)
	{
		return;
	}

	FloatParameters = SourceInstance->FloatParameters;
	BoolParameters = SourceInstance->BoolParameters;
	IntParameters = SourceInstance->IntParameters;
	CurrentTime = SourceInstance->CurrentTime;
	PreviousTime = SourceInstance->PreviousTime;
}

void UAnimGraphInstance::NativeUpdateAnimation(float DeltaTime)
{
	UAnimInstance::NativeUpdateAnimation(DeltaTime);

	PreviousTime = CurrentTime;
	CurrentTime += DeltaTime;

	if (GraphAsset && GraphAsset->RootNodeId >= 0)
	{
		UpdateNode(GraphAsset->RootNodeId, DeltaTime);
	}
}

bool UAnimGraphInstance::EvaluatePose(FPoseContext& OutPoseContext)
{
	if (!GraphAsset || GraphAsset->RootNodeId < 0)
	{
		if (!bLoggedMissingGraph)
		{
			UE_LOG_WARNING(
				"[AnimGraphInstance] EvaluatePose failed | GraphAsset=%s | RootNodeId=%d",
				GraphAsset ? "valid" : "null",
				GraphAsset ? GraphAsset->RootNodeId : -1);
			bLoggedMissingGraph = true;
		}
		return false;
	}

	return EvaluateNode(GraphAsset->RootNodeId, OutPoseContext);
}

void UAnimGraphInstance::SetFloatParameter(const FString& Name, float Value)
{
	FloatParameters[Name] = Value;
}

void UAnimGraphInstance::SetBoolParameter(const FString& Name, bool Value)
{
	BoolParameters[Name] = Value;
}

void UAnimGraphInstance::SetIntParameter(const FString& Name, int32 Value)
{
	IntParameters[Name] = Value;
}

float UAnimGraphInstance::GetFloatParameter(const FString& Name) const
{
	auto It = FloatParameters.find(Name);
	if (It != FloatParameters.end())
	{
		return It->second;
	}
	return 0.0f;
}

bool UAnimGraphInstance::GetBoolParameter(const FString& Name) const
{
	auto It = BoolParameters.find(Name);
	if (It != BoolParameters.end())
	{
		return It->second;
	}
	return false;
}

int32 UAnimGraphInstance::GetIntParameter(const FString& Name) const
{
	auto It = IntParameters.find(Name);
	if (It != IntParameters.end())
	{
		return It->second;
	}
	return 0;
}

void UAnimGraphInstance::UpdateNode(int32 NodeId, float DeltaTime)
{
	if (!GraphAsset) return;

	const FAnimGraphNodeDesc* Node = GraphAsset->FindNode(NodeId);
	if (!Node)
	{
		LogNodeWarningOnce(NodeId, "node id not found during update");
		return;
	}

	switch (Node->Type)
	{
	case EAnimGraphNodeType::OutputPose:
		if (Node->InputPoseNodeId >= 0)
		{
			UpdateNode(Node->InputPoseNodeId, DeltaTime);
		}
		break;

	case EAnimGraphNodeType::SequencePlayer:
		UpdateSequencePlayer(*Node, DeltaTime);
		break;

	case EAnimGraphNodeType::StateMachine:
	{
		FAnimGraphStateMachineCache& Cache = GetOrCreateStateMachineCache(*Node);
		if (Cache.RuntimeMachine)
		{
			Cache.RuntimeMachine->Update(DeltaTime);
		}
		break;
	}

	default:
		break;
	}
}

void UAnimGraphInstance::UpdateSequencePlayer(const FAnimGraphNodeDesc& Node, float DeltaTime)
{
	if (Node.AnimationPath.empty())
	{
		return;
	}

	FAnimGraphSequenceCache& Cache = GetOrCreateSequenceCache(Node.NodeId, Node.AnimationPath);
	if (!Cache.Sequence)
	{
		LogNodeWarningOnce(Node.NodeId, FString("failed to load sequence during update: ") + Node.AnimationPath);
		return;
	}

	Cache.PreviousTime = Cache.CurrentTime;
	Cache.CurrentTime += DeltaTime * Node.PlayRate;

	bool bLooped = false;
	const bool bReverse = Node.PlayRate < 0.0f;
	const float Length = Cache.Sequence->GetPlayLength();

	if (Length <= 0.0f)
	{
		Cache.PreviousTime = 0.0f;
		Cache.CurrentTime = 0.0f;
		return;
	}

	if (Node.bLoop)
	{
		if (!bReverse)
		{
			if (Cache.CurrentTime > Length)
			{
				Cache.CurrentTime = std::fmod(Cache.CurrentTime, Length);
				bLooped = true;
			}
		}
		else
		{
			if (Cache.CurrentTime < 0.0f)
			{
				Cache.CurrentTime = std::fmod(Cache.CurrentTime, Length);
				if (Cache.CurrentTime < 0.0f)
				{
					Cache.CurrentTime += Length;
				}
				bLooped = true;
			}
		}
	}
	else
	{
		Cache.CurrentTime = std::clamp(Cache.CurrentTime, 0.0f, Length);
	}

	TriggerAnimNotifies(Cache.Sequence, Cache.PreviousTime, Cache.CurrentTime, bLooped, bReverse, DeltaTime);
}

bool UAnimGraphInstance::EvaluateNode(int32 NodeId, FPoseContext& OutPoseContext)
{
	if (!GraphAsset) return false;

	const FAnimGraphNodeDesc* Node = GraphAsset->FindNode(NodeId);
	if (!Node)
	{
		LogNodeWarningOnce(NodeId, "node id not found");
		return false;
	}

	switch (Node->Type)
	{
	case EAnimGraphNodeType::OutputPose:
		if (Node->InputPoseNodeId < 0)
		{
			LogNodeWarningOnce(Node->NodeId, "OutputPose has no input pose");
			return false;
		}
		return EvaluateNode(Node->InputPoseNodeId, OutPoseContext);
	case EAnimGraphNodeType::SequencePlayer:
		return EvaluateSequencePlayer(*Node, OutPoseContext);
	case EAnimGraphNodeType::StateMachine:
		return EvaluateStateMachine(*Node, OutPoseContext);
	default:
		return false;
	}
}

bool UAnimGraphInstance::EvaluateSequencePlayer(const FAnimGraphNodeDesc& Node, FPoseContext& OutPoseContext)
{
	if (Node.AnimationPath.empty())
	{
		LogNodeWarningOnce(Node.NodeId, "SequencePlayer AnimationPath is empty");
		return false;
	}

	FAnimGraphSequenceCache& Cache = GetOrCreateSequenceCache(Node.NodeId, Node.AnimationPath);
	if (!Cache.Sequence)
	{
		LogNodeWarningOnce(Node.NodeId, FString("failed to load sequence: ") + Node.AnimationPath);
		return false;
	}

	USkeletalMesh* CurrentMesh = OwnerComponent ? OwnerComponent->GetSkeletalMesh() : nullptr;
	if (Cache.CachedMesh != CurrentMesh)
	{
		BuildBoneMapping(Cache);
	}

	float PlayTime = Cache.CurrentTime;
	const float Length = Cache.Sequence->GetPlayLength();

	if (Length > 0.0f)
	{
		PlayTime = Node.bLoop ? std::fmod(PlayTime, Length) : std::clamp(PlayTime, 0.0f, Length);
		if (PlayTime < 0.0f)
		{
			PlayTime += Length;
		}
	}

	OutPoseContext.TrackToBoneMap = Cache.TrackToBoneMap;
	const bool bEvaluated = Cache.Sequence->GetAnimationPose(PlayTime, OutPoseContext);
	if (!bEvaluated)
	{
		LogNodeWarningOnce(Node.NodeId, FString("sequence returned no pose: ") + Node.AnimationPath);
	}
	return bEvaluated;
}

FAnimGraphSequenceCache& UAnimGraphInstance::GetOrCreateSequenceCache(int32 NodeId, const FString& AnimationPath)
{
	FAnimGraphSequenceCache& Cache = SequenceCacheMap[NodeId];

	if (Cache.AnimationPath != AnimationPath)
	{
		Cache.Sequence = nullptr;
		Cache.AnimationPath = AnimationPath;
		Cache.TrackToBoneMap.clear();
		Cache.CachedMesh = nullptr;
		Cache.PreviousTime = 0.0f;
		Cache.CurrentTime = 0.0f;
	}

	if (!Cache.Sequence)
	{
		UAnimSequenceBase* AnimBase = FResourceManager::Get().LoadAnimSequence(AnimationPath);
		Cache.Sequence = Cast<UAnimSequence>(AnimBase);
	}

	return Cache;
}

void UAnimGraphInstance::BuildBoneMapping(FAnimGraphSequenceCache& Cache)
{
	Cache.TrackToBoneMap.clear();
	Cache.CachedMesh = OwnerComponent ? OwnerComponent->GetSkeletalMesh() : nullptr;

	if (!Cache.CachedMesh || !Cache.Sequence)
	{
		return;
	}

	const TArray<FBoneAnimationTrack>& Tracks = Cache.Sequence->GetBoneAnimationTracks();
	Cache.TrackToBoneMap.resize(Tracks.size(), -1);

	const TArray<FBoneInfo>& Bones = Cache.CachedMesh->GetBones();

	TMap<FName, int32, FName::Hash> BoneNameToIndex;
	BoneNameToIndex.reserve(Bones.size());

	for (int32 BoneIndex = 0; BoneIndex < static_cast<int32>(Bones.size()); ++BoneIndex)
	{
		BoneNameToIndex[FName(Bones[BoneIndex].Name)] = BoneIndex;
	}

	for (int32 TrackIndex = 0; TrackIndex < static_cast<int32>(Tracks.size()); ++TrackIndex)
	{
		auto It = BoneNameToIndex.find(Tracks[TrackIndex].Name);
		if (It != BoneNameToIndex.end())
		{
			Cache.TrackToBoneMap[TrackIndex] = It->second;
		}
	}
}

bool UAnimGraphInstance::EvaluateStateMachine(const FAnimGraphNodeDesc& Node, FPoseContext& OutPoseContext)
{
	FAnimGraphStateMachineCache& Cache = GetOrCreateStateMachineCache(Node);
	if (!Cache.RuntimeMachine)
	{
		LogNodeWarningOnce(Node.NodeId, "failed to build state machine runtime");
		return false;
	}

	return Cache.RuntimeMachine->EvaluatePose(OutPoseContext);
}

bool UAnimGraphInstance::LogNodeWarningOnce(int32 NodeId, const FString& Message)
{
	if (LoggedNodeWarnings.find(NodeId) != LoggedNodeWarnings.end())
	{
		return false;
	}

	LoggedNodeWarnings.insert(NodeId);
	UE_LOG_WARNING("[AnimGraphInstance] Node %d evaluate failed | %s", NodeId, Message.c_str());
	return true;
}

FAnimGraphStateMachineCache& UAnimGraphInstance::GetOrCreateStateMachineCache(const FAnimGraphNodeDesc& Node)
{
	FAnimGraphStateMachineCache& Cache = StateMachineCacheMap[Node.NodeId];

	const FString Signature = BuildStateMachineSignature(Node.StateMachine);
	if (!Cache.RuntimeMachine || Cache.Signature != Signature)
	{
		Cache.RuntimeMachine = BuildStateMachineRuntime(Node.StateMachine);
		Cache.Signature = Signature;
	}

	return Cache;
}

UAnimationStateMachine* UAnimGraphInstance::BuildStateMachineRuntime(const FAnimStateMachineDesc& Desc)
{
	UAnimationStateMachine* Machine = UObjectManager::Get().CreateObject<UAnimationStateMachine>();
	if (!Machine)
	{
		return nullptr;
	}

	Machine->Initialize(OwnerComponent);

	TMap<int32, FString> StateIdToName;

	int32 AddedStateCount = 0;
	for (const FAnimStateDesc& State : Desc.States)
	{
		if (State.StateId < 0 || State.Name.empty() || State.AnimationPath.empty())
		{
			continue;
		}

		StateIdToName[State.StateId] = State.Name;
		Machine->AddStateFromPathWithPlayback(State.Name, State.AnimationPath, State.PlayRate, State.bLoop, State.bAutoAdvanceOnEnd);
		++AddedStateCount;
	}

	if (AddedStateCount == 0)
	{
		UE_LOG_WARNING("[AnimGraphInstance] StateMachine has no valid states with animation paths.");
	}

	auto EntryIt = StateIdToName.find(Desc.EntryStateId);
	if (EntryIt != StateIdToName.end())
	{
		Machine->SetEntryState(FName(EntryIt->second.c_str()));
	}

	for (const FAnimStateTransitionDesc& Transition : Desc.Transitions)
	{
		auto FromIt = StateIdToName.find(Transition.FromStateId);
		auto ToIt = StateIdToName.find(Transition.ToStateId);

		if (FromIt == StateIdToName.end() || ToIt == StateIdToName.end())
		{
			continue;
		}

		Machine->AddTransition(
			FName(FromIt->second.c_str()),
			FName(ToIt->second.c_str()),
			Transition.BlendTime,
			BuildConditionFunction(Transition.Condition),
			Transition.Priority,
			Transition.Condition.Type == EAnimTransitionConditionType::AlwaysTrue);
	}

	return Machine;
}

FAnimTransitionCondition UAnimGraphInstance::BuildConditionFunction(const FAnimTransitionConditionDesc& Desc)
{
	switch (Desc.Type)
	{
	case EAnimTransitionConditionType::AlwaysTrue:
		return []() { return true; };

	case EAnimTransitionConditionType::BoolParameter:
		return [this, Desc]() {
			return GetBoolParameter(Desc.ParameterName) == Desc.BoolValue;
			};

	case EAnimTransitionConditionType::FloatGreater:
		return [this, Desc]() {
			return GetFloatParameter(Desc.ParameterName) > Desc.Threshold;
			};

	case EAnimTransitionConditionType::FloatLess:
		return [this, Desc]() {
			return GetFloatParameter(Desc.ParameterName) < Desc.Threshold;
			};

	case EAnimTransitionConditionType::IntEquals:
		return [this, Desc]() {
			return GetIntParameter(Desc.ParameterName) == Desc.IntValue;
			};

	case EAnimTransitionConditionType::IntGreater:
		return [this, Desc]() {
			return GetIntParameter(Desc.ParameterName) > Desc.IntValue;
			};

	case EAnimTransitionConditionType::IntLess:
		return [this, Desc]() {
			return GetIntParameter(Desc.ParameterName) < Desc.IntValue;
			};

	case EAnimTransitionConditionType::LuaFunction:
		return []() { return false; };
	}

	return []() { return false; };
}

FString UAnimGraphInstance::BuildStateMachineSignature(const FAnimStateMachineDesc& Desc) const
{
	FString Signature = std::to_string(Desc.EntryStateId);

	for (const FAnimStateDesc& State : Desc.States)
	{
		Signature += "|S:";
		Signature += std::to_string(State.StateId);
		Signature += ",";
		Signature += State.Name;
		Signature += ",";
		Signature += State.AnimationPath;
		Signature += ",";
		Signature += std::to_string(State.PlayRate);
		Signature += ",";
		Signature += State.bLoop ? "loop" : "once";
		Signature += ",";
		Signature += State.bAutoAdvanceOnEnd ? "auto" : "manual";
	}

	for (const FAnimStateTransitionDesc& Transition : Desc.Transitions)
	{
		Signature += "|T:";
		Signature += std::to_string(Transition.FromStateId);
		Signature += ">";
		Signature += std::to_string(Transition.ToStateId);
		Signature += ",";
		Signature += std::to_string(Transition.BlendTime);
		Signature += ",";
		Signature += std::to_string(Transition.Priority);
		Signature += ",";
		Signature += std::to_string(static_cast<int32>(Transition.Condition.Type));
		Signature += ",";
		Signature += Transition.Condition.ParameterName;
		Signature += ",";
		Signature += Transition.Condition.BoolValue ? "true" : "false";
		Signature += ",";
		Signature += std::to_string(Transition.Condition.Threshold);
		Signature += ",";
		Signature += std::to_string(Transition.Condition.IntValue);
		Signature += ",";
		Signature += Transition.Condition.LuaFunctionName;
	}

	return Signature;
}

namespace
{
	template <typename TValue>
	void SerializeParameterMap(FArchive& Ar, const char* Key, TMap<FString, TValue>& Parameters)
	{
		int32 Count = static_cast<int32>(Parameters.size());
		Ar.BeginArray(Key, Count);

		if (Ar.IsSaving())
		{
			for (auto& Pair : Parameters)
			{
				FString Name = Pair.first;
				TValue Value = Pair.second;

				Ar.BeginObject("");
				Ar << ParameterNameKey << Name;
				Ar << ParameterValueKey << Value;
				Ar.EndObject();
			}
		}
		else if (Ar.IsLoading())
		{
			Parameters.clear();
			for (int32 Index = 0; Index < Count; ++Index)
			{
				FString Name;
				TValue Value{};

				Ar.BeginObject(Index);
				if (Ar.HasKey(ParameterNameKey))
				{
					Ar << ParameterNameKey << Name;
				}
				if (Ar.HasKey(ParameterValueKey))
				{
					Ar << ParameterValueKey << Value;
				}
				Ar.EndObject();

				if (!Name.empty())
				{
					Parameters[Name] = Value;
				}
			}
		}

		Ar.EndArray();
	}
}
