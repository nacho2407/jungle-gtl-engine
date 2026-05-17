#include "Mesh/Fbx/FbxAnimationImporter.h"
#include "Mesh/Fbx/FbxTransformUtils.h"
#include "Animation/AnimationRuntime.h"
#include "Animation/AnimDataModel.h"
#include "Animation/AnimSequence.h"
#include "Animation/SkeletonTypes.h"
#include "Math/Transform.h"
#include "Math/Rotator.h"
#include "Object/Object.h"
#include "Core/Log.h"

#include <algorithm>
#include <cmath>

namespace
{
	enum class EFbxAnimationBakePolicy : uint8
	{
		DirectBaseLayerOnly,
		DirectLayeredOnly,
		DirectLayeredWithSdkFallback,
		SdkEvaluatorOnly
	};

	static constexpr EFbxAnimationBakePolicy GAnimationBakePolicy      = EFbxAnimationBakePolicy::DirectLayeredOnly;
	static constexpr double                  GDirectBakeErrorTolerance = 0.001;
	
	static float GetSceneSampleRate(FbxScene* Scene)
	{
		if (!Scene)
		{
			return 30.0f;
		}

		const FbxTime::EMode TimeMode = Scene->GetGlobalSettings().GetTimeMode();
		const double         Rate     = FbxTime::GetFrameRate(TimeMode);

		return Rate > 1.0f ? static_cast<float>(Rate) : 30.0f;
	}

	static bool TryResolveAnimationTimeRange(FbxScene* Scene, FbxAnimStack* AnimStack, double& OutStartSecond, double& OutEndSecond)
	{
		if (!Scene || !AnimStack)
		{
			return false;
		}

		auto TrySpan = [&](const FbxTimeSpan& Span) -> bool
		{
			const double Start = Span.GetStart().GetSecondDouble();
			const double End   = Span.GetStop().GetSecondDouble();

			if (End <= Start)
			{
				return false;
			}

			OutStartSecond = Start;
			OutEndSecond   = End;
			return true;
		};

		if (TrySpan(AnimStack->GetLocalTimeSpan()))
		{
			return true;
		}

		if (TrySpan(AnimStack->GetReferenceTimeSpan()))
		{
			return true;
		}

		FbxTimeSpan TimelineSpan;
		Scene->GetGlobalSettings().GetTimelineDefaultTimeSpan(TimelineSpan);
		return TrySpan(TimelineSpan);
	}

	static FString MakeSafeAnimationName(const char* RawName)
	{
		FString Result = RawName ? FString(RawName) : FString();

		for (char& C : Result)
		{
			const bool bValid = (C >= 'a' && C <= 'z') || (C >= 'A' && C <= 'Z') || (C >= '0' && C <= '9') || C == '_' || C == '-';

			if (!bValid)
			{
				C = '_';
			}
		}

		if (Result.empty())
		{
			Result = "Anim";
		}

		return Result;
	}

	static FString MakeUniqueAnimationName(const FString& BaseName, TSet<FString>& UsedNames)
	{
		const FString SafeBase = BaseName.empty() ? FString("Anim") : BaseName;

		FString Candidate = SafeBase;
		int32   Suffix    = 1;

		while (UsedNames.find(Candidate) != UsedNames.end())
		{
			Candidate = SafeBase + "_" + std::to_string(Suffix++);
		}

		UsedNames.insert(Candidate);
		return Candidate;
	}

	struct FFbxTransformCurveSet
	{
		FbxAnimCurve* Translation[3] = { nullptr, nullptr, nullptr };
		FbxAnimCurve* Rotation[3]    = { nullptr, nullptr, nullptr };
		FbxAnimCurve* Scale[3]       = { nullptr, nullptr, nullptr };

		bool HasAnyCurve() const
		{
			return Translation[0] || Translation[1] || Translation[2] || Rotation[0] || Rotation[1] || Rotation[2] || Scale[0] || Scale[1] || Scale[2];
		}
	};

	static FFbxTransformCurveSet GetTransformCurveSet(FbxNode* Node, FbxAnimLayer* AnimLayer)
	{
		FFbxTransformCurveSet Curves;

		if (!Node || !AnimLayer)
		{
			return Curves;
		}

		const char* Axes[3] = { FBXSDK_CURVENODE_COMPONENT_X, FBXSDK_CURVENODE_COMPONENT_Y, FBXSDK_CURVENODE_COMPONENT_Z };

		for (int32 AxisIndex = 0; AxisIndex < 3; ++AxisIndex)
		{
			Curves.Translation[AxisIndex] = Node->LclTranslation.GetCurve(AnimLayer, Axes[AxisIndex]);
			Curves.Rotation[AxisIndex]    = Node->LclRotation.GetCurve(AnimLayer, Axes[AxisIndex]);
			Curves.Scale[AxisIndex]       = Node->LclScaling.GetCurve(AnimLayer, Axes[AxisIndex]);
		}

		return Curves;
	}

	static double EvaluateCurveOrDefault(FbxAnimCurve* Curve, const FbxTime& Time, double DefaultValue)
	{
		return Curve ? Curve->Evaluate(Time) : DefaultValue;
	}

	static FbxDouble3 EvaluateFbxPropertyCurve(FbxPropertyT<FbxDouble3>& Property, FbxAnimLayer* AnimLayer, const FbxTime& Time)
	{
		const FbxDouble3 DefaultValue = Property.Get();

		if (!AnimLayer)
		{
			return DefaultValue;
		}

		const char* Axes[3] = { FBXSDK_CURVENODE_COMPONENT_X, FBXSDK_CURVENODE_COMPONENT_Y, FBXSDK_CURVENODE_COMPONENT_Z };

		FbxDouble3 Result;
		for (int32 AxisIndex = 0; AxisIndex < 3; ++AxisIndex)
		{
			FbxAnimCurve* Curve = Property.GetCurve(AnimLayer, Axes[AxisIndex]);
			Result[AxisIndex]   = EvaluateCurveOrDefault(Curve, Time, DefaultValue[AxisIndex]);
		}

		return Result;
	}

	static FbxAMatrix MakeFbxTranslationMatrix(const FbxVector4& Translation)
	{
		FbxAMatrix Matrix;
		Matrix.SetIdentity();
		Matrix.SetT(Translation);
		return Matrix;
	}

	static FbxAMatrix MakeFbxScalingMatrix(const FbxVector4& Scale)
	{
		FbxAMatrix Matrix;
		Matrix.SetIdentity();
		Matrix.SetS(Scale);
		return Matrix;
	}

