# FBX Import, Skinning, Skeleton Root 처리 정리

작성일: 2026-05-19

이 문서는 FBX skeletal mesh / animation import를 수정하면서 확인한 문제, 원인, 해결 방식, 그리고 남은 주의점을 정리한 문서입니다. 핵심 주제는 다음 네 가지입니다.

- non-skinned mesh를 skeletal mesh 안에 rigid하게 붙이는 문제
- CPU skinning과 GPU skinning 결과가 달라지는 문제
- FBX null / wrapper node를 skeleton hierarchy에 포함할지 여부
- null / wrapper node가 가진 이동 animation을 실제 root bone에 어떻게 넘길지

## 최종 결론

현재 방향은 `null/wrapper node를 skeleton bone으로 넣지 않는다`입니다.

대신 실제 skeleton node만 `FBone` / `FReferenceSkeleton`에 넣고, 실제 skeleton root 위에 non-skeleton wrapper가 있을 때만 wrapper의 bind transform과 animation transform을 첫 실제 root bone에 bake합니다.

즉 구조는 다음과 같습니다.

```text
FBX scene root
└─ NullWrapper    (skeleton bone으로 만들지 않음)
   └─ Pelvis      (첫 실제 skeleton root bone)
      └─ Spine
```

엔진 skeleton에는 이렇게 들어갑니다.

```text
Pelvis (ParentIndex = -1)
└─ Spine
```

단, `NullWrapper`가 가진 bind transform과 animation 이동량은 `Pelvis`의 local bind pose / animation track으로 흡수합니다.

이렇게 해야 하는 이유는 두 가지입니다.

1. `NullWrapper`를 skeleton bone으로 넣으면 기존 skeleton asset과 parent hierarchy가 달라져 animation remap에서 `ParentBone mismatch`가 발생하기 쉽습니다.
2. 그렇다고 `NullWrapper`를 그냥 버리면 wrapper가 들고 있던 translation / rotation / scale이 사라져 mesh가 pelvis 기준으로 고정되거나 face mesh가 떨어지는 문제가 생깁니다.

## 1. Non-skinned mesh가 떨어지는 문제

### 현상

일부 FBX는 skeletal mesh 안에 여러 mesh node가 들어 있지만, 모든 mesh가 skin deformer를 가지고 있지는 않았습니다.

예를 들어 face mesh 또는 accessory mesh가 skin cluster 없이 특정 skeleton node 아래에 붙어 있는 구조일 수 있습니다.

이 경우 importer가 skin weight를 찾지 못하면 해당 mesh는 bone animation을 따라가지 못하고 bind 위치에 남거나, 얼굴이 몸에서 떨어져 보일 수 있습니다.

### 원인

기존에는 skin이 없는 mesh에 대해 "가장 가까운 bone"을 단순히 찾거나, ancestor 기준으로만 처리하는 흐름이 있었습니다.

하지만 실제 FBX 구조에서는 mesh node가 다음처럼 있을 수 있습니다.

```text
FaceMeshNode
└─ FaceRootBone
```

또는 다음처럼 mesh node가 bone 아래가 아니라 wrapper 아래에 있고, 실제 bone은 descendant 쪽에 존재할 수 있습니다.

```text
WrapperOrMeshNode
└─ ActualBone
```

ancestor만 찾으면 실제로 붙어야 하는 bone을 놓칠 수 있습니다.

### 해결 방식

[FbxSkinWeightImporter.cpp](../KraftonEngine/Source/Engine/Mesh/Fbx/FbxSkinWeightImporter.cpp)에서 skin이 없는 mesh에 대해 rigid attachment bone을 다음 순서로 찾도록 했습니다.

1. mesh node 자체가 bone으로 매핑되어 있으면 그 bone 사용
2. descendant 중 첫 번째 실제 bone을 재귀적으로 찾음
3. 그래도 없으면 ancestor 쪽의 bone을 찾음

관련 함수:

- `FindMappedBoneIndex`
- `FindFirstDescendantBoneIndex`
- `FindFirstAncestorBoneIndex`
- `ResolveRigidAttachmentBoneIndex`

skin이 없는 mesh는 최종적으로 다음처럼 처리합니다.

```cpp
Weights.clear();
Weights.push_back({ RigidBoneIndex, 1.0f });
```

즉 해당 mesh의 모든 vertex에 rigid weight 1.0을 줍니다.

### 추가 보정

