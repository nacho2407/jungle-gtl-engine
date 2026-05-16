#include "FbxImporter.h"
#include "FbxImporterInternal.h"

#include "Animation/AnimSequence.h"
#include "Core/Logging/Log.h"
#include "Core/PlatformTime.h"
#include "Object/Object.h"

#include <algorithm>
#include <cmath>
#include <fbxsdk.h>
#include <utility>

using namespace fbxsdk;
using namespace FFbxImporterInternal;

namespace
{
    constexpr int32 DefaultAnimationSampleRate = 30;

    bool IsValidSampleRate(int32 SampleRate)
    {
        return SampleRate > 0 && SampleRate <= 240;
    }

    int32 ResolveSampleRate(const FFbxAnimImportOptions& ImportOptions)
    {
        return IsValidSampleRate(ImportOptions.SampleRate)
            ? ImportOptions.SampleRate
            : DefaultAnimationSampleRate;
    }

    FFrameRate MakeFrameRate(int32 SampleRate)
    {
        FFrameRate FrameRate;
        FrameRate.Numerator = SampleRate;
        FrameRate.Denominator = 1;
        return FrameRate;
    }

    bool NameMatches(const FString& A, const char* B)
    {
        return A == FString(B ? B : "");
    }

    FbxAnimStack* FindAnimationStack(FbxScene* Scene, const FString& StackName)
    {
        if (!Scene)
        {
            return nullptr;
        }

        const int32 StackCount = Scene->GetSrcObjectCount<FbxAnimStack>();
        if (StackCount <= 0)
        {
            return nullptr;
        }

        if (!StackName.empty())
        {
            for (int32 StackIndex = 0; StackIndex < StackCount; ++StackIndex)
            {
                FbxAnimStack* Stack = Scene->GetSrcObject<FbxAnimStack>(StackIndex);
                if (Stack && NameMatches(StackName, Stack->GetName()))
                {
                    return Stack;
                }
            }

            return nullptr;
        }

        // FBX에서 FbxAnimStack은 Maya/Blender 등의 take 또는 clip에 가까운 단위다.
        // 이 importer는 모든 stack을 한 번에 가져오지 않고, 옵션으로 지정된 stack 하나를
        // UAnimSequence 하나의 후보로 사용한다. StackName이 비어 있으면 현재 stack을 우선하고,
        // 현재 stack도 없으면 scene의 첫 번째 stack을 기본 animation clip으로 선택한다.
        if (FbxAnimStack* CurrentStack = Scene->GetCurrentAnimationStack())
        {
            return CurrentStack;
        }

        return Scene->GetSrcObject<FbxAnimStack>(0);
    }

    bool HasCurveKeys(FbxAnimCurve* Curve)
    {
        return Curve && Curve->KeyGetCount() > 0;
    }

    bool HasVectorCurveKeys(FbxPropertyT<FbxDouble3>& Property, FbxAnimLayer* Layer)
    {
        if (!Layer)
        {
            return false;
        }

        // LclTranslation/LclRotation/LclScaling 같은 FBX vector property는
        // X/Y/Z component별 FbxAnimCurve를 가질 수 있다. 여기서는 curve 값을 아직
        // 샘플링하지 않고, 해당 node를 animation track 후보로 볼 수 있는지만 검사한다.
        return HasCurveKeys(Property.GetCurve(Layer, FBXSDK_CURVENODE_COMPONENT_X)) ||
               HasCurveKeys(Property.GetCurve(Layer, FBXSDK_CURVENODE_COMPONENT_Y)) ||
               HasCurveKeys(Property.GetCurve(Layer, FBXSDK_CURVENODE_COMPONENT_Z));
    }

    bool NodeHasTransformCurveKeys(FbxNode* Node, FbxAnimLayer* Layer)
    {
        if (!Node || !Layer)
        {
            return false;
        }

        return HasVectorCurveKeys(Node->LclTranslation, Layer) ||
               HasVectorCurveKeys(Node->LclRotation, Layer) ||
               HasVectorCurveKeys(Node->LclScaling, Layer);
    }

