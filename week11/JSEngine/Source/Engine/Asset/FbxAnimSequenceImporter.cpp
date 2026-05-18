#include "Asset/FbxAnimSequenceImporter.h"

#include "Animation/AnimDataModel.h"
#include "Animation/AnimSequence.h"
#include "Asset/FbxSceneImportContext.h"
#include "Asset/FbxTransformUtils.h"
#include "Asset/SkeletalMesh.h"
#include "Asset/Skeleton.h"
#include "Core/Logging/Log.h"
#include "Object/Object.h"
#include "Engine/Geometry/Transform.h"

#include <fbxsdk.h>
#include <algorithm>
#include <cmath>
// Helper Function
static void BuildNodeNameMap(fbxsdk::FbxNode* Node, TMap<FString, TArray<fbxsdk::FbxNode*>>& OutMap)
{
    if (!Node)
    {
        return;
    }

    OutMap[FString(Node->GetName())].push_back(Node);

    for (int32 ChildIndex = 0; ChildIndex < Node->GetChildCount(); ++ChildIndex)
    {
        BuildNodeNameMap(Node->GetChild(ChildIndex), OutMap);
    }
}

static FString GetNodePath(fbxsdk::FbxNode* Node)
{
    if (!Node)
    {
        return FString("<null>");
    }

    TArray<FString> Names;
    for (fbxsdk::FbxNode* Current = Node; Current; Current = Current->GetParent())
    {
        Names.push_back(FString(Current->GetName()));
    }

    FString Result;
    for (auto It = Names.rbegin(); It != Names.rend(); ++It)
    {
        if (!Result.empty())
        {
            Result += "/";
        }
        Result += *It;
    }

    return Result;
}

static bool HasAncestorNode(fbxsdk::FbxNode* Node, fbxsdk::FbxNode* Ancestor)
{
    if (!Node || !Ancestor)
    {
        return false;
    }

    for (fbxsdk::FbxNode* Current = Node->GetParent(); Current; Current = Current->GetParent())
    {
        if (Current == Ancestor)
        {
            return true;
        }
    }

    return false;
}

static bool HasKnownBoneAncestor(fbxsdk::FbxNode* Node, const TMap<FString, int32>& BoneNameToIndex)
{
    if (!Node)
    {
        return false;
    }

    for (fbxsdk::FbxNode* Current = Node->GetParent(); Current; Current = Current->GetParent())
    {
        if (BoneNameToIndex.find(FString(Current->GetName())) != BoneNameToIndex.end())
        {
            return true;
        }
    }

    return false;
}