rigid mesh도 runtime에서는 일반 skinned vertex와 같은 skin matrix 경로를 탑니다. 그래서 bind pose에서 원래 FBX mesh 위치가 유지되도록 `BuildRigidBindCorrection()`을 적용합니다.

이 보정은 "weight 1.0으로 특정 bone에 붙이되, bind pose 결과가 FBX mesh transform과 맞도록 vertex를 pre-skinned space로 변환"하는 역할입니다.

## 2. CPU skinning과 GPU skinning 결과가 달라지는 문제

### 현상

CPU skinning과 GPU skinning mode를 바꿨을 때 mesh 위치가 다르게 보였습니다. 특히 face mesh가 따로 떨어지는 문제가 있었습니다.

### 원인

문제의 핵심은 `MeshRange`가 별도의 `GlobalMeshMatrix` 또는 `MeshBindGlobal` 같은 mesh 단위 transform을 들고 있고, CPU/GPU skinning 경로가 이 값을 다르게 적용하거나 중복 적용할 가능성이 있었다는 점입니다.

skinning은 원칙적으로 다음처럼 한 가지 기준으로 통일되어야 합니다.

```text
vertex position in bind/pre-skinned space
→ bone skin matrix
→ component/world transform
```

그런데 mesh range별 global matrix가 별도로 남아 있으면 한 경로에서는 vertex에 bake하고, 다른 경로에서는 draw 또는 CPU skinning 시점에 다시 적용하는 식의 차이가 생길 수 있습니다.

### 해결 방식

새 import에서는 mesh bind transform을 vertex import 시점에 bake하고, `FSkeletalMeshRange::MeshBindGlobal`은 legacy serialization slot으로만 남깁니다.

현재 구조:

- `FSkeletalMeshRange::MeshBindGlobal` 필드는 남아 있음
- 새 import에서는 `MeshRange.MeshBindGlobal = FMatrix::Identity`
- 실제 mesh bind transform은 vertex position / normal / tangent 생성 시점에 반영

관련 위치:

- [SkeletalMeshAsset.h](../KraftonEngine/Source/Engine/Mesh/SkeletalMeshAsset.h)
- [FbxSkinWeightImporter.cpp](../KraftonEngine/Source/Engine/Mesh/Fbx/FbxSkinWeightImporter.cpp)

주의할 점:

- 이 변경은 새로 import되는 asset에만 적용됩니다.
- 기존 `.uasset`에 저장된 legacy mesh range transform은 자동으로 바뀌지 않습니다.
- 기존 asset에서 같은 문제가 계속 보이면 reimport가 필요합니다.

## 3. Negative determinant로 front culling이 뒤집히는 문제

### 현상

일부 FBX를 import하면 front culling이 일어난 것처럼 앞면/뒷면이 뒤집혀 보였습니다.

### 원인

FBX node transform 또는 bind matrix에 음수 determinant가 포함되면 좌표계 handedness가 뒤집힙니다. 이 상태에서 index winding을 그대로 쓰면 triangle front face가 반대로 해석됩니다.

대표적인 원인:

- negative scale
- axis conversion 후 basis determinant가 음수
- mesh bind matrix에 mirror transform 포함

### 해결 방식

`FFbxTransformUtils::HasNegativeBasisDeterminant()`로 3x3 basis determinant를 검사하고, 음수일 때 index winding을 뒤집습니다.

적용된 경로:

- static FBX import: [FbxStaticMeshImporter.cpp](../KraftonEngine/Source/Engine/Mesh/Fbx/FbxStaticMeshImporter.cpp)
- skeletal FBX import: [FbxSkinWeightImporter.cpp](../KraftonEngine/Source/Engine/Mesh/Fbx/FbxSkinWeightImporter.cpp)

skeletal 쪽은 `MeshBindGlobal` 기준 determinant를 확인합니다.

```cpp
const bool bReverseWinding =
    FFbxTransformUtils::HasNegativeBasisDeterminant(MeshBindGlobal);
```

static 쪽은 `MeshToWorld` 기준 determinant를 확인합니다.

```cpp
const bool bReverseWinding =
    FFbxTransformUtils::HasNegativeBasisDeterminant(MeshToWorld);
```

주의할 점:

- 이 역시 import 시점 처리입니다.
- 기존 `.uasset`의 index buffer는 자동 수정되지 않으므로, 기존 asset은 reimport가 필요합니다.

## 4. Synthetic root wrapper를 skeleton bone으로 넣던 문제

### 현상