    bool NodeHasTransformCurveKeys(FbxNode* Node, FbxAnimStack* Stack)
    {
        if (!Node || !Stack)
        {
            return false;
        }

        const int32 LayerCount = Stack->GetMemberCount<FbxAnimLayer>();
        for (int32 LayerIndex = 0; LayerIndex < LayerCount; ++LayerIndex)
        {
            // 하나의 AnimStack 안에는 여러 FbxAnimLayer가 있을 수 있다.
            // 이 구현에서는 layer별 curve를 직접 합성해 저장하지는 않지만,
            // 어느 layer에든 transform curve key가 있으면 이 node를 animated node로 본다.
            FbxAnimLayer* Layer = Stack->GetMember<FbxAnimLayer>(LayerIndex);
            if (NodeHasTransformCurveKeys(Node, Layer))
            {
                return true;
            }
        }

        return false;
    }

    bool IsSkeletonNode(FbxNode* Node)
    {
        if (!Node)
        {
            return false;
        }

        FbxNodeAttribute* Attribute = Node->GetNodeAttribute();
        return Attribute && Attribute->GetAttributeType() == FbxNodeAttribute::eSkeleton;
    }

    void AddUniqueNode(TArray<FbxNode*>& Nodes, FbxNode* Node)
    {
        if (!Node)
        {
            return;
        }

        if (std::find(Nodes.begin(), Nodes.end(), Node) == Nodes.end())
        {
            Nodes.push_back(Node);
        }
    }

    void CollectSkeletonNodesRecursive(FbxNode* Node, TArray<FbxNode*>& OutNodes)
    {
        if (!Node)
        {
            return;
        }

        if (IsSkeletonNode(Node))
        {
            AddUniqueNode(OutNodes, Node);
        }

        for (int32 ChildIndex = 0; ChildIndex < Node->GetChildCount(); ++ChildIndex)
        {
            CollectSkeletonNodesRecursive(Node->GetChild(ChildIndex), OutNodes);
        }
    }

    void CollectAnimatedTransformNodesRecursive(FbxNode* Node, FbxAnimStack* Stack, TArray<FbxNode*>& OutNodes)
    {
        if (!Node)
        {
            return;
        }

        if (NodeHasTransformCurveKeys(Node, Stack))
        {
            AddUniqueNode(OutNodes, Node);
        }

        for (int32 ChildIndex = 0; ChildIndex < Node->GetChildCount(); ++ChildIndex)
        {
            CollectAnimatedTransformNodesRecursive(Node->GetChild(ChildIndex), Stack, OutNodes);
        }
    }

    TArray<FbxNode*> CollectAnimationTrackNodes(FbxScene* Scene, FbxAnimStack* Stack)
    {
        TArray<FbxNode*> Nodes;
        if (!Scene || !Scene->GetRootNode())
        {
            return Nodes;
        }

        FbxNode* RootNode = Scene->GetRootNode();
        for (int32 ChildIndex = 0; ChildIndex < RootNode->GetChildCount(); ++ChildIndex)
        {
            // 정상적인 skeletal animation FBX라면 bone node들이 eSkeleton attribute를 가진다.
            // 먼저 skeleton node들을 수집해서 각 bone에 대응하는 animation track 후보로 사용한다.
            CollectSkeletonNodesRecursive(RootNode->GetChild(ChildIndex), Nodes);
        }

        // skeleton attribute가 없는 FBX도 있으므로, 그 경우에는 transform curve가 있는 node를 fallback으로 사용한다.
        if (Nodes.empty())
        {
            for (int32 ChildIndex = 0; ChildIndex < RootNode->GetChildCount(); ++ChildIndex)
            {
                CollectAnimatedTransformNodesRecursive(RootNode->GetChild(ChildIndex), Stack, Nodes);
            }
        }

        return Nodes;
    }

