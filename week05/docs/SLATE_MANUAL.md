# Slate UI 사용 설명서

## 1. 개요

이 프로젝트의 Slate는 "즉시 렌더링"이 아니라 "드로우 리스트 기록 후 렌더링" 구조입니다.

1. 위젯 트리가 `FSlatePaintContext`에 `FUIDrawElement`를 기록합니다.
2. 렌더러(`FScreenUIRenderer`)가 기록된 리스트를 정렬/배칭 후 GPU로 그립니다.

핵심 타입:

- `SWidget`: 모든 위젯의 베이스 클래스
- `SPanel`: 슬롯(`FSlot`) 기반 컨테이너 베이스
- `FSlot`: 패딩/정렬/Fill + `Layer`, `ZOrder` 보관
- `FSlatePaintContext`: DrawList 기록기
- `FUIDrawList`/`FUIDrawElement`: 최종 UI 렌더 입력 데이터

---

## 2. 렌더 순서 규칙

UI 최종 정렬 키는 아래 순서입니다.

1. `Layer` 오름차순
2. `Depth` 오름차순
3. `Order` 오름차순 (기록 순서)

즉, 같은 레이어에서는 `Depth`가 큰 항목이 더 위에 보입니다.

참고:

- `Depth`는 내부 정렬/배칭 시 `1/1024` 단위 키로 양자화됩니다.
- `Layer`와 `Depth` 모두 패널 계층에서 누적(push/pop) 방식으로 적용됩니다.

---

## 3. 입력(마우스) 우선순위 규칙

`SPanel`은 렌더 순서와 입력 순서가 어긋나지 않도록 동일한 기준을 사용합니다.

Paint 순서:

1. 일반 위젯(`WantsPopupPaintPriority == false`)을 `Layer, ZOrder` 오름차순으로 그림
2. 팝업 위젯(`true`)도 `Layer, ZOrder` 오름차순으로 그림

MouseDown 순서:

1. 팝업 위젯을 위에서부터(역순) 검사
2. 일반 위젯을 위에서부터(역순) 검사

---

## 4. `FSlot`에서 순서 제어하기

`FSlot`이 UI 순서 제어의 기본 단위입니다.

주요 API:

- `.SetLayer(int32 InLayer)`
- `.SetZOrder(int32 InZOrder)`
- `.Padding(...)`, `.HAlign(...)`, `.VAlign(...)`
- `.AutoWidth()`, `.FillWidth(...)`, `.AutoHeight()`, `.FillHeight(...)`

예시:

```cpp
SOverlay* Overlay = SlateApp->CreateWidget<SOverlay>();

Overlay->AddSlot()
	.SetLayer(0)
	.SetZOrder(0)
	[
		SlateApp->CreateWidget<SImage>()->SetTint(0xFF202020)
	];

Overlay->AddSlot()
	.SetLayer(1)
	.SetZOrder(10)
	[
		SlateApp->CreateWidget<STextBlock>()->SetText("Foreground")
	];
```

---

## 5. 커스텀 위젯 만들기

```cpp
class SMyWidget : public SWidget
{
public:
	void OnPaint(FSlatePaintContext& Painter) override
	{
		Painter.DrawRectFilled(Rect, 0xFF2A2A2A);
		Painter.DrawRect(Rect, 0xFF00B7FF);
		Painter.DrawText({ Rect.X + 8, Rect.Y + 8 }, "Custom", 0xFFFFFFFF, 14.0f, 1.0f);
	}

	FVector2 ComputeDesiredSize() const override
	{
		return { 120.0f, 40.0f };
	}
};
```

---

## 6. Painter 직접 제어 (고급)

일반적으로는 `FSlot.SetLayer/SetZOrder`만 쓰면 됩니다.

특수한 경우 `OnPaint`에서 직접 제어도 가능합니다.

```cpp
void SSomeWidget::OnPaint(FSlatePaintContext& Painter)
{
	Painter.PushLayer(2);
	Painter.PushDepth(5.0f);

	Painter.DrawRectFilled(Rect, 0xFF404040);
	Painter.DrawText({ Rect.X + 6, Rect.Y + 6 }, "Layer+Depth Override", 0xFFFFFFFF, 12.0f, 1.0f);

	Painter.PopDepth();
	Painter.PopLayer();
}
```

주의:

- push/pop 쌍이 반드시 맞아야 합니다.
- 수동 제어와 슬롯 제어를 섞을 때는 누적값 기준으로 동작합니다.

---

## 7. 배칭 동작 요약

`FScreenUIRenderer`는 정렬된 순서에서 아래 조건이 같으면 메시를 병합합니다.

1. Material 포인터
2. Layer
3. Depth 정렬 키(양자화 키)
4. Topology
5. 인덱스 사용 여부(Indexed/Non-Indexed)

즉, 같은 스타일의 연속 UI는 draw call이 줄어듭니다.

---

## 8. 디버깅 체크리스트

겹친 UI 순서가 이상할 때:

1. 해당 슬롯의 `SetLayer`, `SetZOrder` 값을 확인
2. 부모/자식에서 layer/depth가 누적되는지 확인
3. `WantsPopupPaintPriority()` 사용 여부 확인
4. 같은 Material을 쓰는 UI가 중간에 끊겨 배칭이 깨지는지 확인

클릭 우선순위가 이상할 때:

1. 위젯이 실제로 화면에서 가장 위인지(`Layer/ZOrder`) 확인
2. 팝업 우선 그룹(`WantsPopupPaintPriority`)에 들어가는지 확인
3. 위젯 내부 `OnMouseDown`에서 hit test 조건을 놓치지 않았는지 확인