	static FbxAMatrix MakeFbxAxisRotationMatrix(int32 AxisIndex, double Degree)
	{
		FbxVector4 Euler(0.0, 0.0, 0.0, 0.0);
		Euler[AxisIndex] = Degree;

		FbxAMatrix Matrix;
		Matrix.SetIdentity();
		Matrix.SetR(Euler);
		return Matrix;
	}

	static FbxAMatrix MakeFbxRotationMatrixByOrder(const FbxVector4& EulerDegree, EFbxRotationOrder RotationOrder)
	{
		const FbxAMatrix RX = MakeFbxAxisRotationMatrix(0, EulerDegree[0]);
		const FbxAMatrix RY = MakeFbxAxisRotationMatrix(1, EulerDegree[1]);
		const FbxAMatrix RZ = MakeFbxAxisRotationMatrix(2, EulerDegree[2]);

		// FBX SDK 규약: 회전 순서 A-B-C(A 먼저 적용)는 행렬 합성에서 RC * RB * RA 로 역순.
		// (fbxaffinematrix.h 문서 예제: XYZ → lRotateZM * lRotateYM * lRotateXM)
		switch (RotationOrder)
		{
		case eEulerXYZ:
			return RZ * RY * RX;
		case eEulerXZY:
			return RY * RZ * RX;
		case eEulerYZX:
			return RX * RZ * RY;
		case eEulerYXZ:
			return RZ * RX * RY;
		case eEulerZXY:
			return RY * RX * RZ;
		case eEulerZYX:
			return RX * RY * RZ;
		case eSphericXYZ: default:
			return RZ * RY * RX;
		}
	}

	static FbxAMatrix EvaluateLocalFbxMatrixFromCurves(FbxNode* Node, FbxAnimLayer* AnimLayer, const FbxTime& Time)
	{
		FbxAMatrix Identity;
		Identity.SetIdentity();

		if (!Node)
		{
			return Identity;
		}

		const FbxDouble3 TranslationValue = EvaluateFbxPropertyCurve(Node->LclTranslation, AnimLayer, Time);
		const FbxDouble3 RotationValue    = EvaluateFbxPropertyCurve(Node->LclRotation, AnimLayer, Time);
		const FbxDouble3 ScaleValue       = EvaluateFbxPropertyCurve(Node->LclScaling, AnimLayer, Time);

		const FbxVector4 LclTranslation = FbxVector4(TranslationValue[0], TranslationValue[1], TranslationValue[2], 0.0f);
		const FbxVector4 LclRotation    = FbxVector4(RotationValue[0], RotationValue[1], RotationValue[2], 0.0f);
		const FbxVector4 LclScaling     = FbxVector4(ScaleValue[0], ScaleValue[1], ScaleValue[2], 0.0f);

		EFbxRotationOrder RotationOrder = eEulerXYZ;
		Node->GetRotationOrder(FbxNode::eSourcePivot, RotationOrder);

		const bool bRotationActive = Node->GetRotationActive();

		const FbxAMatrix TranslationMatrix    = MakeFbxTranslationMatrix(LclTranslation);
		const FbxAMatrix RotationOffsetMatrix = MakeFbxTranslationMatrix(Node->GetRotationOffset(FbxNode::eSourcePivot));
		const FbxAMatrix RotationPivotMatrix  = MakeFbxTranslationMatrix(Node->GetRotationPivot(FbxNode::eSourcePivot));
		const FbxAMatrix PreRotationMatrix    = bRotationActive ? MakeFbxRotationMatrixByOrder(Node->GetPreRotation(FbxNode::eSourcePivot), RotationOrder)
		: Identity;
		const FbxAMatrix RotationMatrix = MakeFbxRotationMatrixByOrder(LclRotation, RotationOrder);

		const FbxAMatrix PostRotationMatrix = bRotationActive ? MakeFbxRotationMatrixByOrder(Node->GetPostRotation(FbxNode::eSourcePivot), RotationOrder)
		: Identity;

		const FbxAMatrix ScalingOffsetMatrix = MakeFbxTranslationMatrix(Node->GetScalingOffset(FbxNode::eSourcePivot));

		const FbxAMatrix ScalingPivotMatrix = MakeFbxTranslationMatrix(Node->GetScalingPivot(FbxNode::eSourcePivot));

		const FbxAMatrix ScalingMatrix = MakeFbxScalingMatrix(LclScaling);

		return TranslationMatrix * RotationOffsetMatrix * RotationPivotMatrix * PreRotationMatrix * RotationMatrix * PostRotationMatrix.Inverse() *
		RotationPivotMatrix.Inverse() * ScalingOffsetMatrix * ScalingPivotMatrix * ScalingMatrix * ScalingPivotMatrix.Inverse();
	}

	static FTransform EvaluateLocalTransformFromCurves(FbxNode* Node, FbxAnimLayer* AnimLayer, const FbxTime& Time)
	{
		const FbxAMatrix LocalMatrix = EvaluateLocalFbxMatrixFromCurves(Node, AnimLayer, Time);
		return FTransform(FFbxTransformUtils::ToEngineMatrix(LocalMatrix));
	}

	static double ComputeFbxMatrixMaxError(const FbxMatrix& A, const FbxMatrix& B)
	{
		double MaxError = 0.0;

		for (int32 Row = 0; Row < 4; ++Row)
		{
			for (int32 Col = 0; Col < 4; ++Col)
			{
				MaxError = std::max(MaxError, std::abs(A.Get(Row, Col) - B.Get(Row, Col)));
			}
		}

		return MaxError;
	}

	static int32 CountSourceCurveKeys(const UAnimDataModel* DataModel)
	{
		if (!DataModel)
		{
			return 0;
		}

		int32 Count = 0;
		for (const FBoneAnimationTrack& Track : DataModel->BoneAnimationTracks)
		{
			for (const FSourceTransformCurveLayer& Layer : Track.InternalTrackData.SourceCurveLayers)
			{
				Count += static_cast<int32>(Layer.Translation.X.Keys.size());
				Count += static_cast<int32>(Layer.Translation.Y.Keys.size());
				Count += static_cast<int32>(Layer.Translation.Z.Keys.size());
				Count += static_cast<int32>(Layer.Rotation.X.Keys.size());
				Count += static_cast<int32>(Layer.Rotation.Y.Keys.size());
				Count += static_cast<int32>(Layer.Rotation.Z.Keys.size());
				Count += static_cast<int32>(Layer.Scale.X.Keys.size());
				Count += static_cast<int32>(Layer.Scale.Y.Keys.size());
				Count += static_cast<int32>(Layer.Scale.Z.Keys.size());
			}
		}

		return Count;
	}

