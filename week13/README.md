# KraftonEngine

DirectX 11 기반 실시간 3D 씬 에디터 엔진입니다.
Actor/Component 아키텍처 위에 WYSIWYG 씬 편집, 레이캐스팅 오브젝트 선택, 멀티뷰포트, OBJ/머티리얼 임포트, JSON 직렬화를 지원합니다.

> **Jungle Week 4 — Team 3** (심우진, 조현석, 박세영, 김효범, 강건우)

---

## 빌드 및 실행

### 요구사항

- Visual Studio 2022 (v143 툴셋)
- Windows SDK (DirectX 11 포함)
- 별도 패키지 매니저 불필요 — ImGui, SimpleJSON, DirectXTK 포함

### 빌드

```bash
# Debug (x64)
msbuild KraftonEngine.sln /p:Configuration=Debug /p:Platform=x64

# Release (x64)
msbuild KraftonEngine.sln /p:Configuration=Release /p:Platform=x64

# OBJ Viewer — 독립 메시 프리뷰 도구
msbuild KraftonEngine.sln /p:Configuration=ObjViewDebug /p:Platform=x64
```

출력 경로: `KraftonEngine/Bin/<Configuration>/KraftonEngine.exe`

### 릴리스 패키징

```bash
./ReleaseBuild.bat
```

`ReleaseBuild/` 폴더에 실행파일, 셰이더, 에셋이 복사됩니다.

### 소스 파일 추가/제거 후

```bash
python Scripts/GenerateProjectFiles.py
```

---

## 주요 기능

| 카테고리 | 기능 |
|---------|------|
| **렌더링** | 멀티패스 커맨드 버퍼 파이프라인, 데이터 주도 렌더 상태 테이블 |
| **StaticMesh** | OBJ 임포트, 머티리얼 슬롯, 바이너리 캐싱 (.bin/.mbin) |
| **뷰포트** | Perspective / Orthographic (Top, Bottom, Left, Right, Front, Back, Free Ortho) |
| **선택** | Ray-Triangle 피킹, Stencil Outline, 다중선택 (Shift/Ctrl) |
| **기즈모** | Translate / Rotate / Scale, Screen-space 스케일링 |
| **텍스트** | Font Atlas Billboard, Overlay Stat 텍스트 |
| **파티클** | SubUV 스프라이트 애니메이션, SRV별 배치 드로우 |
| **직렬화** | JSON 씬 파일 (.Scene), 에디터 설정 (editor.ini) |
| **OBJ Viewer** | 독립 메시 프리뷰 모드 (Orbit/Pan/Zoom 카메라) |
| **프로파일링** | GPU/CPU 타이머, 메모리 통계, OverlayStat 표시 |

---

## 아키텍처

### 오브젝트 시스템

커스텀 RTTI (`DECLARE_CLASS` / `DEFINE_CLASS` 매크로 → `FTypeInfo` 체인)로 `IsA<T>()`, `Cast<T>()`를 지원합니다. `UObjectManager` 싱글턴이 오브젝트 생명주기와 자동 네이밍(`ClassName_N`)을 관리합니다.

```
UObject
├── AActor
│   └── AStaticMeshActor
├── UActorComponent
│   └── USceneComponent
│       ├── UCameraComponent
│       └── UPrimitiveComponent
│           ├── UMeshComponent
│           │   └── UStaticMeshComponent
│           ├── UGizmoComponent
│           └── UBillboardComponent
│               ├── UTextRenderComponent
│               └── USubUVComponent
├── UWorld
└── UEngine
    ├── UEditorEngine
    └── UObjViewerEngine
```

### 렌더링 파이프라인

```
RenderCollector → RenderBus → Renderer
 (씬 순회,          (패스별 큐,      (FPassRenderState 룩업,
  FRenderCommand     Batcher 엔트리   GPU 드로우 콜)
  생성)              분류)
```

| 단계 | 담당 | 역할 |
|------|------|------|
| Collect | `RenderCollector` | PrimitiveComponent 가상함수로 다형적 커맨드 수집 |
| Queue | `RenderBus` | 패스별 큐 + Batcher 전용 큐 (Font, SubUV, Grid, AABB) |
| Execute | `Renderer` | `FPassRenderState` 테이블로 GPU 상태 바인딩 후 드로우 |

**렌더 패스 순서**: Opaque → Font → SubUV → Translucent → StencilMask → Outline → Editor → Grid → DepthLess

새 렌더 패스 추가 = `FPassRenderState` 테이블에 한 줄 추가.

### 에디터

