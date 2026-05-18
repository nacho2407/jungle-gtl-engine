# 리플렉션 시스템 개발자 주의사항

> **DECLARE_CLASS(), DEFINE_CLASS(), REGISTER_FACTORY()는 더는 사용되지 않습니다.**  
> 새 클래스는 `UCLASS()`와 `GENERATED_BODY()`를 사용하고, 리플렉션 생성기가 `.gen.cpp`에서 `StaticClass()`와 프로퍼티 등록 코드를 생성하도록 합니다.

---

## 1. 기본 사용 방식

리플렉션 대상 클래스는 헤더에 다음처럼 작성합니다.

```cpp
UCLASS()
class UMyComponent : public UActorComponent
{
    GENERATED_BODY(UMyComponent, UActorComponent)

private:
    UPROPERTY(DisplayName = "Speed", Category = "Movement")
    float Speed = 100.0f;
};
```

주의사항:

- `UCLASS()`가 없는 클래스는 리플렉션 등록 대상이 아닙니다.
- `GENERATED_BODY(현재클래스, 부모클래스)`의 이름은 실제 클래스/부모 클래스와 일치해야 합니다.
- `.gen.cpp`는 직접 수정하지 않습니다.
- 헤더 수정 후에는 리플렉션 코드를 다시 생성해야 합니다.
- 생성된 `.gen.cpp`가 빌드에 포함되어야 합니다.

---

## 2. UCLASS / UPROPERTY / UENUM / UMETA에 적을 수 있는 것

현재 `GenerateReflection.py` 기준으로 주로 지원되는 메타데이터는 아래와 같습니다.

### UCLASS

```cpp
UCLASS()
UCLASS(Abstract)
UCLASS(Placeable)
UCLASS(SpawnableComponent)
UCLASS(DisplayName = "Player Camera", Category = "Camera")
```

지원 항목과 의미:

```txt
Abstract
    추상 클래스임을 표시합니다.
    객체 생성 함수가 등록되지 않거나, 직접 생성 대상에서 제외됩니다.

Placeable
    에디터에서 배치 가능한 클래스임을 표시합니다.
    Actor palette, object spawn 메뉴 등에 노출할 때 사용할 수 있습니다.

SpawnableComponent
    에디터나 런타임에서 추가 가능한 Component임을 표시합니다.
    Add Component 메뉴 등에 노출할 때 사용할 수 있습니다.

DisplayName 또는 Display
    에디터에 표시할 클래스 이름입니다.

Category
    에디터에서 분류할 카테고리 이름입니다.
```

보통은 `UCLASS()`만 사용하면 충분합니다.  
추상 클래스라면 `UCLASS(Abstract)`, 에디터에서 직접 배치할 Actor라면 `UCLASS(Placeable)`, Add Component 메뉴에 노출할 컴포넌트라면 `UCLASS(SpawnableComponent)`를 사용합니다.

---

### UPROPERTY

```cpp
UPROPERTY(DisplayName = "Health", Category = "Stats")
float Health = 100.0f;

UPROPERTY(Transient)
TObjectPtr<USceneComponent> CachedComponent;

UPROPERTY(ReferenceKind = ActorComponent)
TObjectPtr<USceneComponent> UpdatedComponent;

UPROPERTY(Min = 0.0, Max = 100.0, Speed = 0.1)
float Alpha = 1.0f;
```

지원 항목:

```txt
DisplayName 또는 Display
Category

Transient
리플렉션에는 잡히지만 저장/복제 대상에서 빠지는 런타임 값입니다. 
현재 SerializeItem, 일반 CopyValue에서 스킵됩니다.

SaveGame
세이브게임 전용 직렬화를 위한 의미 플래그입니다. 
세이브게임 serializer가 필터링할 때 쓰기 위한 상태로 남아 있습니다.

Animatable
시퀀서/애니메이션 쪽에서 읽고 쓸 수 있는 프로퍼티라는 표시입니다.

LuaRead
Lua에서 이 프로퍼티 값을 읽을 수 있다는 표시입니다.

LuaWrite
Lua에서 이 프로퍼티 값을 쓸 수 있다는 표시입니다.
LuaRead, LuaWrite 둘 다 없다면 Lua 바인딩 대상이 아닙니다.

NoEdit
    리플렉션/직렬화 대상에는 남기지만 `EPropertyFlags::Edit`는 붙이지 않습니다.
    즉 저장과 내부 리플렉션에는 사용하되, generic Details 패널 자동 렌더링에서는 제외합니다.

    예시:

    ```cpp
    UPROPERTY(NoEdit)
    TArray<FAnimGraphNodeDesc> Nodes;

    UPROPERTY(NoEdit)
    int32 RootNodeId = -1;
    ```

    전용 에디터 UI가 이미 있는 데이터에 사용합니다. 예를 들어 AnimGraph 노드는
    `EditorAnimGraphWidget`이 직접 `Name`, `Position`, `AnimationPath`, `PlayRate`,
    `bLoop` 등을 렌더링하므로, 같은 필드가 generic property widget에서 다시
    자동 렌더링되면 ImGui ID stack 안에 `"Name"` / `"Position"` 같은 visible label이
    중복되어 conflicting ID 오류가 날 수 있습니다.

Min, ClampMin, UIMin
Max, ClampMax, UIMax
Speed 또는 Step

ReferenceKind = RuntimeObject
ReferenceKind = ActorComponent
ReferenceKind = Asset
```

