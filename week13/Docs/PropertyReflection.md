# Property Reflection 사용 가이드

이 문서는 팀원이 새 `UCLASS`, `USTRUCT`, `UENUM` 타입이나 `UPROPERTY` 필드를 추가할 때 필요한 작성 규칙을 먼저 설명하고, 뒤쪽에는 현재 리플렉션 시스템이 내부적으로 어떻게 동작하는지 정리한다.

## 빠른 체크리스트

1. 리플렉션 대상 헤더에 `Object/ObjectMacros.h` 또는 이를 포함하는 상위 헤더가 포함되어 있어야 한다.
2. 리플렉션 대상 타입이 있는 헤더에는 자기 자신의 generated header가 포함되어야 한다. 이 include는 pre-build의 `Scripts/GenerateHeaders.py`가 자동으로 삽입/수정한다.
3. `UCLASS()`, `USTRUCT()`, `UENUM()`을 타입 선언 바로 앞에 붙인다.
4. `UCLASS()`와 `USTRUCT()` 내부에는 `GENERATED_BODY()`를 넣는다.
5. 에디터나 저장 시스템에 노출할 필드에는 `UPROPERTY(...)`를 붙인다.
6. 빌드 전에 `Scripts/GenerateHeaders.py`가 실행되어 `Intermediate/Generated` 아래의 generated 파일을 갱신한다. 일반 빌드에서는 프로젝트 pre-build 단계가 이를 수행한다.

## 새 클래스 작성

클래스를 리플렉션에 등록하려면 `UCLASS()`와 `GENERATED_BODY()`가 필요하다. generated header include는 생성기가 자동으로 관리하므로, 새 파일을 처음 작성할 때 경로를 직접 찾지 않아도 된다.

```cpp
#pragma once

#include "Component/ActorComponent.h"

UCLASS()
class UMyComponent : public UActorComponent
{
public:
	GENERATED_BODY()

	UMyComponent() = default;

private:
	UPROPERTY(Edit, Save, Category="Movement", DisplayName="Speed", Min=0.0f, Max=100.0f, Speed=0.1f)
	float Speed = 10.0f;
};
```

주의할 점:

- generated header 경로는 기존 코드처럼 `Source/.../파일명.generated.h` 형태를 사용하지만, 보통은 생성기가 자동으로 넣어준다.
- 잘못된 generated include가 있거나 여러 개 있으면 생성기가 올바른 include 하나로 정리한다.
- `GENERATED_BODY()`가 있는 줄 번호가 generated macro 이름에 들어가므로, 헤더를 수정한 뒤에는 generated 파일을 다시 만들어야 한다.
- `UCLASS()` 타입은 현재 생성기 기준으로 부모 클래스가 있어야 `UClass` 등록 코드가 생성된다. 일반적인 엔진 클래스는 `UObject` 계열을 상속하면 된다.
- `UObject` 자체는 루트 타입이라 예외적으로 별도 처리된다.

## 새 필드 작성

`UPROPERTY(...)`는 노출하려는 필드 선언 바로 앞에 작성한다.

```cpp
UPROPERTY(Edit, Save, Category="Lighting", DisplayName="Intensity", Min=0.0f, Max=50.0f, Speed=0.05f)
float Intensity = 1.0f;
```

자주 쓰는 메타데이터:

- `Category="..."`: 에디터 Details 패널에서 묶을 카테고리 이름.
- `DisplayName="..."`: UI에 표시할 이름. 없으면 멤버 이름을 사용한다.
- `Min=...`, `Max=...`, `Speed=...`: 숫자/벡터/회전값 위젯의 범위와 드래그 속도.
- `Type=...`: C++ 타입만으로 부족할 때만 리플렉션 타입을 명시한다. 일반 필드는 최대한 C++ 타입과 다른 metadata로 추론되게 둔다.
- `Enum=...`: enum 드롭다운으로 보여줄 `UENUM` 이름.
- `Struct=...` 또는 `Type=Struct`: 구조체 프로퍼티로 처리할 때 사용한다.
- `Member=...`: 실제 C++ 멤버 하나 전체가 아니라 내부 멤버나 배열 원소를 노출할 때 사용한다.
- `AssetType="..."`: 에셋 선택 UI에서 사용할 에셋 종류. 예: `Material`, `StaticMesh`, `SkeletalMesh`, `Script`, `Font`, `Particle`, `Texture`.
- `AllowedClass="..."`: 오브젝트/에셋 참조가 허용할 클래스 이름. 보통은 C++ 타입이나 `AssetType`으로 추론되므로, 기본 추론을 덮어써야 할 때만 쓴다.

