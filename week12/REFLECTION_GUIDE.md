# JSEngine Reflection 사용 가이드

## 1. 개요

현재 리플렉션 시스템은 `UClass`와 `FProperty`를 중심으로 동작한다.

기존의 `DECLARE_CLASS`, `DEFINE_CLASS`, `REGISTER_FACTORY`, `FObjectFactory`, `FTypeInfo`, `FPropertyDescriptor` 방식은 제거하고, 클래스 선언에는 `UCLASS()`와 `GENERATED_BODY(ClassName, ParentClass)`를 사용한다.

리플렉션 생성기는 헤더 파일을 스캔해서 `Intermediate/Reflection/*.gen.cpp` 파일을 생성하며, 생성된 코드는 각 클래스의 `StaticClass()`, `UClass` 등록, `FProperty` 등록을 담당한다.

---

## 2. 클래스 리플렉션

리플렉션 대상 클래스는 `UCLASS()`와 `GENERATED_BODY()`를 붙인다.

```cpp
UCLASS()
class AMyActor : public AActor
{
public:
    GENERATED_BODY(AMyActor, AActor)
};
```

`GENERATED_BODY(ClassName, ParentClass)`의 인자는 실제 클래스 선언과 일치해야 한다.

```cpp
class UMyComponent : public UActorComponent
{
public:
    GENERATED_BODY(UMyComponent, UActorComponent)
};
```

---

## 3. 추상 클래스

직접 생성되면 안 되는 클래스는 `UCLASS(Abstract)`를 사용한다.

```cpp
UCLASS(Abstract)
class UPrimitiveComponent : public USceneComponent
{
public:
    GENERATED_BODY(UPrimitiveComponent, USceneComponent)
};
```

`Abstract`가 붙은 클래스는 `CF_Abstract` 플래그를 가지며, `NewObject(UClass*)`, Spawn Actor, Add Component 후보에서 제외된다.

---

## 4. 프로퍼티 리플렉션

에디터에 노출하거나 직렬화 대상이 되는 멤버에는 `UPROPERTY()`를 붙인다.

```cpp
UPROPERTY(DisplayName = "Move Speed", Category = "Movement", Min = "0", Max = "1000", Speed = "1")
float MoveSpeed = 300.0f;
```

기본적으로 `UPROPERTY()`는 `Read`, `Write`, `Edit` 가능한 `FProperty`로 등록된다.

자주 쓰는 메타데이터:

```cpp
UPROPERTY(DisplayName = "Loop")
bool bLooping = true;

UPROPERTY(Category = "Transform")
FVector Offset;

UPROPERTY(Transient)
float RuntimeOnlyValue;

UPROPERTY(SaveGame)
int32 SavedValue;

UPROPERTY(Animatable)
float Alpha;
```

---

## 5. 현재 지원되는 주요 프로퍼티 타입

현재 생성기가 주로 지원하는 타입은 다음과 같다.

```text
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
TArray<FVector>
TArray<FString>
USceneComponent*
TArray<UMaterialInterface*>
UENUM enum
```

일반적인 `UObject*`, `UAnimationAsset*`, `UMaterial*`, `UClass*`, `TSubclassOf<T>`, 임의 `TArray<T>`, `TMap<K,V>`는 아직 일반화된 리플렉션 타입으로 완전히 지원되지 않는다.

에셋은 현재 `FString AssetPath` 형태로 노출하고, 실제 런타임 포인터는 별도로 로드하는 방식이 많다.

---

## 6. Enum 리플렉션

enum을 리플렉션에 등록하려면 `UENUM()`을 붙인다.

```cpp
UENUM()
enum class EAnimationMode
{
    AnimationBlueprint UMETA(DisplayName = "Animation Blueprint"),
    AnimationSingleNode UMETA(DisplayName = "Single Node")
};
```

`UMETA(DisplayName = "...")`는 에디터 표시 이름으로 사용된다.

숨기고 싶은 값은 `UMETA(Hidden)`을 사용할 수 있다.

```cpp
Count UMETA(Hidden)
```

---

## 7. FPropertyHandle

에디터 Details 패널은 `FProperty`를 직접 다루기보다 `FPropertyHandle`을 통해 인스턴스와 프로퍼티를 함께 다룬다.