Unreal에서 호환되는 skeleton/animation이라고 생각한 FBX인데도 animation import 시 `ParentBone mismatch`, `Target skeleton is missing source bone` 같은 문제가 발생했습니다.

특히 `SKM_Manny_Simple_Skeleton` 계열에서 skeleton 호환성이 깨지는 원인이 되었습니다.

### 원인

기존 importer는 실제 skeleton node 위에 non-skeleton wrapper가 있을 때, 그 wrapper를 synthetic root bone으로 추가했습니다.

예:

```text
FBX:
RootWrapper   (non-skeleton)
└─ pelvis     (skeleton)
   └─ spine_01
```

기존 import 결과:

```text
RootWrapper   (synthetic bone)
└─ pelvis
   └─ spine_01
```

하지만 target skeleton이 Unreal 기준으로 다음과 같다면:

```text
pelvis
└─ spine_01
```

source animation skeleton은 `RootWrapper -> pelvis`, target skeleton은 `pelvis`가 root입니다. bone 이름뿐 아니라 parent relationship이 달라지므로 skeleton compatibility check에서 실패합니다.

### 해결 방향

synthetic root wrapper를 skeleton bone으로 넣지 않도록 했습니다.

현재 skeleton import 원칙:

- `FFbxSceneQuery::IsSkeletonNode(Node)`가 true인 실제 skeleton node만 bone으로 추가
- non-skeleton wrapper는 `FBone` / `FReferenceSkeleton`에 넣지 않음
- 첫 실제 root bone의 `ParentIndex`는 `-1`

관련 위치:

- [FbxSkeletonImporter.cpp](../KraftonEngine/Source/Engine/Mesh/Fbx/FbxSkeletonImporter.cpp)

## 5. 처음 시도한 root wrapper 제거 방식이 크게 망가졌던 이유

### 잘못된 시도

처음에는 non-skeleton wrapper를 제외하면서 모든 bone의 local bind pose를 "가장 가까운 skeleton parent 기준"으로 다시 계산했습니다.

개념상으로는 다음을 시도한 것입니다.

```text
ChildLocal = ChildGlobal relative to nearest skeleton parent global
```

### 왜 문제가 되었는가

자식 bone들은 이미 FBX에서 skeleton parent 기준 local transform을 가지고 있습니다.

즉 일반적인 자식 bone은 기존대로 다음 값을 쓰면 됩니다.

```cpp
Node->EvaluateLocalTransform()
```

그런데 모든 자식 bone에 대해 global matrix와 parent inverse를 이용해 local을 다시 계산하면 다음 문제가 생깁니다.

1. 기존 local bind pose와 다른 값이 저장됨
2. runtime은 `LocalMatrix * ParentGlobal` 방식으로 global matrix를 누적함
3. FBX SDK matrix convention과 엔진 matrix convention이 완전히 같다는 보장이 없음
4. 결과적으로 전체 skeleton pose, scale, rotation이 크게 틀어질 수 있음

이 때문에 "잘 붙긴 하는데 스케일이 엄청 커지는 문제" 또는 "아예 skeleton이 망가지는 문제"가 발생했습니다.

### 최종 수정

최종적으로는 범위를 root에만 제한했습니다.

현재 규칙:

- parent skeleton bone이 있는 자식 bone: 기존 `EvaluateLocalTransform()` 유지
- 실제 root bone이고, 그 위에 non-skeleton wrapper가 있을 때만: `EvaluateGlobalTransform()`을 root local bind pose로 사용

즉 자식 bone local pose는 건드리지 않습니다.

## 6. null / wrapper가 이동 animation을 들고 있는 문제

### 현상

FBX에서 실제 skeleton root인 pelvis는 움직이지 않고, 그 위의 null/wrapper node가 이동량을 들고 있는 경우가 있었습니다.

wrapper를 skeleton에서 제거하면 skeleton root는 `ParentIndex = -1`이 되지만, wrapper의 animation track은 skeleton에 없으므로 사라집니다. 그 결과 viewer에서는 pelvis 기준으로 고정되어 전혀 움직이지 않는 것처럼 보입니다.

### 목표

null/wrapper를 skeleton bone으로 넣지 않으면서, wrapper가 가진 이동량은 첫 실제 root bone에 넘겨야 합니다.

즉 다음 FBX를:

```text
NullWrapper animated translation
└─ Pelvis skeleton root
```

엔진 animation에서는 다음처럼 bake합니다.

```text
Pelvis local animation = Pelvis global animation including NullWrapper animation
```

