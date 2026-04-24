// 컴포넌트 영역의 세부 동작을 구현합니다.
#include "TextRenderComponent.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "Object/ObjectFactory.h"
#include "Render/Resources/Buffers/MeshBufferManager.h"
#include "Render/Resources/Meshes/PrimitiveMeshTypes.h"
#include "Render/Scene/Proxies/Primitive/TextRenderSceneProxy.h"
#include "Resource/ResourceManager.h"
#include "Serialization/Archive.h"

IMPLEMENT_CLASS(UTextRenderComponent, UPrimitiveComponent)

FPrimitiveSceneProxy* UTextRenderComponent::CreateSceneProxy()
{
    return new FTextRenderSceneProxy(this);
}

namespace
{
FMatrix BuildStableTextBillboardMatrix(const FVector& CameraForward, const FVector& WorldLocation, const FVector& WorldScale, const FVector& PreferredUp)
{
    FVector Forward = (CameraForward * -1.0f).Normalized();
    FVector UpSeed = PreferredUp;

    if (UpSeed.Dot(UpSeed) < 1.0e-4f)
    {
        UpSeed = FVector(0.0f, 0.0f, 1.0f);
    }

    FVector UpSeedNormalized = UpSeed.Normalized();
    if (std::abs(Forward.Dot(UpSeedNormalized)) > 0.95f)
    {
        UpSeed = FVector(0.0f, 0.0f, 1.0f);
        if (std::abs(Forward.Dot(UpSeed)) > 0.95f)
        {
            UpSeed = FVector(0.0f, 1.0f, 0.0f);
        }
    }

    FVector Right = Forward.Cross(UpSeed);
    if (Right.Dot(Right) < 1.0e-4f)
    {
        Right = FVector(1.0f, 0.0f, 0.0f);
    }
    Right = Right.Normalized();

    const FVector Up = Right.Cross(Forward).Normalized();

    FMatrix RotMatrix;
    RotMatrix.SetAxes(Forward, Right, Up);
    return FMatrix::MakeScaleMatrix(WorldScale) * RotMatrix * FMatrix::MakeTranslationMatrix(WorldLocation);
}
} // namespace


bool UTextRenderComponent::ReacquireDefaultFont()
{
    CachedFont = FResourceManager::Get().FindFont(FontName);
    if (CachedFont && CachedFont->IsLoaded())
    {
        return true;
    }

    FontName = FName("Default");
    CachedFont = FResourceManager::Get().FindFont(FontName);
    return CachedFont && CachedFont->IsLoaded();
}

void UTextRenderComponent::SetText(const FString& InText)
{
    if (Text == InText)
    {
        return;
    }

    Text = InText;
    MarkProxyDirty(ESceneProxyDirtyFlag::Transform);
    MarkWorldBoundsDirty();
}

FMeshBuffer* UTextRenderComponent::GetMeshBuffer() const
{
    return &FMeshBufferManager::Get().GetMeshBuffer(EMeshShape::Quad);
}

FMeshDataView UTextRenderComponent::GetMeshDataView() const
{
    return FMeshDataView::FromMeshData(FMeshBufferManager::Get().GetMeshData(EMeshShape::Quad));
}

void UTextRenderComponent::SetFont(const FName& InFontName)
{
    if (FontName == InFontName)
    {
        return;
    }

    FontName = InFontName;
    CachedFont = FResourceManager::Get().FindFont(FontName);
    if (!CachedFont || !CachedFont->IsLoaded())
    {
        ReacquireDefaultFont();
    }
    MarkProxyDirty(ESceneProxyDirtyFlag::Mesh);
}

void UTextRenderComponent::SetFontSize(float InSize)
{
    if (FontSize == InSize)
    {
        return;
    }

    FontSize = InSize;
    MarkProxyDirty(ESceneProxyDirtyFlag::Transform);
    MarkWorldBoundsDirty();
}

void UTextRenderComponent::SetBillboard(bool bEnable)
{
    if (bBillboard == bEnable)
    {
        return;
    }

    bBillboard = bEnable;
    MarkProxyDirty(ESceneProxyDirtyFlag::Transform);
    MarkWorldBoundsDirty();
}

void UTextRenderComponent::UpdateWorldAABB() const
{
    const float TotalWidth = GetUTF8Length(Text) * CharWidth;
    const float MaxExtent = std::max(TotalWidth, CharHeight);

    const FVector WorldScale = GetWorldScale();
    const float ScaledMax = MaxExtent * std::max({ WorldScale.X, WorldScale.Y, WorldScale.Z });

    const FVector WorldCenter = GetWorldLocation();
    const FVector Extent(ScaledMax, ScaledMax, ScaledMax);

    WorldAABBMinLocation = WorldCenter - Extent;
    WorldAABBMaxLocation = WorldCenter + Extent;
    bWorldAABBDirty = false;
    bHasValidWorldAABB = true;
}

