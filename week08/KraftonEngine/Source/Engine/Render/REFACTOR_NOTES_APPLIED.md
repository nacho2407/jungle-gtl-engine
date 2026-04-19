Applied a best-effort full-tree render refactor:
- reorganized Render folder into Core/Renderer/Pipelines/Passes/Builders/Commands/Collectors/Resource/RHI
- split low-level GPU wrappers under Render/RHI and higher-level managers/batches under Render/Resource
- moved DrawCommand and DrawCommandList under Render/Commands
- introduced builder folders and new RenderPassContext shape oriented around shared execution context
- updated Editor/Engine render call sites toward CreatePassContext + RunRootPipeline

This archive focuses on structural refactoring and include-path migration. Because the original project spans many coupled files and build metadata is not available here, compile-level integration may still require follow-up fixes in some implementation files.
