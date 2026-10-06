# Skeletal Mesh Skinning / FBX Import 수정 기록

작성일: 2026-05-19

이 문서는 `IdleWithSkin.fbx`, `SKM_Manny_Simple.FBX`를 보면서 수정한 Skeletal Mesh import, CPU/GPU skinning, Viewer bone edit 관련 변경 사항을 정리한 기록입니다.

## 1. CPU Skinning과 GPU Skinning 결과 차이

### 문제

기존에는 `FSkeletalMeshRange::MeshBindGlobal`이 import 결과에 남아 있었고, CPU skinning에서 range별 `MeshBindGlobal`을 별도로 곱는 흐름이 있었습니다.

GPU skinning은 같은 range 정보를 사용하지 않고 bone skin matrix만 사용했기 때문에, 같은 skeletal mesh라도 CPU/GPU 모드를 바꾸면 결과가 달라질 수 있었습니다.

특히 face 같은 일부 mesh가 별도 mesh node로 들어오는 FBX에서는 CPU 쪽에서만 range transform이 적용되어 위치가 다르게 보일 수 있었습니다.

### 수정

CPU/GPU 모두 같은 skin matrix를 사용하도록 통합했습니다.

공통 skin matrix:

```cpp
SkinMatrix = InverseBindPoseMatrix * CurrentBoneGlobalMatrix;
```

적용 내용:

- `USkinnedMeshComponent::BuildSkinMatrices()` 추가
- CPU skinning도 `BuildSkinMatrices()` 결과를 사용
- GPU skinning matrix upload도 같은 `BuildSkinMatrices()` 결과를 사용
- `MeshRange.MeshBindGlobal`은 legacy serialization slot으로만 남기고 import 시 `FMatrix::Identity`로 저장
- weight가 없는 vertex fallback도 mesh range transform 대신 원본 bind-space vertex를 그대로 사용

관련 파일:

- `KraftonEngine/Source/Engine/Component/SkinnedMeshComponent.h`
- `KraftonEngine/Source/Engine/Component/SkinnedMeshComponent.cpp`
- `KraftonEngine/Source/Engine/Render/Proxy/SkeletalMeshSceneProxy.cpp`
- `KraftonEngine/Source/Engine/Mesh/SkeletalMeshAsset.h`

## 2. Non-skinned mesh의 rigid bone attachment 방식

### 문제

FBX 안에 skeleton은 있지만 특정 mesh node 자체에는 skin cluster가 없는 경우가 있습니다.

`IdleWithSkin.fbx`의 face/eye mesh가 이런 케이스입니다. 이 mesh들은 skin deformer가 없지만 skeleton 아래에 배치되어 있고, 런타임에서는 특정 bone에 rigid하게 붙어야 합니다.

기존 방식은 가까운 parent bone을 찾는 수준이라 다음 문제가 있었습니다.

- mesh node 자신이 bone인 경우
- mesh node 아래 descendant에 bone이 있는 경우
- parent 쪽으로 올라가야 하는 경우
- skin cluster가 없어서 weight가 비어 있는 경우

이 케이스를 일관되게 처리하지 못했습니다.

### 수정

skin cluster가 없는 mesh는 rigid attachment bone을 재귀적으로 찾고, 모든 control point weight를 해당 bone 하나에 `1.0f`로 강제 할당합니다.

bone 탐색 순서:

1. mesh node 자신이 bone mapping에 있는지 확인
2. child/descendant를 재귀적으로 내려가며 첫 bone 확인
3. parent/ancestor를 올라가며 첫 bone 확인

적용 결과:

- non-skinned mesh도 skinning path를 그대로 통과
- 모든 vertex가 한 bone에 rigid weight 1로 붙음
- face/eye처럼 skin cluster가 없는 mesh가 skeleton과 함께 움직일 수 있음

관련 파일:

- `KraftonEngine/Source/Engine/Mesh/Fbx/FbxSkinWeightImporter.cpp`

## 3. IdleWithSkin face가 떨어지거나 커지는 문제

### 문제

`IdleWithSkin.fbx`의 face/eye mesh는 skin cluster가 없고 `Bip001 Head`에 rigid하게 붙어야 합니다.

단순히 face/eye vertex를 `MeshBindGlobal`로 구운 뒤 Head bone weight 1을 주면 다음 일이 생깁니다.

1. import 시 vertex가 이미 FBX mesh global 위치로 변환됨
2. runtime skinning에서 Head bone의 `InverseBindPose * CurrentGlobal`이 다시 적용됨
3. Head cluster의 bind matrix basis와 mesh node global basis가 다르면 위치 보정이 중복됨

