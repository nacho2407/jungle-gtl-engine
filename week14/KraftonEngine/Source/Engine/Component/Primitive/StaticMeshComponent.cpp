#include "Component/Primitive/StaticMeshComponent.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>
#include "Object/Reflection/ObjectFactory.h"
#include "Component/Primitive/SkinnedMeshComponent.h"
#include "Core/Types/PropertyTypes.h"
#include "Engine/Platform/Paths.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "Collision/Ray/RayUtils.h"
#include "Mesh/Static/StaticMeshAsset.h"
#include "Engine/Runtime/Engine.h"
#include "Render/Shader/ShaderManager.h"
#include "Texture/Texture2D.h"
#include "Render/Proxy/StaticMeshSceneProxy.h"
#include "Render/Proxy/PrimitiveSceneProxy.h"
#include "Serialization/Archive.h"
#include "SimpleJSON/json.hpp"

namespace
{
	constexpr float StaticMeshAttachMatrixDecomposeTolerance = 1.0e-6f;

	FTransform MatrixToStaticMeshAttachTransform(const FMatrix& Matrix)
	{
		FTransform Result;
		Result.Location = Matrix.GetLocation();
		Result.Scale = Matrix.GetScale();

		FMatrix RotationMatrix = Matrix;
		RotationMatrix.M[3][0] = 0.0f;
		RotationMatrix.M[3][1] = 0.0f;
		RotationMatrix.M[3][2] = 0.0f;
		RotationMatrix.M[3][3] = 1.0f;

		if (std::fabs(Result.Scale.X) > StaticMeshAttachMatrixDecomposeTolerance)
		{
			RotationMatrix.M[0][0] /= Result.Scale.X;
			RotationMatrix.M[0][1] /= Result.Scale.X;
			RotationMatrix.M[0][2] /= Result.Scale.X;
		}

		if (std::fabs(Result.Scale.Y) > StaticMeshAttachMatrixDecomposeTolerance)
		{
			RotationMatrix.M[1][0] /= Result.Scale.Y;
			RotationMatrix.M[1][1] /= Result.Scale.Y;
			RotationMatrix.M[1][2] /= Result.Scale.Y;
		}

		if (std::fabs(Result.Scale.Z) > StaticMeshAttachMatrixDecomposeTolerance)
		{
			RotationMatrix.M[2][0] /= Result.Scale.Z;
			RotationMatrix.M[2][1] /= Result.Scale.Z;
			RotationMatrix.M[2][2] /= Result.Scale.Z;
		}

		Result.Rotation = RotationMatrix.ToQuat().GetNormalized();
		return Result;
	}

	float ReadSocketJsonNumber(const json::JSON& Value, float DefaultValue = 0.0f)
	{
		bool bOk = false;
		const double FloatValue = Value.ToFloat(bOk);
		if (bOk) return static_cast<float>(FloatValue);

		const long IntValue = Value.ToInt(bOk);
		if (bOk) return static_cast<float>(IntValue);

		return DefaultValue;
	}

	bool ReadBlenderMatrixAsEngineRowMatrix(json::JSON& MatrixJson, FMatrix& OutMatrix)
	{
		if (MatrixJson.JSONType() != json::JSON::Class::Array || MatrixJson.length() < 4)
		{
			return false;
		}

		float BlenderMatrix[4][4] = {};
		for (int32 Row = 0; Row < 4; ++Row)
		{
			json::JSON& RowJson = MatrixJson[static_cast<unsigned>(Row)];
			if (RowJson.JSONType() != json::JSON::Class::Array || RowJson.length() < 4)
			{
				return false;
			}

			for (int32 Col = 0; Col < 4; ++Col)
			{
				BlenderMatrix[Row][Col] = ReadSocketJsonNumber(RowJson[static_cast<unsigned>(Col)]);
			}
		}

		for (int32 Row = 0; Row < 4; ++Row)
		{
			for (int32 Col = 0; Col < 4; ++Col)
			{
				OutMatrix.M[Row][Col] = BlenderMatrix[Col][Row];
			}
		}

		return true;
	}

