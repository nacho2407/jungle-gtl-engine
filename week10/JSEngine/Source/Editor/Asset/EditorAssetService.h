#pragma once

#include "Core/CoreMinimal.h"

class UEditorEngine;
class UMaterial;
class UMaterialInstance;
class UMaterialInterface;
class UStaticMesh;
class USkeletalMesh;
class UTexture;

enum class EEditorAssetType : uint8
{
	StaticMesh,
	SkeletalMesh,
	Texture,
	Material,
	Font,
	Particle,
	Scene,
	Script,
};

struct FEditorAssetItem
{
	FString Path;
	FString DisplayName;
	EEditorAssetType Type = EEditorAssetType::StaticMesh;
};

class FEditorAssetService
{
public:
	void Initialize(UEditorEngine* InEditorEngine);
	void RefreshAssetDatabase();

	const TArray<FEditorAssetItem>& GetAssets(EEditorAssetType Type) const;

	const TArray<FString>& GetStaticMeshAssetPaths() const { return StaticMeshPaths; }
	const TArray<FString>& GetSkeletalMeshAssetPaths() const { return SkeletalMeshPaths; }
	const TArray<FString>& GetTextureAssetPaths() const { return TexturePaths; }
	const TArray<FString>& GetMaterialInterfaceNames() const { return MaterialInterfaceNames; }
	const TArray<FString>& GetFontNames() const { return FontNames; }
	const TArray<FString>& GetParticleNames() const { return ParticleNames; }

	UStaticMesh* LoadStaticMesh(const FString& Path) const;
	USkeletalMesh* LoadSkeletalMesh(const FString& Path) const;
	UTexture* LoadTexture(const FString& Path) const;
	UMaterialInterface* GetMaterialInterface(const FString& NameOrPath) const;
	UMaterialInterface* ResolveMaterialInterface(const FString& NameOrPath);
	UMaterialInterface* ResolveMaterialInterfaceByIndex(int32 MaterialIndex);
	UTexture* GetMaterialPreviewTexture(UMaterialInterface* Material) const;
	UMaterialInstance* CreateMaterialInstance(const FString& InstancePath, UMaterial* Parent) const;
	bool SaveMaterialInstance(const FString& InstancePath, UMaterialInstance* Instance) const;

private:
	static void AddUniquePath(TArray<FString>& Paths, const FString& Path);
	static void BuildItems(const TArray<FString>& Paths, EEditorAssetType Type, TArray<FEditorAssetItem>& OutItems);

private:
	UEditorEngine* EditorEngine = nullptr;

	TArray<FString> StaticMeshPaths;
	TArray<FString> SkeletalMeshPaths;
	TArray<FString> TexturePaths;
	TArray<FString> MaterialInterfaceNames;
	TArray<FString> FontNames;
	TArray<FString> ParticleNames;
	TArray<UMaterialInterface*> CachedMaterialInterfaces;
	TArray<bool> CachedMaterialInterfaceResolved;

	TArray<FEditorAssetItem> StaticMeshItems;
	TArray<FEditorAssetItem> SkeletalMeshItems;
	TArray<FEditorAssetItem> TextureItems;
	TArray<FEditorAssetItem> MaterialItems;
	TArray<FEditorAssetItem> FontItems;
	TArray<FEditorAssetItem> ParticleItems;
	TArray<FEditorAssetItem> EmptyItems;
};