    bool IsUsableTimeSpan(const FbxTimeSpan& TimeSpan)
    {
        return TimeSpan.GetStop() >= TimeSpan.GetStart();
    }

    FbxTimeSpan ResolveAnimationTimeSpan(FbxScene* Scene, FbxAnimStack* Stack)
    {
        FbxTimeSpan TimeSpan;

        // Animation clip의 길이는 우선 AnimStack 자체의 LocalTimeSpan을 따른다.
        // 이것이 비어 있으면 TakeInfo의 LocalTimeSpan을 보고, 마지막으로 scene timeline을 사용한다.
        // 따라서 curve key의 min/max를 직접 계산하는 방식은 아니며, FBX에 기록된 take/timeline
        // 범위를 기준으로 균일 샘플링할 시간을 정한다.
        if (Stack)
        {
            TimeSpan = Stack->GetLocalTimeSpan();
            if (IsUsableTimeSpan(TimeSpan) && TimeSpan.GetDuration().GetSecondDouble() > 0.0)
            {
                return TimeSpan;
            }
        }

        if (Scene && Stack)
        {
            if (FbxTakeInfo* TakeInfo = Scene->GetTakeInfo(Stack->GetName()))
            {
                TimeSpan = TakeInfo->mLocalTimeSpan;
                if (IsUsableTimeSpan(TimeSpan) && TimeSpan.GetDuration().GetSecondDouble() > 0.0)
                {
                    return TimeSpan;
                }
            }
        }

        if (Scene)
        {
            Scene->GetGlobalSettings().GetTimelineDefaultTimeSpan(TimeSpan);
        }

        return TimeSpan;
    }

    int32 ComputeSampleKeyCount(const FbxTimeSpan& TimeSpan, int32 SampleRate)
    {
        const double DurationSeconds = std::max(0.0, TimeSpan.GetDuration().GetSecondDouble());
        if (DurationSeconds <= 0.0)
        {
            return 1;
        }

        const double RawFrameCount = DurationSeconds * static_cast<double>(SampleRate);
        return std::max(1, static_cast<int32>(std::floor(RawFrameCount + 0.5)) + 1);
    }

    FbxTime MakeSampleTime(const FbxTimeSpan& TimeSpan, int32 KeyIndex, int32 KeyCount)
    {
        const double StartSeconds = TimeSpan.GetStart().GetSecondDouble();
        const double StopSeconds = TimeSpan.GetStop().GetSecondDouble();

        double SampleSeconds = StartSeconds;
        if (KeyCount > 1)
        {
            const double Alpha = static_cast<double>(KeyIndex) / static_cast<double>(KeyCount - 1);
            SampleSeconds = StartSeconds + (StopSeconds - StartSeconds) * Alpha;
        }

        FbxTime SampleTime;
        SampleTime.SetSecondDouble(SampleSeconds);
        return SampleTime;
    }

