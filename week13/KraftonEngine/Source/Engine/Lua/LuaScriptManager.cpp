#include "LuaScriptManager.h"

#include "Core/Log.h"
#include "Component/Movement/FloatingPawnMovementComponent.h"
#include "Component/CameraComponent.h"
#include "Runtime/Engine.h"
#include "Viewport/GameViewportClient.h"
#include "Input/InputSystem.h"
#include "GameFramework/AActor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/CameraManager.h"
#include "GameFramework/World.h"
#include "Object/UClass.h"
#include "Platform/Paths.h"
#include "Math/Vector.h"
#include "UI/UIManager.h"
#include "UI/UserWidget.h"
#include <filesystem>
#include <fstream>

std::unique_ptr<sol::state> FLuaScriptManager::Lua;

void FLuaScriptManager::Initialize()
{
	Lua = std::make_unique<sol::state>();
	Lua->open_libraries(sol::lib::base, sol::lib::package, sol::lib::math, sol::lib::string, sol::lib::table, sol::lib::coroutine);
	(*Lua)["package"]["path"] = FPaths::ToUtf8(FPaths::Combine(FPaths::ScriptDir(), L"?.lua").c_str());
	RegisterBindings(*Lua);
}

void FLuaScriptManager::Shutdown()
{
	Lua.reset();
}

FString FLuaScriptManager::ResolveScriptPath(const FString& ScriptFile)
{
	std::wstring FullPath = FPaths::Combine(FPaths::ScriptDir(), FPaths::ToWide(ScriptFile));
	return FPaths::ToUtf8(FullPath);
}

bool FLuaScriptManager::OpenOrCreateScript(const FString& ScriptFile)
{
	std::wstring FullPath = FPaths::Combine(FPaths::ScriptDir(), FPaths::ToWide(ScriptFile));
	if (!std::filesystem::exists(FullPath))
	{
		FPaths::CreateDir(FPaths::ScriptDir());

		const std::wstring TemplatePath = FPaths::Combine(FPaths::ScriptDir(), L"template.lua");
		std::error_code Error;
		if (std::filesystem::exists(TemplatePath))
		{
			std::filesystem::copy_file(TemplatePath, FullPath, std::filesystem::copy_options::none, Error);
			if (Error)
			{
				UE_LOG("Failed to copy Lua script template: %s", Error.message().c_str());
			}
		}

		if (!std::filesystem::exists(FullPath))
		{
			std::ofstream Out(FullPath);
			if (!Out)
			{
				return false;
			}
		}
	}

	HINSTANCE HInst = ShellExecuteW(nullptr, L"open", FullPath.c_str(), nullptr, nullptr, SW_SHOWNORMAL);

	if ((INT_PTR)HInst <= 32)
	{
		return false;
	}

	return true;
}

sol::state& FLuaScriptManager::GetState()
{
	return *Lua;
}

void FLuaScriptManager::RegisterBindings(sol::state& Lua)
{
	RegisterLuaHelpers(Lua);
	RegisterCoreBindings(Lua);
	RegisterMathBindings(Lua);
	RegisterActorBindings(Lua);
	RegisterUIBindings(Lua);
}

FInputSystemSnapshot FLuaScriptManager::GetLuaInputSnapshot()
{
	if (GEngine)
	{
		if (UGameViewportClient* GameViewportClient = GEngine->GetGameViewportClient())
		{
			FInputSystemSnapshot Snapshot = GameViewportClient->GetGameInputSnapshot();
			return Snapshot;
		}
	}

	return InputSystem::Get().MakeSnapshot();
}

void FLuaScriptManager::RegisterLuaHelpers(sol::state& Lua)
{
	FString CoroutineManagerPath = ResolveScriptPath("CoroutineManager.lua");
	Lua.safe_script_file(CoroutineManagerPath, sol::script_pass_on_error);
}