`UEditorEngine`이 `UEngine`을 상속합니다. ImGui 기반 도킹 UI:

- **Scene Manager** — 씬 저장/로드, 액터 아웃라이너 (Shift/Ctrl 다중선택)
- **Property Widget** — 선택 액터 Transform, 컴포넌트 프로퍼티 편집
- **Viewport Overlay** — View Mode (Lit/Unlit/Wireframe), Show Flags, 그리드/카메라 설정
- **Console** — 디버그 로그
- **Stat Widget** — GPU/CPU 프로파일링

**멀티뷰포트**: 1/2/4분할 레이아웃, 뷰포트별 독립 카메라/렌더옵션, 고정 Ortho + Free Ortho 지원.

### OBJ Viewer

`ObjViewDebug` 빌드로 활성화되는 독립 메시 프리뷰 모드입니다.

- `UObjViewerEngine` — 경량 엔진 서브클래스
- `ObjViewerPanel` — ImGui 메시 목록 + 임포트 옵션 UI
- `ObjViewerRenderPipeline` — 오프스크린 렌더 타겟
- `ObjViewerViewportClient` — Orbit/Pan/Zoom 카메라

### 직렬화

`.Scene` 파일(JSON)로 액터 계층, 컴포넌트, Transform, 카메라 상태를 저장/복원합니다.
에디터 설정은 `Settings/editor.ini`에 영속됩니다.

---

## 프로젝트 구조

```
KraftonEngine/
├── Source/
│   ├── Engine/                    # 엔진 코어
│   │   ├── Object/                #   UObject, RTTI, FName
│   │   ├── Core/                  #   InputSystem, Timer, Stats
│   │   ├── Math/                  #   FVector, FMatrix, FQuat, FRotator
│   │   ├── Runtime/               #   EngineLoop, WindowsApplication
│   │   ├── GameFramework/         #   AActor, AStaticMeshActor, UWorld
│   │   ├── Component/             #   Camera, Mesh, Gizmo, Billboard, SubUV
│   │   ├── Mesh/                  #   ObjManager, ObjImporter, StaticMesh
│   │   ├── Materials/             #   UMaterial
│   │   ├── Serialization/         #   SceneSaveManager, WindowsArchive
│   │   └── Render/
│   │       ├── Device/            #     D3DDevice (스왑체인, 래스터라이저)
│   │       ├── Pipeline/          #     Renderer, RenderBus, RenderCollector
│   │       ├── Batcher/           #     LineBatcher, FontBatcher, SubUVBatcher
│   │       ├── Resource/          #     Buffer, Shader, VertexTypes
│   │       └── Types/             #     ViewTypes, RenderCommand
│   ├── Editor/                    # 에디터 레이어
│   │   ├── Viewport/              #   ViewportClient, ViewportLayout
│   │   ├── Selection/             #   SelectionManager
│   │   ├── UI/                    #   ImGui 위젯들
│   │   └── Settings/              #   EditorSettings
│   └── ObjViewer/                 # 독립 메시 뷰어
├── Shaders/                       # HLSL (런타임 컴파일)
│   ├── Common/                    #   ConstantBuffers, Functions, VertexLayouts
│   ├── Primitive.hlsl
│   ├── StaticMeshShader.hlsl
│   ├── OutlinePostProcess.hlsl
│   ├── Gizmo.hlsl
│   ├── Editor.hlsl
│   ├── ShaderFont.hlsl
│   ├── ShaderOverlayFont.hlsl
│   └── ShaderSubUV.hlsl
├── Asset/                         # 폰트 아틀라스, 파티클 텍스쳐, 기본 씬, MeshCache
├── Data/                          # OBJ 메시 소스 (모델별 폴더)
├── Settings/                      # editor.ini
└── ThirdParty/                    # ImGui, SimpleJSON, DirectXTK
```

---

## 코드 컨벤션

- C++20 (x64), C++17 (Win32/x86)
- UTF-8 BOM, 탭 들여쓰기 (4칸)
- 네이밍: `F` (구조체/값), `U` (UObject), `A` (Actor), `E` (enum)
- HLSL: UTF-8 (BOM 없음)

---

## 팀원

| 이름 | GitHub |
|------|--------|
| 심우진 | [@shimwoojin](https://github.com/shimwoojin) |
| 조현석 | [@JunHyeop3631](https://github.com/JunHyeop3631) |
| 박세영 | [@lin-ion](https://github.com/lin-ion) |
| 김효범 | [@HyoBeom](https://github.com/HyoBeom) |
| 강건우 | [@keonwookang0914](https://github.com/keonwookang0914) |
