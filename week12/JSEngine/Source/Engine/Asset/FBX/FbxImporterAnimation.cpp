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

    bool ClusterHasPositiveWeight(FbxCluster* Cluster)
    {
        if (!Cluster)
        {
            return false;
        }

        const int32 IndexCount = Cluster->GetControlPointIndicesCount();
        double* Weights = Cluster->GetControlPointWeights();
        if (IndexCount <= 0 || !Weights)
        {
            return false;
        }

        for (int32 Index = 0; Index < IndexCount; ++Index)
        {
            if (Weights[Index] > 0.0)
            {
                return true;
            }
        }

        return false;
    }

    void CollectSkinClusterLinkNodes(FbxMesh* Mesh, TArray<FbxNode*>& OutNodes)
    {
        if (!Mesh)
        {
            return;
        }

        const int32 SkinCount = Mesh->GetDeformerCount(FbxDeformer::eSkin);
        for (int32 SkinIndex = 0; SkinIndex < SkinCount; ++SkinIndex)
        {
            FbxSkin* Skin = static_cast<FbxSkin*>(Mesh->GetDeformer(SkinIndex, FbxDeformer::eSkin));
            if (!Skin)
            {
                continue;
            }

            const int32 ClusterCount = Skin->GetClusterCount();
            for (int32 ClusterIndex = 0; ClusterIndex < ClusterCount; ++ClusterIndex)
            {
                FbxCluster* Cluster = Skin->GetCluster(ClusterIndex);
                if (!Cluster || !Cluster->GetLink() || !ClusterHasPositiveWeight(Cluster))
                {
                    continue;
                }

                AddUniqueNode(OutNodes, Cluster->GetLink());
            }
        }
    }

    void CollectSkinClusterLinkNodesRecursive(FbxNode* Node, TArray<FbxNode*>& OutNodes)
    {
        if (!Node)
        {
            return;
        }

        if (FbxMesh* Mesh = Node->GetMesh())
        {
            CollectSkinClusterLinkNodes(Mesh, OutNodes);
        }

        for (int32 ChildIndex = 0; ChildIndex < Node->GetChildCount(); ++ChildIndex)
        {
            CollectSkinClusterLinkNodesRecursive(Node->GetChild(ChildIndex), OutNodes);
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

        for (int32 ChildIndex = 0; ChildIndex < RootNode->GetChildCount(); ++ChildIndex)
        {
            // 일부 FBX는 cluster link node가 eSkeleton attribute를 갖지 않는다.
            // Skeletal mesh import는 cluster link를 bone으로 쓰므로 animation track 후보에도 추가한다.
            CollectSkinClusterLinkNodesRecursive(RootNode->GetChild(ChildIndex), Nodes);
        }

        // skeleton attribute가 없고 skin cluster도 없는 FBX도 있으므로, 그 경우에는 transform curve가 있는 node를 fallback으로 사용한다.
        if (Nodes.empty())
        {
            for (int32 ChildIndex = 0; ChildIndex < RootNode->GetChildCount(); ++ChildIndex)
            {
                CollectAnimatedTransformNodesRecursive(RootNode->GetChild(ChildIndex), Stack, Nodes);
            }
        }

        return Nodes;
    }

    bool StackHasTransformAnimation(FbxScene* Scene, FbxAnimStack* Stack)
    {
        if (!Scene || !Scene->GetRootNode() || !Stack)
        {
            return false;
        }

        TArray<FbxNode*> AnimatedNodes;
        FbxNode* RootNode = Scene->GetRootNode();
        for (int32 ChildIndex = 0; ChildIndex < RootNode->GetChildCount(); ++ChildIndex)
        {
            CollectAnimatedTransformNodesRecursive(RootNode->GetChild(ChildIndex), Stack, AnimatedNodes);
        }

        return !AnimatedNodes.empty();
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

    bool ContainsTrackNode(const TArray<FbxNode*>& TrackNodes, FbxNode* Node)
    {
        return std::find(TrackNodes.begin(), TrackNodes.end(), Node) != TrackNodes.end();
    }

    FbxNode* FindRuntimeTrackParent(FbxNode* Node, const TArray<FbxNode*>& TrackNodes, int32& OutSkippedParentCount)
    {
        OutSkippedParentCount = 0;
        if (!Node)
        {
            return nullptr;
        }

        FbxNode* Parent = Node->GetParent();
        while (Parent)
        {
            if (ContainsTrackNode(TrackNodes, Parent))
            {
                return Parent;
            }

            ++OutSkippedParentCount;
            Parent = Parent->GetParent();
        }

        return nullptr;
    }

    TMap<FbxNode*, FbxNode*> BuildRuntimeTrackParentMap(
        const TArray<FbxNode*>& TrackNodes,
        int32& OutNodeCountWithSkippedParents)
    {
        TMap<FbxNode*, FbxNode*> ParentByNode;
        OutNodeCountWithSkippedParents = 0;

        for (FbxNode* Node : TrackNodes)
        {
            int32 SkippedParentCount = 0;
            FbxNode* ParentNode = FindRuntimeTrackParent(Node, TrackNodes, SkippedParentCount);
            ParentByNode[Node] = ParentNode;

            if (SkippedParentCount > 0)
            {
                ++OutNodeCountWithSkippedParents;
            }
        }

        return ParentByNode;
    }

    bool HasSuspiciousScale(const FVector& Scale)
    {
        constexpr float MinExpectedScale = 0.01f;
        constexpr float MaxExpectedScale = 100.0f;

        return std::abs(Scale.X) < MinExpectedScale ||
               std::abs(Scale.Y) < MinExpectedScale ||
               std::abs(Scale.Z) < MinExpectedScale ||
               std::abs(Scale.X) > MaxExpectedScale ||
               std::abs(Scale.Y) > MaxExpectedScale ||
               std::abs(Scale.Z) > MaxExpectedScale;
    }

    float GetUpper3x3Determinant(const FMatrix& Matrix)
    {
        const FVector XAxis = Matrix.GetScaledAxis(EAxis::X);
        const FVector YAxis = Matrix.GetScaledAxis(EAxis::Y);
        const FVector ZAxis = Matrix.GetScaledAxis(EAxis::Z);
        return FVector::DotProduct(FVector::CrossProduct(XAxis, YAxis), ZAxis);
    }

    float GetSign(float Value)
    {
        return Value < 0.0f ? -1.0f : 1.0f;
    }

    FVector MakeScaleSignConvention(const FMatrix& Matrix)
    {
        FVector ScaleSigns = FVector::OneVector;
        if (GetUpper3x3Determinant(Matrix) < 0.0f)
        {
            // A reflected matrix cannot be represented by a pure quaternion rotation.
            // Keep the reflection in one deterministic scale axis so decomposition does not
            // move it between rotation and scale from frame to frame.
            ScaleSigns.X = -1.0f;
        }

        return ScaleSigns;
    }

    struct FAnimationTrackSamplingState
    {
        bool bInitializedScaleSigns = false;
        bool bLoggedSuspiciousScale = false;
        bool bLoggedDeterminantSignChange = false;
        bool bDetectedDeterminantSignChangeThisSample = false;
        float InitialDeterminantSign = 1.0f;
        FVector ScaleSigns = FVector::OneVector;
    };

    FTransform DecomposeRuntimeLocalForAnimation(
        const FMatrix& Matrix,
        FAnimationTrackSamplingState& SamplingState)
    {
        constexpr float ScaleTolerance = 1.e-8f;

        const FVector Translation = Matrix.GetOrigin();
        const FVector XAxis = Matrix.GetScaledAxis(EAxis::X);
        const FVector YAxis = Matrix.GetScaledAxis(EAxis::Y);
        const FVector ZAxis = Matrix.GetScaledAxis(EAxis::Z);

        SamplingState.bDetectedDeterminantSignChangeThisSample = false;

        FVector Scale(XAxis.Size(), YAxis.Size(), ZAxis.Size());
        if (Scale.X <= ScaleTolerance || Scale.Y <= ScaleTolerance || Scale.Z <= ScaleTolerance)
        {
            return FTransform(Matrix);
        }

        const float DeterminantSign = GetSign(GetUpper3x3Determinant(Matrix));
        if (!SamplingState.bInitializedScaleSigns)
        {
            SamplingState.bInitializedScaleSigns = true;
            SamplingState.InitialDeterminantSign = DeterminantSign;
            SamplingState.ScaleSigns = MakeScaleSignConvention(Matrix);
        }
        else if (!SamplingState.bLoggedDeterminantSignChange &&
            DeterminantSign != SamplingState.InitialDeterminantSign)
        {
            SamplingState.bDetectedDeterminantSignChangeThisSample = true;
        }

        Scale.X *= SamplingState.ScaleSigns.X;
        Scale.Y *= SamplingState.ScaleSigns.Y;
        Scale.Z *= SamplingState.ScaleSigns.Z;

        FMatrix RotationMatrix = FMatrix::Identity;
        RotationMatrix.SetAxes(XAxis / Scale.X, YAxis / Scale.Y, ZAxis / Scale.Z);

        return FTransform(FQuat(RotationMatrix).GetNormalized(), Translation, Scale);
    }

    //1. Animation Import and Sampling Phase(UAnimSingleNodeInstance::BuildBoneMapping()으로 이어짐)
    void AppendSampledRuntimeLocalTransform(
        FbxNode* Node,
        FbxNode* RuntimeParentNode,
        const FbxTime& SampleTime,
        FRawAnimSequenceTrack& OutTrack,
        FAnimationTrackSamplingState& SamplingState)
    {
        FMatrix RuntimeLocalTransform = FMatrix::Identity;
        // 실제 animation key를 생성하는 지점입니다.
        // FBX의 LclTranslation/LclRotation/LclScaling을 직접 조립하지 않고,
        // 샘플 시점의 global transform을 평가한 뒤 런타임에서 사용하는 parent 기준 local transform으로 재계산합니다.
        // 즉, FBX 원본 local 값을 그대로 저장하지 않고, 엔진 런타임 bone 계층에 맞는 local key를 저장합니다.

        if (Node)
        {
            const FbxAMatrix GlobalTransform = Node->EvaluateGlobalTransform(SampleTime);
            const FMatrix EngineGlobalTransform = ToFMatrix(GlobalTransform);
            if (RuntimeParentNode)
            {
                const FbxAMatrix ParentGlobalTransform = RuntimeParentNode->EvaluateGlobalTransform(SampleTime);
                const FMatrix EngineParentGlobalTransform = ToFMatrix(ParentGlobalTransform);
                RuntimeLocalTransform = EngineGlobalTransform * EngineParentGlobalTransform.GetInverse();
            }
            else
            {
                RuntimeLocalTransform = EngineGlobalTransform;
            }
        }

        const FTransform EngineTransform = DecomposeRuntimeLocalForAnimation(
            RuntimeLocalTransform,
            SamplingState);

        if (SamplingState.bDetectedDeterminantSignChangeThisSample)
        {
            SamplingState.bLoggedDeterminantSignChange = true;
            UE_LOG_WARNING("[FbxAnimationImporter] Runtime local determinant sign changed while sampling | Node=%s",
                Node ? Node->GetName() : "<null>");
        }

        if (!SamplingState.bLoggedSuspiciousScale && HasSuspiciousScale(EngineTransform.GetScale3D()))
        {
            SamplingState.bLoggedSuspiciousScale = true;
            UE_LOG_WARNING("[FbxAnimationImporter] Suspicious sampled local scale | Node=%s | Scale=(%.3f, %.3f, %.3f)",
                Node ? Node->GetName() : "<null>",
                EngineTransform.GetScale3D().X,
                EngineTransform.GetScale3D().Y,
                EngineTransform.GetScale3D().Z);
        }

		//엔진에서 쓰이는 FRawAnimSequenceTrack 만들 시간입니다.
		//PosKey
        OutTrack.PosKeys.push_back(EngineTransform.GetTranslation());

		//RotKey
        FQuat Rotation = EngineTransform.GetRotation().GetNormalized();
        if (!OutTrack.RotKeys.empty())
        {
			//쿼터니언은 솔직히 잘 모르겠습니다.
            Rotation.EnforceShortestArcWith(OutTrack.RotKeys.back());
        }
        OutTrack.RotKeys.push_back(Rotation);
        
		//ScaleKey
        OutTrack.ScaleKeys.push_back(EngineTransform.GetScale3D());
    }

    UAnimSequence* BuildAnimSequenceFromStack(
        FbxScene* Scene,
        FbxAnimStack* AnimStack,
        const FString& SourcePath,
        const FFbxAnimImportOptions& ImportOptions)
    {
        if (!Scene || !AnimStack)
        {
            return nullptr;
        }

        Scene->SetCurrentAnimationStack(AnimStack);

        //FbxAnimStack 하나를 UAnimSequence 하나로 변환합니다.
        const TArray<FbxNode*> TrackNodes = CollectAnimationTrackNodes(Scene, AnimStack);
        if (TrackNodes.empty())
        {
            UE_LOG_ERROR("[FbxAnimationImporter] No skeleton or animated transform node found: %s | Stack=%s",
                SourcePath.c_str(),
                AnimStack->GetName());
            return nullptr;
        }

        const int32 SampleRate = ResolveSampleRate(ImportOptions);
        const FbxTimeSpan TimeSpan = ResolveAnimationTimeSpan(Scene, AnimStack);
        const int32 KeyCount = ComputeSampleKeyCount(TimeSpan, SampleRate);
        const double DurationSeconds = std::max(0.0, TimeSpan.GetDuration().GetSecondDouble());
        int32 NodeCountWithSkippedParents = 0;
        const TMap<FbxNode*, FbxNode*> RuntimeParentByNode =
            BuildRuntimeTrackParentMap(TrackNodes, NodeCountWithSkippedParents);

        if (NodeCountWithSkippedParents > 0)
        {
            UE_LOG("[FbxAnimationImporter] Runtime local sampling will bake skipped FBX parent transforms: %d/%zu nodes | Source=%s | Stack=%s",
                NodeCountWithSkippedParents,
                TrackNodes.size(),
                SourcePath.c_str(),
                AnimStack->GetName());
        }

        UAnimSequence* AnimSequence = UObjectManager::Get().CreateObject<UAnimSequence>();
        UAnimDataModel* DataModel = UObjectManager::Get().CreateObject<UAnimDataModel>();
        AnimSequence->SetDataModel(DataModel);
        AnimSequence->SetSourceFilePath(SourcePath);
        AnimSequence->SetSourceStackName(FString(AnimStack->GetName()));
        AnimSequence->SetPreviewMeshPath(ImportOptions.PreviewMeshPath.empty() ? SourcePath : ImportOptions.PreviewMeshPath);

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

            FbxNode* RuntimeParentNode = nullptr;
            auto ParentIt = RuntimeParentByNode.find(Node);
            if (ParentIt != RuntimeParentByNode.end())
            {
                RuntimeParentNode = ParentIt->second;
            }

            FAnimationTrackSamplingState SamplingState;
            for (int32 KeyIndex = 0; KeyIndex < KeyCount; ++KeyIndex)
            {
                const FbxTime SampleTime = MakeSampleTime(TimeSpan, KeyIndex, KeyCount);
                AppendSampledRuntimeLocalTransform(
                    Node,
                    RuntimeParentNode,
                    SampleTime,
                    Track.InternalTrack,
                    SamplingState);
            }

            Tracks.push_back(std::move(Track));
        }

        UE_LOG("[FbxAnimationImporter] Stack imported: %s | Stack=%s | Tracks=%zu | Keys=%d | Length=%.3f | SampleRate=%d",
            SourcePath.c_str(),
            AnimStack->GetName(),
            Tracks.size(),
            KeyCount,
            DurationSeconds,
            SampleRate);

        return AnimSequence;
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

        if (StackHasTransformAnimation(Scene, Stack))
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

TArray<FString> FFbxImporter::GetAnimationStackNames(const FString& Path)
{
    TArray<FString> StackNames;

    FbxManager* Manager = FbxManager::Create();
    if (!Manager)
    {
        UE_LOG_ERROR("[FbxAnimationImporter] Failed to create FbxManager for stack scan");
        return StackNames;
    }

    FbxIOSettings* IOSettings = FbxIOSettings::Create(Manager, IOSROOT);
    Manager->SetIOSettings(IOSettings);

    FbxScene* Scene = FbxScene::Create(Manager, "ScanAnimationStacksScene");
    if (!Scene)
    {
        UE_LOG_ERROR("[FbxAnimationImporter] Failed to create FbxScene for stack scan");
        Manager->Destroy();
        return StackNames;
    }

    if (!ImportScene(Path, Manager, Scene))
    {
        Manager->Destroy();
        return StackNames;
    }

    const int32 StackCount = Scene->GetSrcObjectCount<FbxAnimStack>();
    StackNames.reserve(StackCount);
    for (int32 StackIndex = 0; StackIndex < StackCount; ++StackIndex)
    {
        FbxAnimStack* Stack = Scene->GetSrcObject<FbxAnimStack>(StackIndex);
        if (Stack && StackHasTransformAnimation(Scene, Stack))
        {
            StackNames.push_back(FString(Stack->GetName()));
        }
    }

    Manager->Destroy();
    return StackNames;
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

    UAnimSequence* AnimSequence = BuildAnimSequenceFromStack(Scene, AnimStack, Path, ImportOptions);

    const double EndTime = FPlatformTime::Seconds();
    if (AnimSequence)
    {
        UE_LOG("[FbxAnimationImporter] Animation FBX loaded: %s | Stack=%s | %.3f sec",
            Path.c_str(),
            AnimStack->GetName(),
            EndTime - StartTime);
    }

    Manager->Destroy();
    return AnimSequence;
}

TArray<FFbxAnimStackImportResult> FFbxImporter::LoadAnimSequences(const FString& Path, const FFbxAnimImportOptions& ImportOptions)
{
    TArray<FFbxAnimStackImportResult> Results;

    const double StartTime = FPlatformTime::Seconds();
    UE_LOG("[FbxAnimationImporter] Start loading all animation stacks: %s", Path.c_str());

    FbxManager* Manager = FbxManager::Create();
    if (!Manager)
    {
        UE_LOG_ERROR("[FbxAnimationImporter] Failed to create FbxManager");
        return Results;
    }

    FbxIOSettings* IOSettings = FbxIOSettings::Create(Manager, IOSROOT);
    Manager->SetIOSettings(IOSettings);

    FbxScene* Scene = FbxScene::Create(Manager, "ImportAllAnimationStacksScene");
    if (!Scene)
    {
        UE_LOG_ERROR("[FbxAnimationImporter] Failed to create FbxScene");
        Manager->Destroy();
        return Results;
    }

    if (!ImportScene(Path, Manager, Scene))
    {
        Manager->Destroy();
        return Results;
    }

    const int32 StackCount = Scene->GetSrcObjectCount<FbxAnimStack>();
    if (StackCount <= 0)
    {
        UE_LOG_WARNING("[FbxAnimationImporter] No animation stack found: %s", Path.c_str());
        Manager->Destroy();
        return Results;
    }

    Results.reserve(StackCount);
    for (int32 StackIndex = 0; StackIndex < StackCount; ++StackIndex)
    {
        FbxAnimStack* AnimStack = Scene->GetSrcObject<FbxAnimStack>(StackIndex);
        if (!AnimStack)
        {
            continue;
        }

        if (!StackHasTransformAnimation(Scene, AnimStack))
        {
            UE_LOG("[FbxAnimationImporter] Skip stack with no transform animation: %s | Stack=%s",
                Path.c_str(),
                AnimStack->GetName());
            continue;
        }

        UAnimSequence* Sequence = BuildAnimSequenceFromStack(Scene, AnimStack, Path, ImportOptions);
        if (!Sequence)
        {
            continue;
        }

        FFbxAnimStackImportResult Result;
        Result.StackName = FString(AnimStack->GetName());
        Result.Sequence = Sequence;
        Results.push_back(Result);
    }

    const double EndTime = FPlatformTime::Seconds();
    UE_LOG("[FbxAnimationImporter] Imported animation stacks: %s | Count=%zu | %.3f sec",
        Path.c_str(),
        Results.size(),
        EndTime - StartTime);

    Manager->Destroy();
    return Results;
}