	std::filesystem::path ResolveAttachSocketJsonPath(const FString& Path)
	{
		std::filesystem::path SocketPath(FPaths::ToWide(Path));
		if (SocketPath.is_absolute())
		{
			return SocketPath.lexically_normal();
		}

		return (std::filesystem::path(FPaths::RootDir()) / SocketPath).lexically_normal();
	}
}

FPrimitiveSceneProxy* UStaticMeshComponent::CreateSceneProxy()
{
	return new FStaticMeshSceneProxy(this);
}

void UStaticMeshComponent::SetStaticMesh(UStaticMesh* InMesh)
{
	const bool bHadBody = GetBodyInstance() != nullptr;

	if (bHadBody)
	{
		DestroyPhysicsState();
	}

	StaticMesh = InMesh;
	if (InMesh)
	{
		// 메시 에셋 PathFileName 은 Import 시점에 절대 경로로 들어올 수 있어
		// 컴포넌트 단계에서 프로젝트 상대 경로로 정규화한다 (씬 직렬화 안정성).
		StaticMeshPath = FPaths::MakeProjectRelative(InMesh->GetAssetPathFileName());
		const TArray<FStaticMaterial>& DefaultMaterials = StaticMesh->GetStaticMaterials();

		OverrideMaterials.resize(DefaultMaterials.size());
		MaterialSlots.resize(DefaultMaterials.size());

		for (int32 i = 0; i < (int32)DefaultMaterials.size(); ++i)
		{
			OverrideMaterials[i] = DefaultMaterials[i].MaterialInterface;

			if (OverrideMaterials[i])
				MaterialSlots[i] = OverrideMaterials[i]->GetAssetPathFileName();
			else
				MaterialSlots[i] = "None";
		}
	}
	else
	{
		StaticMeshPath = "None";
		OverrideMaterials.clear();
		MaterialSlots.clear();
	}
	CacheLocalBounds();
	MarkRenderStateDirty();
	MarkWorldBoundsDirty();

	if (bHadBody)
	{
		CreatePhysicsState();
	}
}

void UStaticMeshComponent::CacheLocalBounds()
{
	bHasValidBounds = false;
	if (!StaticMesh) return;
	FStaticMesh* Asset = StaticMesh->GetStaticMeshAsset();
	if (!Asset || Asset->Vertices.empty()) return;

	// FStaticMesh에 이미 계산된 바운드가 있으면 그대로 사용
	if (!Asset->bBoundsValid)
	{
		Asset->CacheBounds();
	}

	CachedLocalCenter = Asset->BoundsCenter;
	CachedLocalExtent = Asset->BoundsExtent;
	bHasValidBounds = Asset->bBoundsValid;
}

bool UStaticMeshComponent::GetCachedLocalBounds(FVector& OutCenter, FVector& OutExtent) const
{
	if (!bHasValidBounds)
	{
		return false;
	}

	OutCenter = CachedLocalCenter;
	OutExtent = CachedLocalExtent;
	return true;
}

void UStaticMeshComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction)
{
	UMeshComponent::TickComponent(DeltaTime, TickType, ThisTickFunction);
	UpdateBoneAttachment();
}

void UStaticMeshComponent::UpdateBoneAttachment()
{
	if (!bAttachToOwnerMeshBone || bEditAttachOffset) return;

	FString EffectiveBoneName = AttachBoneName;
	FString SocketBoneName;
	FMatrix SocketToObjectMatrix;
	if (TryGetSocketOffsetMatrix(SocketBoneName, SocketToObjectMatrix) && !SocketBoneName.empty())
	{
		EffectiveBoneName = SocketBoneName;
	}

	if (EffectiveBoneName.empty()) return;

	USkinnedMeshComponent* TargetMeshComponent = ResolveAttachMeshComponent();
	if (!TargetMeshComponent) return;

	FMatrix BoneWorldMatrix;
	if (!TargetMeshComponent->GetBoneWorldMatrixByName(EffectiveBoneName, BoneWorldMatrix))
	{
		return;
	}

	const FMatrix AttachLocalMatrix = BuildAttachLocalMatrix(TargetMeshComponent, EffectiveBoneName);
	const FMatrix SocketWorldMatrix = AttachLocalMatrix * BoneWorldMatrix;

	FTransform TargetRelativeTransform = MatrixToStaticMeshAttachTransform(SocketWorldMatrix);
	if (USceneComponent* ParentComponent = GetParent())
	{
		const FMatrix TargetRelativeMatrix = SocketWorldMatrix * ParentComponent->GetWorldMatrix().GetInverse();
		TargetRelativeTransform = MatrixToStaticMeshAttachTransform(TargetRelativeMatrix);
	}

	SetRelativeTransform(TargetRelativeTransform);
}

