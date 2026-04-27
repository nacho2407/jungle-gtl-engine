#include "LightComponent.h"
#include "Object/ObjectFactory.h"
#include "Core/PropertyTypes.h"
#include "Render/Resource/ShadowAtlasManager.h"

DEFINE_CLASS(ULightComponent, ULightComponentBase)
REGISTER_FACTORY(ULightComponent)

FMatrix ULightComponent::GetLightViewProj(const FMatrix& CamView, const FMatrix& CamProj,
	const TArray<FBoundingBox>* VisibleObjectsBounds) const
{
	switch (eShadowMapType)
	{
	case EShadowMap::BASIC:
		return ComputeBasicShadowMatrix(CamView, CamProj);
	case EShadowMap::PSM:
		return ComputePerspectiveShadowMatrix(CamView, CamProj, VisibleObjectsBounds);
	case EShadowMap::CSM:
	default:
		return ComputeBasicShadowMatrix(CamView, CamProj);
	}
}

void ULightComponent::PostDuplicate(UObject* Original)
{
	ULightComponentBase::PostDuplicate(Original);
	ULightComponent* Orig = Cast<ULightComponent>(Original);

	eShadowMapType = Orig->eShadowMapType;
}

void ULightComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	ULightComponentBase::GetEditableProperties(OutProps);

	static const char* ShadowMapTypeNames[] = { "Basic", "PSM", "CSM" };
	OutProps.push_back({ "ShadowMapType", EPropertyType::Enum, &eShadowMapType, 0.f, 0.f, 0.f, ShadowMapTypeNames, 3 });

	FShadowAtlasManager& AtlasManager = FShadowAtlasManager::Get();

    const FVector4& SO = bHasDebugShadowAtlasTile
                             ? DebugShadowAtlasScaleOffset
                             : FVector4(1, 1, 0, 0);


	static FSRVDisplayInfo ShadowMapDisplay;
    ShadowMapDisplay = {
        256.f,
        256.f, 
		SO.Z,
        SO.W,
        SO.Z + SO.X,
        SO.W + SO.Y
    };

	OutProps.push_back({ "ShadowMap", EPropertyType::SRV, AtlasManager.GetSRV(), 0.f, 0.f, 0.f, nullptr, 0, &ShadowMapDisplay });

	OutProps.push_back({ "Shadow Resolution Scale", EPropertyType::Int, &ShadowResolutionScale});
	OutProps.push_back({ "Shadow Bias", EPropertyType::Float, &ShadowBias, 0.0f, 1.0f, 0.01f });
	OutProps.push_back({ "Shadow Slope Bias", EPropertyType::Float, &ShadowSlopeBias, 0.0f, 1.0f, 0.01f });
	OutProps.push_back({ "Shadow Sharpen", EPropertyType::Float, &ShadowSharpen });
}

void ULightComponent::Serialize(FArchive& Ar)
{
	ULightComponentBase::Serialize(Ar);
	
	uint32 ShadowMapTypeValue = static_cast<uint32>(eShadowMapType);
	Ar << "ShadowMapType" << ShadowMapTypeValue;
	Ar << "ShadowResolutionScale" << ShadowResolutionScale;
	Ar << "ShadowBias" << ShadowBias;
	Ar << "ShadowSlopeBias" << ShadowSlopeBias;
	Ar << "ShadowSharpen" << ShadowSharpen;

	if (Ar.IsLoading())
	{
		eShadowMapType = static_cast<EShadowMap>(ShadowMapTypeValue);
	}
}