	static int32 CountTransformCurveKeys(const FFbxTransformCurveSet& Curves)
	{
		int32 Count = 0;
		for (int32 AxisIndex = 0; AxisIndex < 3; ++AxisIndex)
		{
			Count += Curves.Translation[AxisIndex] ? Curves.Translation[AxisIndex]->KeyGetCount() : 0;
			Count += Curves.Rotation[AxisIndex] ? Curves.Rotation[AxisIndex]->KeyGetCount() : 0;
			Count += Curves.Scale[AxisIndex] ? Curves.Scale[AxisIndex]->KeyGetCount() : 0;
		}
		return Count;
	}

	static int32 CountAnimatedCurveBones(const TMap<FbxNode*, int32>& NodeToIndex, FbxAnimLayer* AnimLayer)
	{
		int32 Count = 0;
		for (const auto& Pair : NodeToIndex)
		{
			if (GetTransformCurveSet(Pair.first, AnimLayer).HasAnyCurve())
			{
				++Count;
			}
		}
		return Count;
	}

	static int32 CountTransformCurveKeys(const TMap<FbxNode*, int32>& NodeToIndex, FbxAnimLayer* AnimLayer)
	{
		int32 Count = 0;
		for (const auto& Pair : NodeToIndex)
		{
			Count += CountTransformCurveKeys(GetTransformCurveSet(Pair.first, AnimLayer));
		}
		return Count;
	}

	static void CopyFbxFloatCurve(FbxAnimCurve* SourceCurve, double StartSeconds, FRawFloatCurve& OutCurve)
	{
		OutCurve.Keys.clear();

		if (!SourceCurve)
		{
			return;
		}

		const int32 KeyCount = SourceCurve->KeyGetCount();
		OutCurve.Keys.reserve(KeyCount);

		for (int32 KeyIndex = 0; KeyIndex < KeyCount; ++KeyIndex)
		{
			FRawFloatCurveKey NewKey;

			NewKey.TimeSeconds   = static_cast<float>(SourceCurve->KeyGetTime(KeyIndex).GetSecondDouble() - StartSeconds);
			NewKey.Value         = static_cast<float>(SourceCurve->KeyGetValue(KeyIndex));
			NewKey.Interpolation = static_cast<int32>(SourceCurve->KeyGetInterpolation(KeyIndex));
			NewKey.TangentMode   = static_cast<int32>(SourceCurve->KeyGetTangentMode(KeyIndex));

			NewKey.ArriveTangent = SourceCurve->KeyGetLeftDerivative(KeyIndex);
			NewKey.LeaveTangent  = SourceCurve->KeyGetRightDerivative(KeyIndex);

			NewKey.bArriveTangentWeighted = SourceCurve->KeyIsLeftTangentWeighted(KeyIndex);
			NewKey.bLeaveTangentWeighted  = SourceCurve->KeyIsRightTangentWeighted(KeyIndex);

			if (NewKey.bArriveTangentWeighted)
			{
				NewKey.ArriveTangentWeight = SourceCurve->KeyGetLeftTangentWeight(KeyIndex);
			}
			if (NewKey.bLeaveTangentWeighted)
			{
				NewKey.LeaveTangentWeight = SourceCurve->KeyGetRightTangentWeight(KeyIndex);
			}

			OutCurve.Keys.push_back(NewKey);
		}
	}

	static void CopyFbxVectorCurve(FbxAnimCurve* CurveX, FbxAnimCurve* CurveY, FbxAnimCurve* CurveZ, double StartSeconds, FRawVectorCurve& OutCurve)
	{
		CopyFbxFloatCurve(CurveX, StartSeconds, OutCurve.X);
		CopyFbxFloatCurve(CurveY, StartSeconds, OutCurve.Y);
		CopyFbxFloatCurve(CurveZ, StartSeconds, OutCurve.Z);
	}

	static float NormalizeFbxLayerWeight(double RawWeight)
	{
		const double Normalized = RawWeight > 1.0 ? RawWeight / 100.0 : RawWeight;
		return std::clamp(static_cast<float>(Normalized), 0.0f, 1.0f);
	}

	static void CopySourceTransformCurves(FbxNode* Node, FbxAnimStack* AnimStack, double StartSeconds, FRawAnimSequenceTrack& OutRawTrack)
	{
		OutRawTrack.SourceCurveLayers.clear();

		if (!Node || !AnimStack)
		{
			return;
		}

		const char* Axes[3] = { FBXSDK_CURVENODE_COMPONENT_X, FBXSDK_CURVENODE_COMPONENT_Y, FBXSDK_CURVENODE_COMPONENT_Z };

		const int32 LayerCount = AnimStack->GetMemberCount<FbxAnimLayer>();

		for (int32 LayerIndex = 0; LayerIndex < LayerCount; ++LayerIndex)
		{
			FbxAnimLayer* AnimLayer = AnimStack->GetMember<FbxAnimLayer>(LayerIndex);
			if (!AnimLayer)
			{
				continue;
			}

			FSourceTransformCurveLayer LayerCurves;
			LayerCurves.LayerIndex = LayerIndex;
			LayerCurves.LayerName  = AnimLayer->GetName() ? FString(AnimLayer->GetName()) : FString();

			LayerCurves.LayerWeight = NormalizeFbxLayerWeight(AnimLayer->Weight.Get());
			LayerCurves.bMute       = AnimLayer->Mute.Get();
			LayerCurves.bSolo       = AnimLayer->Solo.Get();

			LayerCurves.BlendMode                = static_cast<int32>(AnimLayer->BlendMode.Get());
			LayerCurves.RotationAccumulationMode = static_cast<int32>(AnimLayer->RotationAccumulationMode.Get());
			LayerCurves.ScaleAccumulationMode    = static_cast<int32>(AnimLayer->ScaleAccumulationMode.Get());

			CopyFbxVectorCurve(
				Node->LclTranslation.GetCurve(AnimLayer, Axes[0]),
				Node->LclTranslation.GetCurve(AnimLayer, Axes[1]),
				Node->LclTranslation.GetCurve(AnimLayer, Axes[2]),
				StartSeconds,
				LayerCurves.Translation
			); 

			CopyFbxVectorCurve(
				Node->LclRotation.GetCurve(AnimLayer, Axes[0]),
				Node->LclRotation.GetCurve(AnimLayer, Axes[1]),
				Node->LclRotation.GetCurve(AnimLayer, Axes[2]),
				StartSeconds,
				LayerCurves.Rotation
			);

			CopyFbxVectorCurve(
				Node->LclScaling.GetCurve(AnimLayer, Axes[0]),
				Node->LclScaling.GetCurve(AnimLayer, Axes[1]),
				Node->LclScaling.GetCurve(AnimLayer, Axes[2]),
				StartSeconds,
				LayerCurves.Scale
			);

			if (LayerCurves.HasAnyKeys())
			{
				OutRawTrack.SourceCurveLayers.push_back(LayerCurves);
			}
		}
	}