### 해결 방식

공용 helper를 추가했습니다.

관련 위치:

- [FbxSceneQuery.h](../KraftonEngine/Source/Engine/Mesh/Fbx/FbxSceneQuery.h)
- [FbxSceneQuery.cpp](../KraftonEngine/Source/Engine/Mesh/Fbx/FbxSceneQuery.cpp)

```cpp
static bool HasNonSkeletonWrapperParent(FbxNode* Node);
```

이 함수는 특정 skeleton node 위에 scene root가 아닌 non-skeleton parent가 있는지 확인합니다.

bind pose import에서는 다음 조건일 때만 wrapper transform을 root에 흡수합니다.

```cpp
const bool bAbsorbWrapperTransform =
    Bone.ParentIndex < 0 &&
    FFbxSceneQuery::HasNonSkeletonWrapperParent(Node);
```

animation import에서도 같은 조건을 사용합니다.

```cpp
const bool bAbsorbWrapperTransform =
    Context.Bones[BoneIndex].ParentIndex < 0 &&
    FFbxSceneQuery::HasNonSkeletonWrapperParent(BoneNode);
```

조건이 true이면 root animation track은 local bake 결과가 아니라 global evaluator 결과를 사용합니다.

```cpp
const FbxAMatrix FinalFbxMatrix =
    bAbsorbWrapperTransform
        ? BoneNode->EvaluateGlobalTransform(Time)
        : BakeResult.FinalMatrix;
```

이렇게 하면 wrapper animation이 포함된 root global transform이 root local track으로 들어갑니다. 엔진에서 root는 parent가 없으므로:

```text
RootGlobal = RootLocal
```

이 되어 wrapper 이동량이 사라지지 않습니다.

### 왜 모든 root에 적용하지 않는가

모든 `ParentIndex == -1` bone에 대해 global transform을 쓰면 wrapper가 없는 파일까지 바뀝니다.

예를 들어 어떤 FBX는 원래부터 `Pelvis`가 실제 root일 수 있습니다.

```text
FBX scene root
└─ Pelvis skeleton root
```

이 경우 `Pelvis`의 local transform은 이미 올바른 root local입니다. 여기에 global 변환을 강제로 쓰면 bind/animation 기준이 달라져 pelvis 기준으로 고정되거나 scale/rotation이 틀어질 수 있습니다.

그래서 조건은 "root bone"이 아니라 "root bone이고 실제 non-skeleton wrapper parent가 있음"으로 제한했습니다.

## 7. Viewer에서 root가 계속 고정되던 직접 원인

### 현상

wrapper 이동량을 root track으로 bake한 것처럼 보이는데도, MeshEditorWidget Animation tab에서 pelvis/root가 여전히 고정되어 움직이지 않았습니다.

### 직접 원인

importer가 아니라 viewer 쪽에서 매 frame root pose를 ref pose로 강제로 되돌리고 있었습니다.

기존 흐름:

```cpp
Out.ResetToRefPose();
const FTransform RootRefPose = Out.Pose[0];

NodeInst->EvaluatePose(Out);

Out.Pose[0] = RootRefPose;
Comp->SetBoneLocalTransforms(Out.Pose);
```

이 코드는 animation root track에 어떤 값이 들어 있어도 마지막에 0번 bone을 bind pose로 되돌립니다.

따라서 importer에서 wrapper 이동을 `Pelvis` track으로 제대로 bake해도 viewer에서는 전혀 움직이지 않는 것처럼 보였습니다.

### 해결 방식

[MeshEditorWidget.cpp](../KraftonEngine/Source/Editor/UI/Asset/MeshEditorWidget.cpp)에서 root pose를 강제로 ref pose로 되돌리는 코드를 제거했습니다.

현재 흐름:

```cpp
Out.ResetToRefPose();
NodeInst->EvaluatePose(Out);
Comp->SetBoneLocalTransforms(Out.Pose);
```

이제 viewer는 animation asset에 저장된 pose를 그대로 보여줍니다.

in-place로 보고 싶을 때는 viewer가 강제로 root를 고정하는 것이 아니라, `UAnimSequence`의 옵션을 사용해야 합니다.

- `bForceRootLock`
- `bEnableRootMotion`
- `RootMotionBoneName`

이 옵션들은 [AnimSequence.cpp](../KraftonEngine/Source/Engine/Animation/AnimSequence.cpp)에서 pose 평가 시 root translation을 제한하거나 root motion으로 추출하는 역할을 합니다.

