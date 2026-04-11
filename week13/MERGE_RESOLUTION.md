# Merge Resolution: main → refactor/renderpipeline

**Date**: 2026-04-11
**Base branch**: `refactor/renderpipeline` (e546f97)
**Merged from**: `main` (ac4fe75)

## Conflict Files

### 1. `KraftonEngine/Source/Engine/Render/Types/RenderTypes.h`

**원인**: refactor 브랜치에서 ERenderPass 12→8 통합 (Font/SubUV/Billboard/Translucent → AlphaBlend, Editor/Grid → EditorLines), main에서 Decal 패스 추가.

**해결**: Decal 패스는 `DepthReadOnly` depth state를 사용하므로 AlphaBlend에 합칠 수 없음. 별도 패스로 유지하여 9패스 구조로 확정:

```
Opaque → AlphaBlend → Decal → SelectionMask → EditorLines → PostProcess → GizmoOuter → GizmoInner → OverlayFont
```

### 2. `KraftonEngine/Source/Engine/Render/Pipeline/Renderer.cpp`

**원인**: `InitializePassRenderStates()` 테이블이 양쪽에서 변경.

**해결**: refactor 브랜치의 통합된 8패스 테이블에 main의 Decal 엔트리 추가:

```cpp
S[(uint32)E::Decal] = { EDepthStencilState::DepthReadOnly, EBlendState::AlphaBlend, ERasterizerState::SolidNoCull, TRIANGLELIST, true };
```

## Auto-merged Files (no conflict)

- `KraftonEngine/KraftonEngine.vcxproj` — Decal 소스 파일 추가
- `KraftonEngine/Source/Engine/Render/Pipeline/RenderConstants.h` — `ECBSlot::Decal = 5` 추가
- `KraftonEngine/Source/Engine/Render/Resource/ShaderManager.cpp` — `EShaderType::Decal` 추가

## New Files from main

- `DecalComponent.cpp/h` — 데칼 컴포넌트
- `DecalActor.cpp/h` — 데칼 액터
- `DecalSceneProxy.cpp/h` — 데칼 씬 프록시 (`ERenderPass::Decal` 사용)
- `DecalShader.hlsl` — 데칼 셰이더
- `ConvexVolume.cpp/h` — Frustum 관련 추가
- `BlendStateManager.cpp` — 블렌드 스테이트 수정
- `RenderResources.cpp` — 리소스 수정
