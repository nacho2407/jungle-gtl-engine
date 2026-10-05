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

	struct FAnimationTrackNodeCollection
	{
		TArray<FbxNode*> TrackNodes;
		TArray<FbxNode*> AnimatedTransformNodes;
		bool bHasTransformAnimation = false;
	};

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

	void CollectAnimationTrackNodesRecursive(
		FbxNode* Node,
		FbxAnimStack* Stack,
		FAnimationTrackNodeCollection& OutCollection)
	{
		if (!Node)
		{
			return;
		}

		// 한 번의 scene 순회에서 세 가지를 같이 모은다.
		// 1) eSkeleton node: 일반적인 bone track 후보
		// 2) skin cluster link node: skeleton attribute가 빠진 FBX의 bone 후보
		// 3) transform curve가 실제로 있는 node: animation 존재 여부와 fallback track 후보
		// 이전 구현은 stack 유효성 검사용 순회와 실제 track 수집 순회를 따로 해서
		// FBX scene을 stack마다 최소 두 번 훑었다. 여기서는 같은 순회 결과를 import에 재사용한다.
		if (NodeHasTransformCurveKeys(Node, Stack))
		{
			OutCollection.bHasTransformAnimation = true;
			AddUniqueNode(OutCollection.AnimatedTransformNodes, Node);
		}

		if (IsSkeletonNode(Node))
		{
			AddUniqueNode(OutCollection.TrackNodes, Node);
		}

		if (FbxMesh* Mesh = Node->GetMesh())
		{
			CollectSkinClusterLinkNodes(Mesh, OutCollection.TrackNodes);
		}

		for (int32 ChildIndex = 0; ChildIndex < Node->GetChildCount(); ++ChildIndex)
		{
			CollectAnimationTrackNodesRecursive(Node->GetChild(ChildIndex), Stack, OutCollection);
		}
	}

	bool HasTransformAnimationRecursive(FbxNode* Node, FbxAnimStack* Stack)
	{
		if (!Node)
		{
			return false;
		}

		if (NodeHasTransformCurveKeys(Node, Stack))
		{
			return true;
		}

		for (int32 ChildIndex = 0; ChildIndex < Node->GetChildCount(); ++ChildIndex)
		{
			if (HasTransformAnimationRecursive(Node->GetChild(ChildIndex), Stack))
			{
				return true;
			}
		}

		return false;
	}

	FAnimationTrackNodeCollection CollectAnimationTrackNodes(FbxScene* Scene, FbxAnimStack* Stack)
	{
		FAnimationTrackNodeCollection Collection;
		if (!Scene || !Scene->GetRootNode() || !Stack)
		{
			return Collection;
		}

		FbxNode* RootNode = Scene->GetRootNode();
		for (int32 ChildIndex = 0; ChildIndex < RootNode->GetChildCount(); ++ChildIndex)
		{
			CollectAnimationTrackNodesRecursive(RootNode->GetChild(ChildIndex), Stack, Collection);
		}

		// 정상 skeletal FBX에서는 TrackNodes가 bone/cluster link로 채워진다.
		// 다만 transform animation만 들어있는 FBX도 열 수 있게, bone 후보가 전혀 없을 때만
		// curve가 달린 node들을 runtime track으로 사용한다.
		if (Collection.TrackNodes.empty())
		{
			Collection.TrackNodes = Collection.AnimatedTransformNodes;
		}

		return Collection;
	}

	bool StackHasTransformAnimation(FbxScene* Scene, FbxAnimStack* Stack)
	{
		if (!Scene || !Scene->GetRootNode() || !Stack)
		{
			return false;
		}

		FbxNode* RootNode = Scene->GetRootNode();
		for (int32 ChildIndex = 0; ChildIndex < RootNode->GetChildCount(); ++ChildIndex)
		{
			if (HasTransformAnimationRecursive(RootNode->GetChild(ChildIndex), Stack))
			{
				return true;
			}
		}

		return false;
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

	// Animation Import and Sampling Phase.
	// FBX 원본에는 “본 하나당 transform key 배열”이 바로 들어있는 것이 아니다.
	// 대략 다음 형태다.
	// - FbxAnimStack: 하나의 take/clip. 엔진에서는 UAnimSequence 하나에 대응시킨다.
	// - FbxAnimLayer: stack 안의 layer. FBX SDK evaluator가 현재 stack의 layer/curve를 평가한다.
	// - FbxAnimCurve: node의 LclTranslation/Rotation/Scaling 성분별 X/Y/Z curve.
	// importer는 curve 자체를 저장하지 않고, 일정한 sample rate로 EvaluateGlobalTransform()을 호출해서
	// 엔진 런타임이 바로 사용할 수 있는 FBoneAnimationTrack / FRawAnimSequenceTrack으로 bake한다.
	void BuildSampleGlobalTransformCache(
		const TArray<FbxNode*>& TrackNodes,
		const FbxTime& SampleTime,
		TArray<FMatrix>& OutGlobalTransforms)
	{
		OutGlobalTransforms.clear();
		OutGlobalTransforms.reserve(TrackNodes.size());

		for (FbxNode* Node : TrackNodes)
		{
			OutGlobalTransforms.push_back(ToFMatrix(Node->EvaluateGlobalTransform(SampleTime)));
		}
	}

	int32 FindTrackNodeIndex(const TArray<FbxNode*>& TrackNodes, FbxNode* Node)
	{
		if (!Node)
		{
			return -1;
		}

		for (int32 Index = 0; Index < static_cast<int32>(TrackNodes.size()); ++Index)
		{
			if (TrackNodes[Index] == Node)
			{
				return Index;
			}
		}
		return -1;
	}

	void AppendSampledRuntimeLocalTransform(
		FbxNode* Node,
		int32 TrackIndex,
		int32 RuntimeParentTrackIndex,
		const TArray<FMatrix>& GlobalTransforms,
		FRawAnimSequenceTrack& OutTrack,
		FAnimationTrackSamplingState& SamplingState)
	{
		// 실제 animation key를 생성하는 지점.
		// FBX의 LclTranslation/LclRotation/LclScaling 값을 직접 조립하지 않는다.
		// FBX SDK evaluator가 현재 AnimStack/Layer/Curve를 반영한 global transform을 계산하게 하고,
		// 그 결과를 엔진의 runtime parent 기준 local transform으로 다시 바꾼다.
		// 이렇게 해야 FBX 중간 노드가 animation track에는 없지만 transform 계층에는 있는 경우에도
		// 그 중간 transform이 key 안에 bake되어 런타임 bone 계층과 맞는다.
		const FMatrix EngineGlobalTransform = GlobalTransforms[TrackIndex];
		const FMatrix RuntimeLocalTransform = RuntimeParentTrackIndex >= 0
			? EngineGlobalTransform * GlobalTransforms[RuntimeParentTrackIndex].GetInverse()
			: EngineGlobalTransform;

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

		// 엔진이 재생 때 바로 접근하는 raw track에 sample 하나를 추가한다.
		OutTrack.PosKeys.push_back(EngineTransform.GetTranslation());

		FQuat Rotation = EngineTransform.GetRotation().GetNormalized();
		if (!OutTrack.RotKeys.empty())
		{
			// q와 -q는 같은 회전을 뜻하지만, 인접 key의 부호가 뒤집히면 보간 경로가 길어진다.
			// 이전 key와 같은 hemisphere에 두어 shortest-arc slerp가 안정적으로 동작하게 한다.
			Rotation.EnforceShortestArcWith(OutTrack.RotKeys.back());
		}
		OutTrack.RotKeys.push_back(Rotation);
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

		// FBX evaluator는 Scene의 CurrentAnimationStack을 기준으로 node transform을 평가한다.
		// 이 호출 이후 EvaluateGlobalTransform(Time)은 해당 AnimStack 아래 layer/curve들을 반영한다.
		Scene->SetCurrentAnimationStack(AnimStack);

		// FbxAnimStack 하나를 UAnimSequence 하나로 변환한다.
		// 여기서 track node 수집과 “이 stack에 실제 transform curve가 있는지” 판정을 한 번에 끝낸다.
		const FAnimationTrackNodeCollection TrackNodeCollection = CollectAnimationTrackNodes(Scene, AnimStack);
		if (!TrackNodeCollection.bHasTransformAnimation)
		{
			UE_LOG("[FbxAnimationImporter] Skip stack with no transform animation: %s | Stack=%s",
				SourcePath.c_str(),
				AnimStack->GetName());
			return nullptr;
		}

		const TArray<FbxNode*>& TrackNodes = TrackNodeCollection.TrackNodes;
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

		TArray<FbxNode*> ImportedTrackNodes;
		ImportedTrackNodes.reserve(TrackNodes.size());
		TArray<FAnimationTrackSamplingState> SamplingStates;
		SamplingStates.reserve(TrackNodes.size());
		TArray<int32> RuntimeParentTrackIndices;
		RuntimeParentTrackIndices.reserve(TrackNodes.size());

		for (FbxNode* Node : TrackNodes)
		{
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

			RuntimeParentTrackIndices.push_back(FindTrackNodeIndex(TrackNodes, RuntimeParentNode));
			SamplingStates.push_back(FAnimationTrackSamplingState());
			ImportedTrackNodes.push_back(Node);
			Tracks.push_back(std::move(Track));
		}

		TArray<FMatrix> GlobalTransforms;
		GlobalTransforms.reserve(ImportedTrackNodes.size());

		for (int32 KeyIndex = 0; KeyIndex < KeyCount; ++KeyIndex)
		{
			const FbxTime SampleTime = MakeSampleTime(TimeSpan, KeyIndex, KeyCount);
			BuildSampleGlobalTransformCache(ImportedTrackNodes, SampleTime, GlobalTransforms);

			for (int32 TrackIndex = 0; TrackIndex < static_cast<int32>(Tracks.size()); ++TrackIndex)
			{
				FbxNode* Node = ImportedTrackNodes[TrackIndex];
				AppendSampledRuntimeLocalTransform(
					Node,
					TrackIndex,
					RuntimeParentTrackIndices[TrackIndex],
					GlobalTransforms,
					Tracks[TrackIndex].InternalTrack,
					SamplingStates[TrackIndex]);
			}
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

		FFbxAnimImportOptions StackImportOptions = ImportOptions;
		StackImportOptions.StackName = FString(AnimStack->GetName());

		UAnimSequence* Sequence = BuildAnimSequenceFromStack(Scene, AnimStack, Path, StackImportOptions);
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