	static int32 CountAnimatedCurveBonesAllLayers(const TMap<FbxNode*, int32>& NodeToIndex, FbxAnimStack* AnimStack)
	{
		if (!AnimStack)
		{
			return 0;
		}

		int32       Count      = 0;
		const int32 LayerCount = AnimStack->GetMemberCount<FbxAnimLayer>();

		for (const auto& Pair : NodeToIndex)
		{
			FbxNode* Node         = Pair.first;
			bool     bHasAnyCurve = false;

			for (int32 LayerIndex = 0; LayerIndex < LayerCount; ++LayerIndex)
			{
				FbxAnimLayer* Layer = AnimStack->GetMember<FbxAnimLayer>(LayerIndex);
				if (Layer && GetTransformCurveSet(Node, Layer).HasAnyCurve())
				{
					bHasAnyCurve = true;
					break;
				}
			}

			if (bHasAnyCurve)
			{
				++Count;
			}
		}
		return Count;
	}

	static int32 CountTransformCurveKeysAllLayers(const TMap<FbxNode*, int32>& NodeToIndex, FbxAnimStack* AnimStack)
	{
		if (!AnimStack)
		{
			return 0;
		}

		int32       Count      = 0;
		const int32 LayerCount = AnimStack->GetMemberCount<FbxAnimLayer>();

		for (int32 LayerIndex = 0; LayerIndex < LayerCount; ++LayerIndex)
		{
			FbxAnimLayer* Layer = AnimStack->GetMember<FbxAnimLayer>(LayerIndex);
			if (!Layer)
			{
				continue;
			}

			Count += CountTransformCurveKeys(NodeToIndex, Layer);
		}
		return Count;
	}

	struct FFbxLayerTransformSample
	{
		FVector Translation = FVector::ZeroVector;
		FVector Rotation    = FVector::ZeroVector;
		FVector Scale       = FVector::OneVector;

		bool bHasTranslation = false;
		bool bHasRotation    = false;
		bool bHasScale       = false;

		float Weight = 1.0f;

		FbxAnimLayer::EBlendMode                BlendMode                = FbxAnimLayer::eBlendAdditive;
		FbxAnimLayer::ERotationAccumulationMode RotationAccumulationMode = FbxAnimLayer::eRotationByLayer;
		FbxAnimLayer::EScaleAccumulationMode    ScaleAccumulationMode    = FbxAnimLayer::eScaleMultiply;
	};

	struct FCompositedFbxLocalTRS
	{
		FVector Translation  = FVector::ZeroVector;
		FVector Rotation     = FVector::ZeroVector;
		FQuat   RotationQuat = FQuat::Identity;
		FVector Scale        = FVector::OneVector;

		bool bUseQuatRotation = false;
	};

	struct FFbxBakeMatrixResult
	{
		FbxAMatrix FinalMatrix;
		FbxAMatrix DirectMatrix;
		FbxAMatrix SdkMatrix;

		double Error               = 0.0;
		bool   bUsedSdkFallback    = false;
		bool   bHasDirectCandidate = false;
	};

	struct FFbxLayeredBakeStats
	{
		int32  TestedSamples   = 0;
		int32  FallbackSamples = 0;
		double MaxError        = 0.0;
	};

	static FVector ToFVector(const FbxDouble3& V)
	{
		return FVector(static_cast<float>(V[0]), static_cast<float>(V[1]), static_cast<float>(V[2]));
	}

	static FVector ToFVector(const FbxVector4& V)
	{
		return FVector(static_cast<float>(V[0]), static_cast<float>(V[1]), static_cast<float>(V[2]));
	}

	static FbxVector4 ToFbxVector4(const FVector& V)
	{
		return FbxVector4(static_cast<double>(V.X), static_cast<double>(V.Y), static_cast<double>(V.Z), 0.0);
	}

	static bool HasAnySoloLayer(FbxAnimStack* AnimStack)
	{
		if (!AnimStack)
		{
			return false;
		}

		const int32 LayerCount = AnimStack->GetMemberCount<FbxAnimLayer>();
		for (int32 LayerIndex = 0; LayerIndex < LayerCount; ++LayerIndex)
		{
			FbxAnimLayer* Layer = AnimStack->GetMember<FbxAnimLayer>(LayerIndex);
			if (Layer && Layer->Solo.Get())
			{
				return true;
			}
		}
		return false;
	}

	static bool ShouldUseAnimLayer(FbxAnimLayer* Layer, bool bHasSoloLayer)
	{
		if (!Layer)
		{
			return false;
		}

		if (bHasSoloLayer)
		{
			return Layer->Solo.Get();
		}

		return !Layer->Mute.Get();
	}

	static float EvaluateAnimLayerWeight(FbxAnimLayer* Layer, const FbxTime& Time)
	{
		if (!Layer)
		{
			return 0.0f;
		}

		double RawWeight = Layer->Weight.Get();
		if (FbxAnimCurve* WeightCurve = Layer->Weight.GetCurve(Layer))
		{
			RawWeight = WeightCurve->Evaluate(Time);
		}

		return NormalizeFbxLayerWeight(RawWeight);
	}

	static FFbxLayerTransformSample EvaluateLayerTransformSample(FbxNode* Node, FbxAnimLayer* Layer, const FbxTime& Time)
	{
		FFbxLayerTransformSample Sample;

		if (!Node || !Layer)
		{
			return Sample;
		}

		const FFbxTransformCurveSet Curves = GetTransformCurveSet(Node, Layer);

		const FbxDouble3 T = EvaluateFbxPropertyCurve(Node->LclTranslation, Layer, Time);
		const FbxDouble3 R = EvaluateFbxPropertyCurve(Node->LclRotation, Layer, Time);
		const FbxDouble3 S = EvaluateFbxPropertyCurve(Node->LclScaling, Layer, Time);

		Sample.Translation = ToFVector(T);
		Sample.Rotation    = ToFVector(R);
		Sample.Scale       = ToFVector(S);

		Sample.bHasTranslation = Curves.Translation[0] || Curves.Translation[1] || Curves.Translation[2];
		Sample.bHasRotation    = Curves.Rotation[0] || Curves.Rotation[1] || Curves.Rotation[2];
		Sample.bHasScale       = Curves.Scale[0] || Curves.Scale[1] || Curves.Scale[2];

		Sample.Weight = EvaluateAnimLayerWeight(Layer, Time);

		Sample.BlendMode                = static_cast<FbxAnimLayer::EBlendMode>(Layer->BlendMode.Get());
		Sample.RotationAccumulationMode = static_cast<FbxAnimLayer::ERotationAccumulationMode>(Layer->RotationAccumulationMode.Get());
		Sample.ScaleAccumulationMode    = static_cast<FbxAnimLayer::EScaleAccumulationMode>(Layer->ScaleAccumulationMode.Get());

		return Sample;
	}

