#include "SceneSerializer.h"
#include "ThirdParty/nlohmann/json.hpp"
#include "Renderer/Material.h"
#include "Renderer/MaterialManager.h"
#include "Component/CameraComponent.h"
#include "Camera/Camera.h"
#include "Core/Paths.h"
#include "Actor/Actor.h"
#include "Actor/AttachTestActor.h"
#include "Actor/CubeActor.h"
#include "Actor/SphereActor.h"
#include "Actor/ObjActor.h"
#include "Component/PrimitiveComponent.h"
#include "Scene/Scene.h"
#include "Object/ObjectFactory.h" 
#include "Serializer/Archive.h"
#include "Object/Class.h"
#include <iomanip>
#include <filesystem>
#include <fstream>
void FSceneSerializer::Save(UScene* Scene, const FString& FilePath)
{
	nlohmann::json Json;
	CCamera* Camera = Scene->GetCamera();
	if (Camera)
	{
		const FVector Position = Camera->GetPosition();
		Json["Camera"]["Position"] = { Position.X, Position.Y, Position.Z };
		Json["Camera"]["Rotation"] = { Camera->GetYaw(), Camera->GetPitch() };
	}

	// Materials (로드된 Material 파일 경로를 프로젝트 루트 기준 상대 경로로 저장)
	TArray<FString> LoadedPaths = FMaterialManager::Get().GetLoadedPaths();
	if (!LoadedPaths.empty())
	{
		nlohmann::json Materials = nlohmann::json::array();
		FString Root = FPaths::ProjectRoot().string();
		for (const FString& AbsPath : LoadedPaths)
		{
			// 절대 경로 → 프로젝트 루트 기준 상대 경로
			std::filesystem::path Rel = std::filesystem::relative(AbsPath, Root);
			Materials.push_back(Rel.generic_string());
		}
		Json["Materials"] = Materials;
	}

	// Primitives
	nlohmann::json Primitives;
	int32 Index = 0;
	for (AActor* Actor : Scene->GetActors())
	{
		if (!Actor || Actor->IsPendingDestroy())
			continue;
		if (!Actor->GetRootComponent())
			continue;
		FArchive Ar(true);
		Actor->Serialize(Ar);
		nlohmann::json& ActorJson 
			= *static_cast<nlohmann::json*>(Ar.GetRawJson());
		Primitives[std::to_string(Index)] = ActorJson;
		Index++;
	}

	Json["Primitives"] = Primitives;
	Json["NextUUID"] = FObjectFactory::GetLastUUID();
	std::ofstream File(FilePath);
	if (File.is_open())
	{
		File << std::setw(2) << Json << std::endl;
	}
}

bool FSceneSerializer::Load(UScene* Scene, const FString& FilePath, ID3D11Device* Device)
{
	std::ifstream File(FilePath);
	if (!File.is_open())
	{
		return false;
	}

	nlohmann::json Json;

	try
	{
		File >> Json;
	}
	catch (const std::exception& e)
	{
		return false;
	}

	if (!Json.contains("Primitives"))
		return false;

	CCamera* Camera = Scene->GetCamera();
	if (Camera && Json.contains("Camera"))
	{
		auto& Cam = Json["Camera"];
		if (Cam.contains("Position"))
		{
			auto& P = Cam["Position"];
			Camera->SetPosition({ P[0].get<float>(), P[1].get<float>(), P[2].get<float>() });
		}
		if (Cam.contains("Rotation"))
		{
			auto& R = Cam["Rotation"];
			Camera->SetRotation(R[0].get<float>(), R[1].get<float>());
		}
	}

	int32 ActorIndex = 0;
	for (auto& [Key, Value] : Json["Primitives"].items())
	{
		FString ClassName = Value.value("Class","");
		UClass* ActorClass = UClass::FindClass(ClassName);
		if (!ActorClass)
		{
			ActorIndex++;
			continue;
		}
		const FString ActorName = ClassName + "_" + std::to_string(ActorIndex);
		AActor* Actor = static_cast<AActor*>(FObjectFactory::ConstructObject(ActorClass, Scene, ActorName));
		if (!Actor)
		{
			ActorIndex++;
			continue;
		}

		Scene->RegisterActor(Actor);
		Actor->PostSpawnInitialize();
		FArchive Ar(false);// loading
		*static_cast<nlohmann::json*>(Ar.GetRawJson()) = Value;
		Actor->Serialize(Ar);

		if (Value.contains("Material"))
		{
			const FString MaterialName = Value["Material"].get<FString>();
			const std::shared_ptr<FMaterial> Material = FMaterialManager::Get().FindByName(MaterialName);
			if (Material)
			{
				if (UPrimitiveComponent* PrimitiveComponent = Actor->GetComponentByClass<UPrimitiveComponent>())
				{
					PrimitiveComponent->SetMaterial(Material.get());
				}
			}
		}

		if (Value.contains("PrimitiveFileName"))
		{
			if (Actor->IsA(AObjActor::StaticClass()))
			{
				const FString PrimitiveFileName = Value["PrimitiveFileName"].get<FString>();
				if (PrimitiveFileName != "")
				{
					AObjActor* ObjActor = static_cast<AObjActor*>(Actor);
					if (ObjActor)
						ObjActor->LoadObj(Device, PrimitiveFileName);
				}
			}
		}

		++ActorIndex;

	}
	if (Json.contains("NextUUID"))
	{
		uint32 Saved = Json["NextUUID"].get<uint32>();
		if (Saved > FObjectFactory::GetLastUUID())
		{
			FObjectFactory::SetLastUUID(Saved);
		}
	}

	return true;
}
