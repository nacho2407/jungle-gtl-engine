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
- `GENERATED_BODY(현재클래스, 부모클래스)`의 이름이 실제와 일치해야 합니다.
- `.gen.cpp`는 직접 수정하지 않습니다.

---

## 2. UCLASS / UPROPERTY / UENUM / UMETA

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

DisplayName 또는 Display / Category
    에디터에서 표시할 클래스/카테고리 이름입니다.
```

위젯, 에디터에 노출하지 않을 경우 `UCLASS()`만 사용하면 충분합니다.

---

### UPROPERTY

사용 예시:

```cpp
UPROPERTY(DisplayName = "Health", Category = "Stats", Min = 0.0, Max = 100.0, Speed = 0.1)
float Health = 100.0f;

UPROPERTY(Transient)
TObjectPtr<USceneComponent> CachedComponent;
```

지원 항목:

```txt
DisplayName 또는 Display / Category
    에디터에서 표시할 클래스/카테고리 이름입니다.

Transient
    리플렉션에는 잡히지만 저장/복제 대상에서 빠지는 런타임 값입니다. 
    현재 SerializeItem, 일반 CopyValue에서 스킵됩니다.

Animatable
    시퀀서/애니메이션 쪽에서 읽고 쓸 수 있는 프로퍼티라는 표시입니다.

LuaRead / LuaWrite
    Lua에서 이 프로퍼티 값을 읽고 쓸 수 있다는 표시입니다.

NoEdit
    에디터에 노출되지 않습니다. 저장/리플렉션에는 사용됩니다.

Min, Max, Speed
    자료형의 최소값, 최대값, 변경 단위를 결정합니다.
    
ReferenceKind = (RuntimeObject / ActorComponent / Asset) 
    참조하는 UObject가 런타임 오브젝트인지, 컴포넌트인지, 에셋인지 명시합니다.
```

---

### UENUM / UMETA

단순 `Enum` 값이 에디터에 어떻게 표시될지 `UMETA()`로 정할 수 있습니다.

```cpp
UENUM()
enum class EMoveMode : uint8
{
    WalkSlowly UMETA(DisplayName = "Walk Slowly"),
    WalkFast UMETA(DisplayName = "Walk Fast"),
    None UMETA(Hidden),
};
```

지원 항목:

```txt
DisplayName 또는 Display
에디터에 정의한 이름으로 표시됩니다.

Hidden
에디터에 Enum 값이 노출되지 않습니다.
```

---

### UFUNCTION

Lua나 런타임 시스템에서 C++ 멤버 함수를 이름으로 호출해야 한다면 `UFUNCTION()`을 붙입니다.

```cpp
UFUNCTION(LuaCallable, BlueprintPure, Category = "Actor")
FVector GetActorLocation() const;

UFUNCTION(LuaCallable, BlueprintCallable, Category = "Actor")
void SetActorLocation(const FVector& Location);
```

지원 항목:

```txt
LuaCallable
    Lua에서 같은 이름의 멤버 함수로 호출할 수 있게 합니다.
    예: Actor:GetActorLocation(), Actor:SetActorLocation(Vector(0, 0, 100))

BlueprintCallable
    상태를 바꿀 수 있는 일반 호출 함수라는 표시입니다.
    지금은 메타데이터 플래그로 저장되며, 이후 툴/그래프 노출에 사용할 수 있습니다.

BlueprintPure
    상태를 바꾸지 않는 조회 함수라는 표시입니다.
    const 함수와 함께 사용하는 것을 권장합니다.

DisplayName 또는 Display / Category
    함수 목록이나 툴에서 표시할 이름과 카테고리입니다.
```

`LuaCallable`을 붙인 함수는 Lua에서 일반 멤버 함수처럼 호출할 수 있습니다.

```lua
local Location = Actor:GetActorLocation()
Actor:SetActorLocation(Vector(0, 0, 100))
```

함수 이름을 문자열로 다뤄야 할 때는 모든 UObject에서 공통으로 제공되는 `Call()`을 사용할 수 있습니다.

```lua
Actor:Call("SetActorLocation", Vector(0, 0, 100))
```

주의사항:

- UObject의 `public` 멤버 함수에만 사용합니다. 그 외엔 수동으로 바인딩합니다.
- `static` 함수, inline body가 있는 함수, 기본 인자, 오버로드는 현재 지원하지 않습니다.
- 함수 포인터, `TMap`, `TSet` 타입은 현재 지원하지 않습니다. 

---

## 3. 현재 지원되는 타입

현재 리플렉션 프로퍼티와 `UFUNCTION()` 파라미터/반환값에서 지원되는 타입:

```cpp
bool / int / int32 / float / FString / FName
UENUM()을 적은 모든 열거형
USTRUCT() + GENERATED_STRUCT_BODY()를 적은 모든 구조체

