# Shader Refactor Plan

## Goal

This refactor is not only a shader folder cleanup. The main goal is to prevent Static Mesh, Skeletal Mesh, CPU Skinning, future GPU Skinning, Depth, Shadow, Selection, and Material logic from becoming mixed together.

The target architecture is:

```text
Material       = surface parameters and pixel shader choice
VertexFactory  = vertex input layout, vertex shader choice, mesh-specific resources
ShaderStage    = one compiled shader stage, such as VS, PS, or CS
ShaderProgram  = runtime combination of VS + PS
RenderPass     = render target, pass constants, and draw timing
```

This keeps mesh type differences out of Material and keeps material/surface logic out of mesh input code.

## Migration Policy

This refactor should be treated as an API break, not as a long compatibility transition.

The engine is still controlled by the team, so keeping old shader APIs alive would make the final structure harder to understand. The safer policy is:

```text
1. Introduce the new clean API.
2. Convert all engine call sites to the new API.
3. Run one explicit material/shader asset migration.
4. Delete old runtime APIs and old shader paths.
5. Let compile errors expose any remaining legacy usage.
```

Legacy path remapping should exist only as a migration tool or one-shot conversion table. It should not stay in `ResourceManager`, `MaterialSerializationService`, or runtime material loading after migration is done.

The final runtime should not support both old and new shader path conventions.

## Current Structure

The current shader object is a combined VS + PS program.

```cpp
struct FShader
{
    ID3D11VertexShader* VS;
    ID3D11PixelShader* PS;
    ID3D11InputLayout* InputLayout;
    ID3D11Buffer* ConstantBuffer;
};
```

The current ownership is roughly:

```text
UMaterial
  -> UShader
      -> VertexShader
      -> PixelShader
      -> InputLayout
      -> Material ConstantBuffer
```

The current draw flow is roughly:

```text
RenderCommand
  -> MeshBuffer
  -> Material

OpaqueRenderPass
  -> Material.Bind()
      -> Shader.Bind()
          -> VSSetShader
          -> PSSetShader
          -> IASetInputLayout
      -> Apply material parameters
  -> IASetVertexBuffers
  -> Draw
```

This worked for Static Mesh because a material could imply a complete shader pair. Skeletal Mesh and GPU Skinning break that assumption because the same material pixel shader should be usable with different vertex shaders.

Example:

```text
StaticMeshVS   + UberLitPS
SkeletalMeshVS + UberLitPS
```

The pixel shader is the same. Only the vertex input and vertex transform path are different.

## Current Pain Points

### 1. Material owns too much

Material currently points to a shader that includes both VS and PS. However, mesh type is not a material property.

Bad coupling:

```text
Material knows which complete VS + PS pair is used.
```

Better coupling:

```text
Material knows surface shading and material parameters.
RenderCommand/VertexFactory knows mesh vertex interpretation.
RenderPass combines them into a ShaderProgram.
```

### 2. InputLayout is in the wrong conceptual place

InputLayout depends on the vertex shader input signature and vertex buffer layout. It does not depend on the pixel shader.

So it should belong to:

```text
FVertexShader
```

not the combined shader program.

### 3. Future GPU Skinning needs a different VS

CPU Skinning path:

```text
FSkeletalMeshVertex
  -> CPU Skinning
  -> FSkeletalMeshVertex with skinned Position/Normal/Tangent
  -> SkeletalMeshVS in CPU-skinned/pass-through mode
  -> UberLitPS
```

GPU Skinning path:

```text
FSkeletalMeshVertex
  -> SkeletalMeshVS in GPU-skinned mode
  -> UberLitPS
```

Both CPU and GPU skinning keep the skeletal vertex format. CPU Skinning updates the renderable position, normal, and tangent on the CPU but keeps bone indices and weights available for debug views, later GPU Skinning, and skin-weight visualization.

If VS and PS stay tightly coupled, GPU Skinning will create duplicate shader files or duplicate permutations.

### 4. HLSL files are too flat

The shader folder currently mixes material, pass, post-process, compute, common constants, lighting, and shadow code in one flat area.

This increases include duplication and makes it unclear where shared code belongs.

## Target Structure

### Runtime Concepts

