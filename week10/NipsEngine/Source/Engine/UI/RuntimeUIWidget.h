#pragma once

#include "Core/Containers/Array.h"
#include "Core/Containers/String.h"
#include "UI/RuntimeUIStyle.h"
#include "UI/RuntimeUITypes.h"

struct FRuntimeUIWidgetDesc
{
    FString Id;
    FString ParentId;
    ERuntimeUIWidgetType Type = ERuntimeUIWidgetType::Panel;

    FRuntimeUIVector2 AnchorMin = FRuntimeUIVector2(0.0f, 0.0f);
    FRuntimeUIVector2 AnchorMax = FRuntimeUIVector2(0.0f, 0.0f);
    FRuntimeUIVector2 Pivot = FRuntimeUIVector2(0.0f, 0.0f);
    FRuntimeUIVector2 Position;
    FRuntimeUIVector2 Size = FRuntimeUIVector2(100.0f, 32.0f);

    FString Text;
    FString ImagePath;
    FRuntimeUIUVRect ImageUV;
    ERuntimeUIImageDrawMode ImageDrawMode = ERuntimeUIImageDrawMode::Simple;
    FRuntimeUIMargin NineSliceBorder;

    FString ButtonNormalImagePath;
    FString ButtonHoverImagePath;
    FString ButtonPressedImagePath;
    FString ButtonDisabledImagePath;

    FString ProgressBackgroundImagePath;
    FString ProgressFillImagePath;
    FString ProgressFrameImagePath;
    ERuntimeUIProgressFillDirection ProgressFillDirection = ERuntimeUIProgressFillDirection::LeftToRight;

    FString ActionEvent;
    float Progress = 0.0f;
    int32 ZOrder = 0;

    bool bVisible = true;
    bool bEnabled = true;
    bool bInteractable = true;

    FRuntimeUIStyle Style;
};

class FRuntimeUIWidget
{
public:
    FRuntimeUIWidget() = default;
    explicit FRuntimeUIWidget(const FRuntimeUIWidgetDesc& InDesc);

    const FString& GetId() const { return Id; }
    const FString& GetParentId() const { return ParentId; }
    ERuntimeUIWidgetType GetType() const { return Type; }

    void AddChild(const FString& ChildId);
    void RemoveChild(const FString& ChildId);
    const TArray<FString>& GetChildren() const { return Children; }

    void SetParentId(const FString& InParentId) { ParentId = InParentId; }
    void SetText(const FString& InText) { Text = InText; }
    void SetImagePath(const FString& InImagePath) { ImagePath = InImagePath; }
    void SetImageUV(const FRuntimeUIUVRect& InImageUV) { ImageUV = InImageUV; }
    void SetImageDrawMode(ERuntimeUIImageDrawMode InDrawMode) { ImageDrawMode = InDrawMode; }
    void SetNineSliceBorder(const FRuntimeUIMargin& InBorder) { NineSliceBorder = InBorder; }
    void SetButtonImages(const FString& Normal, const FString& Hover, const FString& Pressed, const FString& Disabled);
    void SetProgressImages(const FString& Background, const FString& Fill, const FString& Frame);
    void SetProgressFillDirection(ERuntimeUIProgressFillDirection InDirection) { ProgressFillDirection = InDirection; }
    void SetProgress(float InProgress) { Progress = RuntimeUIClamp01(InProgress); }
    void SetVisible(bool bInVisible) { bVisible = bInVisible; }
    void SetEnabled(bool bInEnabled) { bEnabled = bInEnabled; }
    void SetInteractable(bool bInInteractable) { bInteractable = bInInteractable; }
    void SetActionEvent(const FString& InActionEvent) { ActionEvent = InActionEvent; }
    void SetLocalPosition(const FRuntimeUIVector2& InPosition) { Position = InPosition; }
    void SetSize(const FRuntimeUIVector2& InSize) { Size = InSize; }
    void SetAnchors(const FRuntimeUIVector2& InAnchorMin, const FRuntimeUIVector2& InAnchorMax);
    void SetPivot(const FRuntimeUIVector2& InPivot) { Pivot = InPivot; }
    void SetStyle(const FRuntimeUIStyle& InStyle) { Style = InStyle; }
    void SetZOrder(int32 InZOrder) { ZOrder = InZOrder; }
    void SetTint(const FRuntimeUIColor& InTint) { Style.Tint = InTint; }
    void SetBackgroundColor(const FRuntimeUIColor& InColor) { Style.BackgroundColor = InColor; }
    void SetTextColor(const FRuntimeUIColor& InColor) { Style.TextColor = InColor; }
    void SetAlpha(float InAlpha) { Style.Alpha = RuntimeUIClamp01(InAlpha); }
    void SetRounding(float InRounding) { Style.Rounding = InRounding; }
    void SetFontScale(float InFontScale) { Style.FontScale = InFontScale; }