void FLuaScriptManager::RegisterCoreBindings(sol::state& Lua)
{
	Lua.set_function("print", [](sol::variadic_args Args)
	{
		FString Message;

		for (auto Arg : Args)
		{
			if (!Message.empty())
			{
				Message += "\t";
			}

			Message += Arg.as<FString>();
		}

		UE_LOG("[Lua] %s", Message.c_str());
	});

	sol::table Input = Lua.create_named_table("Input");
	Input.set_function("GetKeyDown", sol::overload(
		[](int VK)
	{
		return GetLuaInputSnapshot().WasPressed(VK);
	}));
	Input.set_function("GetKey", sol::overload(
		[](int VK)
	{
		return GetLuaInputSnapshot().IsDown(VK);
	}));
	Input.set_function("GetKeyUp", sol::overload(
		[](int VK)
	{
		return GetLuaInputSnapshot().WasReleased(VK);
	}));
	Input.set_function("GetMouseDeltaX", []()
	{
		return GetLuaInputSnapshot().MouseDeltaX;
	});
	Input.set_function("GetMouseDeltaY", []()
	{
		return GetLuaInputSnapshot().MouseDeltaY;
	});

	sol::table Key = Lua.create_named_table("Key");
	Key["W"] = static_cast<int32>('W');
	Key["A"] = static_cast<int32>('A');
	Key["S"] = static_cast<int32>('S');
	Key["D"] = static_cast<int32>('D');
	Key["Space"] = VK_SPACE;
	Key["Escape"] = VK_ESCAPE;
	Key["F1"] = VK_F1;
	Key["F2"] = VK_F2;

	sol::table CameraManager = Lua.create_named_table("CameraManager");
	CameraManager.set_function("ToggleActorCamera", [](const FString& ActorName)
	{
		if (!GEngine || !GEngine->GetWorld())
		{
			return false;
		}

		UCameraManager* Manager = GEngine->GetWorld()->GetCameraManager();
		return Manager ? Manager->ToggleActiveCameraForActor(ActorName) : false;
	});
	CameraManager.set_function("ToggleOwnerCamera", [](AActor* Actor)
	{
		if (!GEngine || !GEngine->GetWorld())
		{
			return false;
		}

		UCameraManager* Manager = GEngine->GetWorld()->GetCameraManager();
		return Manager ? Manager->ToggleActiveCameraForActor(Actor) : false;
	});
	CameraManager.set_function("PossessCamera", [](UCameraComponent* Camera)
	{
		if (!GEngine || !GEngine->GetWorld() || !Camera)
		{
			return false;
		}

		UCameraManager* Manager = GEngine->GetWorld()->GetCameraManager();
		if (!Manager)
		{
			return false;
		}

		Manager->SetActiveCamera(Camera);
		Manager->Possess(Camera);
		return true;
	});
	CameraManager.set_function("GetActiveCameraOwner", []() -> AActor*
	{
		if (!GEngine || !GEngine->GetWorld())
		{
			return nullptr;
		}
		UCameraManager* Manager = GEngine->GetWorld()->GetCameraManager();
		UCameraComponent* ActiveCamera = Manager ? Manager->GetActiveCamera() : nullptr;
		return ActiveCamera ? ActiveCamera->GetOwner() : nullptr;
	});
	CameraManager.set_function("GetPossessedCamera", []() -> AActor*
	{
		if (!GEngine || !GEngine->GetWorld())
		{
			return nullptr;
		}
		UCameraManager* Manager = GEngine->GetWorld()->GetCameraManager();
		UCameraComponent* PossessedCamera = Manager ? Manager->GetPossessedCamera() : nullptr;
		return PossessedCamera ? PossessedCamera->GetOwner() : nullptr;
	});
}