그래서 어떤 상태에서는 face가 떨어지고, 어떤 상태에서는 붙지만 scale이 커지는 현상이 생겼습니다.

### 수정

rigid mesh vertex는 "runtime skinning을 한 번 통과한 뒤 원하는 FBX 위치가 되도록" import 시 미리 역보정합니다.

계산 흐름:

```cpp
DesiredPosition = MeshBindGlobal * LocalPosition;
SkinBindMatrix = Bone.InverseBindPoseMatrix * Bone.GlobalMatrix;
StoredPosition = DesiredPosition * inverse(SkinBindMatrix);
```

runtime에서는 다시 다음 계산이 적용됩니다.

```cpp
FinalPosition = StoredPosition * SkinBindMatrix;
```

결과적으로:

```cpp
FinalPosition == DesiredPosition
```

이 방식은 rigid mesh도 일반 skinned mesh와 같은 CPU/GPU skinning 경로를 사용하면서, face/eye만 별도 좌표계로 튀는 문제를 막습니다.

추가로, rigid mesh가 skinned mesh보다 먼저 import되는 경우에도 bone inverse bind pose가 준비되도록 cluster inverse bind pose를 geometry import 전에 pre-pass로 먼저 수집합니다.

관련 함수:

- `ImportClusterInverseBindPoses()`
- `BuildRigidBindCorrection()`

관련 파일:

- `KraftonEngine/Source/Engine/Mesh/Fbx/FbxSkinWeightImporter.cpp`

## 4. FBX inverse bind pose 계산 방식

### 문제

기존에는 FBX matrix를 engine `FMatrix`로 변환한 뒤 `FMatrix::GetInverse()`를 호출하는 흐름이 있었습니다.

FBX는 axis/unit conversion 이후에도 작은 scale이 들어갈 수 있고, float matrix inverse에서 determinant threshold에 걸리면 inverse가 불안정해질 수 있습니다.

### 수정

FBX SDK의 `FbxAMatrix::Inverse()`를 먼저 사용한 뒤 engine matrix로 변환하는 helper를 추가했습니다.

```cpp
FMatrix FFbxTransformUtils::ToEngineInverseMatrix(const FbxAMatrix& FbxMat)
{
    return ToEngineMatrix(FbxMat.Inverse());
}
```

적용 위치:

- skeleton import 시 bone inverse bind pose
- skin cluster import 시 link inverse bind pose

관련 파일:

- `KraftonEngine/Source/Engine/Mesh/Fbx/FbxTransformUtils.h`
- `KraftonEngine/Source/Engine/Mesh/Fbx/FbxTransformUtils.cpp`
- `KraftonEngine/Source/Engine/Mesh/Fbx/FbxSkeletonImporter.cpp`
- `KraftonEngine/Source/Engine/Mesh/Fbx/FbxSkinWeightImporter.cpp`

## 5. Manny Viewer에서 특정 bone 수정 시 0,0,0으로 무너지는 문제

### 문제

`SKM_Manny_Simple.FBX`는 skeleton root 쪽에 `0.01` 단위 scale이 들어갑니다.

Viewer에서 특정 bone을 gizmo나 우측 패널로 수정하면 setter 내부에서 parent global inverse를 구합니다.

기존 코드:

```cpp
ParentGlobalInv = ParentGlobal.GetInverse();
Local = DesiredGlobal * ParentGlobalInv;
```

문제는 parent global matrix에 `0.01` scale이 누적되면 determinant가 `1e-6` 근처까지 작아질 수 있다는 점입니다. 기존 `FMatrix::GetInverse()`는 이 값을 singular로 보고 zero matrix를 반환할 수 있습니다.

그 결과:

```cpp
Local = DesiredGlobal * ZeroMatrix;
```

가 되어 bone local matrix가 통째로 0으로 무너지고, Viewer에서는 bone이 `0,0,0`으로 간 것처럼 보였습니다.

### 수정

전역 `FMatrix::GetInverse()`를 바꾸지 않고, bone edit setter 경로에만 double precision affine inverse를 추가했습니다.

추가 함수:

```cpp
GetAffineInverseForBoneEdit()
```

적용 위치:

- `SetBoneLocationByIndex()`
- `SetBoneRotationByIndex(FRotator)`
- `SetBoneRotationByIndex(FQuat)`
- `SetBoneScaleByIndex()`

이제 bone edit 경로에서는 작은 scale이 있어도 parent inverse가 zero matrix로 떨어지지 않습니다.

관련 파일:

- `KraftonEngine/Source/Engine/Component/SkinnedMeshComponent.cpp`

## 6. 롤백한 내용