    void AppendSampledLocalTransform(FbxNode* Node, const FbxTime& SampleTime, FRawAnimSequenceTrack& OutTrack)
    {
        // 실제 animation key 생성 지점이다.
        // FBX의 LclTranslation/LclRotation/LclScaling curve를 component별로 직접 평가해서
        // Pos/Rot/Scale을 조립하지 않고, FBX SDK의 EvaluateLocalTransform()에 맡긴다.
        // 이 호출은 현재 scene에 설정된 FbxAnimStack과 그 안의 FbxAnimLayer/curve,
        // rotation order, pivot, pre/post rotation 같은 FBX transform 규칙을 반영한 local transform을 돌려준다.
        const FbxAMatrix LocalTransform = Node->EvaluateLocalTransform(SampleTime);
        const FTransform EngineTransform(ToFMatrix(LocalTransform));

        // UAnimDataModel에 들어갈 bone track의 원시 키 배열이다.
        // Unreal의 FRawAnimSequenceTrack과 같은 형태로 위치, 회전, 스케일 키를 각각 따로 저장한다.
        OutTrack.PosKeys.push_back(EngineTransform.GetTranslation());

        FQuat Rotation = EngineTransform.GetRotation().GetNormalized();
        if (!OutTrack.RotKeys.empty())
        {
            // Quaternion은 q와 -q가 같은 회전을 뜻하므로, 연속된 key 사이에서 갑자기 부호가 뒤집히면
            // 보간 경로가 길어질 수 있다. 이전 key와 같은 반구에 놓이도록 보정해 짧은 회전 경로를 유지한다.
            Rotation.EnforceShortestArcWith(OutTrack.RotKeys.back());
        }
        OutTrack.RotKeys.push_back(Rotation);

        OutTrack.ScaleKeys.push_back(EngineTransform.GetScale3D());
    }
}

namespace FFbxImporterInternal
{
bool HasAnyAnimation(FbxScene* Scene)
{
    if (!Scene || !Scene->GetRootNode())
    {
        return false;
    }

    const int32 StackCount = Scene->GetSrcObjectCount<FbxAnimStack>();
    for (int32 StackIndex = 0; StackIndex < StackCount; ++StackIndex)
    {
        FbxAnimStack* Stack = Scene->GetSrcObject<FbxAnimStack>(StackIndex);
        if (!Stack)
        {
            continue;
        }

        TArray<FbxNode*> AnimatedNodes;
        for (int32 ChildIndex = 0; ChildIndex < Scene->GetRootNode()->GetChildCount(); ++ChildIndex)
        {
            CollectAnimatedTransformNodesRecursive(Scene->GetRootNode()->GetChild(ChildIndex), Stack, AnimatedNodes);
        }

        if (!AnimatedNodes.empty())
        {
            return true;
        }
    }

    return false;
}
}

UAnimSequence* FFbxImporter::LoadAnimSequence(const FString& Path)
{
    FFbxAnimImportOptions ImportOptions;
    return LoadAnimSequence(Path, ImportOptions);
}