void FLuaScriptManager::RegisterMathBindings(sol::state& Lua)
{
	Lua.new_usertype<FVector>("Vector",
		sol::constructors<FVector(), FVector(float, float, float)>(),
		"X", &FVector::X,
		"Y", &FVector::Y,
		"Z", &FVector::Z,
		"Length", &FVector::Length,
		"Normalize", &FVector::Normalize,
		"Normalized", &FVector::Normalized,
		"Dot", &FVector::Dot,
		"Cross", sol::overload(
		static_cast<FVector(FVector::*)(const FVector&) const>(&FVector::Cross),
		static_cast<FVector(*)(const FVector&, const FVector&)>(&FVector::Cross)
	),
		"Distance", &FVector::Distance,
		"DistSquared", &FVector::DistSquared,
		"Lerp", &FVector::Lerp,
		sol::meta_function::addition, sol::overload(
		static_cast<FVector(FVector::*)(const FVector&) const>(&FVector::operator+),
		static_cast<FVector(FVector::*)(float) const>(&FVector::operator+)
	),
		sol::meta_function::subtraction, sol::overload(
		static_cast<FVector(FVector::*)(const FVector&) const>(&FVector::operator-),
		static_cast<FVector(FVector::*)(float) const>(&FVector::operator-)
	),
		sol::meta_function::multiplication, &FVector::operator*,
		sol::meta_function::division, &FVector::operator/,
		"Zero", []() { return FVector::ZeroVector; },
		"One", []() { return FVector::OneVector; },
		"Up", []() { return FVector::UpVector; },
		"Down", []() { return FVector::DownVector; },
		"Forward", []() { return FVector::ForwardVector; },
		"Backward", []() { return FVector::BackwardVector; },
		"Right", []() { return FVector::RightVector; },
		"Left", []() { return FVector::LeftVector; },
		"XAxis", []() { return FVector::XAxisVector; },
		"YAxis", []() { return FVector::YAxisVector; },
		"ZAxis", []() { return FVector::ZAxisVector; });
}