    const FString& GetText() const { return Text; }
    const FRuntimeUIVector2& GetLocalPosition() const { return Position; }
    const FRuntimeUIVector2& GetSize() const { return Size; }
    const FString& GetImagePath() const { return ImagePath; }
    const FRuntimeUIUVRect& GetImageUV() const { return ImageUV; }
    ERuntimeUIImageDrawMode GetImageDrawMode() const { return ImageDrawMode; }
    const FRuntimeUIMargin& GetNineSliceBorder() const { return NineSliceBorder; }
    const FString& GetButtonNormalImagePath() const { return ButtonNormalImagePath; }
    const FString& GetButtonHoverImagePath() const { return ButtonHoverImagePath; }
    const FString& GetButtonPressedImagePath() const { return ButtonPressedImagePath; }
    const FString& GetButtonDisabledImagePath() const { return ButtonDisabledImagePath; }
    const FString& GetProgressBackgroundImagePath() const { return ProgressBackgroundImagePath; }
    const FString& GetProgressFillImagePath() const { return ProgressFillImagePath; }
    const FString& GetProgressFrameImagePath() const { return ProgressFrameImagePath; }
    ERuntimeUIProgressFillDirection GetProgressFillDirection() const { return ProgressFillDirection; }
    const FString& GetCurrentButtonImagePath() const;
    const FString& GetActionEvent() const { return ActionEvent; }
    float GetProgress() const { return Progress; }
    int32 GetZOrder() const { return ZOrder; }
    bool IsVisible() const { return bVisible; }
    bool IsEnabled() const { return bEnabled; }
    bool IsInteractable() const { return bInteractable; }
    const FRuntimeUIStyle& GetStyle() const { return Style; }

    const FRuntimeUIRect& GetComputedRect() const { return ComputedRect; }
    float GetComputedAlpha() const { return ComputedAlpha; }
    bool IsComputedVisible() const { return bComputedVisible; }
    bool IsComputedEnabled() const { return bComputedEnabled; }
    bool IsHovered() const { return bHovered; }
    bool IsPressed() const { return bPressed; }

    void SetHovered(bool bInHovered) { bHovered = bInHovered; }
    void SetPressed(bool bInPressed) { bPressed = bInPressed; }

    void ComputeLayout(
        const FRuntimeUIRect& ParentRect,
        bool bParentVisible,
        bool bParentEnabled,
        float ParentAlpha,
        float LayoutScale = 1.0f);

private:
    FString Id;
    FString ParentId;
    ERuntimeUIWidgetType Type = ERuntimeUIWidgetType::Panel;
    TArray<FString> Children;

    FRuntimeUIVector2 AnchorMin = FRuntimeUIVector2(0.0f, 0.0f);
    FRuntimeUIVector2 AnchorMax = FRuntimeUIVector2(0.0f, 0.0f);
    FRuntimeUIVector2 Pivot = FRuntimeUIVector2(0.0f, 0.0f);
    FRuntimeUIVector2 Position;
    FRuntimeUIVector2 Size = FRuntimeUIVector2(100.0f, 32.0f);

    FString Text;
    FString ImagePath;
    FRuntimeUIUVRect ImageUV;
    ERuntimeUIImageDrawMode ImageDrawMode = ERuntimeUIImageDrawMode::Simple;
    FRuntimeUIMargin NineSliceBorder;

    FString ButtonNormalImagePath;
    FString ButtonHoverImagePath;
    FString ButtonPressedImagePath;
    FString ButtonDisabledImagePath;

    FString ProgressBackgroundImagePath;
    FString ProgressFillImagePath;
    FString ProgressFrameImagePath;
    ERuntimeUIProgressFillDirection ProgressFillDirection = ERuntimeUIProgressFillDirection::LeftToRight;

    FString ActionEvent;
    float Progress = 0.0f;
    int32 ZOrder = 0;

    bool bVisible = true;
    bool bEnabled = true;
    bool bInteractable = true;

    FRuntimeUIStyle Style;

    FRuntimeUIRect ComputedRect;
    float ComputedAlpha = 1.0f;
    bool bComputedVisible = true;
    bool bComputedEnabled = true;
    bool bHovered = false;
    bool bPressed = false;
};
