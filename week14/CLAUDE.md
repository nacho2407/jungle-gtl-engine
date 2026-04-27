# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

DirectX 11 3D scene editor engine built with C++ and ImGui. Actor/Component architecture with WYSIWYG editing, raycasting object selection, multi-scene management, and JSON serialization. Features forward rendering with shadow mapping (CSM for directional lights, cube-map shadows for point lights, atlas-based shadows for spot lights), clustered/tiled light culling, GPU occlusion culling, and FXAA post-processing. Includes a standalone OBJ mesh viewer mode (`ObjViewDebug` build) for previewing static meshes.

## Build Commands

```bash
# Build (x64 Debug) via MSBuild
msbuild KraftonEngine.sln /p:Configuration=Debug /p:Platform=x64

# Build (x64 Release)
msbuild KraftonEngine.sln /p:Configuration=Release /p:Platform=x64

# Build OBJ Viewer (x64) — standalone mesh preview tool
msbuild KraftonEngine.sln /p:Configuration=ObjViewDebug /p:Platform=x64

# Regenerate project files after adding/removing source files
python Scripts/GenerateProjectFiles.py

# Release packaging (copies exe + shaders + Data/ + assets to ReleaseBuild/)
./ReleaseBuild.bat

# Demo packaging
./DemoBuild.bat
```

Output: `KraftonEngine/Bin/<Configuration>/KraftonEngine.exe`

Build configurations: `Debug`, `Release`, `ObjViewDebug` (x64/x86). ObjViewDebug defines `IS_OBJ_VIEWER=1` and excludes most Editor sources, launching `UObjViewerEngine` instead of `UEditorEngine`.

Requirements: Visual Studio 2022 (v143 toolset), Windows SDK with DirectX 11. All dependencies (ImGui, SimpleJSON, DirectXTK) are included in-tree. No package manager needed.

There are no tests or linting tools configured in this project.

## Architecture

### Object System (RTTI)

Custom runtime type information using `DECLARE_CLASS` / `DEFINE_CLASS` macros that build `FTypeInfo` chains. Supports `IsA<T>()` and `Cast<T>()`. `UObjectManager` singleton handles lifecycle and auto-naming (`ClassName_N`). `FName` is a pooled, case-insensitive string identifier.

### Rendering Pipeline

Proxy-based render architecture: Components create SceneProxy objects (`PrimitiveSceneProxy`, `StaticMeshSceneProxy`, etc.) that live on the render side. The pipeline is driven by `IRenderPipeline` / `DefaultRenderPipeline`.

Key stages:
1. `RenderCollector` — scene traversal, builds `DrawCommand` list via `DrawCommandBuilder`
2. `RenderPassPipeline` — orchestrates registered render passes in order
3. Individual `RenderPassBase` subclasses — execute GPU submission (OpaquePass, PreDepthPass, DecalPass, AlphaBlendPass, SelectionMaskPass, EditorLinesPass, GizmoInnerPass/GizmoOuterPass, OverlayFontPass, PostProcessPass, FXAAPass, LightCullingPass)

Adding a new render pass = subclass `RenderPassBase` and register in `RenderPassRegistry`. State management via `PassRenderStateTable`, `BlendStateManager`, `RasterizerStateManager`, `SamplerStateManager`.

### Lighting & Shadows

- Light types: Ambient, Directional, Point, Spot (each with dedicated Component and Actor)
- Light culling: Tiled (`TileBasedLightCulling`) and Clustered (`ClusteredLightCuller`) with compute shaders (`TileLightCulling.hlsl`, `ClusterConstructCS.hlsl`, `LightCullingCS.hlsl`)
- Shadow mapping: `ShadowAtlasQuadTree` for atlas allocation, `ShadowDepth.hlsl` for depth rendering, `ShadowSampling.hlsli` for PCF sampling
- Cascaded Shadow Maps (CSM) for directional lights
- Point light shadows (cube-map based)
- Shadow options configured via `ProjectSettings` (`Settings/ProjectSettings.ini`)

### Culling

- GPU Occlusion Culling (`GPUOcclusionCulling`) with Hi-Z buffer (`HiZGenerate.hlsl`, `OcclusionTest.hlsl`)
- Octree spatial partitioning (`Octree.cpp`, `SpatialPartition.cpp`) for broad-phase shadow caster culling
- BVH for ray picking (`MeshTriangleBVH`, `WorldPrimitivePickingBVH`)

### Editor

`UEditorEngine` extends `UEngine` with `EditorRenderPipeline`. Viewport supports ray-triangle picking, stencil-based selection outline, and gizmo transform manipulation. UI is entirely ImGui-based with docking widgets (Scene hierarchy, Property editor, Content Browser, Material Inspector, Viewport overlay, Play toolbar, Console, Stats). Notification toast system for shader hot-reload errors. PIE (Play-In-Editor) support.

### OBJ Viewer

Standalone mesh preview mode (`Source/ObjViewer/`). `UObjViewerEngine` subclasses `UEngine` and is activated via `IS_OBJ_VIEWER` preprocessor flag in `EngineLoop.cpp`. Components: `ObjViewerPanel` (ImGui mesh list + viewport UI), `ObjViewerRenderPipeline` (offscreen render target), `ObjViewerViewportClient` (orbit/pan/zoom camera).

### Serialization

`.Scene` files are JSON. `FSceneSaveManager` handles read/write of actor hierarchy, components, transforms, camera state. Editor settings persist to `Settings/editor.ini`. Project-wide settings in `Settings/ProjectSettings.ini`.

## Code Conventions

