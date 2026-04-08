# Slate UI Framework Manual

Slate는 Krafton Jungle Engine의 에디터 인터페이스를 구축하기 위한 커스텀 UI 프레임워크입니다. Unreal Engine의 Slate와 유사한 선언적 문법과 슬롯 기반 레이아웃 시스템을 제공합니다.

---

## 1. 핵심 개념

### 1.1 SWidget
모든 UI 요소의 최상위 기본 클래스입니다. `OnPaint`, `ComputeDesiredSize`, `OnMouseDown` 등의 가상 함수를 통해 동작을 정의합니다.

- **주요 속성**:
    - `Rect`: 위젯의 실제 화면 영역 (X, Y, Width, Height)

- **주요 가상 함수**:
    - `OnPaint(FSlatePaintContext& Painter)`: 위젯을 화면에 그립니다.
    - `ComputeDesiredSize()`: 위젯이 원하는 크기를 반환합니다.
    - `ArrangeChildren()`: 자식 위젯들의 위치와 크기를 결정합니다.

### 1.2 FSlateApplication
Slate UI 시스템을 관리하는 중앙 매니저입니다. 뷰포트 레이아웃, 오버레이 위젯, 마우스 입력 처리 등을 담당합니다.

- `CreateWidget<T>(...)`: 새로운 위젯을 생성하고 관리 목록에 추가합니다.
- `AddOverlayWidget(SWidget* W)`: 화면 최상단에 위젯을 추가합니다 (예: 툴바, 통계 창).

### 1.3 FSlot (슬롯)
컨테이너 위젯(VerticalBox, HorizontalBox 등)에서 자식 위젯을 배치하는 단위입니다.

- **속성**:
    - `Padding`: 자식 위젯 주변의 여백
    - `HAlign / VAlign`: 수평/수직 정렬 (Left, Center, Right, Fill / Top, Center, Bottom, Fill)
    - `AutoWidth / FillWidth`: 가로 크기 결정 방식
    - `AutoHeight / FillHeight`: 세로 크기 결정 방식

---

## 2. 주요 위젯

### 2.1 기본 위젯
- **STextBlock**: 텍스트를 출력합니다. 폰트 크기, 색상, 정렬을 설정할 수 있습니다.
- **SButton**: 클릭 가능한 버튼입니다. `OnClicked` 콜백을 통해 이벤트를 처리합니다.
- **SImage**: 색상이 채워진 사각형이나 이미지를 출력합니다.
- **SDropdown**: 여러 옵션 중 하나를 선택할 수 있는 드롭다운 메뉴입니다. `OnSelectionChanged` 콜백을 사용합니다.

### 2.2 컨테이너 위젯
- **SVerticalBox**: 자식들을 세로로 쌓습니다.
- **SHorizontalBox**: 자식들을 가로로 나열합니다.
- **SOverlay**: 자식들을 겹쳐서 배치합니다. 나중에 추가된 자식이 위에 그려집니다.

---

## 3. FSlot 상세 설정

슬롯은 위젯의 배치 방식을 결정하는 다양한 메서드를 제공합니다.

- `.Padding(FMargin(Left, Top, Right, Bottom))`: 여백 설정
- `.HAlign(EHAlign::...)`: 수평 정렬 (Left, Center, Right, Fill)
- `.VAlign(EVAlign::...)`: 수직 정렬 (Top, Center, Bottom, Fill)
- `.AutoWidth() / .FillWidth(Ratio)`: 가로 크기 (컨텐츠 맞춤 / 비율 기반 채우기)
- `.AutoHeight() / .FillHeight(Ratio)`: 세로 크기 (컨텐츠 맞춤 / 비율 기반 채우기)
- `.SetZOrder(int32)`: 겹침 순서 설정 (SOverlay 등에서 사용)

---

## 4. 사용 예시

### 4.1 간단한 위젯 구성 (VerticalBox)

```cpp
auto MainBox = SlateApp->CreateWidget<SVerticalBox>();

MainBox->AddSlot()
    .AutoHeight()
    .HAlign(EHAlign::Center)
    .Padding(10.0f)
    [
        SlateApp->CreateWidget<STextBlock>()
            ->SetText("Hello Slate!")
    ];

MainBox->AddSlot()
    .AutoHeight()
    .Padding(5.0f)
    [
        SlateApp->CreateWidget<SButton>()
            ->SetText("Click Me")
            ->OnClicked = []() { 
                LOG("Button Clicked!"); 
            }
    ];
```

### 4.2 드롭다운 사용 예시

```cpp
auto MyDropdown = SlateApp->CreateWidget<SDropdown>();
MyDropdown->SetOptions({ "Option 1", "Option 2", "Option 3" });
MyDropdown->OnSelectionChanged = [](int32 Index) {
    LOG("Selected: %d", Index);
};
```

### 4.3 복합 레이아웃

```cpp
auto ToolBar = SlateApp->CreateWidget<SHorizontalBox>();

ToolBar->AddSlot()
    .FillWidth(1.0f)
    .VAlign(EVAlign::Center)
    [
        SlateApp->CreateWidget<STextBlock>()->SetText("Left Content")
    ];

ToolBar->AddSlot()
    .AutoWidth()
    .Padding(FMargin(10, 0))
    [
        SlateApp->CreateWidget<SButton>()->SetText("Action")
    ];

SlateApp->AddOverlayWidget(ToolBar);
```

---

## 4. 커스텀 위젯 만들기

`SWidget`을 상속받아 새로운 위젯을 만들 수 있습니다.

```cpp
class SMyWidget : public SWidget
{
public:
    void OnPaint(FSlatePaintContext& Painter) override
    {
        // 배경 그리기
        Painter.DrawRect(Rect, 0xFF0000FF); // 파란색 사각형
        
        // 텍스트 그리기
        Painter.DrawText({ Rect.X, Rect.Y }, "Custom Widget", 0xFFFFFFFF, 12.0f);
    }

    FVector2 ComputeDesiredSize() const override
    {
        return { 100.0f, 50.0f };
    }
};
```

---

## 5. 팁 및 주의사항
- **슬롯 체이닝**: `AddSlot()` 이후에 호출되는 함수들(`.Padding()`, `.HAlign()` 등)은 `FSlot&`을 반환하므로 체이닝하여 설정할 수 있습니다.
- **대괄호 `[]` 연산자**: `FSlot`에 위젯을 할당할 때 사용합니다.
- **좌표계**: Slate는 화면 절대 좌표계가 아닌, 부모 위젯으로부터 결정된 `Rect`를 기반으로 그립니다.