	static FVector ComponentMultiply(const FVector& A, const FVector& B)
	{
		return FVector(A.X * B.X, A.Y * B.Y, A.Z * B.Z);
	}

	static FVector ComponentDivideSafe(const FVector& A, const FVector& B)
	{
		return FVector(std::abs(B.X) > EPSILON ? A.X / B.X : 1.0f, std::abs(B.Y) > EPSILON ? A.Y / B.Y : 1.0f, std::abs(B.Z) > EPSILON ? A.Z / B.Z : 1.0f);
	}

	static float SafePowScale(float Value, float Weight)
	{
		if (Value < 0.0f)
		{
			return -std::pow(std::abs(Value), Weight);
		}
		return std::pow(Value, Weight);
	}

	static FVector PowScaleVector(const FVector& V, float Weight)
	{
		return FVector(SafePowScale(V.X, Weight), SafePowScale(V.Y, Weight), SafePowScale(V.Z, Weight));
	}

	static FVector BlendTranslationLayer(
		const FVector&           Current,
		const FVector&           DefaultValue,
		const FVector&           LayerValue,
		float                    Weight,
		FbxAnimLayer::EBlendMode BlendMode
		)
	{
		switch (BlendMode)
		{
		case FbxAnimLayer::eBlendAdditive:
			return Current + (LayerValue - DefaultValue) * Weight;

		case FbxAnimLayer::eBlendOverride:
			return LayerValue;

		case FbxAnimLayer::eBlendOverridePassthrough: default:
			return FVector::Lerp(Current, LayerValue, Weight);
		}
	}

	static FVector BlendRotationByChannel(
		const FVector&           CurrentEuler,
		const FVector&           DefaultEuler,
		const FVector&           LayerEuler,
		float                    Weight,
		FbxAnimLayer::EBlendMode BlendMode
		)
	{
		switch (BlendMode)
		{
		case FbxAnimLayer::eBlendAdditive:
			return CurrentEuler + (LayerEuler - DefaultEuler) * Weight;

		case FbxAnimLayer::eBlendOverride:
			return LayerEuler;

		case FbxAnimLayer::eBlendOverridePassthrough: default:
			return FVector::Lerp(CurrentEuler, LayerEuler, Weight);
		}
	}

	static FQuat MakeQuatFromFbxEulerDegree(const FVector& EulerDegree, EFbxRotationOrder RotationOrder)
	{
		const FbxVector4 FbxEuler(static_cast<double>(EulerDegree.X), static_cast<double>(EulerDegree.Y), static_cast<double>(EulerDegree.Z), 0.0);

		const FbxAMatrix RotationMatrix = MakeFbxRotationMatrixByOrder(FbxEuler, RotationOrder);
		return FTransform(FFbxTransformUtils::ToEngineMatrix(RotationMatrix)).Rotation.GetNormalized();
	}

	static FQuat BlendRotationByLayer(const FQuat& Current, const FQuat& DefaultQuat, const FQuat& LayerQuat, float Weight, FbxAnimLayer::EBlendMode BlendMode)
	{
		switch (BlendMode)
		{
		case FbxAnimLayer::eBlendAdditive:
		{
			const FQuat Delta         = (DefaultQuat.Inverse() * LayerQuat).GetNormalized();
			const FQuat WeightedDelta = FQuat::Slerp(FQuat::Identity, Delta, Weight);
			return (Current * WeightedDelta).GetNormalized();
		}

		case FbxAnimLayer::eBlendOverride:
			return LayerQuat.GetNormalized();

		case FbxAnimLayer::eBlendOverridePassthrough: default:
			return FQuat::Slerp(Current, LayerQuat, Weight).GetNormalized();
		}
	}

	static FVector BlendScaleLayer(
		const FVector&                       Current,
		const FVector&                       DefaultScale,
		const FVector&                       LayerScale,
		float                                Weight,
		FbxAnimLayer::EBlendMode             BlendMode,
		FbxAnimLayer::EScaleAccumulationMode ScaleAccumulationMode
		)
	{
		switch (BlendMode)
		{
		case FbxAnimLayer::eBlendAdditive:
		{
			if (ScaleAccumulationMode == FbxAnimLayer::eScaleMultiply)
			{
				const FVector Ratio = ComponentDivideSafe(LayerScale, DefaultScale);
				return ComponentMultiply(Current, PowScaleVector(Ratio, Weight));
			}

			return Current + (LayerScale - DefaultScale) * Weight;
		}

		case FbxAnimLayer::eBlendOverride:
			return LayerScale;

		case FbxAnimLayer::eBlendOverridePassthrough: default:
			return FVector::Lerp(Current, LayerScale, Weight);
		}
	}

	static FbxAMatrix MakeFbxRotationMatrixFromQuat(const FQuat& Quat)
	{
		FbxAMatrix Matrix;
		Matrix.SetIdentity();

		const FQuat Q = Quat.GetNormalized();
		Matrix.SetQ(FbxQuaternion(Q.X, Q.Y, Q.Z, Q.W));
		return Matrix;
	}

