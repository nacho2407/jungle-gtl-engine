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
        const FbxAMatrix LocalTransform = Node->EvaluateLocalTransform(SampleTime);
        const FTransform EngineTransform(ToFMatrix(LocalTransform));

        OutTrack.PosKeys.push_back(EngineTransform.GetTranslation());

        FQuat Rotation = EngineTransform.GetRotation().GetNormalized();
        if (!OutTrack.RotKeys.empty())
        {
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

    UAnimSequence* AnimSequence = UObjectManager::Get().CreateObject<UAnimSequence>();
    UAnimDataModel* DataModel = UObjectManager::Get().CreateObject<UAnimDataModel>();
    AnimSequence->SetDataModel(DataModel);

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

        for (int32 KeyIndex = 0; KeyIndex < KeyCount; ++KeyIndex)
        {
            const FbxTime SampleTime = MakeSampleTime(TimeSpan, KeyIndex, KeyCount);
            AppendSampledLocalTransform(Node, SampleTime, Track.InternalTrack);
        }

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