FMatrix ULightComponent::ComputeBasicShadowMatrix(const FMatrix& CamView, const FMatrix& CamProj) const
{
	//FMatrix ViewProj = CamView * CamProj;
	//const FMatrix ViewProjInverse = ViewProj.GetInverse();

	//// NDC 박스를 월드로 보내서 절두체 생성
	//FVector PostCorners[8] =
	//{
	//	FVector(-1, -1, 0),
	//	FVector(1, -1, 0),
	//	FVector(-1, 1, 0),
	//	FVector(1, 1, 0),
	//	FVector(-1, -1, 1),
	//	FVector(1, -1, 1),
	//	FVector(-1, 1, 1),
	//	FVector(1, 1, 1)
	//};

	//FVector Center = { 0.f, 0.f, 0.f };
	//for (int i = 0; i < 8; ++i)
	//{
	//	PostCorners[i] = ViewProjInverse.TransformPosition(PostCorners[i]);
	//	Center += (PostCorners[i]);
	//}
	//Center /= 8.f;
	//
	//// 광원 뷰 행렬 
	//FVector LightDir = GetForwardVector().GetSafeNormal();
	//FVector Ref;

	//// Z-Up (0,0,1)
	//if (std::abs(FVector::DotProduct(LightDir, FVector(0, 0, 1))) < 0.9f)
	//{
	//	Ref = FVector(0.f, 0.f, 1.f); 
	//}
	//else
	//{
	//	Ref = FVector(0.f, 1.f, 0.f); // 광원이 거의 수직일 때 폴백
	//}

	//FVector Right = FVector::CrossProduct(Ref, LightDir).GetSafeNormal();
	//FVector Up = FVector::CrossProduct(LightDir, Right).GetSafeNormal();
	//FMatrix LightView = FMatrix::MakeViewLookAtLH(Center - LightDir * 100.f, Center, Up);

	//FVector Min(FLT_MAX, FLT_MAX, FLT_MAX);
	//FVector Max(-FLT_MAX, -FLT_MAX, -FLT_MAX);

	//// 광원 기준 View 프러스텀으로 이동하여 투영 행렬 생성
	//for (int i = 0; i < 8; ++i)
	//{
	//	FVector4 VertexLightView = FVector4(PostCorners[i], 1.0f) * LightView;
	//	FVector Tmp = { VertexLightView.X, VertexLightView.Y, VertexLightView.Z } ;

	//	Min = FVector::Min(Min, Tmp);
	//	Max = FVector::Max(Max, Tmp);
	//}


	//FMatrix LightProj = FMatrix::MakeOrthographicOffCenterLH(Min.Y, Max.Y, Min.Z, Max.Z, Min.X, Max.X);
	//FMatrix Result = LightView * LightProj;

	//return Result;


	const float FollowDist = 20.0f; // 카메라 앞쪽으로 얼마를 볼지
	const float HalfSizeY = 25.0f; // light-space Y 반폭
	const float HalfSizeZ = 25.0f; // light-space Z 반높이
	const float ShadowBack = 40.0f; // focus 뒤쪽으로 포함할 깊이
	const float ShadowFront = 60.0f; // focus 앞쪽으로 포함할 깊이

	FMatrix CamWorld = CamView.GetInverse();
	FVector CamPos = CamWorld.GetOrigin();
	FVector CamFwd = CamWorld.GetForwardVector().GetSafeNormal();

	FVector Focus = CamPos + CamFwd * FollowDist;

	FVector LightDir = GetForwardVector().GetSafeNormal();
	FVector Ref = (std::abs(FVector::DotProduct(LightDir, FVector(0, 0, 1))) < 0.9f)
		? FVector(0, 0, 1)
		: FVector(0, 1, 0);

	FVector Right = FVector::CrossProduct(Ref, LightDir).GetSafeNormal();
	FVector Up = FVector::CrossProduct(LightDir, Right).GetSafeNormal();

	// 라이트는 focus 뒤쪽에서 focus를 바라보게 둠
	FVector Eye = Focus - LightDir * ShadowBack;
	FMatrix LightView = FMatrix::MakeViewLookAtLH(Eye, Focus, Up);

	// 현재 엔진 규약상 LightView 후
	// X = depth, Y = horizontal, Z = vertical 로 보면 됨
	FVector4 FocusLS4 = FVector4(Focus, 1.0f) * LightView;
	FVector FocusLS(FocusLS4.X, FocusLS4.Y, FocusLS4.Z);

	FMatrix LightProj = FMatrix::MakeOrthographicOffCenterLH(
		FocusLS.Y - HalfSizeY, FocusLS.Y + HalfSizeY,
		FocusLS.Z - HalfSizeZ, FocusLS.Z + HalfSizeZ,
		0.0f,
		ShadowBack + ShadowFront
	);
	return LightView * LightProj;
}