```cpp
FPropertyHandle Handle;
Handle.Owner = Object;
Handle.Property = &Property;
```

`FPropertyHandle`은 다음 역할을 한다.

```text
- 어떤 UObject 인스턴스의 프로퍼티인지 보관
- 값 포인터 획득
- 값 읽기/쓰기
- 에디터 위젯 렌더링의 공통 입력
```

예시:

```cpp
if (float* Value = Handle.GetValuePtr<float>())
{
    ImGui::DragFloat(Handle.GetName(), Value);
}
```

---

## 8. 객체 생성

객체 생성은 `UClass`를 통해 수행한다.

```cpp
UClass* Class = FReflectionRegistry::Get().FindClass("AMyActor");
UObject* Object = NewObject(Class);
```

템플릿 생성도 가능하다.

```cpp
AMyActor* Actor = NewObject<AMyActor>();
```

`NewObject(UClass*)`는 내부적으로 `UClass::CreateObject()`를 호출한다.

---

## 9. GenerateReflection.py 사용법

빌드를 수행하면 자동적으로 프로젝트 루트에서 아래의 코드를 실행하며 리플렉션 코드가 생성된다.

```bash
python Scripts/GenerateReflection.py
```

생성 결과는 다음 위치에 만들어진다.

```text
JSEngine/Intermediate/Reflection/*.gen.cpp
```

생성기는 `JSEngine/Source/**/*.h`를 스캔하며, `Intermediate` 폴더는 제외한다.

생성되는 코드는 대략 다음을 담당한다.

```text
- ClassName::StaticClass() 구현
- UClass 생성
- 부모 클래스 연결
- class flag 등록
- create function 등록
- UPROPERTY 기반 FProperty 등록
- UENUM 기반 UEnum 등록
```

---

## 10. 주의사항

### GENERATED_BODY 인자 일치

아래처럼 실제 부모 클래스와 매크로 인자가 다르면 안 된다.

```cpp
class UMyComponent : public USceneComponent
{
public:
    // 잘못된 예
    GENERATED_BODY(UMyComponent, UObject)
};
```

### 추상 클래스에는 Abstract를 붙이기

pure virtual 함수가 있거나 직접 생성되면 안 되는 클래스는 반드시 `UCLASS(Abstract)`를 붙인다.

```cpp
UCLASS(Abstract)
class UMovementComponent : public UActorComponent
{
public:
    GENERATED_BODY(UMovementComponent, UActorComponent)
};
```

### 에셋 포인터는 아직 주의

현재는 `UAnimationAsset*`, `UMaterial*` 같은 에셋 포인터를 일반 `UPROPERTY()`로 완전히 지원하지 않는다.

대부분 다음처럼 경로 문자열을 리플렉션에 노출하고, 런타임 포인터는 로드해서 사용한다.

```cpp
UPROPERTY(DisplayName = "Animation")
FString AnimationAssetPath;

UAnimationAsset* AnimationToPlay = nullptr;
```

---

## 11. 기본 작성 예시

```cpp
#pragma once

#include "Object/Object.h"
#include "Core/Reflection/ReflectionMacros.h"

UENUM()
enum class EMoveType
{
    Walk UMETA(DisplayName = "Walk"),
    Run UMETA(DisplayName = "Run")
};

UCLASS()
class UMyMovementComponent : public UActorComponent
{
public:
    GENERATED_BODY(UMyMovementComponent, UActorComponent)

    UPROPERTY(DisplayName = "Speed", Category = "Movement", Min = "0", Max = "1000", Speed = "1")
    float Speed = 300.0f;

    UPROPERTY(DisplayName = "Move Type")
    EMoveType MoveType = EMoveType::Walk;
};
```

---

## 12. 요약

```text
UCLASS()                         클래스 리플렉션 대상
UCLASS(Abstract)                 추상 클래스
GENERATED_BODY(Class, Parent)    StaticClass/GetClass 선언
UPROPERTY(...)                   FProperty 등록 대상
UENUM()                          enum 리플렉션 대상
UMETA(DisplayName="...")         enum 값 표시 이름
GenerateReflection.py            .gen.cpp 생성
FPropertyHandle                  인스턴스 바인딩된 프로퍼티 접근
NewObject(UClass*)               UClass 기반 객체 생성
```
