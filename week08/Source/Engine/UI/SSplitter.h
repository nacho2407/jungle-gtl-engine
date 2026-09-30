// UI 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once

#include "UI/SWindow.h"

// ESplitOrientation은 분할 패널의 배치 방향을 정의합니다.
enum class ESplitOrientation : uint8
{
    Horizontal, // 좌/우
    Vertical,   // 상/하
};

// SSplitter는 두 UI 영역을 비율 기반으로 나누는 기본 분할 창입니다.
class SSplitter : public SWindow
{
public:
    SSplitter() = default;
    ~SSplitter() override = default;

    void SetSideLT(SWindow* InSide) { SideLT = InSide; }
    void SetSideRB(SWindow* InSide) { SideRB = InSide; }
    SWindow* GetSideLT() const { return SideLT; }
    SWindow* GetSideRB() const { return SideRB; }

    void SetRatio(float InRatio) { Ratio = InRatio; }
    float GetRatio() const { return Ratio; }

    float GetSplitBarSize() const { return SplitBarSize; }

    ESplitOrientation GetOrientation() const { return Orientation; }

    // 부모 Rect를 받아 자식들의 Rect를 계산합니다.
    virtual void ComputeLayout(const FRect& ParentRect) = 0;

    // 분할 바 영역을 반환합니다.
    const FRect& GetSplitBarRect() const { return SplitBarRect; }

    // 마우스 좌표가 분할 바 위에 있는지 확인합니다.
    bool IsOverSplitBar(FPoint MousePos) const;

    bool IsSplitter() const override { return true; }

    // SWindow 포인터를 SSplitter로 안전하게 변환합니다.
    static SSplitter* AsSplitter(SWindow* InWindow);

    // 트리 내 모든 SSplitter를 수집합니다.
    static void CollectSplitters(SSplitter* Node, TArray<SSplitter*>& OutSplitters);

    // SSplitter 트리만 정리하고 리프 SWindow는 유지합니다.
    static void DestroyTree(SSplitter* Node);

    // 마우스가 올라간 분할 바의 SSplitter를 찾습니다.
    static SSplitter* FindSplitterAtBar(SSplitter* Node, FPoint MousePos);

protected:
    SWindow* SideLT = nullptr; // Left or Top
    SWindow* SideRB = nullptr; // Right or Bottom

    ESplitOrientation Orientation = ESplitOrientation::Horizontal;

    float Ratio = 0.5f;
    float SplitBarSize = 4.0f;

    FRect SplitBarRect;
};

// SSplitterH는 좌우로 영역을 나누는 수평 분할 창입니다.
class SSplitterH : public SSplitter
{
public:
    SSplitterH() { Orientation = ESplitOrientation::Horizontal; }
    ~SSplitterH() override = default;

    void ComputeLayout(const FRect& ParentRect) override;
};

// SSplitterV는 상하로 영역을 나누는 수직 분할 창입니다.
class SSplitterV : public SSplitter
{
public:
    SSplitterV() { Orientation = ESplitOrientation::Vertical; }
    ~SSplitterV() override = default;

    void ComputeLayout(const FRect& ParentRect) override;
};
