#include "TextRenderComponent.h"

#include <cstring>
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "Resource/ResourceManager.h"
#include "Object/ObjectFactory.h"
#include "Render/Resources/MeshBufferManager.h"
#include "Render/Resources/ShaderManager.h"
#include "Render/Scene/Proxies/Primitive/TextRenderSceneProxy.h"
#include "Serialization/Archive.h"

IMPLEMENT_CLASS(UTextRenderComponent, UBillboardComponent)

FPrimitiveSceneProxy* UTextRenderComponent::CreateSceneProxy()
{
	return new FTextRenderSceneProxy(this);
}

void UTextRenderComponent::SetFont(const FName& InFontName)
{
	FontName = InFontName;
	CachedFont = FResourceManager::Get().FindFont(FontName);
}

void UTextRenderComponent::UpdateWorldAABB() const
{
	// 빌보드는 어느 방향에서든 보이므로 view-independent 구형 바운드 사용
	float TotalWidth = GetUTF8Length(Text) * CharWidth;
	float MaxExtent = std::max(TotalWidth, CharHeight);

	FVector WorldScale = GetWorldScale();
	float ScaledMax = MaxExtent * std::max({ WorldScale.X, WorldScale.Y, WorldScale.Z });

	FVector WorldCenter = GetWorldLocation();
	FVector Extent(ScaledMax, ScaledMax, ScaledMax);

	WorldAABBMinLocation = WorldCenter - Extent;
	WorldAABBMaxLocation = WorldCenter + Extent;
}

bool UTextRenderComponent::LineTraceComponent(const FRay& Ray, FHitResult& OutHitResult)
{
	// Ray 방향으로 빌보드 행렬을 계산 (CachedWorldMatrix는 active 카메라 기준이라 다른 뷰포트에서 틀림)
	FMatrix PerRayBillboard = ComputeBillboardMatrix(Ray.Direction);
	FMatrix OutlineWorldMatrix = CalculateOutlineMatrix(PerRayBillboard);
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

void UTextRenderComponent::Serialize(FArchive& Ar)
{
	UBillboardComponent::Serialize(Ar);

	Ar << Text;
	Ar << FontName;
	Ar << Color;
	Ar << FontSize;
	Ar << Spacing;
	Ar << CharWidth;
	Ar << CharHeight;
	Ar << RenderSpace;
	Ar << HAlign;
	Ar << VAlign;
	Ar << ScreenX;
	Ar << ScreenY;
}

void UTextRenderComponent::PostDuplicate()
{
	UBillboardComponent::PostDuplicate();
	// 폰트 리소스 재바인딩 — 직렬화된 FontName 기준으로 ResourceManager에서 다시 lookup
	SetFont(FontName);
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
	// TextRender의 GetEditableProperties는 USceneComponent 베이스를 직접 사용한다.
	USceneComponent::PostEditProperty(PropertyName);

	if (strcmp(PropertyName, "Font") == 0)
	{
		SetFont(FontName);
		MarkProxyDirty(EDirtyFlag::Mesh);
	}
	else if (strcmp(PropertyName, "Text") == 0)
	{
		// 문자 길이가 바뀌면 빌보드 바운드/아웃라인 행렬이 변한다.
		MarkProxyDirty(EDirtyFlag::Transform);
		MarkWorldBoundsDirty();
	}
	else if (strcmp(PropertyName, "Font Size") == 0)
	{
		MarkProxyDirty(EDirtyFlag::Transform);
	}
	else if (strcmp(PropertyName, "Visible") == 0)
	{
		MarkRenderVisibilityDirty();
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

FMatrix UTextRenderComponent::CalculateOutlineMatrix(const FMatrix& BillboardWorldMatrix) const
{
	int32 Len = GetUTF8Length(Text);

	if (Len <= 0) return FMatrix::Identity;

	float TotalLocalWidth = (Len * CharWidth);

	float CenterY = TotalLocalWidth * -0.5f;
	float CenterZ = 0.0f;

	FMatrix ScaleMatrix = FMatrix::MakeScaleMatrix(FVector(1.0f, TotalLocalWidth, CharHeight));
	FMatrix TransMatrix = FMatrix::MakeTranslationMatrix(FVector(0.0f, CenterY, CenterZ));

	return (ScaleMatrix * TransMatrix) * BillboardWorldMatrix;
}

int32 UTextRenderComponent::GetUTF8Length(const FString& str) const {
	int32 count = 0;
	for (size_t i = 0; i < str.length(); ++i) {
		// UTF-8의 첫 바이트가 10xxxxxx 이 아니면 새로운 글자의 시작임
		if ((str[i] & 0xC0) != 0x80) count++;
	}
	return count;
}