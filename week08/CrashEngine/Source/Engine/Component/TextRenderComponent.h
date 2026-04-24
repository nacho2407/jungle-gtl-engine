// 컴포넌트 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once

#include "Core/ResourceTypes.h"
#include "Object/FName.h"
#include "PrimitiveComponent.h"

// 텍스트 수평 정렬
enum class ETextHAlign : int32
{
    Left,
    Center,
    Right
};

// 텍스트 수직 정렬
enum class ETextVAlign : int32
{
    Top,
    Center,
    Bottom
};

// UTextRenderComponent 컴포넌트이다.
class UTextRenderComponent : public UPrimitiveComponent
{
public:
    DECLARE_CLASS(UTextRenderComponent, UPrimitiveComponent)

    UTextRenderComponent();
    ~UTextRenderComponent() override = default;

    void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
    void PostEditProperty(const char* PropertyName) override;

    void Serialize(FArchive& Ar) override;
    void PostDuplicate() override;

    void SetText(const FString& InText);
    bool ReacquireDefaultFont();
    const FString& GetText() const { return Text; }

    FString GetOwnerUUIDToString() const;
    FString GetOwnerNameToString() const;

    void SetFont(const FName& InFontName);
    const FFontResource* GetFont() const { return CachedFont; }
    const FName& GetFontName() const { return FontName; }

    void SetColor(const FVector4& InColor) { Color = InColor; }
    const FVector4& GetColor() const { return Color; }

    void SetFontSize(float InSize);
    float GetFontSize() const { return FontSize; }

    void SetBillboard(bool bEnable);
    bool IsBillboard() const { return bBillboard; }

    void SetHorizontalAlignment(ETextHAlign InAlign) { HAlign = InAlign; }
    ETextHAlign GetHorizontalAlignment() const { return HAlign; }

    void SetVerticalAlignment(ETextVAlign InAlign) { VAlign = InAlign; }
    ETextVAlign GetVerticalAlignment() const { return VAlign; }

    FPrimitiveSceneProxy* CreateSceneProxy() override;

    FMeshBuffer* GetMeshBuffer() const override;
    FMeshDataView GetMeshDataView() const override;

    void UpdateWorldAABB() const override;
    bool LineTraceComponent(const FRay& Ray, FHitResult& OutHitResult) override;

    FMatrix CalculateBillboardWorldMatrix(const FVector& CameraForward) const;
    FMatrix CalculateOutlineMatrix() const;
    FMatrix CalculateOutlineMatrix(const FMatrix& TextWorldMatrix) const;
    int32 GetUTF8Length(const FString& Str) const;

private:
    FString Text;
    FName FontName = FName("Default");
    FFontResource* CachedFont = nullptr; // ResourceManager 소유, 여기선 참조만

    FVector4 Color = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
    float FontSize = 1.0f;
    float Spacing = 0.1f;
    float CharWidth = 0.5f;
    float CharHeight = 0.5f;
    bool bBillboard = true;

    ETextHAlign HAlign = ETextHAlign::Center;
    ETextVAlign VAlign = ETextVAlign::Center;
};
