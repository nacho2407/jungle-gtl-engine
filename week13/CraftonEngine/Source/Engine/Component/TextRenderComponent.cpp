#include "TextRenderComponent.h"

#include <cstring>
#include "GameFramework/AActor.h"
#include "Core/ResourceManager.h"
#include "Object/ObjectFactory.h"
#include "Render/Mesh/MeshManager.h"

DEFINE_CLASS(UTextRenderComponent, UBillboardComponent)
REGISTER_FACTORY(UTextRenderComponent)

void UTextRenderComponent::SetFont(const FName& InFontName)
{
	FontName = InFontName;
	CachedFont = FResourceManager::Get().FindFont(FontName);
}

void UTextRenderComponent::UpdateWorldAABB() const
{
	float TotalWidth = GetUTF8Length(Text) * 0.5f;
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
	WorldCenter -= WorldRight * (ScaledWidth * 0.5f); 

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


FMatrix UTextRenderComponent::CalculateOutlineMatrix() const
{
	int32 Len = GetUTF8Length(Text);

	if (Len <= 0) return FMatrix::Identity;

	float TotalLocalWidth = (Len * CharWidth);

	float CenterY = TotalLocalWidth * -0.5f;
	float CenterZ = 0.0f; // 상하 정렬이 중앙이라면 0

	FMatrix ScaleMatrix = FMatrix::MakeScaleMatrix(FVector(1.0f, TotalLocalWidth, CharHeight));
	FMatrix TransMatrix = FMatrix::MakeTranslationMatrix(FVector(0.0f, CenterY, CenterZ));

	return (ScaleMatrix * TransMatrix) * CachedWorldMatrix;
}

int32 UTextRenderComponent::GetUTF8Length(const FString& str) const{
	int32 count = 0;
	for (size_t i = 0; i < str.length(); ++i) {
		// UTF-8의 첫 바이트가 10xxxxxx 이 아니면 새로운 글자의 시작임
		if ((str[i] & 0xC0) != 0x80) count++;
	}
	return count;
}