static fbxsdk::FbxNode* ResolveBoneNodeForSkeleton(
    int32 BoneIndex,
    const TArray<FBoneInfo>& Bones,
    const TMap<FString, TArray<fbxsdk::FbxNode*>>& NodeNameMap,
    const TMap<FString, int32>& BoneNameToIndex,
    TArray<fbxsdk::FbxNode*>& BoneNodes,
    TArray<uint8>& ResolveState,
    const FString& SourcePath)
{
    if (BoneIndex < 0 || BoneIndex >= static_cast<int32>(Bones.size()))
    {
        return nullptr;
    }

    if (ResolveState[BoneIndex] == 2)
    {
        return BoneNodes[BoneIndex];
    }

    if (ResolveState[BoneIndex] == 1)
    {
        UE_LOG_WARNING("[FbxAnimSequenceImporter] Cyclic reference skeleton parent chain. Bone=%s Fbx=%s",
                       Bones[BoneIndex].Name.ToString().c_str(),
                       SourcePath.c_str());
        return nullptr;
    }

    ResolveState[BoneIndex] = 1;

    const FBoneInfo& Bone = Bones[BoneIndex];
    fbxsdk::FbxNode* ParentBoneNode = nullptr;
    if (Bone.ParentIndex >= 0 && Bone.ParentIndex < static_cast<int32>(Bones.size()))
    {
        ParentBoneNode = ResolveBoneNodeForSkeleton(
            Bone.ParentIndex,
            Bones,
            NodeNameMap,
            BoneNameToIndex,
            BoneNodes,
            ResolveState,
            SourcePath);
    }

    const FString BoneNameString = Bone.Name.ToString();
    auto CandidatesIt = NodeNameMap.find(BoneNameString);
    if (CandidatesIt == NodeNameMap.end() || CandidatesIt->second.empty())
    {
        ResolveState[BoneIndex] = 2;
        return nullptr;
    }

    const TArray<fbxsdk::FbxNode*>& Candidates = CandidatesIt->second;
    fbxsdk::FbxNode* SelectedNode = nullptr;
    int32 MatchCount = 0;

    if (ParentBoneNode)
    {
        for (fbxsdk::FbxNode* Candidate : Candidates)
        {
            if (HasAncestorNode(Candidate, ParentBoneNode))
            {
                if (!SelectedNode)
                {
                    SelectedNode = Candidate;
                }
                ++MatchCount;
            }
        }
    }
    else
    {
        for (fbxsdk::FbxNode* Candidate : Candidates)
        {
            if (!HasKnownBoneAncestor(Candidate, BoneNameToIndex))
            {
                if (!SelectedNode)
                {
                    SelectedNode = Candidate;
                }
                ++MatchCount;
            }
        }
    }

    if (!SelectedNode)
    {
        SelectedNode = Candidates[0];
    }

    if (Candidates.size() > 1)
    {
        UE_LOG_WARNING("[FbxAnimSequenceImporter] Duplicate FBX bone node names. Bone=%s Candidates=%zu HierarchyMatches=%d Selected=%s Fbx=%s",
                       BoneNameString.c_str(),
                       Candidates.size(),
                       MatchCount,
                       GetNodePath(SelectedNode).c_str(),
                       SourcePath.c_str());
    }

    if (ParentBoneNode && !HasAncestorNode(SelectedNode, ParentBoneNode))
    {
        UE_LOG_WARNING("[FbxAnimSequenceImporter] Bone node hierarchy mismatch. Bone=%s Selected=%s ExpectedParent=%s Fbx=%s",
                       BoneNameString.c_str(),
                       GetNodePath(SelectedNode).c_str(),
                       GetNodePath(ParentBoneNode).c_str(),
                       SourcePath.c_str());
    }

    BoneNodes[BoneIndex] = SelectedNode;
    ResolveState[BoneIndex] = 2;
    return SelectedNode;
}

static fbxsdk::FbxAnimStack* GetFirstAnimStack(fbxsdk::FbxScene* Scene)
{
    if (!Scene || Scene->GetSrcObjectCount<fbxsdk::FbxAnimStack>() <= 0)
    {
        return nullptr;
    }

    return Scene->GetSrcObject<fbxsdk::FbxAnimStack>(0);
}
static fbxsdk::FbxAnimStack* FindAnimStackByName(fbxsdk::FbxScene* Scene, const FString& AnimStackName)
{
    if (!Scene)
    {
        return nullptr;
    }

    const int32 StackCount = Scene->GetSrcObjectCount<fbxsdk::FbxAnimStack>();
    for (int32 StackIndex = 0; StackIndex < StackCount; ++StackIndex)
    {
        fbxsdk::FbxAnimStack* Stack =
            Scene->GetSrcObject<fbxsdk::FbxAnimStack>(StackIndex);

        if (!Stack)
        {
            continue;
        }

        if (FString(Stack->GetName()) == AnimStackName)
        {
            return Stack;
        }
    }

    return nullptr;
}

static fbxsdk::FbxTimeSpan GetAnimationTimeSpan(fbxsdk::FbxScene* Scene, fbxsdk::FbxAnimStack* Stack)
{
    fbxsdk::FbxTimeSpan TimeSpan;

    if (Stack)
    {
        TimeSpan = Stack->GetLocalTimeSpan();
    }

    if (TimeSpan.GetDuration().GetSecondDouble() <= 0.0 && Scene)
    {
        Scene->GetGlobalSettings().GetTimelineDefaultTimeSpan(TimeSpan);
    }

    return TimeSpan;
}
static float GetSceneFrameRate(fbxsdk::FbxScene* Scene)
{
    if (!Scene)
    {
        return 30.0f;
    }

    const double Rate = fbxsdk::FbxTime::GetFrameRate(
        Scene->GetGlobalSettings().GetTimeMode());

    return Rate > 0.0 ? static_cast<float>(Rate) : 30.0f;
}