bool UStaticMeshComponent::TryGetSocketOffsetMatrix(
	FString& OutBoneName,
	FMatrix& OutSocketToObjectMatrix,
	FMatrix* OutObjectWorldMatrixAtExport,
	bool* bOutHasObjectWorldMatrixAtExport,
	bool* bOutIsSocketLocalMeshSpace)
{
	OutBoneName = "";
	OutSocketToObjectMatrix = FMatrix::Identity;
	if (OutObjectWorldMatrixAtExport)
	{
		*OutObjectWorldMatrixAtExport = FMatrix::Identity;
	}
	if (bOutHasObjectWorldMatrixAtExport)
	{
		*bOutHasObjectWorldMatrixAtExport = false;
	}
	if (bOutIsSocketLocalMeshSpace)
	{
		*bOutIsSocketLocalMeshSpace = false;
	}

	if (AttachSocketOffsetPath.empty())
	{
		return false;
	}

	if (bSocketOffsetCacheResolved &&
		CachedAttachSocketOffsetPath == AttachSocketOffsetPath &&
		CachedAttachSocketObjectName == AttachSocketObjectName)
	{
		if (bCachedSocketOffsetValid)
		{
			OutBoneName = CachedSocketTargetBoneName;
			OutSocketToObjectMatrix = CachedSocketToObjectMatrix;
			if (OutObjectWorldMatrixAtExport)
			{
				*OutObjectWorldMatrixAtExport = CachedObjectWorldMatrixAtExport;
			}
			if (bOutHasObjectWorldMatrixAtExport)
			{
				*bOutHasObjectWorldMatrixAtExport = bCachedHasObjectWorldMatrixAtExport;
			}
			if (bOutIsSocketLocalMeshSpace)
			{
				*bOutIsSocketLocalMeshSpace = bCachedIsSocketLocalMeshSpace;
			}
		}
		return bCachedSocketOffsetValid;
	}

	bSocketOffsetCacheResolved = true;
	bCachedSocketOffsetValid = false;
	CachedAttachSocketOffsetPath = AttachSocketOffsetPath;
	CachedAttachSocketObjectName = AttachSocketObjectName;
	CachedSocketTargetBoneName = "";
	CachedSocketToObjectMatrix = FMatrix::Identity;
	CachedObjectWorldMatrixAtExport = FMatrix::Identity;
	bCachedHasObjectWorldMatrixAtExport = false;
	bCachedIsSocketLocalMeshSpace = false;

	const std::filesystem::path ResolvedPath = ResolveAttachSocketJsonPath(AttachSocketOffsetPath);
	std::ifstream File(ResolvedPath, std::ios::binary);
	if (!File.is_open())
	{
		return false;
	}

	std::stringstream Buffer;
	Buffer << File.rdbuf();
	json::JSON Root = json::JSON::Load(Buffer.str());
	if (Root.JSONType() != json::JSON::Class::Object)
	{
		return false;
	}

	FString TargetBoneName = Root.hasKey("target_bone")
		? Root["target_bone"].ToString()
		: AttachBoneName;

	const FString SocketSpace = Root.hasKey("space") ? Root["space"].ToString() : "";
	const FString AttachMode = Root.hasKey("attach_mode") ? Root["attach_mode"].ToString() : "";
	bCachedIsSocketLocalMeshSpace =
		SocketSpace == "socket_local_mesh" ||
		AttachMode == "set_weapon_world_to_socket_world";

	if (!Root.hasKey("objects") || Root["objects"].JSONType() != json::JSON::Class::Object)
	{
		return false;
	}

	json::JSON& ObjectsJson = Root["objects"];
	FString ObjectName = AttachSocketObjectName;
	if (ObjectName.empty())
	{
		for (auto& Pair : ObjectsJson.ObjectRange())
		{
			ObjectName = Pair.first;
			break;
		}
	}

	if (ObjectName.empty() || !ObjectsJson.hasKey(ObjectName))
	{
		return false;
	}

	json::JSON& ObjectJson = ObjectsJson[ObjectName];
	if (!ObjectJson.hasKey("socket_to_object_matrix"))
	{
		return false;
	}

	FMatrix SocketToObjectMatrix = FMatrix::Identity;
	if (!ReadBlenderMatrixAsEngineRowMatrix(ObjectJson["socket_to_object_matrix"], SocketToObjectMatrix))
	{
		return false;
	}

	CachedSocketTargetBoneName = TargetBoneName;
	CachedSocketToObjectMatrix = SocketToObjectMatrix;

	if (ObjectJson.hasKey("object_world_matrix_at_export"))
	{
		FMatrix ObjectWorldMatrixAtExport = FMatrix::Identity;
		if (ReadBlenderMatrixAsEngineRowMatrix(ObjectJson["object_world_matrix_at_export"], ObjectWorldMatrixAtExport))
		{
			CachedObjectWorldMatrixAtExport = ObjectWorldMatrixAtExport;
			bCachedHasObjectWorldMatrixAtExport = true;
		}
	}

	bCachedSocketOffsetValid = true;

	OutBoneName = CachedSocketTargetBoneName;
	OutSocketToObjectMatrix = CachedSocketToObjectMatrix;
	if (OutObjectWorldMatrixAtExport)
	{
		*OutObjectWorldMatrixAtExport = CachedObjectWorldMatrixAtExport;
	}
	if (bOutHasObjectWorldMatrixAtExport)
	{
		*bOutHasObjectWorldMatrixAtExport = bCachedHasObjectWorldMatrixAtExport;
	}
	if (bOutIsSocketLocalMeshSpace)
	{
		*bOutIsSocketLocalMeshSpace = bCachedIsSocketLocalMeshSpace;
	}
	return true;
}