void FLuaScriptManager::RegisterActorBindings(sol::state& Lua)
{
	Lua.new_usertype<UFloatingPawnMovementComponent>("FloatingPawnMovementComponent",
		"SetMoveInput", &UFloatingPawnMovementComponent::SetMoveInput,
		"SetLookInput", &UFloatingPawnMovementComponent::SetLookInput);

	Lua.new_usertype<UCameraComponent>("CameraComponent");

	Lua.new_usertype<AActor>("Actor",
		"Location", sol::property(
		[](AActor& Actor)
	{
		return Actor.GetActorLocation();
	},
		[](AActor& Actor, const FVector& Location)
	{
		Actor.SetActorLocation(Location);
	}
	),
		"Rotation", sol::property(
		[](AActor& Actor)
	{
		return Actor.GetActorRotation().ToVector();
	},
		[](AActor& Actor, const FVector& Rotation)
	{
		Actor.SetActorRotation(Rotation);
	}
	),

		"Scale", sol::property(
		[](AActor& Actor)
	{
		return Actor.GetActorScale();
	},
		[](AActor& Actor, const FVector& Scale)
	{
		Actor.SetActorScale(Scale);
	}
	),

		"Forward", sol::property([](AActor& Actor)
	{
		return Actor.GetActorForward();
	}
	),
		
		"Right", sol::property([](AActor& Actor)
	{
		return Actor.GetActorRight();
	}
	),

		"AddWorldOffset", [](AActor& Actor, const FVector& Offset)
	{
		Actor.AddActorWorldOffset(Offset);
	},

		"Destroy", [](AActor& Actor)
	{
		// World->DestroyActor가 EndPlay + 정리. Lua는 호출 후 해당 액터를 더 참조하지 말 것.
		if (UWorld* W = Actor.GetWorld()) W->DestroyActor(&Actor);
	},

		"IsValid", [](AActor* Actor)
	{
		// Lua가 보유한 actor 핸들이 cpp 측에서 destroy됐는지 확인. nil/destroyed면 false.
		return Actor != nullptr && IsAliveObject(Actor);
	},

		"GetFloatingPawnMovement", [](AActor& Actor)
	{
		return Actor.GetComponentByClass<UFloatingPawnMovementComponent>();
	},

		"GetCamera", [](AActor& Actor)
	{
		return Actor.GetComponentByClass<UCameraComponent>();
	},

		"UUID", sol::property([](AActor& Actor)
	{
		return Actor.GetUUID();
	}),

		"Name", sol::property([](AActor& Actor)
	{
		return Actor.GetFName().ToString();
	}));

	Lua.new_usertype<APawn>("Pawn",
		sol::base_classes, sol::bases<AActor>(),
		"IsPossessed", &APawn::IsPossessed,
		"SetAutoPossessPlayer", &APawn::SetAutoPossessPlayer,
		"GetAutoPossessPlayer", &APawn::GetAutoPossessPlayer);

	// --- World binding — 런타임 액터 spawn 용 (Engine 일반 기능) ---
	sol::table World = Lua.create_named_table("World");
	World.set_function("SpawnActor", [](const FString& ClassName) -> AActor*
	{
		if (!GEngine) return nullptr;
		UWorld* W = GEngine->GetWorld();
		if (!W) return nullptr;
		UClass* Cls = UClass::FindByName(ClassName.c_str());
		if (!Cls) return nullptr;
		return W->SpawnActorByClass(Cls);
	});
	World.set_function("FindActorByName", [](const FString& ActorName) -> AActor*
	{
		if (!GEngine || !GEngine->GetWorld()) return nullptr;
		for (AActor* Actor : GEngine->GetWorld()->GetActors())
		{
			if (Actor && Actor->GetFName().ToString() == ActorName)
			{
				return Actor;
			}
		}
		return nullptr;
	});
	World.set_function("FindFirstActorByClass", [](const FString& ClassName) -> AActor*
	{
		if (!GEngine || !GEngine->GetWorld()) return nullptr;
		UClass* Cls = UClass::FindByName(ClassName.c_str());
		if (!Cls) return nullptr;
		for (AActor* Actor : GEngine->GetWorld()->GetActors())
		{
			if (Actor && Actor->GetClass()->IsA(Cls))
			{
				return Actor;
			}
		}
		return nullptr;
	});

	// 게임 특화 usertype/enum/global(GetGameState 등) 은 Game 모듈의
	// RegisterGameLuaBindings 가 등록한다. 호출 순서는 GameEngine/EditorEngine::Init
	// 에서 UEngine::Init() 직후.
}

void FLuaScriptManager::RegisterUIBindings(sol::state& Lua)
{
	Lua.new_usertype<UUserWidget>("UserWidget",
		"AddToViewport", [](UUserWidget& Widget)
	{
		Widget.AddToViewport();
	},
		"AddToViewportZ", [](UUserWidget& Widget, int32 ZOrder)
	{
		Widget.AddToViewport(ZOrder);
	},
		"RemoveFromParent", &UUserWidget::RemoveFromParent,
		"Show", [](UUserWidget& Widget)
	{
		Widget.AddToViewport();
	},
		"Hide", &UUserWidget::RemoveFromParent,
		"show", [](UUserWidget& Widget)
	{
		Widget.AddToViewport();
	},
		"hide", &UUserWidget::RemoveFromParent,
		"IsInViewport", &UUserWidget::IsInViewport,
		"bind_click", [](UUserWidget& Widget, const FString& ElementId, sol::protected_function Callback)
	{
		Widget.BindClick(ElementId, Callback);
	},
		"SetText", &UUserWidget::SetText,
		"set_text", &UUserWidget::SetText,
		"SetProperty", &UUserWidget::SetProperty,
		"set_property", &UUserWidget::SetProperty,
		"SetWantsMouse", &UUserWidget::SetWantsMouse,
		"WantsMouse", &UUserWidget::WantsMouse);

	sol::table UI = Lua.create_named_table("UI");
	UI.set_function("CreateWidget", [](const FString& DocumentPath)
	{
		return UUIManager::Get().CreateWidget(nullptr, DocumentPath);
	});
}
