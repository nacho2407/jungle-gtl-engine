# DirectX 11 3D Editor Engine

DirectX 11과 ImGui 기반의 실시간 3D 씬 에디터 엔진입니다.
Actor/Component 아키텍처 위에 WYSIWYG 씬 편집, 레이캐스팅 오브젝트 선택, 멀티씬 관리, JSON 직렬화를 지원합니다.

> Jungle Week3 — Team6 (조현석, 심우진, 김효범, 강건우)

---

## 목차

- [빌드 및 실행](#빌드-및-실행)
- [프로젝트 구조](#프로젝트-구조)
- [아키텍처 개요](#아키텍처-개요)
  - [오브젝트 시스템 (UObject, RTTI, FName)](#오브젝트-시스템)
  - [렌더링 파이프라인](#렌더링-파이프라인)
  - [에디터](#에디터)
  - [입력 시스템](#입력-시스템)
  - [직렬화](#직렬화)
- [셰이더](#셰이더)
- [에셋](#에셋)
- [주요 기능 요약](#주요-기능-요약)

---

## 빌드 및 실행

### 요구사항

- Visual Studio 2022 (v143 툴셋)
- Windows SDK (DirectX 11 포함)
- 패키지 매니저 불필요 — 모든 의존성(ImGui, SimpleJSON, DirectXTK)이 프로젝트에 포함되어 있음

### 빌드

```bash
# MSBuild (x64 Debug)
msbuild KraftonEngine.sln /p:Configuration=Debug /p:Platform=x64

# 또는 Visual Studio에서 솔루션 열어 빌드
```

| 구성 | 플랫폼 | C++ 표준 | 서브시스템 |
|------|--------|---------|-----------|
| Debug | x64 | C++20 | Windows |
| Release | x64 | C++20 | Windows |
| Debug | Win32 | C++17 | Console |

출력 경로: `KraftonEngine/Bin/<Configuration>/KraftonEngine.exe`

### 릴리스 패키징

`ReleaseBuild.bat`을 실행하면 `ReleaseBuild/` 폴더에 실행파일, 셰이더, 에셋이 복사됩니다.

---

## 프로젝트 구조

```
KraftonEngine/
├── Source/
│   ├── Engine/                     # 엔진 코어
│   │   ├── Object/                 # UObject, RTTI, FName, ObjectFactory
│   │   ├── Core/                   # InputSystem, Timer, ResourceManager, Stats
│   │   ├── Math/                   # FVector, FMatrix, FTransform
│   │   ├── Runtime/                # EngineLoop, WindowsApplication, Launch
│   │   ├── GameFramework/          # AActor, UWorld, WorldContext
│   │   ├── Component/              # Scene/Primitive/Camera/Billboard/Gizmo/SubUV
│   │   ├── Serialization/          # SceneSaveManager (JSON)
│   │   └── Render/
│   │       ├── Device/             # D3DDevice (스왑체인, 래스터라이저, 뎁스)
│   │       ├── Scene/              # RenderCollector, RenderBus, RenderCommand
│   │       ├── Renderer/           # Renderer, IRenderPipeline
│   │       ├── Resource/           # Buffer, Shader, VertexTypes
│   │       ├── Mesh/               # MeshManager
│   │       └── (Batchers)          # LineBatcher, FontBatcher, SubUVBatcher
│   │
│   └── Editor/                     # 에디터 전용
│       ├── EditorEngine            # UEngine 상속, 메인 에디터 런타임
│       ├── Viewport/               # 뷰포트 클라이언트, 카메라, Picking
│       ├── Selection/              # SelectionManager (다중선택 + Gizmo)
│       ├── Settings/               # EditorSettings (Save/Load)
│       └── UI/                     # ImGui 위젯들
│           ├── EditorMainPanel
│           ├── EditorPropertyWidget
│           ├── EditorSceneWidget
│           ├── EditorViewportOverlayWidget
│           ├── EditorConsoleWidget
│           ├── EditorControlWidget
│           └── EditorStatWidget
│
├── Shaders/                        # HLSL 셰이더 (8개, 런타임 컴파일)
├── Asset/                          # 폰트 아틀라스, 파티클 텍스쳐, 기본 씬
├── Saves/                          # 사용자 씬 파일 (.Scene)
├── Settings/                       # 에디터 설정 (editor.ini)
└── ThirdParty/                     # ImGui, SimpleJSON
```

### 인클루드 경로

프로젝트는 `Source/Engine`, `Source`, `Source/Editor`, `ThirdParty`, `ThirdParty/ImGui`를 인클루드 경로로 사용합니다.
헤더는 이 루트들로부터의 상대 경로로 포함합니다.

```cpp
#include "Engine/Core/InputSystem.h"     // Source/Engine 기준
#include "Editor/EditorEngine.h"          // Source/Editor 기준
#include "ImGui/imgui.h"                  // ThirdParty/ImGui 기준
```

---

## 아키텍처 개요

### 오브젝트 시스템

커스텀 RTTI와 오브젝트 관리를 자체 구현합니다.

**UObject 상속 계층:**

```
UObject
├── AActor                           # 씬에 배치되는 엔티티, 컴포넌트 소유
│   ├── ACubeActor
│   ├── ASphereActor
│   └── APlaneActor
├── UActorComponent
│   └── USceneComponent              # Transform 계층 (부모-자식)
│       ├── UCameraComponent
│       └── UPrimitiveComponent      # 렌더링 + 충돌
│           ├── UCubeComponent
│           ├── USphereComponent
│           ├── UPlaneComponent
│           ├── UGizmoComponent
│           └── UBillboardComponent
│               ├── UTextRenderComponent
│               └── USubUVComponent
├── UWorld
└── UEngine
    └── UEditorEngine
```

**RTTI**: `DECLARE_CLASS` / `DEFINE_CLASS` 매크로로 `FTypeInfo` 체인을 생성하며, `IsA<T>()`, `Cast<T>()`를 지원합니다.

**FName**: 문자열 풀(Pool)에 저장하고 인덱스로 비교하는 경량 문자열 시스템입니다. 대소문자를 무시합니다.

**UObjectManager**: 싱글턴으로 오브젝트 생성/소멸을 관리하며, `ClassName_N` 형식의 자동 네이밍을 제공합니다.

---

### 렌더링 파이프라인

커맨드 버퍼 패턴을 사용하는 멀티패스 렌더링 파이프라인입니다.

```
CollectWorld ──→ RenderBus ──→ GetAlignedCommands ──→ PrepareBatchers ──→ Render ──→ Present
 (씬 순회)       (패스별 큐)      (SRV 정렬)           (Batcher 수집)      (GPU 제출)
```

**3단계 실행 흐름:**

| 단계 | 담당 클래스 | 역할 |
|------|------------|------|
| Collect | RenderCollector | 씬의 모든 Actor/Component를 순회하며 FRenderCommand 생성 |
| Transport | RenderBus | 커맨드를 패스별로 분류·저장 |
| Execute | Renderer | FPassRenderState 룩업 테이블로 GPU 상태 적용 후 드로우 |

**렌더 패스 (ERenderPass):**

Opaque → Font → SubUV → Translucent → StencilMask → Outline → Editor → Grid → DepthLess

**데이터 주도 설계:**

- `FPassRenderState`: 패스별 렌더 상태(DepthStencil, Blend, Rasterizer, Topology)를 룩업 테이블로 관리. 새 패스 추가 시 테이블에 한 줄만 추가.
- `bWireframeAware` 플래그로 패스 수준에서 Wireframe 모드 전환 제어.

**Batcher 시스템:**

| Batcher | 용도 | 특징 |
|---------|------|------|
| LineBatcher | 디버그 라인, 그리드, AABB | 모든 라인을 하나의 VB/IB로 통합 |
| FontBatcher | Billboard 텍스트 렌더링 | Font Atlas UV 계산, Screen-Aligned Quad |
| SubUVBatcher | 스프라이트 애니메이션 | SRV별 배치 드로우, DrawIndexed offset 기법 |

각 Batcher는 자체 셰이더/샘플러를 소유하며, Renderer는 `Flush()`만 호출합니다.

---

### 에디터

`UEditorEngine`이 `UEngine`을 상속하여 에디터 전용 기능을 추가합니다.

**뷰포트 기능:**

- Ray-Triangle Intersection 기반 오브젝트 피킹 (클릭 선택)
- Stencil 기반 Selection Outline (2-pass: StencilWrite → StencilOutline)
- Gizmo 트랜스폼 조작 (Translate / Rotate / Scale)
- 카메라 WASD/QE 이동, 우클릭 회전

**UI (ImGui 기반):**

- Scene Widget — 씬 계층 트리뷰
- Property Widget — 선택된 오브젝트의 프로퍼티 편집
- Viewport Overlay — View Mode(Lit/Unlit/Wireframe), Show Flags, 그리드/카메라 설정
- Console Widget — 디버그 콘솔
- Stat Widget — GPU/CPU 프로파일링 정보

**Show Flags**: Primitives, Grid, Gizmo, BillboardText, BoundingVolume 토글을 지원하며, `EditorSettings`를 통해 JSON으로 저장/복원합니다.

---

### 입력 시스템

`InputSystem`은 싱글턴으로 매 프레임 `GetAsyncKeyState()`를 폴링합니다.

- 키보드: `GetKey()`, `GetKeyDown()`, `GetKeyUp()` (프레임 단위 엣지 검출)
- 마우스: 위치, 델타, 드래그 상태 머신 (threshold 기반)
- 스크롤: `WM_MOUSEWHEEL` 메시지 기반
- 포커스 가드: 윈도우 포커스가 없으면 모든 입력 차단
- ImGui 연동: `bUsingKeyboard` / `bUsingMouse` 플래그로 UI 입력 우선

---

### 직렬화

`FSceneSaveManager`가 `.Scene` 파일(JSON)을 읽고 씁니다.

저장 내용: Actor 계층, Component 구성, Transform, 카메라 상태, 씬 메타데이터

```json
{
  "Actors": [{
    "ClassName": "ACubeActor",
    "RootComponent": {
      "ClassName": "UCubeComponent",
      "Properties": { "Location": [0, 0, 0], "Scale": [1, 1, 1] },
      "Children": [...]
    }
  }],
  "ClassName": "UWorld",
  "Version": 2
}
```

JSON 파싱에 `SimpleJSON/json.hpp`를 사용합니다.

---

## 셰이더

`Shaders/` 디렉토리에 8개의 HLSL 파일이 있으며, 런타임에 컴파일됩니다.

| 셰이더 | 용도 |
|--------|------|
| Common.hlsl | 공통 유틸리티 (상수 버퍼, 구조체) |
| Primitive.hlsl | 기본 프리미티브 렌더링 |
| Outline.hlsl | 선택 오브젝트 아웃라인 |
| ShaderLine.hlsl | 디버그 라인/그리드 |
| ShaderFont.hlsl | Billboard 텍스트 |
| ShaderSubUV.hlsl | SubUV 스프라이트 애니메이션 |
| Editor.hlsl | 에디터 전용 시각화 |
| Gizmo.hlsl | 트랜스폼 기즈모 |

---

## 에셋

```
Asset/
├── Font/
│   └── FontAtlas.dds          # 폰트 텍스쳐 아틀라스 (ASCII)
├── Particle/
│   ├── Effect.dds             # 이펙트 파티클 스프라이트 시트
│   └── Explosion.dds          # 폭발 파티클 스프라이트 시트
└── Scene/
    └── Default.Scene          # 기본 씬 템플릿
```

---

## 주요 기능 요약

| 카테고리 | 기능 |
|---------|------|
| **렌더링** | 멀티패스 파이프라인, 데이터 주도 렌더 상태, Batcher 시스템 |
| **텍스트** | Font Atlas 기반 Billboard Text, Screen-Aligned Quad |
| **파티클** | SubUV 스프라이트 애니메이션, SRV별 배치 드로우 |
| **에디터** | WYSIWYG 씬 편집, ImGui UI, Show Flags, View Mode |
| **선택** | Ray-Triangle 피킹, Stencil Outline, 다중선택(Shift/Ctrl) |
| **기즈모** | Translate/Rotate/Scale, Screen-space 스케일링 |
| **오브젝트** | UObject RTTI, FName 문자열 풀, UUID |
| **직렬화** | JSON 씬 파일, 에디터 설정 Save/Load |
| **디버그** | Batch Line 렌더링, AABB 바운딩 박스, GPU 프로파일러 |
| **그리드** | 동적 중심/범위 계산, 2차 감쇠 페이드, 축 라인 |