FMatrix UStaticMeshComponent::BuildAttachLocalMatrix(USkinnedMeshComponent* TargetMeshComponent, const FString& EffectiveBoneName)
{
	FMatrix SocketToObjectMatrix = FMatrix::Identity;
	FMatrix ObjectWorldMatrixAtExport = FMatrix::Identity;
	bool bHasObjectWorldMatrixAtExport = false;
	bool bIsSocketLocalMeshSpace = false;
	FString SocketBoneName;
	TryGetSocketOffsetMatrix(
		SocketBoneName,
		SocketToObjectMatrix,
		&ObjectWorldMatrixAtExport,
		&bHasObjectWorldMatrixAtExport,
		&bIsSocketLocalMeshSpace);

	const FTransform ManualOffsetTransform(AttachOffsetLocation, AttachOffsetRotation, AttachOffsetScale);
	const FMatrix ManualOffsetMatrix = ManualOffsetTransform.ToMatrix();
	const FMatrix DesiredSocketLocalMatrix = ManualOffsetMatrix * SocketToObjectMatrix;

	if (!bUseBakedBindPoseCorrection || !TargetMeshComponent || EffectiveBoneName.empty())
	{
		return DesiredSocketLocalMatrix;
	}

	if (bIsSocketLocalMeshSpace)
	{
		return DesiredSocketLocalMatrix;
	}

	if (bHasObjectWorldMatrixAtExport)
	{
		return ObjectWorldMatrixAtExport.GetInverse() * DesiredSocketLocalMatrix;
	}

	FMatrix ReferenceBoneGlobalMatrix;
	if (!TargetMeshComponent->GetReferenceBoneGlobalMatrixByName(EffectiveBoneName, ReferenceBoneGlobalMatrix))
	{
		return DesiredSocketLocalMatrix;
	}

	const FMatrix BakedObjectBindMatrix = SocketToObjectMatrix * ReferenceBoneGlobalMatrix;
	return BakedObjectBindMatrix.GetInverse() * DesiredSocketLocalMatrix;
}