	static FbxAMatrix MakeLocalFbxMatrixFromTRS(FbxNode* Node, const FVector& Translation, const FVector& RotationEulerDegree, const FVector& Scale)
	{
		FbxAMatrix Identity;
		Identity.SetIdentity();

		if (!Node)
		{
			return Identity;
		}

		EFbxRotationOrder RotationOrder = eEulerXYZ;
		Node->GetRotationOrder(FbxNode::eSourcePivot, RotationOrder);

		const bool bRotationActive = Node->GetRotationActive();

		const FbxAMatrix TranslationMatrix    = MakeFbxTranslationMatrix(ToFbxVector4(Translation));
		const FbxAMatrix RotationOffsetMatrix = MakeFbxTranslationMatrix(Node->GetRotationOffset(FbxNode::eSourcePivot));
		const FbxAMatrix RotationPivotMatrix  = MakeFbxTranslationMatrix(Node->GetRotationPivot(FbxNode::eSourcePivot));
		const FbxAMatrix PreRotationMatrix    = bRotationActive ? MakeFbxRotationMatrixByOrder(Node->GetPreRotation(FbxNode::eSourcePivot), RotationOrder)
		: Identity;
		const FbxAMatrix RotationMatrix     = MakeFbxRotationMatrixByOrder(ToFbxVector4(RotationEulerDegree), RotationOrder);
		const FbxAMatrix PostRotationMatrix = bRotationActive ? MakeFbxRotationMatrixByOrder(Node->GetPostRotation(FbxNode::eSourcePivot), RotationOrder)
		: Identity;
		const FbxAMatrix ScalingOffsetMatrix = MakeFbxTranslationMatrix(Node->GetScalingOffset(FbxNode::eSourcePivot));
		const FbxAMatrix ScalingPivotMatrix  = MakeFbxTranslationMatrix(Node->GetScalingPivot(FbxNode::eSourcePivot));
		const FbxAMatrix ScalingMatrix       = MakeFbxScalingMatrix(ToFbxVector4(Scale));

		return TranslationMatrix * RotationOffsetMatrix * RotationPivotMatrix * PreRotationMatrix * RotationMatrix * PostRotationMatrix.Inverse() *
		RotationPivotMatrix.Inverse() * ScalingOffsetMatrix * ScalingPivotMatrix * ScalingMatrix * ScalingPivotMatrix.Inverse();
	}

	static FbxAMatrix MakeLocalFbxMatrixFromTRSQuat(FbxNode* Node, const FVector& Translation, const FQuat& RotationQuat, const FVector& Scale)
	{
		FbxAMatrix Identity;
		Identity.SetIdentity();

		if (!Node)
		{
			return Identity;
		}

		EFbxRotationOrder RotationOrder = eEulerXYZ;
		Node->GetRotationOrder(FbxNode::eSourcePivot, RotationOrder);

		const bool bRotationActive = Node->GetRotationActive();

		const FbxAMatrix TranslationMatrix    = MakeFbxTranslationMatrix(ToFbxVector4(Translation));
		const FbxAMatrix RotationOffsetMatrix = MakeFbxTranslationMatrix(Node->GetRotationOffset(FbxNode::eSourcePivot));
		const FbxAMatrix RotationPivotMatrix  = MakeFbxTranslationMatrix(Node->GetRotationPivot(FbxNode::eSourcePivot));
		const FbxAMatrix PreRotationMatrix    = bRotationActive ? MakeFbxRotationMatrixByOrder(Node->GetPreRotation(FbxNode::eSourcePivot), RotationOrder)
		: Identity;
		const FbxAMatrix RotationMatrix     = MakeFbxRotationMatrixFromQuat(RotationQuat);
		const FbxAMatrix PostRotationMatrix = bRotationActive ? MakeFbxRotationMatrixByOrder(Node->GetPostRotation(FbxNode::eSourcePivot), RotationOrder)
		: Identity;
		const FbxAMatrix ScalingOffsetMatrix = MakeFbxTranslationMatrix(Node->GetScalingOffset(FbxNode::eSourcePivot));
		const FbxAMatrix ScalingPivotMatrix  = MakeFbxTranslationMatrix(Node->GetScalingPivot(FbxNode::eSourcePivot));
		const FbxAMatrix ScalingMatrix       = MakeFbxScalingMatrix(ToFbxVector4(Scale));

		return TranslationMatrix * RotationOffsetMatrix * RotationPivotMatrix * PreRotationMatrix * RotationMatrix * PostRotationMatrix.Inverse() *
		RotationPivotMatrix.Inverse() * ScalingOffsetMatrix * ScalingPivotMatrix * ScalingMatrix * ScalingPivotMatrix.Inverse();
	}

	static FCompositedFbxLocalTRS CompositeAnimLayersToLocalTRS(FbxNode* Node, FbxAnimStack* AnimStack, const FbxTime& Time)
	{
		FCompositedFbxLocalTRS Result;

		if (!Node || !AnimStack)
		{
			return Result;
		}

		const FVector DefaultTranslation = ToFVector(Node->LclTranslation.Get());
		const FVector DefaultRotation    = ToFVector(Node->LclRotation.Get());
		const FVector DefaultScale       = ToFVector(Node->LclScaling.Get());

		EFbxRotationOrder RotationOrder = eEulerXYZ;
		Node->GetRotationOrder(FbxNode::eSourcePivot, RotationOrder);

		const FQuat DefaultQuat = MakeQuatFromFbxEulerDegree(DefaultRotation, RotationOrder);

		Result.Translation  = DefaultTranslation;
		Result.Rotation     = DefaultRotation;
		Result.RotationQuat = DefaultQuat;
		Result.Scale        = DefaultScale;

		const bool  bHasSoloLayer = HasAnySoloLayer(AnimStack);
		const int32 LayerCount    = AnimStack->GetMemberCount<FbxAnimLayer>();

		for (int32 LayerIndex = 0; LayerIndex < LayerCount; ++LayerIndex)
		{
			FbxAnimLayer* Layer = AnimStack->GetMember<FbxAnimLayer>(LayerIndex);
			if (!ShouldUseAnimLayer(Layer, bHasSoloLayer))
			{
				continue;
			}

			const FFbxLayerTransformSample Sample = EvaluateLayerTransformSample(Node, Layer, Time);
			if (Sample.Weight <= 0.0f)
			{
				continue;
			}

			if (Sample.bHasTranslation)
			{
				Result.Translation = BlendTranslationLayer(Result.Translation, DefaultTranslation, Sample.Translation, Sample.Weight, Sample.BlendMode);
			}

			if (Sample.bHasRotation)
			{
				if (Sample.RotationAccumulationMode == FbxAnimLayer::eRotationByChannel)
				{
					if (Result.bUseQuatRotation)
					{
						Result.Rotation = Result.RotationQuat.ToRotator().ToVector();
					}
					Result.Rotation         = BlendRotationByChannel(Result.Rotation, DefaultRotation, Sample.Rotation, Sample.Weight, Sample.BlendMode);
					Result.RotationQuat     = MakeQuatFromFbxEulerDegree(Result.Rotation, RotationOrder);
					Result.bUseQuatRotation = false;
				}
				else
				{
					const FQuat LayerQuat   = MakeQuatFromFbxEulerDegree(Sample.Rotation, RotationOrder);
					Result.RotationQuat     = BlendRotationByLayer(Result.RotationQuat, DefaultQuat, LayerQuat, Sample.Weight, Sample.BlendMode);
					Result.bUseQuatRotation = true;
				}
			}

			if (Sample.bHasScale)
			{
				Result.Scale = BlendScaleLayer(Result.Scale, DefaultScale, Sample.Scale, Sample.Weight, Sample.BlendMode, Sample.ScaleAccumulationMode);
			}
		}

		Result.RotationQuat = Result.RotationQuat.GetNormalized();
		return Result;
	}