## Property Flag

현재 생성기가 인식하는 주요 플래그는 다음과 같다.

| UPROPERTY 토큰 | 생성되는 flag | 현재 의미 |
| --- | --- | --- |
| `Edit` | `PF_Edit` | 에디터 Details 패널에 표시된다. |
| `EditAnywhere` | `PF_Edit` | 현재는 `Edit`와 같은 의미다. |
| `VisibleAnywhere` | `PF_Edit \| PF_ReadOnly` | 에디터에 표시되지만 수정은 막는다. |
| `EditDefaultsOnly` | `PF_Edit` | 현재는 별도 defaults/instance 구분 없이 `Edit`와 같다. |
| `EditInstanceOnly` | `PF_Edit` | 현재는 별도 defaults/instance 구분 없이 `Edit`와 같다. |
| `Save` | `PF_Save` | scene save/load 대상이 된다. |
| `SaveGame` | `PF_Save` | 현재는 `Save`와 같은 의미다. |
| `ReadOnly` | `PF_ReadOnly` | Details UI에서 비활성화되어 수정할 수 없다. |
| `Transient` | `PF_Transient` | 생성 시 `PF_Save`를 제거한다. 현재 별도 런타임 필터는 제한적이므로 저장 제외 의도를 명확히 할 때만 사용한다. |

권장 사용:

- 에디터에서 수정하고 저장까지 해야 하면 `Edit, Save`.
- 에디터에서 보여주기만 하고 수정하면 안 되면 `VisibleAnywhere` 또는 `Edit, ReadOnly`.
- 저장만 필요하고 UI에는 보이지 않아도 되면 `Save`.
- `Transient`는 현재 시스템에서 의미가 아직 강하지 않다. 우선은 `Save`를 붙이지 않는 방식으로 저장 제외를 표현하는 것이 더 명확하다.

## 타입별 작성 예시

### 기본 타입

```cpp
UPROPERTY(Edit, Save, Category="Actor", DisplayName="Visible")
bool bVisible = true;

UPROPERTY(Edit, Save, Category="Movement", DisplayName="Max Speed", Min=0.0f, Max=200.0f, Speed=0.5f)
float MaxSpeed = 60.0f;

UPROPERTY(Edit, Save, Category="Actor", DisplayName="Name")
FName Name;

UPROPERTY(Edit, Save, Category="Text", DisplayName="Text")
FString Text;
```

생성기가 C++ 타입으로 기본 추론하는 타입:

- `bool` -> `FBoolProperty`
- `int`, `int32` -> `FIntProperty`
- `float` -> `FFloatProperty`
- `FString`, `std::string` -> `FStringProperty`
- `FName` -> `FNameProperty`
- `FVector` -> `FGenericProperty` + `EPropertyType::Vec3`
- `FVector4` -> `FGenericProperty` + `EPropertyType::Vec4`
- `FRotator` -> `FGenericProperty` + `EPropertyType::Rotator`

### Vec3, Vec4, Color4, Rotator

현재 `Vec3`, `Vec4`, `Color4`, `Rotator`는 별도 파생 property가 아니라 `FGenericProperty`가 처리한다. UI는 `EPropertyType`을 보고 적절한 위젯을 선택한다.

```cpp
UPROPERTY(Edit, Save, Category="Transform", DisplayName="Location", Min=0.0f, Max=0.0f, Speed=0.1f)
FVector Location;

UPROPERTY(Edit, Save, Category="Lighting", DisplayName="Color", Type=Color4)
FVector4 Color;

UPROPERTY(Edit, Save, Category="Movement", DisplayName="Rotation Rate", Min=0.0f, Max=0.0f, Speed=0.1f)
FRotator RotationRate;
```

