# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

A 3D Editor Engine built with DirectX 11 and ImGui on Windows. Features an actor/component architecture, WYSIWYG scene editor, ray-casting object selection, multi-scene support, and JSON-based scene serialization.

## Build

- **IDE**: Visual Studio 2022 (v143 toolset)
- **Solution**: `W3_Jungle_Team6.sln`
- **Configurations**: Debug / Release × Win32 / x64
- **Language**: C++17 (`/std:c++17`)
- Build from VS or via MSBuild:
  ```
  msbuild W3_Jungle_Team6.sln /p:Configuration=Debug /p:Platform=x64
  ```
- No package manager — all dependencies (ImGui, SimpleJSON) are bundled in-tree.
- No automated tests or CI pipeline.

## Architecture

### Object Hierarchy & RTTI

Custom reflection via `DECLARE_CLASS` / `DEFINE_CLASS` macros in `Object/Object.h`. All engine objects inherit from `UObject`. The inheritance chain:

```
UObject
  ├── AActor  (owns components, lives in UWorld)
  └── UActorComponent
        └── USceneComponent  (transform hierarchy)
              └── UPrimitiveComponent  (renderable + collidable)
                    ├── UCubeComponent
                    ├── USphereComponent
                    └── UPlaneComponent
```

`UObjectManager` (singleton) handles object creation and lifetime. `ObjectFactory<T>` provides polymorphic construction.

### Rendering (Engine/Render/)

DirectX 11 pipeline using a command-buffer pattern:

- **D3DDevice** — device, swapchain, rasterizer/depth states
- **RenderCollector** gathers draw commands from the scene into **RenderBus**, which sorts them by pass type (component, depth-less, editor, grid, outline, overlay)
- **Renderer** executes multi-pass rendering from the bus
- Shaders are HLSL (`ShaderW0.hlsl`), compiled at runtime via `Shader.h/cpp`

### Editor (Editor/)

- **EditorEngine** — main editor runtime; manages worlds, editor camera, scene switching
- **EditorMainPanel** — ImGui-based UI
- **EditorViewportClient** — 3D viewport, object picking via ray-cast
- **GizmoComponent** — transform manipulation handles

### Input & Physics

- `InputSystem` — static singleton, Windows message-based keyboard/mouse with drag detection
- `CollisionManager` — AABB collision detection
- Ray-triangle intersection for viewport object selection (`RayTypes.h`)

### Serialization

`FSceneSaveManager` reads/writes `.Scene` files (JSON via `SimpleJSON/json.hpp`) storing actors, components, transforms, and camera state. Save files live in `Saves/`.

## Key Include Paths

The project adds `Engine`, `.` (project root), and `Editor` to the include path. Headers are typically included relative to these roots.

## Commit Message Convention

Use the format `Action: description`. Action is a past-tense verb or keyword describing what was done.

Examples:
- `Add: Line Batcher for debug drawing`
- `Fix: picking object outline z-fighting`
- `Refactor: separate engine and editor init/shutdown`
- `Remove: hardcoded test cube spawn`
- `Update: EditorSettings full Save/Load support`