TObjectPtr<T>
TSoftObjectPtr<T>
TArray<T>
```

SRV, CubeSRV, 버튼, 태그 편집기, 일회성 preview 값은 `FProperty`가 아니라 별도 debug/editor UI에서 처리합니다.

---

## 4. ObjectPtr / SoftObjectPtr

`ObjectPtr`는 UObject를 상속받는 객체들에 대한 포인터로 사용됩니다.

`TObjectPtr<T>`는 포인터를 감싸고, `TSoftObjectPtr<T>`는 에셋의 경로를 감쌉니다.

지금까지 쓰던 `USceneComponent*` 대신 `TObjectPtr<USceneComponent*>`를 사용하고, `USkeletalMesh*` 대신 `TSoftObjectPtr<USkeletalMesh>`를 사용한다고 보시면 됩니다.  

---

### raw pointer를 잘 써왔는데, 왜 사용해야 하나요?

`UObject`가 어떻게 참조되는지 `리플렉션 시스템`, `가비지 컬렉터`에게 알려주기 위해서입니다. `raw pointer`는 의미가 없는 주소이므로 어떤 식으로 사용되는지 구분할 수가 없습니다.

예로, `USceneComponent*` `UpdatedComponent`는 런타임 객체를 참조하고, `UStaticMesh*` `UStaticMeshPath`는 에셋을 참조합니다. 생 포인터로는 구분하지 못합니다.

```
이 포인터를 직접 저장해야 하나?
경로로 저장해야 하나?
Duplicate할 때 remap을 해 줘야 하나?
에디터에서 asset picker를 띄워 줘야 하나?
component picker를 띄워 줘야 하나?
GC 대상인가, 아니면 그냥 cache인가?
```

구분할 정보가 없습니다. `TObjectPtr<T>`, `TSoftObjectPtr<T>`가 이 정보를 제공합니다.

---

### 그러면 raw pointer는 이제 쓰면 안 되나요?

`소유하지 않고, 저장하지 않고, 리플렉션으로 관리할 필요도 없다면 raw pointer를 사용합니다.`

사실 이렇게 말하면 너무 어렵습니다. 아래의 세 가지 질문을 던져 봅시다.

```
이 포인터를 저장을 해 줘야 할까?
이 포인터를 복제할 때 다시 맵핑을 해 줘야 할까?
이 포인터를 에디터에 노출해 줘야 할까?
```

세 가지 질문을 통과했다면 `TObjectPtr<T>`, `TSoftObjectPtr<T>`를 사용합시다.

---

### TObjectPtr

Component 등 런타임에 존재하는 객체를 참조할 때 사용합니다. 

`Get()`, `Set()`을 사용해 평소처럼 사용할 수 있습니다.  

```cpp
UPROPERTY(DisplayName = "Updated Component")
TObjectPtr<USceneComponent> UpdatedComponent;
```

주의사항:

- `Asset`의 포인터는 `SoftObjectPtr<T>`를 사용해서 경로로 참조하는 것을 권장합니다.
- `UObjectPtr<T>`인데 Asset을 참조할 경우 `ReferenceKind`를 명시합니다.
- `TArray<USkeletalMesh*>` 등 raw pointer 배열일 경우에도 `ReferenceKind`를 명시합니다.

예시:

```cpp
UPROPERTY(ReferenceKind = Asset)
TObjectPtr<UAnimInstance> AnimInstance = nullptr

UPROPERTY(DisplayName = "Materials", ReferenceType = Asset)
TArray<UMaterialInterface*> Materials;
```

---

### TSoftObjectPtr

에셋 경로를 저장할 때 사용합니다.

```cpp
UPROPERTY(DisplayName = "Static Mesh")
TSoftObjectPtr<UStaticMesh> StaticMeshAsset;
```

주의사항:

- `Actor`, `Component` 같은 런타임 객체 참조에는 사용하지 않습니다.
- 실제 로드는 `PostEditProperty()`, 초기화 함수, 컴포넌트 갱신 함수 등에서 처리합니다.
- 잘못된 path가 들어올 것을 대비해 null 체크가 필요합니다.

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
[ ] 최소값, 최대값, 변경 정도를 Min, Max, Speed로 조절했는가? 
[ ] 저장하지 않을 값은 Transient를 적거나 UPROPERTY()를 붙이지 않았는가?
[ ] 런타임 객체 참조는 TObjectPtr을 사용했는가?
[ ] 에셋 참조는 TSoftObjectPtr을 사용했는가?
[ ] Lua에서 호출할 멤버 함수에는 UFUNCTION(LuaCallable)을 붙였는가?
```

---

### 에디터 위젯 / Details Panel 작업

위의 작업을 잘 했다면 알아서 처리됩니다. 

에디터에 특별한 UI로 보여주고 싶을 때에만 위젯을 직접 수정합니다. 

---

### Duplicate / PostEditProperty

프로퍼티 변경 후 런타임 상태, 리소스를 갱신해야 한다면 `PostEditProperty()`를 사용합니다.

다음과 같은 상황에서 사용됩니다.

```txt
StaticMesh path 변경 후 mesh reload
SkeletalMesh path 변경 후 mesh reload
Animation asset 변경 후 animation 갱신
Material 변경 후 render resource 갱신
Transform 관련 값 변경 후 world matrix 갱신
```