- C++20 (x64), C++17 (Win32/x86)
- UTF-8 BOM for C++/H files, tab indentation (size 4)
- UTF-8 (no BOM) for HLSL shaders (`.hlsl` for shader files, `.hlsli` for include headers)
- Include paths root at: `Source/Engine`, `Source`, `Source/Editor`, `Source/ObjViewer`, `ThirdParty`, `ThirdParty/ImGui`
- Headers use relative paths from these roots: `#include "Engine/Core/InputSystem.h"`
- Naming: `F` prefix for structs/data types (FVector, FName), `U` for UObject derivatives, `A` for Actors, `E` for enums
- HLSL shaders in `KraftonEngine/Shaders/` are compiled at runtime with hot-reload support

## Key Source Layout

- `KraftonEngine/Source/Engine/` — core engine
  - `Collision/` — Octree, BVH, ray utilities (SIMD), OBB, spatial partitioning
  - `Component/` — PrimitiveComponent, StaticMeshComponent, Billboard, SubUV, Text, Pendulum/Projectile movement
  - `Component/Light/` — LightComponentBase, Directional, Point, Spot, Ambient light components
  - `Core/` — EngineTypes, Log, Notification, Singleton, TickFunction
  - `Debug/` — DebugDrawQueue, DrawDebugHelpers
  - `GameFramework/` — Level, WorldContext, Actor types (StaticMesh, HeightFog, Light actors)
  - `Input/` — InputSystem
  - `Materials/` — MaterialManager
  - `Math/` — Vector, Quat, Rotator, Transform, MathUtils
  - `Mesh/` — ObjImporter, ObjManager, StaticMesh, StaticMeshAsset, MeshSimplifier
  - `Object/` — UObject, FName, ObjectIterator, UUIDGenerator
  - `Platform/` — ConsoleHelper
  - `Profiling/` — performance instrumentation
  - `Render/` — rendering subsystem
    - `Command/` — DrawCommand, DrawCommandList, DrawCommandBuilder
    - `Culling/` — GPUOcclusionCulling, TileBasedLightCulling, ClusteredLightCuller
    - `Device/` — D3DDevice (DX11 device wrapper)
    - `Geometry/` — FontGeometry, LineGeometry
    - `Pipeline/` — IRenderPipeline, DefaultRenderPipeline, RenderCollector
    - `Proxy/` — PrimitiveSceneProxy, StaticMeshSceneProxy, Billboard/Decal/Gizmo/SubUV/TextRender proxies
    - `RenderPass/` — OpaquePass, PreDepthPass, DecalPass, AlphaBlendPass, FXAAPass, PostProcessPass, SelectionMaskPass, EditorLinesPass, GizmoInner/OuterPass, OverlayFontPass, LightCullingPass, RenderPassRegistry, RenderPassPipeline
    - `RenderState/` — BlendStateManager, RasterizerStateManager, SamplerStateManager, PassRenderStateTable
    - `Resource/` — Buffer, MeshBufferManager
    - `Scene/` — FScene, SceneEnvironment
    - `Shader/` — Shader, ShaderInclude
    - `Shadow/` — ShadowAtlasQuadTree
    - `Types/` — FrameContext, RenderTypes, RenderConstants, ViewTypes, VertexTypes, FogParams, ForwardLightData, ShadowSettings, MaterialTextureSlot, LODContext
  - `Resource/` — resource management
  - `Runtime/` — Engine, EngineLoop, Launch, WindowsApplication, WindowsWindow
  - `Serialization/` — scene save/load
  - `Texture/` — texture loading/management
  - `UI/` — UI utilities
  - `Viewport/` — viewport management
- `KraftonEngine/Source/Editor/` — editor layer
  - `EditorEngine`, `EditorRenderPipeline`
  - `PIE/` — Play-In-Editor
  - `Selection/` — actor selection
  - `Settings/` — ProjectSettings, editor settings
  - `Subsystem/` — editor subsystems
  - `UI/` — ImGui widgets (Viewport, Stat, PlayToolbar, MaterialInspector, ContentBrowser, DragSource, NotificationToast, ImGuiSetting)
  - `Viewport/` — LevelEditorViewportClient
- `KraftonEngine/Source/ObjViewer/` — standalone mesh viewer
- `KraftonEngine/Shaders/` — HLSL shader files
  - `Common/` — shared includes (`ConstantBuffers.hlsli`, `Functions.hlsli`, `VertexLayouts.hlsli`, `ForwardLighting.hlsli`, `ForwardLightData.hlsli`, `ShadowSampling.hlsli`, `SystemResources.hlsli`, `SystemSamplers.hlsli`)
  - `Editor/` — Editor.hlsl, Gizmo.hlsl
  - `Geometry/` — Primitive.hlsl, Decal.hlsl, UberLit.hlsl
  - `Lighting/` — HiZGenerate.hlsl, OcclusionTest.hlsl, TileLightCulling.hlsl, ShadowDepth.hlsl, ClusterConstructCS.hlsl, LightCullingCS.hlsl, ClusterHeatMap.hlsl
  - `PostProcess/` — FXAA.hlsl, HeightFog.hlsl, Outline.hlsl, SceneDepth.hlsl, SceneNormal.hlsl, NormalView.hlsl, LightCulling.hlsl
  - `UI/` — Billboard.hlsl, Font.hlsl, OverlayFont.hlsl, SubUV.hlsl
- `KraftonEngine/ThirdParty/` — ImGui and SimpleJSON (vendored)
- `KraftonEngine/Asset/` — font atlas, particle textures, default scene, MeshCache (prebuilt .bin meshes), StaticMesh
- `KraftonEngine/Data/` — mesh source files (.obj, .mtl, textures) organized by model name
