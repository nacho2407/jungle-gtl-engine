#include "Asset/FbxAnimSequenceImporter.h"

#include "Animation/AnimDataModel.h"
#include "Animation/AnimSequence.h"
#include "Asset/FbxSceneImportContext.h"
#include "Asset/FbxTransformUtils.h"
#include "Asset/SkeletalMesh.h"
#include "Asset/Skeleton.h"
#include "Core/Logging/Log.h"
#include "Object/Object.h"

#include <fbxsdk.h>
#include <algorithm>
#include <cmath>
// Helper Function
static void BuildNodeNameMap(fbxsdk::FbxNode* Node, TMap<FString, fbxsdk::FbxNode*>& OutMap)
{
    if (!Node)
    {
        return;
    }

    OutMap[FString(Node->GetName())] = Node;

    for (int32 ChildIndex = 0; ChildIndex < Node->GetChildCount(); ++ChildIndex)
    {
        BuildNodeNameMap(Node->GetChild(ChildIndex), OutMap);
    }
}

static fbxsdk::FbxAnimStack* GetFirstAnimStack(fbxsdk::FbxScene* Scene)
{
    if (!Scene || Scene->GetSrcObjectCount<fbxsdk::FbxAnimStack>() <= 0)
    {
        return nullptr;
    }

    return Scene->GetSrcObject<fbxsdk::FbxAnimStack>(0);
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
UAnimSequence* FFbxAnimSequenceImporter::LoadAnimSequence(const FString& Path, const FString& TargetSkeletalMeshPath, USkeletalMesh* TargetSkeletalMesh)
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
        UE_LOG_ERROR("[FbxAnimSequenceImporter] Invalid target skeletal mesh: %s",TargetSkeletalMeshPath.c_str());
        return nullptr;
    }

    USkeleton* Skeleton = TargetSkeletalMesh->GetSkeleton();
    if (!Skeleton)
    {
        UE_LOG_ERROR("[FbxAnimSequenceImporter] Target mesh has no skeleton: %s",TargetSkeletalMeshPath.c_str());
        return nullptr;
    }
    const TArray<FBoneInfo>& Bones = TargetSkeletalMesh->GetBones();
    if (Bones.empty())
    {
        UE_LOG_ERROR("[FbxAnimSequenceImporter] Target skeleton has no bones: %s",TargetSkeletalMeshPath.c_str());
        return nullptr;
    }
    FFbxSceneImportContext Context;
    if (!Context.Import(Path))
    {
        return nullptr;
    }
	fbxsdk::FbxAnimStack* Stack = GetFirstAnimStack(Context.Scene);
    if (!Stack)
    {
        UE_LOG_ERROR("[FbxAnimSequenceImporter] No AnimStack found: %s", Path.c_str());
        return nullptr;
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
	
	TMap<FString, fbxsdk::FbxNode*> NodeNameMap;
    BuildNodeNameMap(Context.Scene->GetRootNode(), NodeNameMap);

    TArray<FAnimationTrack> Tracks;
    Tracks.reserve(Bones.size());
    for (int32 BoneIndex = 0; BoneIndex < static_cast<int32>(Bones.size()); ++BoneIndex)
    {
        const FBoneInfo& Bone = Bones[BoneIndex];
        const FString BoneNameString = Bone.Name.ToString();

		FAnimationTrack Track;
        Track.BoneName = Bone.Name;
        Track.BoneIndex = BoneIndex;
        auto NodeIt = NodeNameMap.find(BoneNameString);
        if (NodeIt == NodeNameMap.end() || !NodeIt->second)
        {
            UE_LOG_WARNING("[FbxAnimSequenceImporter] Bone node not found. Bone=%s Fbx=%s",
                           BoneNameString.c_str(),
                           Path.c_str());

            Tracks.push_back(Track);
            continue;
        }
        fbxsdk::FbxNode* BoneNode = NodeIt->second;
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

			const fbxsdk::FbxAMatrix LocalTransform = BoneNode->EvaluateLocalTransform(SampleTime);

			const FVector Translation = FFbxTransformUtils::ToFVector(LocalTransform.GetT());

			FQuat Rotation = FFbxTransformUtils::ToFQuat(LocalTransform.GetQ());

			const FVector Scale = FFbxTransformUtils::ToFVector(LocalTransform.GetS());
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
    AnimSequence->Skeleton = Skeleton;
    AnimSequence->DataModel = DataModel;

       UE_LOG("[FbxAnimSequenceImporter] Loaded AnimSequence: %s Stack=%s Frames=%d Tracks=%zu Length=%.3f",
           Path.c_str(), Stack->GetName(), NumberOfFrames, Tracks.size(), SequenceLength);

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