```text
FVertexShader
  - ID3D11VertexShader
  - ID3D11InputLayout
  - reflection info for vertex input

FPixelShader
  - ID3D11PixelShader
  - reflection info for textures and material constants

FComputeShader
  - ID3D11ComputeShader

FShaderProgram
  - FVertexShader*
  - FPixelShader*
  - Bind()

FVertexFactory
  - vertex layout policy
  - VS entry point policy
  - extra resource binding policy
```

Minimal C++ shape:

```cpp
struct FShaderStageKey
{
    FString FilePath;
    FString EntryPoint;
    FString Target; // vs_5_0, ps_5_0, cs_5_0
    uint32 PermutationKey = 0;
};

struct FVertexShader
{
    ID3D11VertexShader* Shader = nullptr;
    ID3D11InputLayout* InputLayout = nullptr;
};

struct FPixelShader
{
    ID3D11PixelShader* Shader = nullptr;
    FShaderReflectionInfo Reflection;
};

struct FShaderProgramKey
{
    FShaderStageKey VS;
    FShaderStageKey PS;
};

struct FShaderProgram
{
    FVertexShader* VS = nullptr;
    FPixelShader* PS = nullptr;

    void Bind(ID3D11DeviceContext* Context) const;
};
```

The bind behavior:

```cpp
void FShaderProgram::Bind(ID3D11DeviceContext* Context) const
{
    Context->IASetInputLayout(VS->InputLayout);
    Context->VSSetShader(VS->Shader, nullptr, 0);
    Context->PSSetShader(PS->Shader, nullptr, 0);
}
```

### Shader Resource Cache

Current:

```text
ShaderPath + PermutationKey -> UShader(VS + PS)
```

Target:

```text
VertexShaderKey -> FVertexShader
PixelShaderKey  -> FPixelShader
ComputeShaderKey -> FComputeShader
ProgramKey      -> FShaderProgram
```

This lets multiple programs reuse the same pixel shader:

```text
StaticMeshVS   + UberLitPS
SkeletalMeshVS + UberLitPS
```

### Material

Material should become pixel/material-side data.

Target responsibility:

```text
UMaterial
  - Material name/path
  - Pixel shader path/entry point, or material shader id
  - Material parameters
  - Texture bindings
  - Render states
```

Material should not decide whether the mesh is Static or Skeletal.

Current:

```text
Material -> ShaderPath = "Shaders/UberLit.hlsl"
```

Target:

```text
Material -> MaterialShader = "Shaders/Material/UberLit.hlsl"
Material -> PixelEntryPoint = "UberLitPS"
```

The render pass combines this with the vertex factory:

```text
StaticMeshVertexFactory   + UberLit material -> StaticMeshVS   + UberLitPS
SkeletalMeshVertexFactory + UberLit material -> SkeletalMeshVS + UberLitPS
```

### VertexFactory

VertexFactory is not exactly the VS itself. It is the rule that tells the renderer how to interpret a mesh vertex stream and which vertex shader path to use.

Responsibilities:

```text
1. Define vertex layout expectation
2. Select VS file and entry point
3. Bind additional mesh-specific GPU resources
4. Convert source vertex data into the PS input expected by the pass
```

Examples:

```text
StaticMeshVertexFactory
  - Vertex type: FNormalVertex
  - VS: StaticMeshVS
  - Extra resources: none

SkeletalMeshVertexFactory
  - Vertex type: FSkeletalMeshVertex
  - VS: SkeletalMeshVS
  - Extra resources: BoneMatrixBuffer
```

Pixel shaders receive common interpolated data:

```hlsl
struct FBasePassInterpolants
{
    float4 ClipPos : SV_POSITION;
    float3 WorldPos : TEXCOORD0;
    float3 WorldNormal : TEXCOORD1;
    float2 UV : TEXCOORD2;
    float4 WorldTangent : TEXCOORD5;
};
```

Static and Skeletal vertex shaders both output this shape for the base pass:

```text
StaticMeshVS
  FNormalVertex -> FBasePassInterpolants

SkeletalMeshVS
  FSkeletalMeshVertex + BoneMatrices -> FBasePassInterpolants
```

### RenderCommand

RenderCommand should carry the vertex factory type.