`ReferenceKind`는 **ObjectPtr 계열 참조를 어떤 방식으로 다룰지** 정하는 값입니다.

```txt
RuntimeObject
    일반 런타임 UObject 참조입니다.

ActorComponent
    Actor 내부 Component 참조입니다.

Asset
    ObjectPtr 계열이지만 에셋처럼 다룰 참조입니다.
```

`ReferenceKind`가 필요한 경우는 다음처럼 **SoftObjectPtr이 아닌데 에셋처럼 취급해야 하는 ObjectPtr 계열**입니다.

```cpp
UPROPERTY(ReferenceKind = Asset)
TObjectPtr<UMaterialInstance> MaterialInstanceAsset;

UPROPERTY(ReferenceKind = Asset)
UMaterialInterface* MaterialAsset;
```

자동 추론은 대략 다음 기준을 사용합니다.

```txt
TSoftObjectPtr<T>        -> Asset
UStaticMesh 등 에셋 타입 -> Asset
USceneComponent 계열     -> ActorComponent
그 외 UObject 계열       -> RuntimeObject
```

자동 추론이 원하는 의미와 다를 때만 직접 적습니다.

```cpp
UPROPERTY(ReferenceKind = RuntimeObject)
TObjectPtr<UMaterialInstance> RuntimeMaterialInstance;
```

---

### UENUM / UMETA

```cpp
UENUM()
enum class EMoveMode : uint8
{
    Walk UMETA(DisplayName = "Walk"),
    Run  UMETA(DisplayName = "Run"),
    None UMETA(Hidden),
};
```

지원 항목:

```txt
UENUM()
UMETA(DisplayName = "...")
UMETA(Display = "...")
UMETA(Hidden)
```

`Hidden`이 붙은 enum 값은 에디터 표시용 enum metadata에서 제외됩니다.

---

## 3. 지원 타입과 피해야 할 타입

주로 지원되는 타입:

```cpp
bool
int / int32
float
FVector
FVector4
FString
FName
FColor
FGuid
FQuat

UENUM enum
TObjectPtr<T>
TSoftObjectPtr<T>
TArray<T>
UObject 파생 타입 포인터
```

사용 예시:

```cpp
UPROPERTY()
FVector LocationOffset;

UPROPERTY()
TArray<FVector> ControlPoints;

UPROPERTY()
TObjectPtr<USceneComponent> TargetComponent;

UPROPERTY()
TSoftObjectPtr<UStaticMesh> StaticMeshAsset;
```

현재 피하는 것이 좋은 타입:

```txt
TMap, TSet
중첩 배열: TArray<TArray<T>>
복잡한 커스텀 struct 배열
함수 포인터
GPU 리소스 포인터: ID3D11ShaderResourceView*, ID3D11Buffer* 등
임시 editor-only preview/button/tag editor용 값
```

SRV, CubeSRV, 버튼, 태그 편집기, 일회성 preview 값은 `FProperty`가 아니라 별도 debug/editor UI에서 처리합니다.

---

## 4. ObjectPtr / SoftObjectPtr 사용 기준

간단히 구분하면 다음과 같습니다.

```txt
TObjectPtr<T>
    현재 씬이나 런타임에 존재하는 객체 참조

TSoftObjectPtr<T>
    에셋 path 참조
```

### TObjectPtr

런타임에 존재하는 객체나 Actor 내부 Component를 참조할 때 사용합니다.

```cpp
UPROPERTY(DisplayName = "Updated Component")
TObjectPtr<USceneComponent> UpdatedComponent;
```

사용할 때는 `Get()`으로 꺼내서 null 체크합니다.

```cpp
if (USceneComponent* Comp = UpdatedComponent.Get())
{
    Comp->SetRelativeLocation(FVector(0, 0, 0));
}
```

주의사항:

- `TObjectPtr<T>`나 raw `UObject*`를 에셋처럼 다뤄야 한다면 자동 추론이 불가능하므로 `ReferenceKind = Asset`을 기록해줘야 합니다.
- Duplicate 시 원본 Component가 아니라 복제본 Component를 가리키도록 remap되어야 합니다.
- raw `UObject*`도 리플렉션 생성기에서 `ObjectPtr`로 처리할 수 있지만, 가능하면 `TObjectPtr<T>`를 사용합니다.

---

### TSoftObjectPtr

에셋 경로를 저장할 때 사용합니다.