## 8. Root motion 옵션과 이번 수정의 관계

`UAnimSequence::GetBonePose()`에는 root motion 관련 suppress 로직이 있습니다.

```cpp
const bool bSuppressHorizontalTranslation =
    bIsRootMotionBone && bForceRootLock;

const bool bSuppressAllTranslation =
    bIsRootMotionBone && bEnableRootMotion;
```

이 옵션이 켜져 있으면 root track이 존재하더라도 pose에는 translation이 적용되지 않을 수 있습니다.

따라서 root가 움직이지 않을 때 확인해야 하는 순서는 다음입니다.

1. importer가 root track에 이동 key를 넣었는가
2. `RootMotionBoneName`이 해당 root/pelvis track으로 잡혔는가
3. `bForceRootLock` 또는 `bEnableRootMotion`이 켜져 있어서 translation을 suppress하고 있지 않은가
4. viewer가 root pose를 강제로 덮어쓰고 있지 않은가

이번 마지막 문제는 4번이 직접 원인이었습니다.

## 9. Import 시간 overlay

FBX를 Content Browser에서 열 때 import에 걸린 시간을 기록하고, MeshEditorWidget overlay에 표시하도록 했습니다.

관련 위치:

- [ContentBrowserElement.cpp](../KraftonEngine/Source/Editor/UI/ContentBrowser/ContentBrowserElement.cpp)
- [MeshEditorWidget.cpp](../KraftonEngine/Source/Editor/UI/Asset/MeshEditorWidget.cpp)

동작:

1. `.fbx` 더블클릭
2. skin deformer가 있는지 probe
3. skeletal FBX import 시작 시각 기록
4. import 완료 후 `FMeshEditorWidget::RecordImportDurationForAsset()`에 초 단위 시간 저장
5. MeshEditorWidget overlay에서 vertex/index/triangle 수와 함께 `Import Time: x.xxx sec` 표시

현재 overlay는 다음 정보를 표시합니다.

- Triangles
- Vertices
- Indices
- Import Time

주의:

- 현재 기록 경로는 Content Browser에서 FBX를 열어 skeletal mesh import를 수행한 경우를 기준으로 합니다.
- 이미 저장된 `.uasset`을 다시 열면 import 시간이 새로 계산되지 않으므로 기록을 clear하거나 표시하지 않습니다.

## 10. Existing asset / uasset에 대한 영향

이번 변경들은 대부분 import 시점에 mesh vertex, skeleton hierarchy, animation track을 만드는 로직입니다.

따라서 기존 `.uasset`에는 자동으로 반영되지 않습니다.

다음 문제를 확인하려면 reimport가 필요합니다.

- face mesh가 떨어지는 문제
- winding이 뒤집힌 문제
- wrapper 제거 후 skeleton hierarchy가 달라지는 문제
- null/wrapper animation을 root track으로 bake하는 문제
- `MeshRange.MeshBindGlobal` legacy transform 제거 효과

기존 asset이 이미 synthetic root wrapper bone을 포함한 skeleton으로 저장되어 있다면, 해당 asset은 reimport 전까지 parent hierarchy가 그대로 유지됩니다.

## 11. 남은 주의점

### null/wrapper가 root의 ancestor가 아닌 경우

현재 bake 방식은 wrapper가 실제 skeleton root의 ancestor일 때만 정확합니다.

가능한 구조:

```text
NullWrapper
└─ Pelvis
```

이 경우 `Pelvis->EvaluateGlobalTransform(Time)`에 wrapper motion이 포함되므로 root track으로 bake할 수 있습니다.

하지만 다음 구조라면 현재 방식으로는 wrapper motion을 root에 넘길 수 없습니다.

```text
NullWrapper animated
├─ MeshNode
└─ SomeOtherNode

Pelvis skeleton root
```

wrapper가 skeleton root의 ancestor가 아니기 때문입니다.

이 경우는 FBX export 구조를 바꾸거나, 명시적인 virtual root bone을 도입해야 합니다.

### 여러 skeleton root가 같은 wrapper 아래에 있는 경우

다음처럼 여러 skeleton root가 하나의 wrapper 아래에 있으면:

```text
NullWrapper
├─ RootA
└─ RootB
```

각 root에 wrapper motion이 bake될 수 있습니다.

이 구조가 실제로 필요한 경우에는 wrapper를 제거하는 방식보다 virtual root bone을 명시적으로 추가하는 방식이 더 안전할 수 있습니다.