static FMatrix EvaluateEngineLocalBoneMatrix(
    fbxsdk::FbxNode* BoneNode,
    fbxsdk::FbxNode* ParentBoneNode,
    fbxsdk::FbxTime SampleTime)
{
    if (!BoneNode)
    {
        return FMatrix::Identity;
    }

    const FMatrix BoneGlobal =
        FFbxTransformUtils::ToFMatrix(BoneNode->EvaluateGlobalTransform(SampleTime));

    if (!ParentBoneNode)
    {
        return BoneGlobal;
    }

    const FMatrix ParentGlobal =
        FFbxTransformUtils::ToFMatrix(ParentBoneNode->EvaluateGlobalTransform(SampleTime));

    // Runtime pose composition is Local * ParentGlobal, so import local is Global * ParentGlobal^-1.
    return BoneGlobal * ParentGlobal.GetInverse();
}

UAnimSequence* FFbxAnimSequenceImporter::LoadAnimSequence(const FString& Path, const FString& TargetSkeletalMeshPath, USkeletalMesh* TargetSkeletalMesh)
{	
	return LoadAnimSequence(Path, TargetSkeletalMeshPath, FString(), TargetSkeletalMesh);

}

UAnimSequence* FFbxAnimSequenceImporter::LoadAnimSequence(const FString& Path, const FString& TargetSkeletalMeshPath, const FString& AnimStackName, USkeletalMesh* TargetSkeletalMesh)
{
    // 1. target mesh 검증
    // 2. FBX scene import
    // 3. anim stack 선택
    // 4. time span/frame rate/frame count 계산
    // 5. FBX node map 생성
    // 6. skeleton bone order 기준 track 생성
    // 7. frame sampling
    // 8. UAnimDataModel 생성
    // 9. UAnimSequence 생성

    if (!TargetSkeletalMesh || !TargetSkeletalMesh->HasValidMeshData())
    {
        UE_LOG_ERROR("[FbxAnimSequenceImporter] Invalid target skeletal mesh: %s", TargetSkeletalMeshPath.c_str());
        return nullptr;
    }

    USkeleton* Skeleton = TargetSkeletalMesh->GetSkeleton();
    if (!Skeleton)
    {
        UE_LOG_ERROR("[FbxAnimSequenceImporter] Target mesh has no skeleton: %s", TargetSkeletalMeshPath.c_str());
        return nullptr;
    }
    const TArray<FBoneInfo>& Bones = TargetSkeletalMesh->GetBones();
    if (Bones.empty())
    {
        UE_LOG_ERROR("[FbxAnimSequenceImporter] Target skeleton has no bones: %s", TargetSkeletalMeshPath.c_str());
        return nullptr;
    }
    FFbxSceneImportContext Context;
    if (!Context.Import(Path))
    {
        return nullptr;
    }
    fbxsdk::FbxAnimStack* Stack = nullptr;

    if (!AnimStackName.empty())
    {
        Stack = FindAnimStackByName(Context.Scene, AnimStackName);
        if (!Stack)
        {
            UE_LOG_ERROR("[FbxAnimSequenceImporter] AnimStack not found. Fbx=%s Stack=%s", Path.c_str(), AnimStackName.c_str());
            return nullptr;
        }
    }
    else
    {
        Stack = GetFirstAnimStack(Context.Scene);
    }
    Context.Scene->SetCurrentAnimationStack(Stack);

    const fbxsdk::FbxTimeSpan TimeSpan = GetAnimationTimeSpan(Context.Scene, Stack);
    const double StartSeconds = TimeSpan.GetStart().GetSecondDouble();
    const double EndSeconds = TimeSpan.GetStop().GetSecondDouble();
    const double DurationSeconds = EndSeconds - StartSeconds;
    if (DurationSeconds <= 0.0)
    {
        UE_LOG_ERROR("[FbxAnimSequenceImporter] Invalid animation duration: %s", Path.c_str());
        return nullptr;
    }
    const float FrameRate = GetSceneFrameRate(Context.Scene);
    const float SequenceLength = static_cast<float>(DurationSeconds);
    const int32 NumberOfFrames = static_cast<int32>(std::floor(SequenceLength * FrameRate)) + 1;

    TMap<FString, TArray<fbxsdk::FbxNode*>> NodeNameMap;
    BuildNodeNameMap(Context.Scene->GetRootNode(), NodeNameMap);

    int32 FbxNodeNameCount = 0;
    int32 DuplicateFbxNodeNameGroupCount = 0;
    for (const auto& Pair : NodeNameMap)
    {
        FbxNodeNameCount += static_cast<int32>(Pair.second.size());
        if (Pair.second.size() > 1)
        {
            ++DuplicateFbxNodeNameGroupCount;
        }
    }

    TMap<FString, int32> BoneNameToIndex;
    int32 DuplicateTargetBoneNameCount = 0;
    for (int32 BoneIndex = 0; BoneIndex < static_cast<int32>(Bones.size()); ++BoneIndex)
    {
        const FString BoneNameString = Bones[BoneIndex].Name.ToString();
        if (BoneNameToIndex.find(BoneNameString) != BoneNameToIndex.end())
        {
            ++DuplicateTargetBoneNameCount;
            UE_LOG_WARNING("[FbxAnimSequenceImporter] Duplicate target skeleton bone names. Bone=%s Target=%s",
                           BoneNameString.c_str(),
                           TargetSkeletalMeshPath.c_str());
            continue;
        }
        BoneNameToIndex[BoneNameString] = BoneIndex;
    }

    TArray<fbxsdk::FbxNode*> BoneNodes;
    BoneNodes.resize(Bones.size(), nullptr);
    TArray<uint8> ResolveState;
    ResolveState.resize(Bones.size(), 0);
    for (int32 BoneIndex = 0; BoneIndex < static_cast<int32>(Bones.size()); ++BoneIndex)
    {
        ResolveBoneNodeForSkeleton(
            BoneIndex,
            Bones,
            NodeNameMap,
            BoneNameToIndex,
            BoneNodes,
            ResolveState,
            Path);
    }

    int32 MatchedBoneNodeCount = 0;
    int32 MissingBoneNodeCount = 0;
    for (fbxsdk::FbxNode* BoneNode : BoneNodes)
    {
        if (BoneNode)
        {
            ++MatchedBoneNodeCount;
        }
        else
        {
            ++MissingBoneNodeCount;
        }
    }

    UE_LOG("[FbxAnimSequenceImporter] BoneNodeMatchSummary Fbx=%s Target=%s Stack=%s Bones=%zu Matched=%d Missing=%d FbxNodes=%d UniqueNodeNames=%zu DuplicateNodeNameGroups=%d DuplicateTargetBoneNames=%d",
           Path.c_str(),
           TargetSkeletalMeshPath.c_str(),
           Stack->GetName(),
           Bones.size(),
           MatchedBoneNodeCount,
           MissingBoneNodeCount,
           FbxNodeNameCount,
           NodeNameMap.size(),
           DuplicateFbxNodeNameGroupCount,
           DuplicateTargetBoneNameCount);

    TArray<FAnimationTrack> Tracks;
    Tracks.reserve(Bones.size());
    for (int32 BoneIndex = 0; BoneIndex < static_cast<int32>(Bones.size()); ++BoneIndex)
    {
        const FBoneInfo& Bone = Bones[BoneIndex];
        const FString BoneNameString = Bone.Name.ToString();

        FAnimationTrack Track;
        Track.BoneName = Bone.Name;
        Track.BoneIndex = BoneIndex;
        fbxsdk::FbxNode* BoneNode = BoneNodes[BoneIndex];
        if (!BoneNode)
        {
            UE_LOG_WARNING("[FbxAnimSequenceImporter] Bone node not found. Bone=%s Fbx=%s", BoneNameString.c_str(), Path.c_str());
            FTransform BindTransform(Bone.LocalBindTransform);

            FRawAnimSequenceTrack& Raw = Track.InternalTrackData;
            Raw.PosKeys.push_back(BindTransform.GetTranslation());
            Raw.RotKeys.push_back(BindTransform.GetRotation());
            Raw.ScaleKeys.push_back(BindTransform.GetScale3D());

            Raw.PosKeyTimes.push_back(0.0f);
            Raw.RotKeyTimes.push_back(0.0f);
            Raw.ScaleKeyTimes.push_back(0.0f);
            Tracks.push_back(Track);
            continue;
        }
        FRawAnimSequenceTrack& Raw = Track.InternalTrackData;

        Raw.PosKeys.reserve(NumberOfFrames);
        Raw.RotKeys.reserve(NumberOfFrames);
        Raw.ScaleKeys.reserve(NumberOfFrames);

        Raw.PosKeyTimes.reserve(NumberOfFrames);
        Raw.RotKeyTimes.reserve(NumberOfFrames);
        Raw.ScaleKeyTimes.reserve(NumberOfFrames);

        for (int32 FrameIndex = 0; FrameIndex < NumberOfFrames; ++FrameIndex)
        {
            const float LocalSeconds = std::min(static_cast<float>(FrameIndex) / FrameRate, SequenceLength);
            fbxsdk::FbxTime SampleTime;

            SampleTime.SetSecondDouble(StartSeconds + static_cast<double>(LocalSeconds));

            fbxsdk::FbxNode* ParentBoneNode = nullptr;
            if (Bone.ParentIndex >= 0 && Bone.ParentIndex < static_cast<int32>(BoneNodes.size()))
            {
                ParentBoneNode = BoneNodes[Bone.ParentIndex];
            }

            const FMatrix LocalMatrix =
                EvaluateEngineLocalBoneMatrix(BoneNode, ParentBoneNode, SampleTime);
            const FTransform LocalTransform(LocalMatrix);

            const FVector Translation = LocalTransform.GetTranslation();

            FQuat Rotation = LocalTransform.GetRotation();

            const FVector Scale = LocalTransform.GetScale3D();
            if (!Raw.RotKeys.empty() && FQuat::DotProduct(Raw.RotKeys.back(), Rotation) < 0.0f)
                Rotation = Rotation * -1.0f;

            Rotation.Normalize();

            Raw.PosKeys.push_back(Translation);
            Raw.RotKeys.push_back(Rotation);
            Raw.ScaleKeys.push_back(Scale);

            Raw.PosKeyTimes.push_back(LocalSeconds);
            Raw.RotKeyTimes.push_back(LocalSeconds);
            Raw.ScaleKeyTimes.push_back(LocalSeconds);
        }
        Tracks.push_back(Track);
    }
    UAnimDataModel* DataModel = new UAnimDataModel();
    DataModel->SequenceLength = SequenceLength;
    DataModel->FrameRate = FrameRate;
    DataModel->NumberOfFrames = NumberOfFrames;
    DataModel->BoneAnimationTracks = Tracks;

    UAnimSequence* AnimSequence =
        UObjectManager::Get().CreateObject<UAnimSequence>();

    AnimSequence->AssetPath = Path;
    AnimSequence->SourceFbxPath = Path;
    AnimSequence->TargetSkeletonPath = TargetSkeletalMeshPath;
    AnimSequence->AnimStackName = FString(Stack->GetName());
    AnimSequence->Skeleton = Skeleton;
    AnimSequence->DataModel = DataModel;

    UE_LOG("[FbxAnimSequenceImporter] Loaded AnimSequence: %s Stack=%s FrameRate=%.3f Frames=%d Tracks=%zu Length=%.3f MatchedBones=%d MissingBones=%d",
           Path.c_str(),
           Stack->GetName(),
           FrameRate,
           NumberOfFrames,
           Tracks.size(),
           SequenceLength,
           MatchedBoneNodeCount,
           MissingBoneNodeCount);

    return AnimSequence;
}

TArray<FString> FFbxAnimSequenceImporter::ListAnimStacks(const FString& Path)
{
    TArray<FString> Result;

    FFbxSceneImportContext Context;
    if (!Context.Import(Path))
    {
        return Result;
    }

    const int32 StackCount = Context.Scene->GetSrcObjectCount<fbxsdk::FbxAnimStack>();
    for (int32 StackIndex = 0; StackIndex < StackCount; ++StackIndex)
    {
        fbxsdk::FbxAnimStack* Stack =
            Context.Scene->GetSrcObject<fbxsdk::FbxAnimStack>(StackIndex);

        if (!Stack)
        {
            continue;
        }

        Result.push_back(FString(Stack->GetName()));
    }

    return Result;
}
//node traversal helper
//    anim stack listing
//        anim sequence loading
