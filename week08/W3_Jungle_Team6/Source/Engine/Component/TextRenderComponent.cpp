#include "TextRenderComponent.h"

#include <cstring>
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "Component/CameraComponent.h"
#include "Core/ResourceManager.h"
#include "Object/ObjectFactory.h"
#include "Render/Mesh/MeshManager.h"

DEFINE_CLASS(UTextRenderComponent, UPrimitiveComponent)
REGISTER_FACTORY(UTextRenderComponent)

void UTextRenderComponent::SetFont(const FName& InFontName)
{
	FontName = InFontName;
	CachedFont = FResourceManager::Get().FindFont(FontName);
}

void UTextRenderComponent::UpdateWorldAABB() const
{
	float TotalWidth = Text.length() * 0.5f;
	float TotalHeight = 0.5f;

	FVector WorldScale = GetWorldScale();
	float ScaledWidth = TotalWidth * WorldScale.Y;
	float ScaledHeight = TotalHeight * WorldScale.Z;

	FVector WorldRight = FVector(CachedWorldMatrix.M[1][0], CachedWorldMatrix.M[1][1], CachedWorldMatrix.M[1][2]).Normalized();
	FVector WorldUp = FVector(CachedWorldMatrix.M[2][0], CachedWorldMatrix.M[2][1], CachedWorldMatrix.M[2][2]).Normalized();

	float Ex = std::abs(WorldRight.X) * (ScaledWidth * 0.5f) + std::abs(WorldUp.X) * (ScaledHeight * 0.5f);
	float Ey = std::abs(WorldRight.Y) * (ScaledWidth * 0.5f) + std::abs(WorldUp.Y) * (ScaledHeight * 0.5f);
	float Ez = std::abs(WorldRight.Z) * (ScaledWidth * 0.5f) + std::abs(WorldUp.Z) * (ScaledHeight * 0.5f);
	FVector Extent(Ex, Ey, Ez);

	FVector WorldCenter = GetWorldLocation();
	WorldCenter -= WorldRight * (ScaledWidth * 0.5f); // -=가 아니라 += 입니다!

	WorldAABBMinLocation = WorldCenter - Extent;
	WorldAABBMaxLocation = WorldCenter + Extent;
}

bool UTextRenderComponent::RaycastMesh(const FRay& Ray, FHitResult& OutHitResult)
{
	FMatrix OutlineWorldMatrix = CalculateOutlineMatrix();
	FMatrix InvWorldMatrix = OutlineWorldMatrix.GetInverse();

	FRay LocalRay;
	LocalRay.Origin = InvWorldMatrix.TransformPositionWithW(Ray.Origin);
	LocalRay.Direction = InvWorldMatrix.TransformVector(Ray.Direction).Normalized();


	if (std::abs(LocalRay.Direction.X) < 0.00111f) return false;

	float t = -LocalRay.Origin.X / LocalRay.Direction.X;

	if (t < 0.0f) return false;

	FVector LocalHitPos = LocalRay.Origin + LocalRay.Direction * t;

	if (LocalHitPos.Y >= -0.5f && LocalHitPos.Y <= 0.5f &&
		LocalHitPos.Z >= -0.5f && LocalHitPos.Z <= 0.5f)
	{
		FVector WorldHitPos = OutlineWorldMatrix.TransformVector(LocalHitPos);
		OutHitResult.Distance = (WorldHitPos - Ray.Origin).Length();
		return true;
	}

	return false;
}

FString UTextRenderComponent::GetOwnerUUIDToString() const
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return FName::None.ToString();
	}
	return std::to_string(OwnerActor->GetUUID());
}

FString UTextRenderComponent::GetOwnerNameToString() const
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return FName::None.ToString();
	}

	FName Name = OwnerActor->GetFName();
	if (Name.IsValid())
	{
		return Name.ToString();
	}
	return FName::None.ToString();
}

UTextRenderComponent::UTextRenderComponent()
{
	MeshData = &FMeshManager::GetQuad();
}

void UTextRenderComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	USceneComponent::GetEditableProperties(OutProps);
	OutProps.push_back({ "Text", EPropertyType::String, &Text });
	OutProps.push_back({ "Font", EPropertyType::Name, &FontName });
	//OutProps.push_back({ "Color", EPropertyType::Vec4, &Color });
	OutProps.push_back({ "Font Size", EPropertyType::Float, &FontSize, 0.1f, 100.0f, 0.1f });
	OutProps.push_back({ "Visible", EPropertyType::Bool, &bIsVisible });
}

void UTextRenderComponent::PostEditProperty(const char* PropertyName)
{
	if (strcmp(PropertyName, "Font") == 0)
	{
		SetFont(FontName);
	}
}


void UTextRenderComponent::TickComponent(float DeltaTime)
{
	FVector WorldLocation = GetWorldLocation();

	const UCameraComponent* ActiveCamera = GetOwner()->GetWorld()->GetActiveCamera();

	FVector CameraForward = ActiveCamera->GetForwardVector().Normalized();
	FVector Forward = CameraForward * -1;
	FVector WorldUp = FVector(0.0f, 0.0f, 1.0f);

	if (std::abs(Forward.Dot(WorldUp)) > 0.99f)
	{
		WorldUp = FVector(0.0f, 1.0f, 0.0f); // 임시 Up축 변경
	}

	FVector Right = WorldUp.Cross(Forward).Normalized();
	FVector Up = Forward.Cross(Right).Normalized();

	FMatrix RotMatrix;
	RotMatrix.SetAxes(Forward, Right, Up);

	CachedWorldMatrix = FMatrix::MakeScaleMatrix(GetWorldScale()) * RotMatrix * FMatrix::MakeTranslationMatrix(WorldLocation);

	UpdateWorldAABB();
}

FMatrix UTextRenderComponent::CalculateOutlineMatrix() const
{
	// 1. 실제 글자들이 배치된 총 로컬 너비 계산
	// (글자수 * 기본너비) + (간격 * (글자수-1))
	float CharWidth = 0.5f;
	float Spacing = 0.1f; // 실제 사용하시는 자간 값
	int32 Len = (int32)Text.length()-1;

	// 글자가 없을 때 에러 방지
	if (Len <= 0) return FMatrix::Identity;

	float TotalLocalWidth = (Len * CharWidth) + ((Len - 1) * Spacing);
	float TotalLocalHeight = 0.5f; // 폰트 높이

	// 2. 정렬(Alignment)에 따른 중심점 오프셋 (YZ 평면 기준)
	// 텍스트가 시작점(0,0,0)에서 오른쪽(+Y)으로 그려진다고 가정할 때
	float CenterY = TotalLocalWidth * -0.5f;
	float CenterZ = 0.0f; // 상하 정렬이 중앙이라면 0

	// 3. 로컬 피벗 행렬 구성
	// 순서: 1x1 표준 쿼드를 (너비, 높이)만큼 키우고 -> 정렬된 중심점으로 이동
	FMatrix ScaleM = FMatrix::MakeScaleMatrix(FVector(1.0f, TotalLocalWidth, TotalLocalHeight));
	FMatrix TransM = FMatrix::MakeTranslationMatrix(FVector(0.0f, CenterY, CenterZ));

	// 4. 최종 아웃라인 월드 행렬
	// (표준쿼드 -> 텍스트전체크기) * (컴포넌트 월드 변환)
	return (ScaleM * TransM) * CachedWorldMatrix;
}