```cpp
enum class EVertexFactoryType : uint8
{
    StaticMesh,
    SkeletalMesh,
    ProceduralMesh,
    Primitive,
    Billboard,
    SubUV,
    Line,
    Text,
    Gizmo,
    Decal,
};

struct FRenderCommand
{
    FMeshBuffer* MeshBuffer = nullptr;
    UMaterialInterface* Material = nullptr;
    EVertexFactoryType VertexFactoryType = EVertexFactoryType::StaticMesh;

    // Optional for GPU skinning.
    const TArray<FMatrix>* SkinningMatrices = nullptr;
};
```

For CPU-skinned skeletal rendering, the command should still use the `SkeletalMesh` vertex factory. The vertex buffer remains `FSkeletalMeshVertex`, but the VS entry/define uses a pass-through path because the vertex has already been skinned on the CPU.

For GPU-skinned skeletal rendering, the command also uses the `SkeletalMesh` vertex factory, but binds bone matrices and lets the VS apply skinning.

## Mesh Buffer Direction

Mesh buffer should be vertex-stride based instead of StaticMesh-specific.

Current:

```cpp
void FMeshBuffer::CreateForStaticMesh(
    ID3D11Device* Device,
    const TArray<FNormalVertex>& Vertices,
    const TArray<uint32>& Indices);
```

Target:

```cpp
void FMeshBuffer::CreateImmutableVertexBuffer(
    ID3D11Device* Device,
    const void* VertexData,
    uint32 VertexCount,
    uint32 VertexStride,
    const TArray<uint32>& Indices);

void FMeshBuffer::CreateDynamicVertexBuffer(
    ID3D11Device* Device,
    uint32 MaxVertexCount,
    uint32 VertexStride,
    const TArray<uint32>& Indices);

void FMeshBuffer::UpdateDynamicVertexBuffer(
    ID3D11DeviceContext* Context,
    const void* VertexData,
    uint32 VertexCount);
```

Wrapper:

```cpp
template<typename TVertex>
void CreateImmutableVertices(
    ID3D11Device* Device,
    const TArray<TVertex>& Vertices,
    const TArray<uint32>& Indices);
```

This supports:

```text
StaticMesh      -> immutable FNormalVertex
ProceduralMesh  -> dynamic or recreate FNormalVertex
CPU Skinning    -> dynamic FSkeletalMeshVertex with skinned Position/Normal/Tangent
GPU Skinning    -> immutable/dynamic FSkeletalMeshVertex + bone buffer
```

## HLSL Folder Layout

Target folder layout:

```text
Shaders/
  Common/
    Common.hlsli
    FrameConstants.hlsli
    ObjectConstants.hlsli
    VertexCommon.hlsli
    Math.hlsli

  VertexFactory/
    StaticMeshVertexFactory.hlsli
    SkeletalMeshVertexFactory.hlsli
    BillboardVertexFactory.hlsli
    LineVertexFactory.hlsli

  Material/
    UberLit.hlsl
    MaterialCommon.hlsli

  Lighting/
    Lighting.hlsli
    ShadowCommon.hlsli
    ShadowSampling.hlsli

  Pass/
    DepthPrepass.hlsl
    Shadow.hlsl
    VSMShadow.hlsl
    SelectionMask.hlsl
    IDPick.hlsl
    IDPickDebug.hlsl
    Decal.hlsl
    Editor.hlsl
    Gizmo.hlsl
    Primitive.hlsl
    Line.hlsl
    Font.hlsl
    SubUV.hlsl
    ScreenOverlay.hlsl

  PostProcess/
    PostProcess.hlsl
    FXAA.hlsl
    Fog.hlsl
    Outline.hlsl
    Sandervistan.hlsl

  Compute/
    LightCullingCS.hlsl
    VSMBlurCS.hlsl
    HiZDownsampleCS.hlsl
```

The new `UberLit.hlsl` should be an entry-point file, not a giant owner of every helper function.

Example:

