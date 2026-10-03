# ThirdParty Build Policy

Third-party source code stays in this repository so the project can regenerate local static libraries when needed.

`JSEngine.vcxproj` calls the RmlUi and SoLoud prebuild scripts on every engine build, but those scripts should exit quickly when the generated library is already valid.

Static libraries are rebuilt only when one of these conditions is true:

- The expected `.lib` file is missing.
- The generated `.manifest` file is missing.
- Build settings in the manifest changed, such as configuration, platform, runtime, include paths, defines, or compile flags.
- One of the tracked third-party source or header files changed. The scripts compare SHA256 hashes, not local timestamps.

The manifest deliberately does not store local machine paths such as `ProjectDir` or the absolute `cl.exe` path. Moving the project folder or building on another computer should not force a rebuild when the checked-out source, build settings, and existing `.lib` are the same.

Generated outputs live under:

- `ThirdParty/RmlUi/Lib/<Platform>/<Configuration>/RmlUiCore.lib`
- `ThirdParty/SoLoud/Lib/<Platform>/<Configuration>/SoLoud.lib`

These generated `Lib/<Platform>/Debug` and `Lib/<Platform>/Release` outputs are currently ignored by the repository-wide Visual Studio ignore rules. A fresh checkout without these local libraries will build them once, then subsequent engine builds should skip them until the manifest changes.

Debug uses the Debug third-party libraries. Release, GameClientDebug, GameClientRelease, and ObjViewer configurations use the Release third-party libraries through `ThirdPartyBinaryConfiguration` in `JSEngine.vcxproj`.
