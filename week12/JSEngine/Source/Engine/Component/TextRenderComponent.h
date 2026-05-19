#pragma once

#include "PrimitiveComponent.h"
#include "Core/ResourceTypes.h"
#include "Object/FName.h"

// 텍스트 렌더링 공간 모드
UENUM()
enum class ETextRenderSpace : int32
{
	World UMETA(DisplayName = "World"),		// 3D 공간에 빌보드로 렌더링
	Screen UMETA(DisplayName = "Screen")		// 2D 스크린 좌표에 고정 렌더링
};

// 텍스트 수평 정렬
UENUM()
enum class ETextHAlign : int32
{
	Left UMETA(DisplayName = "Left"),
	Center UMETA(DisplayName = "Center"),
	Right UMETA(DisplayName = "Right")
};

// 텍스트 수직 정렬
UENUM()
enum class ETextVAlign : int32
{
	Top UMETA(DisplayName = "Top"),
	Center UMETA(DisplayName = "Center"),
	Bottom UMETA(DisplayName = "Bottom")
};

// 텍스트를 월드 공간에 빌보드로 렌더링하는 컴포넌트.
// PrimitiveComponent를 상속받아 RenderCollector에 자동으로 감지됩니다.
// MeshBuffer를 사용하지 않으며, FontBatcher가 드로우콜을 처리합니다.
UCLASS(SpawnableComponent, DisplayName = "TextRender Component", Category = "Basic")
class UTextRenderComponent : public UPrimitiveComponent
{
public:
	GENERATED_BODY(UTextRenderComponent, UPrimitiveComponent)

	UTextRenderComponent();
	~UTextRenderComponent() override = default;

	virtual void PostDuplicate(UObject* Original) override;

	virtual void Serialize(FArchive& Ar) override;
	void PostEditProperty(const char* PropertyName) override;

	// --- Text ---
	void SetText(const FString& InText) { Text = InText; }
	const FString& GetText() const { return Text; }

	// Owner의 UUID를 문자열로 반환
	FString GetOwnerUUIDToString() const;

	// Owner의 FName을 문자열로 반환
	FString GetOwnerNameToString() const;

	// --- Font ---
	// FName 키로 ResourceManager에서 FFontResource*를 찾아 캐싱
	void SetFont(const FName& InFontName);
	const FFontResource* GetFont() const;
	const FName& GetFontName() const { return FontName; }

	// --- Appearance ---
	void SetColor(const FVector4& InColor) { Color = InColor; }
	const FVector4& GetColor() const { return Color; }

	void SetFontSize(float InSize) { FontSize = InSize; }
	float GetFontSize() const { return FontSize; }

	// --- Space ---
	void SetRenderSpace(ETextRenderSpace InSpace) { RenderSpace = InSpace; }
	ETextRenderSpace GetRenderSpace() const { return RenderSpace; }

	// Screen 모드 전용: 스크린 좌표 (픽셀)
	void SetScreenPosition(float X, float Y) { ScreenX = X; ScreenY = Y; }
	float GetScreenX() const { return ScreenX; }
	float GetScreenY() const { return ScreenY; }

	// --- Alignment ---
	void SetHorizontalAlignment(ETextHAlign InAlign) { HAlign = InAlign; }
	ETextHAlign GetHorizontalAlignment() const { return HAlign; }

	void SetVerticalAlignment(ETextVAlign InAlign) { VAlign = InAlign; }
	ETextVAlign GetVerticalAlignment() const { return VAlign; }

	// --- PrimitiveComponent 인터페이스 ---
	EPrimitiveType GetPrimitiveType() const override { return PrimitiveType; }
	bool SupportsOutline() const override { return true; }
	static constexpr EPrimitiveType PrimitiveType = EPrimitiveType::EPT_Text;

	//Collision
	void UpdateWorldAABB() const override;
	bool RaycastMesh(const FRay& Ray, FHitResult& OutHitResult);

	FMatrix GetTextMatrix() const;
	int32 GetUTF8Length(const FString& str) const;

private:
	UPROPERTY(DisplayName = "Text", LuaReadWrite, LuaName = Text)
	FString Text;

	UPROPERTY(DisplayName = "Font")
	FName FontName = FName("Default");

	FFontResource* CachedFont = nullptr;	// ResourceManager 소유, 여기선 참조만

	UPROPERTY(DisplayName = "Color")
	FVector4 Color = FVector4(1.0f, 1.0f, 1.0f, 1.0f);

	UPROPERTY(DisplayName = "Font Size", Min = 0.1f, Max = 100.0f, Speed = 0.1f, LuaReadWrite, LuaName = FontSize)
	float FontSize = 1.0f;

	UPROPERTY(DisplayName = "Spacing", Min = 0.0f, Max = 10.0f, Speed = 0.01f)
	float Spacing = 0.1f;

	UPROPERTY(DisplayName = "Char Width", Min = 0.01f, Max = 10.0f, Speed = 0.01f)
	float CharWidth = 0.5f;

	UPROPERTY(DisplayName = "Char Height", Min = 0.01f, Max = 10.0f, Speed = 0.01f)
	float CharHeight = 0.5f;

	UPROPERTY(DisplayName = "Render Space")
	ETextRenderSpace RenderSpace = ETextRenderSpace::World;

	UPROPERTY(DisplayName = "Horizontal Alignment")
	ETextHAlign HAlign = ETextHAlign::Center;

	UPROPERTY(DisplayName = "Vertical Alignment")
	ETextVAlign VAlign = ETextVAlign::Center;

	// Screen 모드 전용
	UPROPERTY(DisplayName = "Screen X")
	float ScreenX = 0.0f;

	UPROPERTY(DisplayName = "Screen Y")
	float ScreenY = 0.0f;
};