```hlsl
#include "../Common/FrameConstants.hlsli"
#include "../Common/ObjectConstants.hlsli"
#include "../Common/VertexCommon.hlsli"
#include "../VertexFactory/StaticMeshVertexFactory.hlsli"
#include "../VertexFactory/SkeletalMeshVertexFactory.hlsli"
#include "../Material/MaterialCommon.hlsli"
#include "../Lighting/Lighting.hlsli"
#include "../Lighting/ShadowCommon.hlsli"

FBasePassInterpolants StaticMeshVS(FStaticMeshVertexInput input)
{
    return BuildStaticMeshBasePassInterpolants(input);
}

FBasePassInterpolants SkeletalMeshVS(FSkeletalMeshVertexInput input)
{
    return BuildSkeletalMeshBasePassInterpolants(input);
}

FBasePassOutput UberLitPS(FBasePassInterpolants input)
{
    return EvaluateUberLit(input);
}
```

## FBX Material Handling

FBX material data fits naturally into the target Material model.

FBX data:

```text
DiffuseColor
SpecularColor
EmissiveColor
Shininess
DiffuseTexture
NormalTexture
SpecularTexture
EmissiveTexture
```

Engine target:

```text
FBX Material
  -> UMaterial or UMaterialInstance
      -> MaterialShader = UberLit
      -> Params
          DiffuseColor
          SpecularColor
          EmissiveColor
          Shininess
          DiffuseMap
          BumpMap
          SpecularMap
          EmissiveMap
```

The same imported material can be used by Static Mesh and Skeletal Mesh.

```text
Static FBX Mesh
  -> StaticMeshVertexFactory
  -> UberLitPS
  -> FBX material params

Skeletal FBX Mesh
  -> SkeletalMeshVertexFactory
  -> UberLitPS
  -> FBX material params
```

## Pass-Specific Vertex Factories

Base pass is not the only consumer of vertex data.

Different passes need different VS outputs:

```text
Base/Opaque Pass
  Needs WorldPos, WorldNormal, UV, Tangent

DepthPrepass
  Needs ClipPos

Shadow Pass
  Needs LightClipPos

Selection/ID Pass
  Needs ClipPos and optional UV alpha test
```

So the long-term naming may become:

```text
StaticMeshBasePassVS
SkeletalMeshBasePassVS

StaticMeshDepthVS
SkeletalMeshDepthVS

StaticMeshShadowVS
SkeletalMeshShadowVS

StaticMeshSelectionVS
SkeletalMeshSelectionVS
```

However, the first implementation can start with only Base/Opaque pass and add depth/shadow skeletal variants when GPU Skinning actually enters those passes.

## Permutation Strategy

Current `PermutationKey` mixes material, lighting, culling, and shadow features.

Target bit ownership should be documented even if it remains a single `uint32`.

Suggested ranges:

```text
bits 0-7    Material features
            diffuse map, normal map, specular map, emissive map, alpha mask

bits 8-10   Lighting model
            unlit, gouraud, lambert, blinn-phong, heatmap

bits 11-12  Light culling
            tiled, clustered

bits 13-17  Shadow features
            CSM, PSM, PCF, VSM, cascade visualization

bits 20-23  Vertex factory
            static, skeletal, billboard, line, text, gizmo

bits 24-27  Pass or debug flags if needed
```

Even better long-term:

```cpp
struct FShaderPermutationKey
{
    uint32 MaterialFeatures;
    uint32 LightingModel;
    uint32 ShadowFeatures;
    uint32 LightCulling;
    EVertexFactoryType VertexFactoryType;
    ERenderPassType PassType;
};
```

But the first refactor can keep a packed `uint32` if the bit ranges are clearly documented.

## Batch Execution Plan

Progress is tracked by completed batches. There are 10 batches total.

```text
Batch 1  - Add shader stage/program and vertex factory design types
Batch 2  - Generalize FMeshBuffer with immutable/dynamic vertex buffer APIs
Batch 3  - Add EVertexFactoryType to FRenderCommand and command builders
Batch 4  - Split shader stage compilation/cache from combined shader program
Batch 5  - Break Material API away from VS+PS binding
Batch 6  - Connect Base/Opaque pass to VertexFactory + Material PS program selection
Batch 7  - Migrate material/shader paths and remove old runtime shader path usage
Batch 8  - Reorganize HLSL folders and includes
Batch 9  - Extend Depth/Shadow/Selection paths for static/skeletal vertex factories
Batch 10 - Cleanup old APIs, unused macros, and update final documentation
```

Current progress:

```text
Current batch: Batch 10
Completed: 10 / 10
Progress: 100%
```

Batch 7 completion snapshot:

```text
Completed through Batch 7:
  - Opaque/Base pass no longer calls Material.Bind().
  - Opaque/Base pass now combines VertexFactory VS + Material PS through FShaderProgram.
  - FontBatcher, LineBatcher, SubUVBatcher now bind FShaderProgram directly.
  - Decal, DepthLess, Outline, Translucent passes now bind FShaderProgram directly.
  - UMaterial and UMaterialInstance no longer own or bind UShader.
  - Material now exposes pixel shader path/entry plus render-state and parameter binding.

Remaining legacy shader users:
  - Completed in Batch 10.
  - Renderer, render passes, batchers, and material binding no longer use UShader directly.

Remaining material asset migration:
  - Completed in Batch 8.
  - .mat files now serialize "PixelShader".
  - Material deserialization now reads "PixelShader" directly.
```

Batch 8 completion snapshot:

```text
HLSL folder structure:
  - Shaders/Common: shared .hlsli include files
  - Shaders/Material: UberLit and material/decal shaders
  - Shaders/UI: font, line, SubUV, screen overlay shaders
  - Shaders/EditorDebug: gizmo, selection, ID pick, primitive/editor debug shaders
  - Shaders/Depth: depth prepass shaders
  - Shaders/Shadow: shadow and VSM shaders
  - Shaders/PostProcess: FXAA, fog, light, outline, and screen postprocess passes
  - Shaders/Compute: compute shaders

Code/data migration:
  - C++ hardcoded shader paths now point at the new folders.
  - Material assets were migrated from "Shader" to "PixelShader".
  - Shared shader path constants and default PS entry-point mapping live in ShaderPaths.h.
  - Files touched by path migration were normalized back to valid UTF-8.

Verification:
  - Debug x64 MSBuild passed.
  - Build result: 0 warnings, 0 errors.
```

Batch 9 completion snapshot:

```text
Depth/Shadow/Selection migration:
  - FVertexFactoryDesc now stores pass-specific VS file paths.
  - DepthPrePass now binds FShaderProgram from VertexFactory depth VS + depth PS.
  - ShadowPass now binds FShaderProgram per command and per shadow-map permutation.
  - SelectionMaskRenderPass now binds FShaderProgram instead of UShader.
  - SelectionMask has a VSSkeletalMesh entry matching skeletal vertex layout.

Verification:
  - Debug x64 MSBuild passed.
  - Build result: 0 warnings, 0 errors.

Batch 10 completion snapshot:
  - PostProcess, FXAA, Fog, Light, Sandervistan, VSM conversion, ID pick, ID pick debug, and screen overlay paths now bind FShaderProgram directly.
  - Renderer shader preloading no longer loads VS+PS UShader programs for draw passes.
  - Pass render states no longer carry draw-pass shaders; passes and batchers bind their own program.
  - UShader, LoadShader, GetShader, EnsureShaderPermutation, and ReloadShader combined-program APIs were removed.
  - The unused Render/Resource/Shader.h and Shader.cpp files were removed from the project.
  - Shader hot reload now invalidates shader stage/program cache entries by file path so the next draw recompiles lazily.

Verification:
  - Debug x64 MSBuild passed.
  - Build result: 0 warnings, 0 errors.

Follow-up recommendation:
  - Run an editor/runtime smoke test because most shader programs compile lazily on first use.
  - If external team code still included Shader.h for UShader, migrate it to FShaderProgram/FShaderStageKey.
```

## Migration Plan

### Phase 1: Introduce the new shader API

1. Add `FVertexShader`, `FPixelShader`, `FComputeShader`, and `FShaderProgram`.
2. Move `InputLayout` ownership to `FVertexShader`.
3. Replace the old combined `FShader` meaning with explicit stage/program concepts.
4. Add a new program loading API instead of preserving old `LoadShader(file, vs, ps)` as a long-term wrapper.

Recommended API shape:

```cpp
FVertexShader* GetOrCreateVertexShader(const FShaderStageKey& Key, const FVertexLayoutDesc& Layout);
FPixelShader* GetOrCreatePixelShader(const FShaderStageKey& Key);
FComputeShader* GetOrCreateComputeShader(const FShaderStageKey& Key);

FShaderProgram* GetOrCreateProgram(
    const FShaderStageKey& VSKey,
    const FShaderStageKey& PSKey,
    const FVertexLayoutDesc& VertexLayout);
```