`FVector`와 `FRotator`는 C++ 타입으로 추론되므로 `Type=Vec3`, `Type=Rotator`를 쓰지 않아도 된다. 반면 `Color4`는 C++ 타입이 `FVector4`라 `Vec4`와 구분할 수 없으므로 `Type=Color4`가 필요하다.

### 내부 멤버 노출

`Member=...`를 사용하면 실제 필드 전체가 아니라 내부 멤버를 별도 프로퍼티처럼 노출할 수 있다. 이 경우 선언부는 세미콜론만 있어도 되며, `Type`을 명시하는 것이 안전하다.

```cpp
UPROPERTY(Edit, Save, Category="Camera", DisplayName="FOV", Member=CameraState.FOV, Type=Float, Min=0.1f, Max=3.14f, Speed=0.01f);
UPROPERTY(Edit, Save, Category="Camera", DisplayName="Orthographic", Member=CameraState.bIsOrthogonal, Type=Bool);
FCameraState CameraState;
```

배열 원소도 같은 방식으로 노출할 수 있다.

```cpp
UPROPERTY(Edit, Save, Category="Collision", DisplayName="WorldStatic", Member=Responses[0], Enum=ECollisionResponse);
ECollisionResponse Responses[static_cast<int32>(ECollisionChannel::MAX)];
```

### Enum

드롭다운으로 보여줄 enum은 `UENUM()`으로 등록하고, 필드에는 `Enum=...`을 붙인다.

```cpp
UENUM()
enum class EMoveMode : uint8
{
	Walk,
	Run,
	Sprint,

	COUNT
};

UPROPERTY(Edit, Save, Category="Movement", DisplayName="Move Mode", Enum=EMoveMode)
EMoveMode MoveMode = EMoveMode::Walk;
```

생성기는 enum entry를 등록할 때 `COUNT`, `MAX`, `ActiveCount`, `Num`, `NUM`, `Count`를 sentinel로 보고 그 지점에서 노출을 끊는다. UI에 보여야 하는 값은 sentinel보다 위에 둔다.

### Struct

구조체 내부 필드를 Details 패널에서 펼쳐 보여주려면 구조체에 `USTRUCT()`와 `GENERATED_BODY()`를 붙이고 내부 필드도 `UPROPERTY`로 작성한다.

```cpp
USTRUCT()
struct FWeaponSettings
{
	GENERATED_BODY()

	UPROPERTY(Edit, Save, Category="Weapon", DisplayName="Damage", Min=0.0f, Max=1000.0f, Speed=1.0f)
	float Damage = 10.0f;
};

UCLASS()
class UWeaponComponent : public UActorComponent
{
public:
	GENERATED_BODY()

private:
	UPROPERTY(Edit, Save, Category="Weapon", DisplayName="Settings", Type=Struct)
	FWeaponSettings Settings;
};
```

`Type=Struct`를 사용하면 generated code가 해당 타입의 `StaticStruct()`를 `FStructProperty`에 넘긴다. 대상 구조체가 리플렉션 등록되지 않으면 컴파일 또는 런타임에서 구조체 자식 프로퍼티를 찾을 수 없다.

### Object Reference

실제 UObject 포인터를 직접 참조하려면 `ObjectRef`를 사용한다. `TObjectPtr<T>`는 자동으로 object ref로 추론되지만, UI 제한을 위해 `AllowedClass`를 명시하는 것이 좋다.

```cpp
UPROPERTY(Edit, Category="Mesh", DisplayName="Static Mesh")
TObjectPtr<UStaticMesh> StaticMesh;
```

이 값은 현재 메모리에 존재하는 객체를 가리킨다. 저장 가능한 에셋 경로는 보통 아래의 soft object reference를 따로 둔다.

### Soft Object Reference

