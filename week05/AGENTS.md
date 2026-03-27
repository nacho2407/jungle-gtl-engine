# Repository Guidelines

## Project Structure & Module Organization
`Engine/Source` contains core runtime systems: `Core`, `Scene`, `Renderer`, `Math`, `Object`, `Actor`, `Component`, `Input`, and `Platform`. `Editor/Source` contains editor-only code such as `UI`, `Picking`, `Gizmo`, `Axis`, `Controller`, and `Pawn`. `Client/Source` is the game entry point. Runtime data lives under `Assets/` (`Scenes`, `Materials`), while `Content/` holds content files and `docs/` contains generated documentation.

The current architecture keeps rendering in `CCore`, with behavior split by `ViewportClient` and scene role split by `SceneContext` (`Game`, `Editor`, `Preview`).

## Build, Test, and Development Commands
- `msbuild KraftonJungleEngine.sln /p:Configuration=Debug /p:Platform=x64`
  Builds the full solution.
- `msbuild Engine\\Engine.vcxproj /p:Configuration=Debug /p:Platform=x64`
  Builds the engine DLL.
- `msbuild Editor\\Editor.vcxproj /p:Configuration=Debug /p:Platform=x64`
  Builds the editor and copies `Engine.dll`.
- `msbuild Client\\Client.vcxproj /p:Configuration=Debug /p:Platform=x64`
  Builds the client executable.
- `BuildEngine.bat`
  Convenience wrapper for engine-only builds.
- `GenerateProjectFiles.bat`
  Regenerates Visual Studio project files if project structure changes.

## Coding Style & Naming Conventions
Use tabs with width 4; `.editorconfig` sets `indent_style = tab`, `indent_size = 4`, and `charset = utf-8`. Keep edited source files in `CRLF` on Windows. Follow the project prefixes: `A` for actors, `U` for `UObject`-based classes, `F` for structs/value types, `C` for general classes, and `T` for templates. Use PascalCase for types and methods.

Prefer current include paths such as `Actor/Actor.h`, not older `Object/Actor/...` paths. When editing code manually, keep changes localized and consistent with existing DirectX 11 / LH / Z-up math assumptions.

## Testing Guidelines
There is no separate unit-test suite. Validation is build-first:
- build `Engine`, `Editor`, and `Client` in `Debug|x64`
- for editor-facing changes, smoke-test launch, viewport interaction, and asset/scene workflows
- for math/rendering changes, verify both `Editor` and `Client` still build cleanly

## Commit & Pull Request Guidelines
Recent history uses short Conventional Commit messages such as `feat:`, `fix:`, and `refactor:`. Branch syncs use explicit merge commits, e.g. `Merge branch 'main' into feature/...`. PRs should include:
- a concise summary of changed systems
- build results for `Engine`, `Editor`, and `Client`
- screenshots or short notes for Editor UI/viewport changes
- any scene, asset, or serialization compatibility impact
