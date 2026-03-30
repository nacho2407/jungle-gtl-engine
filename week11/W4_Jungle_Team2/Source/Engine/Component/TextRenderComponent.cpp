#include "TextRenderComponent.h"

#include <cmath>
#include <cstring>
#include "Editor/Viewport/ViewportCamera.h"
#include "GameFramework/AActor.h"
#include "Core/ResourceManager.h"
#include "Object/ObjectFactory.h"

DEFINE_CLASS(UTextRenderComponent, UBillboardComponent)
REGISTER_FACTORY(UTextRenderComponent)

void UTextRenderComponent::SetFont(const FName& InFontName)
{
	FontName = InFontName;
	CachedFont = FResourceManager::Get().FindFont(FontName);
}

void UTextRenderComponent::UpdateWorldAABB() const
{
	WorldAABB.Reset();

	float TotalWidth = GetUTF8Length(Text) * 0.5f;
	float TotalHeight = 0.5f;

	FVector WorldScale = GetWorldScale();
	const float HalfWidth = TotalWidth * WorldScale.Y * 0.5f;
	const float HalfHeight = TotalHeight * WorldScale.Z * 0.5f;
	const float Radius = std::sqrt((HalfWidth * HalfWidth) + (HalfHeight * HalfHeight));
	const FVector Extent(Radius, Radius, Radius);
	const FVector WorldCenter = GetWorldLocation();

	WorldAABB.Expand(WorldCenter - Extent);
	WorldAABB.Expand(WorldCenter + Extent);
}

bool UTextRenderComponent::RaycastMesh(const FRay& Ray, FHitResult& OutHitResult)
{
	FMatrix BillboardWorldMatrix = GetWorldMatrix();
	const FViewportCamera* ActiveCamera = nullptr;
	if (TryGetActiveCamera(ActiveCamera))
	{
		BillboardWorldMatrix = MakeBillboardWorldMatrix(
			GetWorldLocation(),
			GetWorldScale(),
			ActiveCamera->GetEffectiveForward(),
			ActiveCamera->GetEffectiveRight(),
			ActiveCamera->GetEffectiveUp());
	}

	FMatrix OutlineWorldMatrix = CalculateOutlineMatrix(BillboardWorldMatrix);
	FMatrix InvWorldMatrix = OutlineWorldMatrix.GetInverse();

	FRay LocalRay;
	LocalRay.Origin = InvWorldMatrix.TransformPosition(Ray.Origin);
	LocalRay.Direction = InvWorldMatrix.TransformVector(Ray.Direction).Normalized();


	if (std::abs(LocalRay.Direction.X) < 0.00111f) return false;

	float t = -LocalRay.Origin.X / LocalRay.Direction.X;

	if (t < 0.0f) return false;

	FVector LocalHitPos = LocalRay.Origin + LocalRay.Direction * t;

	if (LocalHitPos.Y >= -0.5f && LocalHitPos.Y <= 0.5f &&
		LocalHitPos.Z >= -0.5f && LocalHitPos.Z <= 0.5f)
	{
		FVector WorldHitPos = OutlineWorldMatrix.TransformPosition(LocalHitPos);
		OutHitResult.bHit = true;
		OutHitResult.HitComponent = this;
		OutHitResult.Distance = (WorldHitPos - Ray.Origin).Size();
		OutHitResult.Location = WorldHitPos;
		OutHitResult.Normal = OutlineWorldMatrix.GetForwardVector();
		OutHitResult.FaceIndex = 0;
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
	return CalculateOutlineMatrix(GetWorldMatrix());
}

FMatrix UTextRenderComponent::CalculateOutlineMatrix(const FMatrix& BillboardWorldMatrix) const
{
	int32 Len = GetUTF8Length(Text);

	if (Len <= 0) return FMatrix::Identity;

	float TotalLocalWidth = (Len * CharWidth);

	float CenterY = TotalLocalWidth * -0.5f;
	float CenterZ = 0.0f; // 상하 정렬이 중앙이라면 0

	FMatrix ScaleMatrix = FMatrix::MakeScaleMatrix(FVector(1.0f, TotalLocalWidth, CharHeight));
	FMatrix TransMatrix = FMatrix::MakeTranslationMatrix(FVector(0.0f, CenterY, CenterZ));

	return (ScaleMatrix * TransMatrix) * BillboardWorldMatrix;
}

int32 UTextRenderComponent::GetUTF8Length(const FString& str) const{
	int32 count = 0;
	for (size_t i = 0; i < str.length(); ++i) {
		// UTF-8의 첫 바이트가 10xxxxxx 이 아니면 새로운 글자의 시작임
		if ((str[i] & 0xC0) != 0x80) count++;
	}
	return count;
}
