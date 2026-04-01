# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

DirectX 11 3D scene editor engine built with C++ and ImGui. Actor/Component architecture with WYSIWYG editing, raycasting object selection, multi-scene management, and JSON serialization. Includes a standalone OBJ mesh viewer mode (`ObjViewDebug` build) for previewing static meshes.

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

# Release packaging (copies exe + shaders + assets to ReleaseBuild/)
./ReleaseBuild.bat
```

Output: `KraftonEngine/Bin/<Configuration>/KraftonEngine.exe`

Build configurations: `Debug`, `Release`, `ObjViewDebug` (x64/x86). ObjViewDebug defines `IS_OBJ_VIEWER=1` and excludes most Editor sources, launching `UObjViewerEngine` instead of `UEditorEngine`.

Requirements: Visual Studio 2022 (v143 toolset), Windows SDK with DirectX 11. All dependencies (ImGui, SimpleJSON, DirectXTK) are included in-tree. No package manager needed.

There are no tests or linting tools configured in this project.

## Architecture

### Object System (RTTI)

Custom runtime type information using `DECLARE_CLASS` / `DEFINE_CLASS` macros that build `FTypeInfo` chains. Supports `IsA<T>()` and `Cast<T>()`. `UObjectManager` singleton handles lifecycle and auto-naming (`ClassName_N`). `FName` is a pooled, case-insensitive string identifier.

### Rendering Pipeline

Multi-pass command buffer pattern: `RenderCollector` (scene traversal → `FRenderCommand`) → `RenderBus` (per-pass queuing) → `Renderer` (GPU submission via `FPassRenderState` lookup tables).

Render pass order: Opaque → Font → SubUV → Translucent → StencilMask → Outline → Editor → Grid → DepthLess.

Adding a new render pass = one entry in the `FPassRenderState` table. Batchers (Line, Font, SubUV) own their shaders and are flushed by the Renderer.

### Editor

`UEditorEngine` extends `UEngine`. Viewport supports ray-triangle picking, stencil-based selection outline, and gizmo transform manipulation. UI is entirely ImGui-based with docking widgets (Scene hierarchy, Property editor, Viewport overlay, Console, Stats).

### OBJ Viewer

Standalone mesh preview mode (`Source/ObjViewer/`). `UObjViewerEngine` subclasses `UEngine` and is activated via `IS_OBJ_VIEWER` preprocessor flag in `EngineLoop.cpp`. Components: `ObjViewerPanel` (ImGui mesh list + viewport UI), `ObjViewerRenderPipeline` (offscreen render target), `ObjViewerViewportClient` (orbit/pan/zoom camera).

### Serialization

`.Scene` files are JSON. `FSceneSaveManager` handles read/write of actor hierarchy, components, transforms, camera state. Editor settings persist to `Settings/editor.ini`.

## Code Conventions

- C++20 (x64), C++17 (Win32/x86)
- UTF-8 BOM for C++/H files, tab indentation (size 4)
- UTF-8 (no BOM) for HLSL shaders
- Include paths root at: `Source/Engine`, `Source`, `Source/Editor`, `Source/ObjViewer`, `ThirdParty`, `ThirdParty/ImGui`
- Headers use relative paths from these roots: `#include "Engine/Core/InputSystem.h"`
- Naming: `F` prefix for structs/data types (FVector, FName), `U` for UObject derivatives, `A` for Actors, `E` for enums
- HLSL shaders in `KraftonEngine/Shaders/` are compiled at runtime

## Key Source Layout

- `KraftonEngine/Source/Engine/` — core engine (Object, Math, Render, GameFramework, Component, Serialization, Core, Runtime)
- `KraftonEngine/Source/Editor/` — editor layer (UI widgets, viewport, selection, settings)
- `KraftonEngine/Source/ObjViewer/` — standalone mesh viewer (ObjViewerEngine, Panel, RenderPipeline, ViewportClient)
- `KraftonEngine/Shaders/` — 8 HLSL files + `Common/` subdirectory (`ConstantBuffers.hlsl`, `Functions.hlsl`, `VertexLayouts.hlsl`)
- `KraftonEngine/ThirdParty/` — ImGui and SimpleJSON (vendored)
- `KraftonEngine/Asset/` — font atlas, particle textures, default scene, MeshCache (prebuilt .bin meshes), StaticMesh
- `KraftonEngine/Data/` — mesh source files (.obj, .mtl, textures) organized by model name
