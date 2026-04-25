#pragma once

#include "UI/SWindow.h"

// 분할 방향
enum class ESplitOrientation : uint8
{
	Horizontal,  // 좌/우
	Vertical,    // 상/하
};

// 스플리터 기반 — 두 개의 자식 창을 담아 영역을 분배하는 컨테이너
class SSplitter : public SWindow
{
public:
	SSplitter() = default;
	~SSplitter() override = default;

	void SetSideLT(SWindow* InSide) { SideLT = InSide; }
	void SetSideRB(SWindow* InSide) { SideRB = InSide; }
	SWindow* GetSideLT() const { return SideLT; }
	SWindow* GetSideRB() const { return SideRB; }

	void SetRatio(float InRatio);
	float GetRatio() const { return Ratio; }
	void SetTargetRatio(float InRatio, bool bAnimate = true);
	bool UpdateAnimation(float DeltaTime);
	bool IsAnimating() const { return bIsAnimating; }
	void StopAnimation(bool bSnapToTarget);

	float GetSplitBarSize() const { return SplitBarSize; }

	ESplitOrientation GetOrientation() const { return Orientation; }

	// 부모 Rect를 받아 자식들의 Rect를 계산 (재귀)
	virtual void ComputeLayout(const FRect& ParentRect) = 0;

	// 분할 바 영역 반환 (히트 테스트/렌더용)
	const FRect& GetSplitBarRect() const { return SplitBarRect; }

	// 분할 바 히트 테스트
	bool IsOverSplitBar(FPoint MousePos) const;

	bool IsSplitter() const override { return true; }

	// 자식이 SSplitter인지 타입 체크 (dynamic_cast 대체)
	static SSplitter* AsSplitter(SWindow* InWindow);

	// 재귀 유틸리티 — 트리 내 모든 SSplitter를 수집
	static void CollectSplitters(SSplitter* Node, TArray<SSplitter*>& OutSplitters);

	// 재귀 유틸리티 — 트리에서 SSplitter 노드만 삭제 (SWindow 리프는 유지)
	static void DestroyTree(SSplitter* Node);

	// 재귀 유틸리티 — 마우스가 올라간 분할 바의 SSplitter를 반환
	static SSplitter* FindSplitterAtBar(SSplitter* Node, FPoint MousePos);

protected:
	SWindow* SideLT = nullptr;  // Left or Top
	SWindow* SideRB = nullptr;  // Right or Bottom

	ESplitOrientation Orientation = ESplitOrientation::Horizontal;

	float Ratio = 0.5f;
	float TargetRatio = 0.5f;
	bool bIsAnimating = false;
	float SplitBarSize = 4.0f;

	FRect SplitBarRect;
};

// 수평 분할 (좌/우)
class SSplitterH : public SSplitter
{
public:
	SSplitterH() { Orientation = ESplitOrientation::Horizontal; }
	~SSplitterH() override = default;

	void ComputeLayout(const FRect& ParentRect) override;
};

// 수직 분할 (상/하)
class SSplitterV : public SSplitter
{
public:
	SSplitterV() { Orientation = ESplitOrientation::Vertical; }
	~SSplitterV() override = default;

	void ComputeLayout(const FRect& ParentRect) override;
};