The old `LoadShader(File, VS, PS)` style should be removed after call sites are converted.

### Phase 2: Add stage cache and program cache

1. Compile VS and PS independently.
2. Cache stages by `FShaderStageKey`.
3. Cache programs by `FShaderProgramKey`.
4. Make program binding use `FShaderProgram::Bind`.

### Phase 3: Split Material binding

Current:

```text
Material.Bind()
  -> Shader.Bind()
  -> Material params bind
```

Target:

```text
RenderPass
  -> Program.Bind()
  -> Material.BindRenderStates()
  -> Material.BindParameters()
  -> VertexFactory.BindResources()
  -> Draw
```

Recommended Material API:

```cpp
class UMaterial : public UMaterialInterface
{
public:
    void SetPixelShader(const FString& ShaderPath, const FString& EntryPoint = "UberLitPS");

    const FString& GetPixelShaderPath() const;
    const FString& GetPixelShaderEntryPoint() const;

    void BindRenderStates(ID3D11DeviceContext* Context) const;
    void BindParameters(ID3D11DeviceContext* Context, const FPixelShader* PixelShader) const;
};
```

`Material.Bind()` should no longer bind VS or PS in the final structure.

### Phase 4: Introduce VertexFactoryType

1. Add `EVertexFactoryType`.
2. Add `VertexFactoryType` to `FRenderCommand`.
3. Static and procedural mesh commands use `StaticMesh`.
4. CPU-skinned skeletal commands use `SkeletalMesh` with CPU-skinned/pass-through VS mode.
5. GPU-skinned skeletal commands use `SkeletalMesh`.

### Phase 5: HLSL folder migration

1. Create new folders.
2. Move files in small groups.
3. Update includes.
4. Update every hardcoded shader path at the same time.
5. Do not keep old runtime shader paths after this phase.

### Phase 6: Material asset migration

Material assets currently store old shader paths. They should be converted once.

Migration conversion table:

```text
Shaders/UberLit.hlsl                    -> Shaders/Material/UberLit.hlsl
Shaders/ShaderLine.hlsl                 -> Shaders/UI/Line.hlsl
Shaders/ShaderFont.hlsl                 -> Shaders/UI/Font.hlsl
Shaders/ShaderSubUV.hlsl                -> Shaders/UI/SubUV.hlsl
Shaders/ScreenOverlay.hlsl              -> Shaders/UI/ScreenOverlay.hlsl
Shaders/Gizmo.hlsl                      -> Shaders/EditorDebug/Gizmo.hlsl
Shaders/Editor.hlsl                     -> Shaders/EditorDebug/Editor.hlsl
Shaders/Primitive.hlsl                  -> Shaders/EditorDebug/Primitive.hlsl
Shaders/SelectionMask.hlsl              -> Shaders/EditorDebug/SelectionMask.hlsl
Shaders/IDPick.hlsl                     -> Shaders/EditorDebug/IDPick.hlsl
Shaders/IDPickDebug.hlsl                -> Shaders/EditorDebug/IDPickDebug.hlsl
Shaders/DepthPrepass.hlsl               -> Shaders/Depth/DepthPrepass.hlsl
Shaders/Shadow.hlsl                     -> Shaders/Shadow/Shadow.hlsl
Shaders/VSMShadow.hlsl                  -> Shaders/Shadow/VSMShadow.hlsl
Shaders/OutlinePostProcess.hlsl         -> Shaders/PostProcess/Outline.hlsl
Shaders/Multipass/PostProcess.hlsl      -> Shaders/PostProcess/PostProcess.hlsl
Shaders/Multipass/FXAAPass.hlsl         -> Shaders/PostProcess/FXAAPass.hlsl
Shaders/Multipass/FogPass.hlsl          -> Shaders/PostProcess/FogPass.hlsl
Shaders/Multipass/LightPass.hlsl        -> Shaders/PostProcess/LightPass.hlsl
Shaders/Multipass/SandervistanPass.hlsl -> Shaders/PostProcess/SandervistanPass.hlsl
Shaders/LightCullingCS.hlsl             -> Shaders/Compute/LightCullingCS.hlsl
Shaders/OcclusionCull.hlsl              -> Shaders/Compute/OcclusionCull.hlsl
Shaders/HiZDownsample.hlsl              -> Shaders/Compute/HiZDownsample.hlsl
Shaders/HeightFogPixelShader.hlsl       -> Shaders/Atmosphere/HeightFogPixelShader.hlsl
Shaders/VSMBlurComputeShader.hlsl       -> Shaders/Shadow/VSMBlurComputeShader.hlsl
Shaders/HiZDownsample.hlsl              -> Shaders/Compute/HiZDownsampleCS.hlsl
```

