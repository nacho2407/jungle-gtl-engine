#include "DecalComponent.h"
#include "Resource/ResourceManager.h"

IMPLEMENT_CLASS(UDecalComponent, UPrimitiveComponent)

void UDecalComponent::Serialize(FArchive& Ar)
{
}

void UDecalComponent::PostDuplicate()
{
	UPrimitiveComponent::PostDuplicate();
	SetTexture(TextureName);
}

void UDecalComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
}

void UDecalComponent::PostEditProperty(const char* PropertyName)
{
}

void UDecalComponent::UpdateLocalExtents()
{
	LocalExtents = DecalSize * 0.5f;
}

void UDecalComponent::SetTexture(const FName& InTextureName)
{
	TextureName = InTextureName;
	CachedTexture = FResourceManager::Get().FindTexture(InTextureName);
	// 텍스처 유무가 batcher/Primitive 경로 분기를 좌우하므로 Mesh 단계까지 재갱신 필요.
	MarkProxyDirty(EDirtyFlag::Material);
}

void UDecalComponent::SetDecalSize(const FVector& InSize)
{
	DecalSize = InSize;
	MarkRenderTransformDirty();
	UpdateLocalExtents();
	MarkWorldBoundsDirty();
}