void UStaticMeshComponent::InvalidateAttachSocketCache()
{
	bSocketOffsetCacheResolved = false;
	bCachedSocketOffsetValid = false;
	CachedAttachSocketOffsetPath = "";
	CachedAttachSocketObjectName = "";
	CachedSocketTargetBoneName = "";
	CachedSocketToObjectMatrix = FMatrix::Identity;
	CachedObjectWorldMatrixAtExport = FMatrix::Identity;
	bCachedHasObjectWorldMatrixAtExport = false;
	bCachedIsSocketLocalMeshSpace = false;
}

USkinnedMeshComponent* UStaticMeshComponent::ResolveAttachMeshComponent()
{
	if (!AttachMeshComponent)
	{
		if (AActor* TargetActor = ResolveAttachTargetActor())
		{
			AttachMeshComponent = TargetActor->GetComponentByClass<USkinnedMeshComponent>();
		}
	}

	return AttachMeshComponent;
}

AActor* UStaticMeshComponent::ResolveAttachTargetActor()
{
	if (AttachTargetActor && AttachTargetActorName == AttachTargetActor->GetName())
	{
		return AttachTargetActor;
	}

	AttachTargetActor = nullptr;

	if (AttachTargetActorName.empty())
	{
		AttachTargetActor = GetOwner();
		return AttachTargetActor;
	}

	UWorld* World = GetWorld();
	if (!World) return nullptr;

	for (AActor* Actor : World->GetActors())
	{
		if (Actor && Actor->GetName() == AttachTargetActorName)
		{
			AttachTargetActor = Actor;
			return AttachTargetActor;
		}
	}

	return nullptr;
}

void UStaticMeshComponent::CaptureAttachOffsetFromCurrentTransform()
{
	bCaptureCurrentAttachOffset = false;

	if (!bAttachToOwnerMeshBone || AttachBoneName.empty()) return;

	USkinnedMeshComponent* TargetMeshComponent = ResolveAttachMeshComponent();
	if (!TargetMeshComponent) return;

	FMatrix BoneWorldMatrix;
	if (!TargetMeshComponent->GetBoneWorldMatrixByName(AttachBoneName, BoneWorldMatrix))
	{
		return;
	}

	const FMatrix OffsetMatrix = GetWorldMatrix() * BoneWorldMatrix.GetInverse();
	const FTransform OffsetTransform = MatrixToStaticMeshAttachTransform(OffsetMatrix);

	AttachOffsetLocation = OffsetTransform.Location;
	AttachOffsetRotation = OffsetTransform.Rotation.ToRotator();
	AttachOffsetScale = OffsetTransform.Scale;
	bEditAttachOffset = false;

	UpdateBoneAttachment();
}

UStaticMesh* UStaticMeshComponent::GetStaticMesh() const
{
	return StaticMesh;
}

void UStaticMeshComponent::SetMaterial(int32 ElementIndex, UMaterialInterface* InMaterial)
{
	if (ElementIndex >= 0 && ElementIndex < static_cast<int32>(OverrideMaterials.size()))
	{
		OverrideMaterials[ElementIndex] = InMaterial;

		// MaterialSlots 동기화 — 씬 저장 시 경로가 올바르게 직렬화되도록
		if (ElementIndex < static_cast<int32>(MaterialSlots.size()))
		{
			MaterialSlots[ElementIndex] = InMaterial
				? InMaterial->GetAssetPathFileName()
				: "None";
		}

		// 프록시에 Material dirty 전파
		MarkProxyDirty(EDirtyFlag::Material);
	}
}

