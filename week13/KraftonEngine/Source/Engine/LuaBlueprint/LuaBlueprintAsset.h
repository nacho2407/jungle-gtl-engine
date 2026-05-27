#pragma once

#include "Core/Types/CoreTypes.h"
#include "LuaBlueprint/LuaBlueprintTypes.h"
#include "Object/Object.h"

#include "Source/Engine/LuaBlueprint/LuaBlueprintAsset.generated.h"

class FLuaBlueprintCompiler;

UCLASS()class ULuaBlueprintAsset : public UObject
{
public:
    GENERATED_BODY()

    ULuaBlueprintAsset()           = default;
    ~ULuaBlueprintAsset() override = default;

    void           SetSourcePath(const FString& InPath) { SourcePath = InPath; }
    const FString& GetSourcePath() const { return SourcePath; }

    FLuaBlueprintNode* AddNode(ELuaBlueprintNodeType Type, const FName& DisplayName, float X, float Y);
    FLuaBlueprintPin*  AddPin(
        FLuaBlueprintNode&   Node,
        ELuaBlueprintPinKind Kind,
        ELuaBlueprintPinType PinType,
        const FName&         DisplayName
        );
    FLuaBlueprintLink*     AddLink(uint32 FromPinId, uint32 ToPinId);
    FLuaBlueprintVariable* AddVariable(const FName& Name, ELuaBlueprintPinType Type);

    static bool IsConcreteDataPinType(ELuaBlueprintPinType Type);
    static bool CanConvertPinTypes(ELuaBlueprintPinType FromType, ELuaBlueprintPinType ToType);
    static bool ArePinTypesCompatibleForLink(ELuaBlueprintPinType FromType, ELuaBlueprintPinType ToType);

    FLuaBlueprintNode* AddNodeOfType(ELuaBlueprintNodeType Type, float X, float Y);
    void               InitializeDefault();
    bool               Compile();

    bool RemoveNode(uint32 NodeId);
    bool RemoveLink(uint32 LinkId);
    bool RemoveInvalidLinks();
    bool CanLinkPins(uint32 PinAId, uint32 PinBId, uint32* OutFromPinId = nullptr, uint32* OutToPinId = nullptr) const;

    FLuaBlueprintNode*       FindNode(uint32 NodeId);
    const FLuaBlueprintNode* FindNode(uint32 NodeId) const;
    FLuaBlueprintPin*        FindPin(uint32 PinId);
    const FLuaBlueprintPin*  FindPin(uint32 PinId) const;
    const FLuaBlueprintLink* FindLinkToInput(uint32 InputPinId) const;
    const FLuaBlueprintLink* FindFirstLinkFromOutput(uint32 OutputPinId) const;

    const TArray<FLuaBlueprintNode>&       GetNodes() const { return Nodes; }
    TArray<FLuaBlueprintNode>&             GetMutableNodes() { return Nodes; }
    const TArray<FLuaBlueprintLink>&       GetLinks() const { return Links; }
    TArray<FLuaBlueprintLink>&             GetMutableLinks() { return Links; }
    const TArray<FLuaBlueprintVariable>&   GetVariables() const { return Variables; }
    TArray<FLuaBlueprintVariable>&         GetMutableVariables() { return Variables; }
    const TArray<FLuaBlueprintDiagnostic>& GetDiagnostics() const { return Diagnostics; }

    const FString& GetGeneratedLuaSource() const { return GeneratedLuaSource; }
    const FString& GetLastGoodGeneratedLuaSource() const { return LastGoodGeneratedLuaSource; }
    const FString& GetRuntimeLuaSource() const;
    bool           HasCompileErrors() const;
    bool           HasRunnableLuaSource() const;
    bool           WasLastCompileSuccessful() const { return bLastCompileSucceeded; }

    uint32 GetVersion() const { return Version; }
    uint32 GetRuntimeVersion() const { return RuntimeVersion; }

    void BumpVersion()
    {
        ++Version;
        ++RuntimeVersion;
        bCompileDirty = true;
    }

    // 에디터 시각 정보만 바뀐 경우 런타임 Lua reload 를 일으키지 않는다.
    void BumpEditorVersion()
    {
        ++Version;
    }

    void RefreshNodePinTypes(FLuaBlueprintNode& Node);
    void RefreshAllNodePinTypes();

    bool IsCompileDirty() const { return bCompileDirty; }

    void Serialize(FArchive& Ar) override;
    void AddReferencedObjects(FReferenceCollector& Collector) override;

private:
    uint32 AllocateId() { return NextId++; }
    void   SetCompileResult(const FString& InSource, TArray<FLuaBlueprintDiagnostic>&& InDiagnostics, bool bSuccess);
    void   ApplyResolvedPinTypesForLink(uint32 FromPinId, uint32 ToPinId);

    TArray<FLuaBlueprintNode>       Nodes;
    TArray<FLuaBlueprintLink>       Links;
    TArray<FLuaBlueprintVariable>   Variables;
    TArray<FLuaBlueprintDiagnostic> Diagnostics;
    uint32                          NextId  = 1;
    uint32                          Version = 0;
    FString                         SourcePath;
    FString                         GeneratedLuaSource;
    FString                         LastGoodGeneratedLuaSource;
    uint32                          RuntimeVersion              = 0;
    uint32                          LastCompiledCompilerVersion = 0;
    bool                            bCompileDirty               = true;
    bool                            bLastCompileSucceeded       = false;
};