	static FbxAMatrix EvaluateLocalFbxMatrixFromCompositedLayersCandidate(FbxNode* Node, FbxAnimStack* AnimStack, const FbxTime& Time)
	{
		FbxAMatrix Identity;
		Identity.SetIdentity();

		if (!Node || !AnimStack)
		{
			return Identity;
		}

		const FCompositedFbxLocalTRS TRS = CompositeAnimLayersToLocalTRS(Node, AnimStack, Time);

		if (TRS.bUseQuatRotation)
		{
			return MakeLocalFbxMatrixFromTRSQuat(Node, TRS.Translation, TRS.RotationQuat, TRS.Scale);
		}

		return MakeLocalFbxMatrixFromTRS(Node, TRS.Translation, TRS.Rotation, TRS.Scale);
	}

	static FFbxBakeMatrixResult EvaluateLocalFbxMatrixForBake(
		FbxNode*       Node,
		FbxAnimStack*  AnimStack,
		FbxAnimLayer*  BaseLayer,
		const FbxTime& Time,
		int32          AnimLayerCount
		)
	{
		FFbxBakeMatrixResult Result;

		FbxAMatrix Identity;
		Identity.SetIdentity();

		Result.FinalMatrix  = Identity;
		Result.DirectMatrix = Identity;
		Result.SdkMatrix    = Identity;

		if (!Node)
		{
			return Result;
		}

		Result.SdkMatrix = Node->EvaluateLocalTransform(Time);

		switch (GAnimationBakePolicy)
		{
		case EFbxAnimationBakePolicy::DirectBaseLayerOnly:
		{
			Result.DirectMatrix        = EvaluateLocalFbxMatrixFromCurves(Node, BaseLayer, Time);
			Result.FinalMatrix         = Result.DirectMatrix;
			Result.bHasDirectCandidate = true;
			break;
		}
		case EFbxAnimationBakePolicy::DirectLayeredOnly:
		{
			Result.DirectMatrix = AnimLayerCount > 1 ? EvaluateLocalFbxMatrixFromCompositedLayersCandidate(Node, AnimStack, Time)
			: EvaluateLocalFbxMatrixFromCurves(Node, BaseLayer, Time);

			Result.FinalMatrix         = Result.DirectMatrix;
			Result.bHasDirectCandidate = true;
			break;
		}
		case EFbxAnimationBakePolicy::SdkEvaluatorOnly:
		{
			Result.FinalMatrix      = Result.SdkMatrix;
			Result.bUsedSdkFallback = true;
			break;
		}
		case EFbxAnimationBakePolicy::DirectLayeredWithSdkFallback: default:
		{
			Result.DirectMatrix = AnimLayerCount > 1 ? EvaluateLocalFbxMatrixFromCompositedLayersCandidate(Node, AnimStack, Time)
			: EvaluateLocalFbxMatrixFromCurves(Node, BaseLayer, Time);

			Result.bHasDirectCandidate = true;
			Result.Error               = ComputeFbxMatrixMaxError(Result.DirectMatrix, Result.SdkMatrix);

			if (Result.Error <= GDirectBakeErrorTolerance)
			{
				Result.FinalMatrix = Result.DirectMatrix;
			}
			else
			{
				Result.FinalMatrix      = Result.SdkMatrix;
				Result.bUsedSdkFallback = true;
			}
			break;
		}
		}

		if (Result.bHasDirectCandidate)
		{
			Result.Error = ComputeFbxMatrixMaxError(Result.DirectMatrix, Result.SdkMatrix);
		}

		return Result;
	}

}