UMaterialInterface* UStaticMeshComponent::GetMaterial(int32 ElementIndex) const
{
	if (ElementIndex >= 0 && ElementIndex < OverrideMaterials.size())
	{
		return OverrideMaterials[ElementIndex];
	}
	return nullptr;
}

FMeshBuffer* UStaticMeshComponent::GetMeshBuffer() const
{
	if (!StaticMesh) return nullptr;
	FStaticMesh* Asset = StaticMesh->GetStaticMeshAsset();
	if (!Asset || !Asset->RenderBuffer) return nullptr;
	return Asset->RenderBuffer.get();
}

FMeshDataView UStaticMeshComponent::GetMeshDataView() const
{
	if (!StaticMesh) return {};
	FStaticMesh* Asset = StaticMesh->GetStaticMeshAsset();
	if (!Asset || Asset->Vertices.empty()) return {};

	FMeshDataView View;
	View.VertexData  = Asset->Vertices.data();
	View.VertexCount = (uint32)Asset->Vertices.size();
	View.Stride      = sizeof(FNormalVertex);
	View.IndexData   = Asset->Indices.data();
	View.IndexCount  = (uint32)Asset->Indices.size();
	return View;
}

void UStaticMeshComponent::UpdateWorldAABB() const
{
	if (!bHasValidBounds)
	{
		UPrimitiveComponent::UpdateWorldAABB();
		return;
	}

	FVector WorldCenter = CachedWorldMatrix.TransformPositionWithW(CachedLocalCenter);

	float Ex = std::abs(CachedWorldMatrix.M[0][0]) * CachedLocalExtent.X
		+ std::abs(CachedWorldMatrix.M[1][0]) * CachedLocalExtent.Y
		+ std::abs(CachedWorldMatrix.M[2][0]) * CachedLocalExtent.Z;
	float Ey = std::abs(CachedWorldMatrix.M[0][1]) * CachedLocalExtent.X
		+ std::abs(CachedWorldMatrix.M[1][1]) * CachedLocalExtent.Y
		+ std::abs(CachedWorldMatrix.M[2][1]) * CachedLocalExtent.Z;
	float Ez = std::abs(CachedWorldMatrix.M[0][2]) * CachedLocalExtent.X
		+ std::abs(CachedWorldMatrix.M[1][2]) * CachedLocalExtent.Y
		+ std::abs(CachedWorldMatrix.M[2][2]) * CachedLocalExtent.Z;

	WorldAABBMinLocation = WorldCenter - FVector(Ex, Ey, Ez);
	WorldAABBMaxLocation = WorldCenter + FVector(Ex, Ey, Ez);
	bWorldAABBDirty = false;
	bHasValidWorldAABB = true;
}

bool UStaticMeshComponent::LineTraceComponent(const FRay& Ray, FHitResult& OutHitResult)
{
	const FMatrix& WorldMatrix = GetWorldMatrix();
	const FMatrix& WorldInverse = GetWorldInverseMatrix();
	return LineTraceStaticMeshFast(Ray, WorldMatrix, WorldInverse, OutHitResult);
}