bool UTextRenderComponent::LineTraceComponent(const FRay& Ray, FHitResult& OutHitResult)
{
    const FMatrix TextWorldMatrix = bBillboard ? CalculateBillboardWorldMatrix(Ray.Direction) : GetWorldMatrix();
    const FMatrix OutlineWorldMatrix = CalculateOutlineMatrix(TextWorldMatrix);
    const FMatrix InvWorldMatrix = OutlineWorldMatrix.GetInverse();

    FRay LocalRay;
    LocalRay.Origin = InvWorldMatrix.TransformPositionWithW(Ray.Origin);
    LocalRay.Direction = InvWorldMatrix.TransformVector(Ray.Direction).Normalized();

    if (std::abs(LocalRay.Direction.X) < 0.00111f)
    {
        return false;
    }

    const float t = -LocalRay.Origin.X / LocalRay.Direction.X;
    if (t < 0.0f)
    {
        return false;
    }

    const FVector LocalHitPos = LocalRay.Origin + LocalRay.Direction * t;
    if (LocalHitPos.Y >= -0.5f && LocalHitPos.Y <= 0.5f &&
        LocalHitPos.Z >= -0.5f && LocalHitPos.Z <= 0.5f)
    {
        const FVector WorldHitPos = OutlineWorldMatrix.TransformPositionWithW(LocalHitPos);
        OutHitResult.Distance = (WorldHitPos - Ray.Origin).Length();
        OutHitResult.HitComponent = this;
        return true;
    }

    return false;
}

void UTextRenderComponent::Serialize(FArchive& Ar)
{
    UPrimitiveComponent::Serialize(Ar);

    Ar << Text;
    Ar << FontName;
    Ar << Color;
    Ar << FontSize;
    Ar << Spacing;
    Ar << CharWidth;
    Ar << CharHeight;
    Ar << bBillboard;
    Ar << HAlign;
    Ar << VAlign;

    if (Ar.IsLoading())
    {
        ReacquireDefaultFont();
    }
}

void UTextRenderComponent::PostDuplicate()
{
    UPrimitiveComponent::PostDuplicate();
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
    return Name.IsValid() ? Name.ToString() : FName::None.ToString();
}

UTextRenderComponent::UTextRenderComponent()
{
    ReacquireDefaultFont();
}

void UTextRenderComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    UPrimitiveComponent::GetEditableProperties(OutProps);
    OutProps.push_back({ "Text", EPropertyType::String, &Text });
    OutProps.push_back({ "Font", EPropertyType::Name, &FontName });
    OutProps.push_back({ "Font Size", EPropertyType::Float, &FontSize, 0.1f, 100.0f, 0.1f });
    OutProps.push_back({ "Billboard", EPropertyType::Bool, &bBillboard });
}

void UTextRenderComponent::PostEditProperty(const char* PropertyName)
{
    UPrimitiveComponent::PostEditProperty(PropertyName);

    if (strcmp(PropertyName, "Font") == 0)
    {
        SetFont(FontName);
        MarkProxyDirty(ESceneProxyDirtyFlag::Mesh);
    }
    else if (strcmp(PropertyName, "Text") == 0)
    {
        MarkProxyDirty(ESceneProxyDirtyFlag::Transform);
        MarkWorldBoundsDirty();
    }
    else if (strcmp(PropertyName, "Font Size") == 0 || strcmp(PropertyName, "Billboard") == 0)
    {
        MarkProxyDirty(ESceneProxyDirtyFlag::Transform);
        MarkWorldBoundsDirty();
    }
}

FMatrix UTextRenderComponent::CalculateBillboardWorldMatrix(const FVector& CameraForward) const
{
    return BuildStableTextBillboardMatrix(CameraForward, GetWorldLocation(), GetWorldScale(), GetUpVector());
}

FMatrix UTextRenderComponent::CalculateOutlineMatrix() const
{
    const FMatrix BaseWorldMatrix = bBillboard ? CachedWorldMatrix : GetWorldMatrix();
    return CalculateOutlineMatrix(BaseWorldMatrix);
}

FMatrix UTextRenderComponent::CalculateOutlineMatrix(const FMatrix& TextWorldMatrix) const
{
    const int32 Len = GetUTF8Length(Text);
    if (Len <= 0)
    {
        return FMatrix::Identity;
    }

    const float TotalLocalWidth = (Len * CharWidth);
    const float CenterY = TotalLocalWidth * -0.5f;
    const float CenterZ = 0.0f;

    const FMatrix ScaleMatrix = FMatrix::MakeScaleMatrix(FVector(1.0f, TotalLocalWidth, CharHeight));
    const FMatrix TransMatrix = FMatrix::MakeTranslationMatrix(FVector(0.0f, CenterY, CenterZ));

    return (ScaleMatrix * TransMatrix) * TextWorldMatrix;
}

int32 UTextRenderComponent::GetUTF8Length(const FString& Str) const
{
    int32 Count = 0;
    for (size_t i = 0; i < Str.length(); ++i)
    {
        if ((Str[i] & 0xC0) != 0x80)
        {
            Count++;
        }
    }
    return Count;
}