bool FFbxAnimationImporter::ImportAnimations(FbxScene* Scene, FFbxImportContext& Context, FString* OutMessage)
{
	Context.AnimSequences.clear();

	if (!Scene || Context.Bones.empty() || Context.BoneNodeToIndex.empty())
	{
		return true;
	}

	const int32 AnimStackCount = Scene->GetSrcObjectCount<FbxAnimStack>();
	if (AnimStackCount <= 0)
	{
		return true;
	}

	const float SampleRate = GetSceneSampleRate(Scene);

	TSet<FString> UsedAnimationNames;

	for (int32 StackIndex = 0; StackIndex < AnimStackCount; ++StackIndex)
	{
		FbxAnimStack* AnimStack = Scene->GetSrcObject<FbxAnimStack>(StackIndex);
		if (!AnimStack)
		{
			continue;
		}

		Scene->SetCurrentAnimationStack(AnimStack);

		const int32 AnimLayerCount = AnimStack->GetMemberCount<FbxAnimLayer>();
		if (AnimLayerCount <= 0)
		{
			UE_LOG("Animation import skipped: AnimLayer not found. Stack=%s", AnimStack->GetName());
			continue;
		}

		FbxAnimLayer* AnimLayer = AnimStack->GetMember<FbxAnimLayer>(0);
		if (!AnimLayer)
		{
			UE_LOG("Animation import skipped: Base AnimLayer not found. Stack=%s", AnimStack->GetName());
			continue;
		}

		if (AnimLayerCount > 1)
		{
			UE_LOG(
				"FBX multi-layer animation detected: Stack=%s, LayerCount=%d",
				AnimStack->GetName(),
				AnimLayerCount
			);
		}

		double StartSeconds = 0.0;
		double EndSeconds   = 0.0;

		if (!TryResolveAnimationTimeRange(Scene, AnimStack, StartSeconds, EndSeconds))
		{
			continue;
		}

		const double DurationSeconds = EndSeconds - StartSeconds;
		if (DurationSeconds <= 0.0)
		{
			continue;
		}

		const int32 NumFrames = std::max(1, static_cast<int32>(std::ceil(DurationSeconds * static_cast<double>(SampleRate))) + 1);

		const int32 AnimatedCurveBoneCount = CountAnimatedCurveBonesAllLayers(Context.BoneNodeToIndex, AnimStack);
		const int32 TransformCurveKeyCount = CountTransformCurveKeysAllLayers(Context.BoneNodeToIndex, AnimStack);

		UE_LOG(
			"Animation curve import: Stack=%s, LayerCount=%d, SampleRate=%.2f, NumFrames=%d, CurveBones=%d, CurveKeys=%d",
			AnimStack->GetName(),
			AnimLayerCount,
			SampleRate,
			NumFrames,
			AnimatedCurveBoneCount,
			TransformCurveKeyCount
		);

		UAnimDataModel* DataModel = UObjectManager::Get().CreateObject<UAnimDataModel>();
		DataModel->SetTiming(static_cast<float>(DurationSeconds), SampleRate, NumFrames);
		DataModel->BoneAnimationTracks.resize(Context.Bones.size());

		for (int32 BoneIndex = 0; BoneIndex < static_cast<int32>(Context.Bones.size()); ++BoneIndex)
		{
			FBoneAnimationTrack& Track = DataModel->BoneAnimationTracks[BoneIndex];

			Track.BoneTreeIndex = BoneIndex;
			Track.BoneName = Context.Bones[BoneIndex].Name;
			Track.InternalTrackData.PosKeys.reserve(NumFrames);
			Track.InternalTrackData.RotKeys.reserve(NumFrames);
			Track.InternalTrackData.ScaleKeys.reserve(NumFrames);
		}

		for (const auto& Pair : Context.BoneNodeToIndex)
		{
			FbxNode*    BoneNode  = Pair.first;
			const int32 BoneIndex = Pair.second;

			if (!BoneNode)
			{
				continue;
			}

			if (BoneIndex < 0 || BoneIndex >= static_cast<int32>(DataModel->BoneAnimationTracks.size()))
			{
				continue;
			}

			FRawAnimSequenceTrack& RawTrack = DataModel->BoneAnimationTracks[BoneIndex].InternalTrackData;

			CopySourceTransformCurves(BoneNode, AnimStack, StartSeconds, RawTrack);
		}

		const int32 SourceCurveKeyCount = CountSourceCurveKeys(DataModel);
		UE_LOG("Animation source curves stored: Stack=%s, LayerCount=%d, SourceCurveKeys=%d", AnimStack->GetName(), AnimLayerCount, SourceCurveKeyCount);

		FFbxLayeredBakeStats BakeStats;

		TArray<FTransform> BindLocalTransforms;
		BindLocalTransforms.resize(Context.Bones.size());
		for (int32 BoneIndex = 0; BoneIndex < static_cast<int32>(Context.Bones.size()); ++BoneIndex)
		{
			BindLocalTransforms[BoneIndex] = FAnimationRuntime::DecomposeMatrix(Context.Bones[BoneIndex].LocalMatrix);
		}

		TArray<FTransform> BoneLocalTransforms;
		BoneLocalTransforms.reserve(Context.Bones.size());

		for (int32 FrameIndex = 0; FrameIndex < NumFrames; ++FrameIndex)
		{
			const double LocalSeconds = (NumFrames > 1)
				? (static_cast<double>(FrameIndex) / static_cast<double>(NumFrames - 1)) * DurationSeconds
				: 0.0;

			FbxTime Time;
			Time.SetSecondDouble(StartSeconds + LocalSeconds);

			// 기본값은 미리 계산한 bind/local pose다. FBX node가 있는 bone은 아래에서 curve 평가값으로 덮어쓴다.
			BoneLocalTransforms = BindLocalTransforms;

			for (const auto& Pair : Context.BoneNodeToIndex)
			{
				FbxNode*    BoneNode  = Pair.first;
				const int32 BoneIndex = Pair.second;

				if (!BoneNode)
				{
					continue;
				}

				if (BoneIndex < 0 || BoneIndex >= static_cast<int32>(Context.Bones.size()))
				{
					continue;
				}

				// 직접 multi-layer candidate를 만들고, policy에 따라 direct / SDK fallback / SDK only 중 하나를 최종 bake 값으로 사용한다.
				const FFbxBakeMatrixResult BakeResult = EvaluateLocalFbxMatrixForBake(BoneNode, AnimStack, AnimLayer, Time, AnimLayerCount);

				BakeStats.TestedSamples++;
				BakeStats.MaxError = std::max(BakeStats.MaxError, BakeResult.Error);
				if (BakeResult.bUsedSdkFallback)
				{
					BakeStats.FallbackSamples++;
				}

				if (FrameIndex == 0 || FrameIndex == NumFrames / 2 || FrameIndex == NumFrames - 1)
				{
					if (BakeResult.Error > GDirectBakeErrorTolerance)
					{
						UE_LOG(
							"FBX layered bake mismatch: Stack=%s, Bone=%s, Frame=%d, LayerCount=%d, Error=%.6f, UsedSdkFallback=%d",
							AnimStack->GetName(),
							BoneNode->GetName(),
							FrameIndex,
							AnimLayerCount,
							BakeResult.Error,
							BakeResult.bUsedSdkFallback ? 1 : 0
						);
					}
				}

				BoneLocalTransforms[BoneIndex] = FAnimationRuntime::DecomposeMatrix(FFbxTransformUtils::ToEngineMatrix(BakeResult.FinalMatrix));
			}

			for (int32 BoneIndex = 0; BoneIndex < static_cast<int32>(Context.Bones.size()); ++BoneIndex)
			{
				const FTransform&      LocalTransform = BoneLocalTransforms[BoneIndex];
				FRawAnimSequenceTrack& Raw            = DataModel->BoneAnimationTracks[BoneIndex].InternalTrackData;

				Raw.PosKeys.push_back(LocalTransform.Location);
				Raw.RotKeys.push_back(LocalTransform.Rotation.GetNormalized());
				Raw.ScaleKeys.push_back(LocalTransform.Scale);
			}
		}

		UE_LOG(
			"FBX layered bake stats: Stack=%s, LayerCount=%d, Tested=%d, Fallback=%d, MaxError=%.6f",
			AnimStack->GetName(),
			AnimLayerCount,
			BakeStats.TestedSamples,
			BakeStats.FallbackSamples,
			BakeStats.MaxError
		);

		UAnimSequence* Sequence = UObjectManager::Get().CreateObject<UAnimSequence>();

		const FString BaseAnimName = MakeSafeAnimationName(AnimStack->GetName());
		const FString AnimName     = MakeUniqueAnimationName(BaseAnimName, UsedAnimationNames);

		if (AnimName != BaseAnimName)
		{
			UE_LOG("Animation stack name duplicated. Stack=%s, UniqueName=%s", AnimStack->GetName(), AnimName.c_str());
		}

		Sequence->SetFName(FName(AnimName));
		Sequence->SetDataModel(DataModel);

		Context.AnimSequences.push_back(Sequence);
	}
	return true;
}