bool UStaticMeshComponent::LineTraceStaticMeshFast(
	const FRay& Ray,
	const FMatrix& WorldMatrix,
	const FMatrix& WorldInverse,
	FHitResult& OutHitResult)
{
	if (!StaticMesh) return false;

	FVector LocalOrigin = WorldInverse.TransformPositionWithW(Ray.Origin);
	FVector LocalDirection = WorldInverse.TransformVector(Ray.Direction);
	LocalDirection.Normalize();

	// Mesh BVH만 사용하는 전용 경로입니다. 월드 BVH는 이 함수를 직접 호출해 가상 호출 비용을 피합니다.
	if (StaticMesh->RaycastMeshTrianglesWithBVHLocal(LocalOrigin, LocalDirection, OutHitResult))
	{
		const FVector LocalHitPoint = LocalOrigin + LocalDirection * OutHitResult.Distance;
		const FVector WorldHitPoint = WorldMatrix.TransformPositionWithW(LocalHitPoint);
		OutHitResult.Distance = FVector::Distance(Ray.Origin, WorldHitPoint);
		OutHitResult.HitComponent = this;
		return true;
	}

	// 실패하면 기존 방식하던 걸 주석 처리. 성능개선이 일단 확인됨.
	//bool bHit = FRayUtils::RaycastTriangles(
	//	Ray, GetWorldMatrix(),
	//	GetWorldInverseMatrix(),
	//	&Asset->Vertices[0].pos,
	//	sizeof(FNormalVertex),
	//	Asset->Indices,
	//	OutHitResult);

	//if (bHit)
	//{
	//	OutHitResult.HitComponent = this;
	//}
	/*codex의 답변
왜 빨라졌냐면, 주석 처리된 Jungle_Week5_Team3/KraftonEngine/Source/Engine/Collision/RayUtils.cpp:60의
RaycastTriangles()는 BVH 없이 Indices를 처음부터 끝까지 3개씩 돌면서 모든 triangle에 IntersectTriangle()를 합니다.
즉 후보 메시에 대해 매번 풀 스캔입니다.
월드 단계에서 이미 Jungle_Week5_Team3/KraftonEngine/Source/Engine/Collision/WorldPrimitivePickingBVH.cpp:87가
primitive AABB 기준으로 후보만 추립니다.
그런데 AABB는 보수적이라 “박스는 맞았지만 실제 삼각형은 안 맞는” 후보가 꽤 나옵니다.
예전 코드에서는 이런 BVH miss 후보마다 fallback 전체 triangle 스캔이 한 번 더 돌았습니다.
즉 “안 맞은 객체”를 확인하는 비용이 너무 컸던 겁니다.
	*/
	return false; // bHit;
}

void UStaticMeshComponent::PostDuplicate()
{
	UMeshComponent::PostDuplicate();

	AttachMeshComponent = nullptr;
	AttachTargetActor = nullptr;
	InvalidateAttachSocketCache();

	// 메시 에셋 재로딩
	if (!StaticMeshPath.empty() && StaticMeshPath != "None")
	{
		ID3D11Device* Device = GEngine->GetRenderer().GetFD3DDevice().GetDevice();
		UStaticMesh* Loaded = FMeshManager::LoadStaticMesh(StaticMeshPath, Device);
		if (Loaded)
		{
			// SetStaticMesh는 MaterialSlots를 덮어쓰므로, 직렬화된 슬롯 정보를 백업·복원한다.
			TArray<FSoftObjectPtr> SavedSlots = MaterialSlots;
			SetStaticMesh(Loaded);

			// Override material 재로딩
			for (int32 i = 0; i < (int32)MaterialSlots.size() && i < (int32)SavedSlots.size(); ++i)
			{
				MaterialSlots[i] = SavedSlots[i];
				const FString& MatPath = MaterialSlots[i];
				if (MatPath.empty() || MatPath == "None")
				{
					OverrideMaterials[i] = nullptr;
				}
				else
				{
					UMaterialInterface* LoadedMat = FMaterialManager::Get().GetOrCreateMaterialInterface(MatPath);
					OverrideMaterials[i] = LoadedMat;
				}
			}
		}
	}

	CacheLocalBounds();
	UpdateBoneAttachment();
	MarkRenderStateDirty();
	MarkWorldBoundsDirty();
}