UAnimSequence* FFbxImporter::LoadAnimSequence(const FString& Path, const FFbxAnimImportOptions& ImportOptions)
{
    const double StartTime = FPlatformTime::Seconds();
    UE_LOG("[FbxAnimationImporter] Start loading animation FBX: %s", Path.c_str());

    FbxManager* Manager = FbxManager::Create();
    if (!Manager)
    {
        UE_LOG_ERROR("[FbxAnimationImporter] Failed to create FbxManager");
        return nullptr;
    }

    FbxIOSettings* IOSettings = FbxIOSettings::Create(Manager, IOSROOT);
    Manager->SetIOSettings(IOSettings);

    FbxScene* Scene = FbxScene::Create(Manager, "ImportAnimationScene");
    if (!Scene)
    {
        UE_LOG_ERROR("[FbxAnimationImporter] Failed to create FbxScene");
        Manager->Destroy();
        return nullptr;
    }

    if (!ImportScene(Path, Manager, Scene))
    {
        Manager->Destroy();
        return nullptr;
    }

    FbxAnimStack* AnimStack = FindAnimationStack(Scene, ImportOptions.StackName);
    if (!AnimStack)
    {
        if (!ImportOptions.StackName.empty())
        {
            UE_LOG_ERROR("[FbxAnimationImporter] Animation stack not found: %s | Path=%s",
                ImportOptions.StackName.c_str(), Path.c_str());
        }
        else
        {
            UE_LOG_ERROR("[FbxAnimationImporter] No animation stack found: %s", Path.c_str());
        }

        Manager->Destroy();
        return nullptr;
    }

    Scene->SetCurrentAnimationStack(AnimStack);

    // 여기부터는 "선택된 FbxAnimStack 하나 -> UAnimSequence 하나"로 변환하는 단계다.
    // CurrentAnimationStack을 바꿔 두어야 이후 EvaluateLocalTransform()이 이 stack의 animation curve를 기준으로 평가된다.
    const TArray<FbxNode*> TrackNodes = CollectAnimationTrackNodes(Scene, AnimStack);
    if (TrackNodes.empty())
    {
        UE_LOG_ERROR("[FbxAnimationImporter] No skeleton or animated transform node found: %s", Path.c_str());
        Manager->Destroy();
        return nullptr;
    }

    const int32 SampleRate = ResolveSampleRate(ImportOptions);
    const FbxTimeSpan TimeSpan = ResolveAnimationTimeSpan(Scene, AnimStack);
    const int32 KeyCount = ComputeSampleKeyCount(TimeSpan, SampleRate);
    const double DurationSeconds = std::max(0.0, TimeSpan.GetDuration().GetSecondDouble());

    // 최종 산출물은 UAnimSequence이고, 실제 editable animation data는 UAnimDataModel에 저장된다.
    // DataModel에는 frame rate, 재생 길이, frame/key 개수와 함께 여러 FBoneAnimationTrack이 들어간다.
    UAnimSequence* AnimSequence = UObjectManager::Get().CreateObject<UAnimSequence>();
    UAnimDataModel* DataModel = UObjectManager::Get().CreateObject<UAnimDataModel>();
    AnimSequence->SetDataModel(DataModel);
    AnimSequence->SetSourceFilePath(Path);
    AnimSequence->SetSourceStackName(FString(AnimStack->GetName()));

    DataModel->SetFrameRate(MakeFrameRate(SampleRate));
    DataModel->SetPlayLength(static_cast<float>(DurationSeconds));
    DataModel->SetNumberOfFrames(std::max(0, KeyCount - 1));
    DataModel->SetNumberOfKeys(KeyCount);

    TArray<FBoneAnimationTrack>& Tracks = DataModel->GetMutableBoneAnimationTracks();
    Tracks.reserve(TrackNodes.size());

    for (FbxNode* Node : TrackNodes)
    {
        if (!Node)
        {
            continue;
        }

        FBoneAnimationTrack Track;
        Track.Name = FName(FString(Node->GetName()));
        Track.InternalTrack.PosKeys.reserve(KeyCount);
        Track.InternalTrack.RotKeys.reserve(KeyCount);
        Track.InternalTrack.ScaleKeys.reserve(KeyCount);

        // 하나의 FbxNode가 하나의 FBoneAnimationTrack이 된다.
        // Track.Name에는 bone/node 이름을 넣고, InternalTrack(FRawAnimSequenceTrack)에
        // sample time마다 평가한 PosKeys/RotKeys/ScaleKeys를 순서대로 쌓는다.
        for (int32 KeyIndex = 0; KeyIndex < KeyCount; ++KeyIndex)
        {
            const FbxTime SampleTime = MakeSampleTime(TimeSpan, KeyIndex, KeyCount);
            AppendSampledLocalTransform(Node, SampleTime, Track.InternalTrack);
        }

        // 여러 bone track이 UAnimDataModel의 BoneAnimationTracks 배열에 들어가고,
        // 그 DataModel을 가진 UAnimSequence가 LoadAnimSequence()의 최종 결과가 된다.
        Tracks.push_back(std::move(Track));
    }

    const double EndTime = FPlatformTime::Seconds();
    UE_LOG("[FbxAnimationImporter] Animation FBX Loaded: %s (Stack=%s, Tracks=%zu, Keys=%d, Length=%.3f, SampleRate=%d, %.3f sec)",
        Path.c_str(),
        AnimStack->GetName(),
        Tracks.size(),
        KeyCount,
        DurationSeconds,
        SampleRate,
        EndTime - StartTime);

    Manager->Destroy();
    return AnimSequence;
}