에셋 경로나 지연 로드 대상은 `FSoftObjectPtr` 또는 일부 `FString` 필드와 `AssetType`/`AllowedClass` 메타데이터를 사용한다. 현재 soft object reference는 `FSoftObjectProperty`가 담당한다.

```cpp
UPROPERTY(Save, Category="Mesh", DisplayName="Static Mesh Path", AssetType="StaticMesh")
FSoftObjectPtr StaticMeshPath = "None";

UPROPERTY(Edit, Save, Category="Script", DisplayName="ScriptFile", AssetType="Script")
FString ScriptFile;
```

에셋 목록 배열은 `TArray<FSoftObjectPtr>`와 `AssetType`으로 추론된다.

```cpp
UPROPERTY(Edit, Save, Category="Materials", DisplayName="Materials", AssetType="Material")
TArray<FSoftObjectPtr> MaterialSlots;
```

## 자주 하는 실수

- generated header include는 생성기가 자동으로 고친다. 단, `--no-fix-generated-includes`를 쓰거나 생성기를 거치지 않으면 `GENERATED_BODY()`가 올바른 macro로 확장되지 않는다.
- `UPROPERTY`와 실제 필드 선언 사이에 다른 선언을 끼우면 생성기가 잘못 해석할 수 있다.
- `Member=...`를 쓰면서 `Type` 또는 `Enum`을 빼면 생성기가 실제 타입을 알 수 없어서 기본값으로 잘못 떨어질 수 있다.
- `Edit`만 붙이고 `Save`를 빼면 에디터에서 바꿀 수는 있지만 scene 저장 대상이 아니다.
- `Save`만 붙이면 저장/load 대상이지만 Details 패널에는 나오지 않는다.
- `ReadOnly`는 UI 수정을 막는 플래그이지 저장 제외 플래그가 아니다.
- `VisibleAnywhere`는 현재 `PF_Edit | PF_ReadOnly`로 처리된다.

# 구현 상세

## 전체 흐름

1. 헤더에 `UCLASS`, `USTRUCT`, `UENUM`, `UPROPERTY`, `GENERATED_BODY`를 작성한다.
2. `Scripts/GenerateHeaders.py`가 `Source/` 아래 헤더를 스캔하고 generated header include를 먼저 자동 정리한다.
3. 생성기는 `Intermediate/Generated/.../*.generated.h`와 `*.generated.cpp`를 만든다.
4. generated header는 `GENERATED_BODY()`가 확장될 macro를 정의한다.
5. generated cpp는 `UClass`, `UStruct`, `FEnum`, `FProperty` 정적 객체를 만들고 등록한다.
6. 런타임에서는 `UObject::GetClass()` 또는 `UStruct::GetPropertyRefs()`를 통해 property 목록을 가져온다.
7. 에디터 UI와 저장 시스템은 각 `FProperty`의 offset, type, flag, metadata를 이용해 실제 객체의 필드 값을 읽고 쓴다.

## GENERATED_BODY가 하는 일

`ObjectMacros.h`의 `GENERATED_BODY()`는 현재 파일 id와 줄 번호를 조합해 generated header에 있는 macro로 확장된다.

```cpp
#define GENERATED_BODY() \
    KR_EXPAND(KR_CONCAT4(CURRENT_FILE_ID, _, __LINE__, _GENERATED_BODY))
```

generated header는 이 macro 안에 타입별 boilerplate를 넣는다.

`UCLASS`의 generated body가 제공하는 것:

- `using Super = ...`
- 정적 `UClass StaticClassInstance`
- 정적 class registrar
- `StaticClass()`
- `GetClass()`
- `RegisterProperties(UStruct* Struct)`

`USTRUCT`의 generated body가 제공하는 것:

- 정적 `UStruct StaticStructInstance`
- 정적 struct registrar
- `StaticStruct()`
- `RegisterProperties(UStruct* Struct)`

즉 팀원이 직접 클래스 등록 코드나 property 등록 함수를 작성하지 않아도, generated cpp가 이 함수들의 정의와 정적 등록 객체를 만든다.

## FProperty가 실제 필드 위치를 찾는 방식