void UStaticMeshComponent::PostEditProperty(const char* PropertyName)
{
	UPrimitiveComponent::PostEditProperty(PropertyName);

	if (!PropertyName) return;

	if (strcmp(PropertyName, "bUseBoundingBoxCollider") == 0 || strcmp(PropertyName, "Use Bounding Box Collider") == 0)
	{
		// Static Mesh 물리 shape 종류 변경에 따른 body 재생성
		RecreatePhysicsState();
	}

	if (strcmp(PropertyName, "StaticMeshPath") == 0 || strcmp(PropertyName, "Static Mesh") == 0)
	{
		if (StaticMeshPath.empty() || StaticMeshPath == "None")
		{
			StaticMesh = nullptr;
		}
		else
		{
			ID3D11Device* Device = GEngine->GetRenderer().GetFD3DDevice().GetDevice();
			UStaticMesh* Loaded = FMeshManager::LoadStaticMesh(StaticMeshPath, Device);
			SetStaticMesh(Loaded);
		}
		CacheLocalBounds();
		UpdateBoneAttachment();
		MarkWorldBoundsDirty();
	}

	if (strncmp(PropertyName, "Element ", 8) == 0)
	{
		// "Element 0"에서 8번째 인덱스부터 시작하는 숫자를 정수로 변환
		int32 Index = atoi(&PropertyName[8]);

		// 인덱스 범위 유효성 검사
		if (Index >= 0 && Index < (int32)MaterialSlots.size())
		{
			FString NewMatPath = MaterialSlots[Index];

			if (NewMatPath == "None" || NewMatPath.empty())
			{
				SetMaterial(Index, nullptr);
			}
			else
			{
				UMaterialInterface* LoadedMat = FMaterialManager::Get().GetOrCreateMaterialInterface(NewMatPath);
				if (LoadedMat)
				{
					SetMaterial(Index, LoadedMat);
				}
			}
		}
	}

	if (strcmp(PropertyName, "MaterialSlots") == 0 || strcmp(PropertyName, "Materials") == 0)
	{
		for (int32 Index = 0; Index < (int32)MaterialSlots.size(); ++Index)
		{
			const FString& NewMatPath = MaterialSlots[Index];
			if (NewMatPath == "None" || NewMatPath.empty())
			{
				SetMaterial(Index, nullptr);
			}
			else
			{
				UMaterialInterface* LoadedMat = FMaterialManager::Get().GetOrCreateMaterialInterface(NewMatPath);
				if (LoadedMat)
				{
					SetMaterial(Index, LoadedMat);
				}
			}
		}
	}

	if (strcmp(PropertyName, "bCaptureCurrentAttachOffset") == 0 ||
		strcmp(PropertyName, "Capture Current Attach Offset") == 0)
	{
		if (bCaptureCurrentAttachOffset)
		{
			CaptureAttachOffsetFromCurrentTransform();
		}
		else
		{
			bCaptureCurrentAttachOffset = false;
		}

		return;
	}

	if (strcmp(PropertyName, "bAttachToOwnerMeshBone") == 0 ||
		strcmp(PropertyName, "Attach To Owner Mesh Bone") == 0 ||
		strcmp(PropertyName, "Attach To Mesh Bone") == 0 ||
		strcmp(PropertyName, "AttachTargetActorName") == 0 ||
		strcmp(PropertyName, "Attach Target Actor") == 0 ||
		strcmp(PropertyName, "AttachSocketOffsetPath") == 0 ||
		strcmp(PropertyName, "Socket Offset JSON") == 0 ||
		strcmp(PropertyName, "AttachSocketObjectName") == 0 ||
		strcmp(PropertyName, "Socket Object Name") == 0 ||
		strcmp(PropertyName, "bUseBakedBindPoseCorrection") == 0 ||
		strcmp(PropertyName, "Use Baked Bind Pose Correction") == 0 ||
		strcmp(PropertyName, "bEditAttachOffset") == 0 ||
		strcmp(PropertyName, "Edit Attach Offset") == 0 ||
		strcmp(PropertyName, "AttachBoneName") == 0 ||
		strcmp(PropertyName, "Attach Bone") == 0 ||
		strcmp(PropertyName, "AttachOffsetLocation") == 0 ||
		strcmp(PropertyName, "Attach Offset Location") == 0 ||
		strcmp(PropertyName, "AttachOffsetRotation") == 0 ||
		strcmp(PropertyName, "Attach Offset Rotation") == 0 ||
		strcmp(PropertyName, "AttachOffsetScale") == 0 ||
		strcmp(PropertyName, "Attach Offset Scale") == 0)
	{
		AttachMeshComponent = nullptr;
		AttachTargetActor = nullptr;
		InvalidateAttachSocketCache();
		if (!bEditAttachOffset)
		{
			UpdateBoneAttachment();
		}
	}
}
