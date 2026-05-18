#include "AnimGraphInstance.h"

#include "Core/ResourceManager.h"
#include "Component/SkeletalMeshComponent.h"

void UAnimGraphInstance::SetGraphAsset(UAnimGraphAsset* InAsset)
{
    GraphAsset = InAsset;
    CurrentTime = 0.0f;
    PreviousTime = 0.0f;
    SequenceCacheMap.clear();
}

void UAnimGraphInstance::NativeUpdateAnimation(float DeltaTime)
{
    UAnimInstance::NativeUpdateAnimation(DeltaTime);

    PreviousTime = CurrentTime;
	CurrentTime += DeltaTime;
}

bool UAnimGraphInstance::EvaluatePose(FPoseContext& OutPoseContext)
{
    if (!GraphAsset || GraphAsset->RootNodeId < 0)
    {
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

bool UAnimGraphInstance::EvaluateNode(int32 NodeId, FPoseContext& OutPoseContext)
{
    if (!GraphAsset) return false;

    const FAnimGraphNodeDesc* Node = GraphAsset->FindNode(NodeId);
    if (!Node)
    {
        return false;
    }

    switch (Node->Type)
    {
    case EAnimGraphNodeType::OutputPose:
		return EvaluateNode(Node->InputPoseNodeId, OutPoseContext);
    case EAnimGraphNodeType::SequencePlayer:
		return EvaluateSequencePlayer(*Node, OutPoseContext);
	case EAnimGraphNodeType::StateMachine:
        // 다음에 구현
        return false;
    default:
        return false;
    }
}

bool UAnimGraphInstance::EvaluateSequencePlayer(const FAnimGraphNodeDesc& Node, FPoseContext& OutPoseContext)
{
    if (Node.AnimationPath.empty())
    {
        return false;
    }

	FAnimGraphSequenceCache& Cache = GetOrCreateSequenceCache(Node.NodeId, Node.AnimationPath);
    if (!Cache.Sequence)
    {
        return false;
    }

	USkeletalMesh* CurrentMesh = OwnerComponent ? OwnerComponent->GetSkeletalMesh() : nullptr;
    if (Cache.CachedMesh != CurrentMesh)
    {
        BuildBoneMapping(Cache);
    }

    float PlayTime = CurrentTime * Node.PlayRate;
    const float Length = Cache.Sequence->GetPlayLength();

    if (Length > 0.0f)
    {
        if (Node.bLoop)
        {
            PlayTime = std::fmod(PlayTime, Length);
            if (PlayTime < 0.0f)
            {
                PlayTime += Length;
            }
        }
        else
        {
            PlayTime = std::clamp(PlayTime, 0.0f, Length);
        }
    }

    OutPoseContext.TrackToBoneMap = Cache.TrackToBoneMap;
    return Cache.Sequence->GetAnimationPose(PlayTime, OutPoseContext);
}

FAnimGraphSequenceCache& UAnimGraphInstance::GetOrCreateSequenceCache(int32 NodeId, const FString& AnimationPath)
{
    FAnimGraphSequenceCache& Cache = SequenceCacheMap[NodeId];

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