단, virtual root bone은 skeleton compatibility에 영향을 주므로 target skeleton과 animation skeleton 양쪽에 같은 정책을 적용해야 합니다.

### matrix convention 문제

FBX SDK의 matrix multiplication convention과 엔진의 `Local * ParentGlobal` convention이 다를 수 있습니다.

그래서 모든 bone에 대해 `ChildGlobal * ParentInverse` 같은 방식으로 local을 재계산하는 것은 위험합니다.

이번 최종 수정은 이 위험을 피하기 위해 다음 원칙을 사용합니다.

- 자식 bone local은 FBX SDK의 `EvaluateLocalTransform()`을 그대로 사용
- wrapper가 있는 실제 root bone만 SDK의 `EvaluateGlobalTransform()`을 사용

## 12. 관련 파일 목록

### FBX scene query

- [FbxSceneQuery.h](../KraftonEngine/Source/Engine/Mesh/Fbx/FbxSceneQuery.h)
- [FbxSceneQuery.cpp](../KraftonEngine/Source/Engine/Mesh/Fbx/FbxSceneQuery.cpp)

역할:

- skeleton node 판정
- mesh node 수집
- non-skeleton wrapper parent 판정

### Skeleton import

- [FbxSkeletonImporter.cpp](../KraftonEngine/Source/Engine/Mesh/Fbx/FbxSkeletonImporter.cpp)

역할:

- 실제 skeleton node만 bone으로 추가
- synthetic root wrapper 제거
- wrapper가 있는 root bone만 global bind pose를 local bind pose로 흡수

### Animation import

- [FbxAnimationImporter.cpp](../KraftonEngine/Source/Engine/Mesh/Fbx/FbxAnimationImporter.cpp)

역할:

- FBX animation layer bake
- wrapper가 있는 root bone만 global animation transform을 local track으로 bake
- root motion bone 자동 감지

### Skin weight import

- [FbxSkinWeightImporter.cpp](../KraftonEngine/Source/Engine/Mesh/Fbx/FbxSkinWeightImporter.cpp)

역할:

- skin cluster weight import
- non-skinned mesh rigid attachment
- rigid bind correction
- skeletal winding reversal
- mesh bind transform을 vertex에 bake

### Static mesh import

- [FbxStaticMeshImporter.cpp](../KraftonEngine/Source/Engine/Mesh/Fbx/FbxStaticMeshImporter.cpp)

역할:

- static mesh FBX import
- negative determinant winding reversal

### Mesh editor viewer

- [MeshEditorWidget.cpp](../KraftonEngine/Source/Editor/UI/Asset/MeshEditorWidget.cpp)

역할:

- mesh stats overlay
- import time overlay
- animation preview
- root pose 강제 고정 제거

## 13. 검증

코드 변경 후 `ClCompile`로 컴파일 확인했습니다.

사용한 명령:

```powershell
$env:Path = "C:\Users\jungle\Documents\GitHub\Jungle_Week11_Team7\Scripts\python;" + $env:Path
msbuild KraftonEngine\KraftonEngine.vcxproj /t:ClCompile /p:Configuration=Debug /p:Platform=x64 /m /verbosity:minimal
```

결과:

```text
ClCompile 통과
```

## 14. 디버깅 체크리스트

FBX import 후 root, face mesh, CPU/GPU skinning이 이상하면 다음 순서로 확인합니다.

1. 해당 FBX의 mesh node에 skin deformer가 있는지 확인
2. skin이 없는 mesh라면 rigid attachment bone이 어떤 bone으로 resolve되는지 확인
3. `MeshBindGlobal` determinant가 음수인지 확인
4. 새 import에서 `MeshRange.MeshBindGlobal`이 identity인지 확인
5. skeleton hierarchy에 non-skeleton wrapper가 bone으로 들어갔는지 확인
6. 첫 실제 root bone 위에 non-skeleton wrapper가 있는지 확인
7. wrapper가 있으면 root bind pose가 global 기준으로 들어갔는지 확인
8. wrapper가 있으면 root animation track이 `EvaluateGlobalTransform(Time)` 기준으로 들어갔는지 확인
9. `RootMotionBoneName`, `bForceRootLock`, `bEnableRootMotion` 상태 확인
10. Viewer에서 root pose를 강제로 ref pose로 되돌리는 코드가 없는지 확인
11. 기존 `.uasset`이 아니라 새로 reimport한 asset으로 확인