중간에 scale이 크게 나오는 원인을 찾기 위해 animation/local hierarchy 쪽을 수정한 시도가 있었지만, 최종 수정에는 포함하지 않았습니다.

최종 상태에서 유지하지 않은 내용:

- `FbxAnimationImporter.cpp`의 hierarchy-local fallback 변경
- `Global * ParentGlobalInverse` 방식으로 skeleton local matrix를 재계산하는 변경
- animation bake policy 변경

현재 최종 수정은 다음 두 축으로 제한되어 있습니다.

- FBX import의 bind/rigid mesh 처리
- Viewer bone edit의 parent inverse 안정화

## 7. 재import 필요 여부

### 재import 필요

다음 수정은 import 결과 vertex, weight, inverse bind pose에 영향을 주므로 기존 `.uasset`에는 자동 반영되지 않습니다.

- non-skinned mesh rigid weight 처리
- rigid bind correction
- cluster inverse bind pose pre-pass
- `MeshRange.MeshBindGlobal = Identity`
- FBX inverse bind pose 계산 방식

따라서 아래 FBX들은 수정 확인 시 다시 import해야 합니다.

- `KraftonEngine/Data/hirasawa-yui/IdleWithSkin.fbx`
- `KraftonEngine/Data/Mannequins/SKM_Manny_Simple.FBX`

### 재import 불필요

Viewer에서 bone을 수정하면 `0,0,0`으로 무너지는 문제는 runtime/editor component code 문제였으므로 asset 재import가 필요 없습니다.

## 8. 검증

수정 후 컴파일 확인:

```powershell
$env:Path = "C:\Users\jungle\Documents\GitHub\Jungle_Week11_Team7\Scripts\python;" + $env:Path
msbuild KraftonEngine\KraftonEngine.vcxproj /t:ClCompile /p:Configuration=Debug /p:Platform=x64 /m /verbosity:minimal
```

결과:

- `FbxSkinWeightImporter.cpp` 컴파일 성공
- `SkinnedMeshComponent.cpp` 컴파일 성공
- warning 0개
- error 0개

전체 link는 `KraftonEngine.exe`가 실행 중이면 `LNK1168`이 발생할 수 있으므로, 확인이 필요하면 에디터 실행 프로세스를 종료한 뒤 전체 빌드를 수행해야 합니다.

## 9. 현재 기대 동작

- CPU skinning과 GPU skinning은 같은 skin matrix를 사용합니다.
- `MeshRange.MeshBindGlobal` 차이 때문에 CPU/GPU 결과가 갈라지는 경로는 제거되었습니다.
- skin cluster가 없는 face/eye mesh는 rigid bone weight 1로 붙습니다.
- `IdleWithSkin.fbx`의 face/eye는 Head bone skin matrix를 통과해도 원래 FBX 위치에 남도록 import됩니다.
- Manny Viewer bone edit에서 작은 root scale 때문에 parent inverse가 zero matrix가 되는 문제를 피합니다.

## 10. FBX negative determinant winding 보정

### 문제

일부 FBX는 import transform의 basis determinant가 음수입니다.

이 경우 vertex position과 normal은 변환되지만 triangle index 순서는 그대로 유지되어, 엔진의 `SolidBackCull` 기준에서 앞면이 back face로 판정될 수 있습니다. 결과적으로 front culling처럼 보입니다.

### 수정

FBX import 시 mesh transform의 3x3 basis determinant를 검사합니다.

- Static FBX: `MeshToWorld`
- Skeletal FBX: `MeshBindGlobal`

determinant가 음수이면 triangle index 1/2를 swap합니다.

```cpp
std::swap(TriIndices[1], TriIndices[2]);
std::swap(PendingSectionIndices[1], PendingSectionIndices[2]);
```

이 처리는 section index 저장과 tangent accumulation 전에 수행하므로, 렌더링 winding과 tangent basis가 같은 방향으로 보정됩니다.

관련 파일:

- `KraftonEngine/Source/Engine/Mesh/Fbx/FbxTransformUtils.h`
- `KraftonEngine/Source/Engine/Mesh/Fbx/FbxTransformUtils.cpp`
- `KraftonEngine/Source/Engine/Mesh/Fbx/FbxStaticMeshImporter.cpp`
- `KraftonEngine/Source/Engine/Mesh/Fbx/FbxSkinWeightImporter.cpp`

### 재import 필요 여부

필요합니다. winding은 import된 index buffer에 저장되므로, 이미 만들어진 `.uasset`에는 코드 수정만으로 자동 반영되지 않습니다.

다만 전체 asset을 모두 다시 만들 필요는 없고, front/back culling이 뒤집혀 보이는 FBX에서 생성된 `.uasset`만 재import하면 됩니다.