This table is for a one-shot migration tool or script. Runtime loaders should not silently remap old paths after migration.

Expected migration result:

```text
Before migration:
  .mat files and hardcoded paths may reference Shaders/UberLit.hlsl

After migration:
  .mat files and hardcoded paths reference Shaders/Material/UberLit.hlsl
  Runtime no longer knows about Shaders/UberLit.hlsl
```

## Expected Side Effects

### 1. Existing material files may reference old shader paths

Mitigation:

```text
Run the migration conversion before committing the refactor.
Do not keep runtime fallback in MaterialSerializationService or ResourceManager.
Treat remaining old paths as errors.
```

### 2. Include paths may break

Moving HLSL files will break old include paths.

Mitigation:

```text
Move files in stages.
Use explicit relative includes.
Build after each group.
```

### 3. Shader reflection behavior may change

InputLayout creation moves from a combined shader object to vertex shader stage.

Mitigation:

```text
Reflect VS input only when compiling FVertexShader.
Reflect PS resources only when compiling FPixelShader.
```

### 4. Material.Bind behavior changes

Old code expects `Material.Bind()` to also bind VS/PS.

Mitigation:

```text
Remove this behavior.
Move render passes to Program.Bind + Material.BindRenderStates + Material.BindParameters.
Use compiler errors to find old Material.Bind call sites.
```

### 5. Vertex layout mismatch risk for GPU Skinning

`uint8 BoneIndices[4]` may be awkward for HLSL input reflection and DXGI format mapping.

Recommended GPU-friendly layout:

```cpp
struct FSkeletalMeshVertex
{
    FVector Position;
    FColor Color;
    FVector Normal;
    FVector2 UVs;
    FVector4 Tangent;
    uint32 BoneIndices[4];
    FVector4 BoneWeights;
};
```

Later optimization can pack bone indices into `R8G8B8A8_UINT`.

### 6. Program permutation count may initially rise

Splitting stages can expose existing permutation complexity.

Mitigation:

```text
Cache stages independently.
Only compile programs actually requested by a draw pass.
```

## Pros

1. Static and Skeletal meshes can share the same material.
2. GPU Skinning becomes a VS/VertexFactory problem instead of a material problem.
3. Pixel shader duplication is reduced.
4. InputLayout ownership becomes correct.
5. Depth, Shadow, and Selection skeletal support can be added consistently.
6. HLSL shared code has clear homes.
7. Material import from FBX remains clean and mesh-type independent.
8. The design remains understandable without copying Unreal Engine's full complexity.

## Cons

1. Initial refactor scope is large.
2. Team members need to learn three concepts: ShaderStage, ShaderProgram, VertexFactory.
3. Several systems are affected:

```text
UShader
FShaderResourceCache
UMaterial / UMaterialInstance
MaterialLoadService
MaterialSerializationService
ResourceManager
OpaqueRenderPass
DepthPrePass
ShadowPass
SelectionMaskRenderPass
ShaderHelper
Renderer shader preload code
Existing .mat assets
Hardcoded shader paths
```

4. The migration must be done carefully because old shader paths and old shader APIs are intentionally removed instead of kept alive.

## Things Not To Do Yet

Avoid these in this refactor:

```text
Material graph system
Full RHI abstraction
Full Unreal-style render dependency graph
Over-generalized pass framework
Automatic everything via reflection
```

Those would expand the scope too much for the current engine stage.

## Recommended Final Rule

Use this as the team rule:

```text
Material decides how a surface is shaded.
VertexFactory decides how mesh vertex data becomes shader input.
ShaderProgram is only the runtime pair of VS + PS.
RenderPass decides when and where the program draws.
```

If a change makes Material know whether a mesh is Static or Skeletal, the design is drifting in the wrong direction.