```cpp
UPROPERTY(DisplayName = "Static Mesh")
TSoftObjectPtr<UStaticMesh> StaticMeshAsset;
```

주의사항:

- 실제 객체를 항상 들고 있는 포인터가 아니라 path wrapper입니다.
- Actor나 Component 같은 런타임 객체 참조에는 사용하지 않습니다.
- 실제 로드는 `PostEditProperty()`, 초기화 함수, 컴포넌트 갱신 함수 등에서 처리합니다.
- 잘못된 path가 들어올 수 있으므로 null 체크가 필요합니다.

예시:

```cpp
void UStaticMeshComponent::PostEditProperty(const char* PropertyName)
{
    Super::PostEditProperty(PropertyName);

    if (strcmp(PropertyName, "StaticMeshAsset") == 0)
    {
        ReloadStaticMeshFromPath(StaticMeshAsset.GetPath());
    }
}
```

---

## 5. 새 컴포넌트 / 위젯 / 후처리 작업 시 주의사항

### 새 컴포넌트 작성

새 컴포넌트는 다음 형태를 기본으로 합니다.

```cpp
UCLASS()
class UMyNewComponent : public UActorComponent
{
    GENERATED_BODY(UMyNewComponent, UActorComponent)

public:
    UMyNewComponent() = default;

private:
    UPROPERTY(DisplayName = "Power", Category = "Custom")
    float Power = 1.0f;

    UPROPERTY(DisplayName = "Target")
    TObjectPtr<USceneComponent> Target;
};
```

간단 체크리스트:

```txt
[ ] UCLASS()를 붙였는가?
[ ] GENERATED_BODY(현재클래스, 부모클래스)가 맞는가?
[ ] 에디터/저장 대상 멤버에 UPROPERTY()를 붙였는가?
[ ] 저장하지 않을 값은 Transient를 적거나 UPROPERTY()를 붙이지 않았는가?
[ ] 런타임 객체 참조는 TObjectPtr을 사용했는가?
[ ] 에셋 참조는 TSoftObjectPtr을 사용했는가?
```

---

### 에디터 위젯 / Details Panel 작업

새 UI를 만들 때는 먼저 기존 `FProperty` 구조로 표현 가능한지 확인합니다.

```txt
일반 값 -> Bool, Int, Float, String, Vec3 등
런타임 객체 참조 -> ObjectPtr
에셋 경로 참조 -> SoftObjectPtr
배열 -> Array + InnerProperty
```

불필요한 특수 타입 분기를 늘리지 않습니다.

주의사항:

- 배열 UI는 `InnerProperty`를 재귀적으로 렌더링합니다.
- ObjectPtr UI는 `ObjectClass`에 맞는 객체만 선택하게 해야 합니다.
- SoftObjectPtr UI는 path 입력/변경 후 필요한 리소스 갱신을 호출해야 합니다.
- 값 변경 후 `PostEditProperty(PropertyName)` 흐름을 유지합니다.
- ImGui ID 충돌을 피하기 위해 배열 원소마다 고유 ID를 사용합니다.
- null 값, 빈 배열, 잘못된 path를 안전하게 처리합니다.

---

### Duplicate / PostEditProperty

Duplicate 시 `TObjectPtr`이 원본 객체를 계속 가리키면 안 됩니다.

```txt
Original Actor
    RootComponent A
    MovementComponent.UpdatedComponent -> A

Duplicated Actor
    RootComponent A'
    MovementComponent'.UpdatedComponent -> A'
```

주의사항:

- 복제 시 `FDuplicateContext`를 전달합니다.
- ObjectPtr은 context를 통해 복제본 객체로 remap합니다.
- SoftObjectPtr은 path만 복사하면 됩니다.
- 복제 후 원본 component를 가리키는 포인터가 남아 있지 않은지 확인합니다.

프로퍼티 변경 후 실제 런타임 상태나 리소스를 갱신해야 한다면 `PostEditProperty()`를 사용합니다.

주로 필요한 경우:

```txt
StaticMesh path 변경 후 mesh reload
SkeletalMesh path 변경 후 mesh reload
Animation asset 변경 후 animation 갱신
Material 변경 후 render resource 갱신
Transform 관련 값 변경 후 world matrix 갱신
```

---

## 6. 최종 요약

새 코드를 작성할 때는 아래 기준을 먼저 확인합니다.

```txt
새 클래스
    -> UCLASS + GENERATED_BODY

에디터/저장 대상 값
    -> UPROPERTY

런타임 객체 참조
    -> TObjectPtr

에셋 참조
    -> TSoftObjectPtr

배열
    -> TArray<T>

값 변경 후 후처리
    -> PostEditProperty

생성 코드
    -> 직접 수정 금지
```

가장 중요한 원칙:

> 새 특수 타입을 늘리기보다, `ObjectPtr`, `SoftObjectPtr`, `Array + InnerProperty`로 표현할 수 있는지 먼저 확인합니다.