각 property는 클래스당 정적으로 하나만 존재한다. 대신 property 생성 시 `offsetof(OwnerType, Member)`가 저장된다.

generated cpp는 대략 이런 코드를 만든다.

```cpp
static const FFloatProperty GUMyComponent_Speed_0_Property(
	"Speed",
	"Movement",
	PF_Edit | PF_Save,
	offsetof(UMyComponent, Speed),
	sizeof(static_cast<UMyComponent*>(nullptr)->Speed),
	0.0f,
	100.0f,
	0.1f,
	"Speed",
	{{"category", "Movement"}, {"displayname", "Speed"}},
	"UMyComponent"
);
Struct->AddProperty(&GUMyComponent_Speed_0_Property);
```

런타임에서 값 주소가 필요하면 `FProperty::GetValuePtrFor(Container)`가 container 주소에 offset을 더한다.

```cpp
return Container ? reinterpret_cast<uint8*>(Container) + Offset : nullptr;
```

따라서 property 자체가 특정 객체를 들고 있지는 않다. property는 “이 클래스 레이아웃에서 필드가 몇 바이트 떨어져 있는지”만 알고 있고, 실제 객체 주소는 `FPropertyValue::ContainerPtr` 또는 호출자가 넘기는 container로 받는다.

이 전제가 맞으려면 해당 property는 반드시 그 property가 등록된 class/struct의 인스턴스에 적용되어야 한다. 일반적인 경로에서는 `Object->GetClass()->GetPropertyRefs()`로 property를 가져오기 때문에 이 조건이 자연스럽게 맞는다.

## Property 상속 구조와 책임

`FProperty`는 공통 metadata, flag, offset, size, owner name, serialize 인터페이스를 가진다. 실제 타입별 읽기/쓰기와 serialize 구현은 파생 property가 담당한다.

현재 주요 property:

- `FBoolProperty`: `bool`
- `FIntProperty`, `FFloatProperty`: 숫자 타입
- `FStringProperty`: 문자열
- `FNameProperty`: `FName`
- `FEnumProperty`: `UENUM` 기반 enum
- `FObjectProperty`: `UObject*`, `TObjectPtr<T>` 같은 강한 객체 참조
- `FSoftObjectProperty`: `FSoftObjectPtr` 또는 에셋 경로 문자열
- `FStructProperty`: `USTRUCT` 구조체
- `FArrayProperty`: 배열 컨테이너와 inner property
- `FGenericProperty`: 아직 별도 property 클래스로 분리하지 않은 `Vec3`, `Vec4`, `Color4`, `Rotator`, `SceneComponentRef`, `ByteBool` 등

`EPropertyType`은 여전히 UI dispatch와 일부 generic 처리에 필요하다. 다만 실제 저장/로드 권한은 가능한 한 타입별 `FProperty` 파생 클래스에 두는 방향이 맞다.

## Editor UI 흐름

에디터 Details 패널은 대략 다음 순서로 동작한다.

1. 선택된 객체에서 editable property 목록을 얻는다.
2. `PF_Edit`이 있는 property를 카테고리별로 보여준다.
3. `EditorPropertyWidget`이 `EPropertyType`을 switch해서 적절한 ImGui 위젯을 렌더링한다.
4. `PF_ReadOnly`가 있으면 위젯을 disabled 상태로 감싼다.
5. 값이 바뀌면 `FPropertyChangedEvent`를 만들고 `PostEditChangeProperty`/`PostEditProperty` 계열 후처리를 호출한다.

여기서 `ReadOnly`는 UI 입력을 막는 역할이다. 저장 여부는 `PF_Save`가 따로 결정한다.

## Save/Load 흐름

scene 저장 시스템은 property 목록 중 `PF_Save`가 있는 항목을 대상으로 serialize한다. 각 property는 virtual `SerializeValue`/`DeserializeValue`를 통해 자신의 타입에 맞게 JSON 또는 archive 값을 처리한다.

중요한 구분:

- `PF_Edit`: 에디터 표시 여부.
- `PF_ReadOnly`: 에디터 수정 가능 여부.
- `PF_Save`: scene 저장/load 대상 여부.
- `PF_Transient`: 생성 시 `PF_Save`를 제거하는 의도 표현. 현재는 `PF_Save`를 붙이지 않는 쪽이 더 직접적인 저장 제외 방법이다.

## Type과 Property Class 선택

생성기는 `UPROPERTY`의 C++ 타입과 metadata를 조합해 property class를 고른다.

우선순위는 대략 다음과 같다.

1. `Type=...`이 있으면 명시 타입을 우선한다. 단, C++ 타입으로 추론 가능한 경우에는 쓰지 않는다.
2. `Enum=...`이 있으면 `FEnumProperty`.
3. `Struct=...` 또는 `Type=Struct`이면 `FStructProperty`.
4. `TObjectPtr<T>` 또는 raw pointer는 `FObjectProperty`.
5. `FSoftObjectPtr`, 또는 `FString` + `AssetType`/`AllowedClass`는 `FSoftObjectProperty`.
6. `TArray<FSoftObjectPtr>` 또는 `TArray<FString>` + `AssetType`/`AllowedClass`는 `FArrayProperty` + inner `FSoftObjectProperty`.
7. 기본 C++ 타입은 전용 property로 매핑된다.
8. 나머지 리플렉션 타입은 `FGenericProperty`.

soft object reference는 현재 `FGenericProperty`가 아니라 `FSoftObjectProperty`가 맡는다. 반대로 `Vec3`, `Vec4`, `Color4`, `Rotator`는 아직 `FGenericProperty`가 담당한다.

`AssetType`이 다음 값이면 `AllowedClass`는 생성기가 자동으로 채운다.

- `StaticMesh` -> `UStaticMesh`
- `SkeletalMesh` -> `USkeletalMesh`
- `Material` -> `UMaterial`
- `Texture` -> `UTexture2D`

따라서 일반적인 에셋 참조는 `AssetType`만 쓰고, 같은 에셋 UI를 쓰되 허용 클래스를 다르게 제한해야 할 때만 `AllowedClass`를 추가한다.

## Generated 파일의 역할

`Intermediate/Generated/Reflection.generated.cpp`는 각 타입별 generated cpp를 include하는 허브 파일이다. 실제 타입별 등록 코드는 source header 경로를 따라 생성된다.

예:

- `Source/Engine/Component/StaticMeshComponent.h`
- `Intermediate/Generated/Source/Engine/Component/StaticMeshComponent.generated.h`
- `Intermediate/Generated/Source/Engine/Component/UStaticMeshComponent.generated.cpp`

generated 파일은 수동으로 수정하지 않는다. 수정해야 할 내용은 원본 header의 macro와 metadata에 작성하고, 생성기를 다시 실행한다.

생성기는 기본적으로 원본 header도 아주 제한적으로 수정한다. `UCLASS`/`USTRUCT`가 있는 헤더에서 `*.generated.h` include가 없거나 틀리면 올바른 source-relative include로 고친다. 이 동작을 끄고 싶으면 `--no-fix-generated-includes`를 사용한다.

## 새 Property 타입을 추가할 때

새 파생 property가 필요하면 보통 다음 위치를 함께 확인해야 한다.

1. `Source/Engine/Core/PropertyTypes.h`: `EPropertyType`, forward declaration, include 연결.
2. `Source/Engine/Core/Property/...`: 새 property class 구현.
3. `Scripts/GenerateHeaders.py`: C++ 타입 또는 `Type=...`이 새 property class로 생성되도록 매핑.
4. `Source/Editor/UI/EditorPropertyWidget.cpp`: Details 패널에서 새 타입 위젯 렌더링.
5. scene save/load가 필요한 경우 해당 property의 `SerializeValue`/`DeserializeValue`.

다만 모든 타입을 무조건 파생 property로 만들 필요는 없다. `Vec3`, `Vec4`, `Color4`, `Rotator`처럼 단순 값 타입이고 serialize/UI 차이만 작으면 `FGenericProperty` + `EPropertyType`으로 두는 편이 현재 구조에서는 더 단순하다.
