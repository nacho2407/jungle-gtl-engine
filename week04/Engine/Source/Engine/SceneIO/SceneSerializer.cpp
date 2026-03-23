#include "SceneSerializer.h"

#include "SceneJson.h"
#include "SceneTypeRegistry.h"
#include "Engine/Component/PrimitiveComponent.h"
#include "Engine/Component/SceneComponent.h"
#include "Engine/Component/UnknownComponent.h"
#include "Engine/Game/Actor.h"
#include "Engine/Game/UnknownActor.h"
#include "Engine/Scene.h"

#include <fstream>

namespace
{
    constexpr const char* SceneSchemaName = "JungleScene";
    constexpr int32 SceneSchemaVersion = 1;

    FSceneJsonValue MakeNumberArray(std::initializer_list<double> Values)
    {
        FSceneJsonValue::Array ArrayValue;
        ArrayValue.reserve(Values.size());
        for (double Value : Values)
        {
            ArrayValue.emplace_back(Value);
        }
        return FSceneJsonValue(std::move(ArrayValue));
    }

    bool TryGetObject(const FSceneJsonValue& Value, const FSceneJsonValue::Object*& OutObject,
                      FString* OutErrorMessage, const char* Context)
    {
        OutObject = Value.TryGetObject();
        if (OutObject != nullptr)
        {
            return true;
        }

        if (OutErrorMessage != nullptr)
        {
            *OutErrorMessage = FString(Context) + " must be an object.";
        }
        return false;
    }

    bool TryGetArray(const FSceneJsonValue& Value, const FSceneJsonValue::Array*& OutArray,
                     FString* OutErrorMessage, const char* Context)
    {
        OutArray = Value.TryGetArray();
        if (OutArray != nullptr)
        {
            return true;
        }

        if (OutErrorMessage != nullptr)
        {
            *OutErrorMessage = FString(Context) + " must be an array.";
        }
        return false;
    }

    const FSceneJsonValue* FindRequiredField(const FSceneJsonValue::Object& ObjectValue,
                                             const FString& FieldName,
                                             FString* OutErrorMessage,
                                             const char* Context)
    {
        const auto Iterator = ObjectValue.find(FieldName);
        if (Iterator != ObjectValue.end())
        {
            return &Iterator->second;
        }

        if (OutErrorMessage != nullptr)
        {
            *OutErrorMessage =
                FString(Context) + " is missing required field '" + FieldName + "'.";
        }
        return nullptr;
    }

    bool TryReadStringField(const FSceneJsonValue::Object& ObjectValue, const FString& FieldName,
                            FString& OutValue, FString* OutErrorMessage,
                            const char* Context)
    {
        const FSceneJsonValue* FieldValue =
            FindRequiredField(ObjectValue, FieldName, OutErrorMessage, Context);
        if (FieldValue == nullptr)
        {
            return false;
        }

        if (FieldValue->TryGetString(OutValue))
        {
            return true;
        }

        if (OutErrorMessage != nullptr)
        {
            *OutErrorMessage = FString(Context) + "." + FieldName + " must be a string.";
        }
        return false;
    }

    bool TryReadBoolField(const FSceneJsonValue::Object& ObjectValue, const FString& FieldName,
                          bool& OutValue, FString* OutErrorMessage,
                          const char* Context)
    {
        const FSceneJsonValue* FieldValue =
            FindRequiredField(ObjectValue, FieldName, OutErrorMessage, Context);
        if (FieldValue == nullptr)
        {
            return false;
        }

        if (FieldValue->TryGetBool(OutValue))
        {
            return true;
        }

        if (OutErrorMessage != nullptr)
        {
            *OutErrorMessage = FString(Context) + "." + FieldName + " must be a bool.";
        }
        return false;
    }

    bool TryReadUIntField(const FSceneJsonValue::Object& ObjectValue, const FString& FieldName,
                          uint32& OutValue, FString* OutErrorMessage,
                          const char* Context)
    {
        const FSceneJsonValue* FieldValue =
            FindRequiredField(ObjectValue, FieldName, OutErrorMessage, Context);
        if (FieldValue == nullptr)
        {
            return false;
        }

        double NumberValue = 0.0;
        if (!FieldValue->TryGetNumber(NumberValue) || NumberValue < 0.0)
        {
            if (OutErrorMessage != nullptr)
            {
                *OutErrorMessage =
                    FString(Context) + "." + FieldName + " must be a non-negative number.";
            }
            return false;
        }

        OutValue = static_cast<uint32>(NumberValue);
        return true;
    }

    bool TryReadIntField(const FSceneJsonValue::Object& ObjectValue, const FString& FieldName,
                         int32& OutValue, FString* OutErrorMessage,
                         const char* Context)
    {
        const FSceneJsonValue* FieldValue =
            FindRequiredField(ObjectValue, FieldName, OutErrorMessage, Context);
        if (FieldValue == nullptr)
        {
            return false;
        }

        double NumberValue = 0.0;
        if (!FieldValue->TryGetNumber(NumberValue))
        {
            if (OutErrorMessage != nullptr)
            {
                *OutErrorMessage = FString(Context) + "." + FieldName + " must be a number.";
            }
            return false;
        }

        OutValue = static_cast<int32>(NumberValue);
        return true;
    }

    bool TryReadVector3(const FSceneJsonValue& Value, FVector& OutVector,
                        FString* OutErrorMessage, const char* Context)
    {
        const FSceneJsonValue::Array* ArrayValue = nullptr;
        if (!TryGetArray(Value, ArrayValue, OutErrorMessage, Context))
        {
            return false;
        }

        if (ArrayValue->size() != 3)
        {
            if (OutErrorMessage != nullptr)
            {
                *OutErrorMessage = FString(Context) + " must have exactly 3 numbers.";
            }
            return false;
        }

        double X = 0.0;
        double Y = 0.0;
        double Z = 0.0;
        if (!(*ArrayValue)[0].TryGetNumber(X) || !(*ArrayValue)[1].TryGetNumber(Y) ||
            !(*ArrayValue)[2].TryGetNumber(Z))
        {
            if (OutErrorMessage != nullptr)
            {
                *OutErrorMessage = FString(Context) + " must contain only numbers.";
            }
            return false;
        }

        OutVector = FVector(static_cast<float>(X), static_cast<float>(Y), static_cast<float>(Z));
        return true;
    }

    bool TryReadQuat(const FSceneJsonValue& Value, FQuat& OutQuat,
                     FString* OutErrorMessage, const char* Context)
    {
        const FSceneJsonValue::Array* ArrayValue = nullptr;
        if (!TryGetArray(Value, ArrayValue, OutErrorMessage, Context))
        {
            return false;
        }

        if (ArrayValue->size() != 4)
        {
            if (OutErrorMessage != nullptr)
            {
                *OutErrorMessage = FString(Context) + " must have exactly 4 numbers.";
            }
            return false;
        }

        double X = 0.0;
        double Y = 0.0;
        double Z = 0.0;
        double W = 0.0;
        if (!(*ArrayValue)[0].TryGetNumber(X) || !(*ArrayValue)[1].TryGetNumber(Y) ||
            !(*ArrayValue)[2].TryGetNumber(Z) || !(*ArrayValue)[3].TryGetNumber(W))
        {
            if (OutErrorMessage != nullptr)
            {
                *OutErrorMessage = FString(Context) + " must contain only numbers.";
            }
            return false;
        }

        OutQuat = FQuat(static_cast<float>(X), static_cast<float>(Y), static_cast<float>(Z),
                        static_cast<float>(W));
        OutQuat.Normalize();
        return true;
    }

    bool TryReadColor(const FSceneJsonValue& Value, FVector4& OutColor,
                      FString* OutErrorMessage, const char* Context)
    {
        const FSceneJsonValue::Array* ArrayValue = nullptr;
        if (!TryGetArray(Value, ArrayValue, OutErrorMessage, Context))
        {
            return false;
        }

        if (ArrayValue->size() != 4)
        {
            if (OutErrorMessage != nullptr)
            {
                *OutErrorMessage = FString(Context) + " must have exactly 4 numbers.";
            }
            return false;
        }

        double X = 0.0;
        double Y = 0.0;
        double Z = 0.0;
        double W = 0.0;
        if (!(*ArrayValue)[0].TryGetNumber(X) || !(*ArrayValue)[1].TryGetNumber(Y) ||
            !(*ArrayValue)[2].TryGetNumber(Z) || !(*ArrayValue)[3].TryGetNumber(W))
        {
            if (OutErrorMessage != nullptr)
            {
                *OutErrorMessage = FString(Context) + " must contain only numbers.";
            }
            return false;
        }

        OutColor = FVector4(static_cast<float>(X), static_cast<float>(Y), static_cast<float>(Z),
                            static_cast<float>(W));
        return true;
    }

    FSceneJsonValue::Object BuildComponentObject(const AActor& OwnerActor,
                                                 const Engine::Component::USceneComponent& Component)
    {
        FSceneJsonValue::Object ComponentObject;

        if (Component.IsA(Engine::Component::UUnknownComponent::GetClass()))
        {
            const auto& UnknownComponent =
                static_cast<const Engine::Component::UUnknownComponent&>(Component);
            if (!UnknownComponent.GetSerializedPayload().empty())
            {
                FSceneJsonValue ParsedPayload;
                if (FSceneJsonParser::Parse(UnknownComponent.GetSerializedPayload(), ParsedPayload))
                {
                    if (const auto* ParsedObject = ParsedPayload.TryGetObject())
                    {
                        ComponentObject = *ParsedObject;
                    }
                }
            }
        }

        ComponentObject["type"] = FSceneTypeRegistry::ResolveComponentTypeName(Component);
        ComponentObject["uuid"] = static_cast<double>(Component.UUID);
        ComponentObject["name"] = Component.Name.IsValid() ? Component.Name.ToFString() : "";
        ComponentObject["is_root"] = OwnerActor.GetRootComponent() == &Component;

        FSceneJsonValue::Object TransformObject;
        const FVector Location = Component.GetRelativeLocation();
        const FQuat Rotation = Component.GetRelativeQuaternion();
        const FVector Scale = Component.GetRelativeScale3D();

        TransformObject["location"] = MakeNumberArray({Location.X, Location.Y, Location.Z});
        TransformObject["rotation_quat"] =
            MakeNumberArray({Rotation.X, Rotation.Y, Rotation.Z, Rotation.W});
        TransformObject["scale"] = MakeNumberArray({Scale.X, Scale.Y, Scale.Z});
        ComponentObject["transform"] = std::move(TransformObject);

        FSceneJsonValue::Object DataObject;
        const auto ExistingDataIterator = ComponentObject.find("data");
        if (ExistingDataIterator != ComponentObject.end())
        {
            if (const auto* ExistingDataObject = ExistingDataIterator->second.TryGetObject())
            {
                DataObject = *ExistingDataObject;
            }
        }

        if (const auto* PrimitiveComponent =
                Cast<Engine::Component::UPrimitiveComponent>(
                    const_cast<Engine::Component::USceneComponent*>(&Component)))
        {
            const FVector4& Color = PrimitiveComponent->GetColor();
            DataObject["color"] = MakeNumberArray({Color.X, Color.Y, Color.Z, Color.W});
        }

        ComponentObject["data"] = std::move(DataObject);
        return ComponentObject;
    }

    FSceneJsonValue SerializeComponent(const AActor& OwnerActor,
                                       const Engine::Component::USceneComponent& Component)
    {
        return FSceneJsonValue(BuildComponentObject(OwnerActor, Component));
    }

    FSceneJsonValue SerializeActor(const AActor& Actor)
    {
        FSceneJsonValue::Object ActorObject;

        if (Actor.IsA(AUnknownActor::GetClass()))
        {
            const auto& UnknownActor = static_cast<const AUnknownActor&>(Actor);
            if (!UnknownActor.GetSerializedPayload().empty())
            {
                FSceneJsonValue ParsedPayload;
                if (FSceneJsonParser::Parse(UnknownActor.GetSerializedPayload(), ParsedPayload))
                {
                    if (const auto* ParsedObject = ParsedPayload.TryGetObject())
                    {
                        ActorObject = *ParsedObject;
                    }
                }
            }
        }

        ActorObject["type"] = FSceneTypeRegistry::ResolveActorTypeName(Actor);
        ActorObject["uuid"] = static_cast<double>(Actor.UUID);
        ActorObject["name"] = Actor.Name.IsValid() ? Actor.Name.ToFString() : "";
        ActorObject["pickable"] = Actor.IsPickable();

        FSceneJsonValue::Array ComponentsArray;
        const auto& Components = Actor.GetOwnedComponents();
        ComponentsArray.reserve(Components.size());
        for (Engine::Component::USceneComponent* Component : Components)
        {
            if (Component != nullptr)
            {
                ComponentsArray.push_back(SerializeComponent(Actor, *Component));
            }
        }

        ActorObject["components"] = std::move(ComponentsArray);
        return FSceneJsonValue(std::move(ActorObject));
    }

    bool ApplyComponentJson(const FSceneJsonValue::Object& ComponentObject,
                            Engine::Component::USceneComponent& Component,
                            FString* OutErrorMessage)
    {
        uint32 ComponentUuid = Component.UUID;
        if (!TryReadUIntField(ComponentObject, "uuid", ComponentUuid, OutErrorMessage,
                              "Component"))
        {
            return false;
        }
        Component.UUID = ComponentUuid;

        FString ComponentName;
        if (!TryReadStringField(ComponentObject, "name", ComponentName, OutErrorMessage,
                                "Component"))
        {
            return false;
        }
        Component.Name = ComponentName;

        const FSceneJsonValue* TransformValue =
            FindRequiredField(ComponentObject, "transform", OutErrorMessage, "Component");
        if (TransformValue == nullptr)
        {
            return false;
        }

        const FSceneJsonValue::Object* TransformObject = nullptr;
        if (!TryGetObject(*TransformValue, TransformObject, OutErrorMessage,
                          "Component.transform"))
        {
            return false;
        }

        const FSceneJsonValue* LocationValue =
            FindRequiredField(*TransformObject, "location", OutErrorMessage,
                              "Component.transform");
        const FSceneJsonValue* RotationValue =
            FindRequiredField(*TransformObject, "rotation_quat", OutErrorMessage,
                              "Component.transform");
        const FSceneJsonValue* ScaleValue =
            FindRequiredField(*TransformObject, "scale", OutErrorMessage,
                              "Component.transform");
        if (LocationValue == nullptr || RotationValue == nullptr || ScaleValue == nullptr)
        {
            return false;
        }

        FVector Location = FVector::ZeroVector;
        FVector Scale = FVector::OneVector;
        FQuat Rotation = FQuat::Identity;
        if (!TryReadVector3(*LocationValue, Location, OutErrorMessage,
                            "Component.transform.location") ||
            !TryReadQuat(*RotationValue, Rotation, OutErrorMessage,
                         "Component.transform.rotation_quat") ||
            !TryReadVector3(*ScaleValue, Scale, OutErrorMessage,
                            "Component.transform.scale"))
        {
            return false;
        }

        Component.SetRelativeLocation(Location);
        Component.SetRelativeRotation(Rotation);
        Component.SetRelativeScale3D(Scale);

        const auto DataIterator = ComponentObject.find("data");
        if (DataIterator != ComponentObject.end())
        {
            if (auto* PrimitiveComponent =
                    Cast<Engine::Component::UPrimitiveComponent>(&Component))
            {
                if (const auto* DataObject = DataIterator->second.TryGetObject())
                {
                    const auto ColorIterator = DataObject->find("color");
                    if (ColorIterator != DataObject->end())
                    {
                        FVector4 Color;
                        if (!TryReadColor(ColorIterator->second, Color, OutErrorMessage,
                                          "Component.data.color"))
                        {
                            return false;
                        }

                        PrimitiveComponent->SetColor(Color);
                    }
                }
            }
        }

        return true;
    }

    Engine::Component::USceneComponent* FindReusableComponent(
        const FString& TypeName,
        const TArray<Engine::Component::USceneComponent*>& ExistingComponents,
        TArray<bool>& ReusedFlags)
    {
        for (size_t Index = 0; Index < ExistingComponents.size(); ++Index)
        {
            Engine::Component::USceneComponent* ExistingComponent = ExistingComponents[Index];
            if (ExistingComponent == nullptr || ReusedFlags[Index])
            {
                continue;
            }

            if (FSceneTypeRegistry::ResolveComponentTypeName(*ExistingComponent) != TypeName)
            {
                continue;
            }

            ReusedFlags[Index] = true;
            return ExistingComponent;
        }

        return nullptr;
    }

    bool DeserializeActorValue(const FSceneJsonValue& ActorValue, FScene& OutScene,
                               FString* OutErrorMessage)
    {
        const FSceneJsonValue::Object* ActorObject = nullptr;
        if (!TryGetObject(ActorValue, ActorObject, OutErrorMessage, "Actor"))
        {
            return false;
        }

        FString ActorTypeName;
        if (!TryReadStringField(*ActorObject, "type", ActorTypeName, OutErrorMessage, "Actor"))
        {
            return false;
        }

        bool bKnownActorType = false;
        std::unique_ptr<AActor> Actor(FSceneTypeRegistry::ConstructActor(ActorTypeName,
                                                                         &bKnownActorType));
        if (Actor == nullptr)
        {
            if (OutErrorMessage != nullptr)
            {
                *OutErrorMessage = "Failed to construct actor '" + ActorTypeName + "'.";
            }
            return false;
        }

        if (!bKnownActorType)
        {
            UE_LOG(SceneIO, ELogVerbosity::Error,
                   "Unknown actor type '%s' restored as placeholder.", ActorTypeName.c_str());
        }

        if (Actor->IsA(AUnknownActor::GetClass()))
        {
            auto* UnknownActor = static_cast<AUnknownActor*>(Actor.get());
            UnknownActor->SetOriginalTypeName(ActorTypeName);
            UnknownActor->SetSerializedPayload(FSceneJsonWriter::Write(ActorValue, true));
        }

        uint32 ActorUuid = Actor->UUID;
        if (!TryReadUIntField(*ActorObject, "uuid", ActorUuid, OutErrorMessage, "Actor"))
        {
            return false;
        }
        Actor->UUID = ActorUuid;

        FString ActorName;
        if (!TryReadStringField(*ActorObject, "name", ActorName, OutErrorMessage, "Actor"))
        {
            return false;
        }
        Actor->Name = ActorName;

        bool bPickable = true;
        if (!TryReadBoolField(*ActorObject, "pickable", bPickable, OutErrorMessage, "Actor"))
        {
            return false;
        }
        Actor->SetPickable(bPickable);

        const FSceneJsonValue* ComponentsValue =
            FindRequiredField(*ActorObject, "components", OutErrorMessage, "Actor");
        if (ComponentsValue == nullptr)
        {
            return false;
        }

        const FSceneJsonValue::Array* ComponentsArray = nullptr;
        if (!TryGetArray(*ComponentsValue, ComponentsArray, OutErrorMessage, "Actor.components"))
        {
            return false;
        }

        const auto ExistingComponents = Actor->GetOwnedComponents();
        TArray<bool> ReusedFlags(ExistingComponents.size(), false);

        for (const FSceneJsonValue& ComponentValue : *ComponentsArray)
        {
            const FSceneJsonValue::Object* ComponentObject = nullptr;
            if (!TryGetObject(ComponentValue, ComponentObject, OutErrorMessage, "Component"))
            {
                return false;
            }

            FString ComponentTypeName;
            if (!TryReadStringField(*ComponentObject, "type", ComponentTypeName, OutErrorMessage,
                                    "Component"))
            {
                return false;
            }

            bool bIsRootComponent = false;
            if (!TryReadBoolField(*ComponentObject, "is_root", bIsRootComponent,
                                  OutErrorMessage, "Component"))
            {
                return false;
            }

            Engine::Component::USceneComponent* Component =
                FindReusableComponent(ComponentTypeName, ExistingComponents, ReusedFlags);
            bool bKnownComponentType = true;
            if (Component == nullptr)
            {
                Component =
                    FSceneTypeRegistry::ConstructComponent(ComponentTypeName, &bKnownComponentType);
                if (Component == nullptr)
                {
                    if (OutErrorMessage != nullptr)
                    {
                        *OutErrorMessage =
                            "Failed to construct component '" + ComponentTypeName + "'.";
                    }
                    return false;
                }

                Actor->AddOwnedComponent(Component, false);
            }

            if (!bKnownComponentType)
            {
                UE_LOG(SceneIO, ELogVerbosity::Error,
                       "Unknown component type '%s' restored as placeholder.",
                       ComponentTypeName.c_str());
            }

            if (Component->IsA(Engine::Component::UUnknownComponent::GetClass()))
            {
                auto* UnknownComponent =
                    static_cast<Engine::Component::UUnknownComponent*>(Component);
                UnknownComponent->SetOriginalTypeName(ComponentTypeName);
                UnknownComponent->SetSerializedPayload(
                    FSceneJsonWriter::Write(ComponentValue, true));
            }

            if (!ApplyComponentJson(*ComponentObject, *Component, OutErrorMessage))
            {
                return false;
            }

            if (bIsRootComponent)
            {
                Actor->SetRootComponent(Component);
            }
        }

        if (Actor->GetRootComponent() == nullptr && !Actor->GetOwnedComponents().empty())
        {
            Actor->SetRootComponent(Actor->GetOwnedComponents().front());
        }

        OutScene.AddActor(Actor.release());
        return true;
    }
} // namespace

bool FSceneSerializer::Serialize(const FScene& Scene, FString& OutJson,
                                 FString* OutErrorMessage)
{
    (void)OutErrorMessage;

    FSceneJsonValue::Object RootObject;
    RootObject["schema"] = SceneSchemaName;
    RootObject["version"] = static_cast<double>(SceneSchemaVersion);

    FSceneJsonValue::Array ActorsArray;
    const TArray<AActor*>& Actors = Scene.GetActors();
    ActorsArray.reserve(Actors.size());
    for (AActor* Actor : Actors)
    {
        if (Actor != nullptr)
        {
            ActorsArray.push_back(SerializeActor(*Actor));
        }
    }

    RootObject["actors"] = std::move(ActorsArray);
    OutJson = FSceneJsonWriter::Write(FSceneJsonValue(std::move(RootObject)), true);
    return true;
}

bool FSceneSerializer::SaveToFile(const FScene& Scene, const std::filesystem::path& FilePath,
                                  FString* OutErrorMessage)
{
    FString JsonText;
    if (!Serialize(Scene, JsonText, OutErrorMessage))
    {
        return false;
    }

    std::error_code ErrorCode;
    std::filesystem::create_directories(FilePath.parent_path(), ErrorCode);

    std::ofstream File(FilePath, std::ios::binary | std::ios::trunc);
    if (!File.is_open())
    {
        if (OutErrorMessage != nullptr)
        {
            *OutErrorMessage = "Failed to open scene file for writing.";
        }
        return false;
    }

    File.write(JsonText.data(), static_cast<std::streamsize>(JsonText.size()));
    if (!File.good())
    {
        if (OutErrorMessage != nullptr)
        {
            *OutErrorMessage = "Failed to write scene file.";
        }
        return false;
    }

    return true;
}

std::unique_ptr<FScene> FSceneDeserializer::Deserialize(const FString& JsonSource,
                                                        FString* OutErrorMessage)
{
    FSceneJsonValue RootValue;
    if (!FSceneJsonParser::Parse(JsonSource, RootValue, OutErrorMessage))
    {
        return nullptr;
    }

    const FSceneJsonValue::Object* RootObject = nullptr;
    if (!TryGetObject(RootValue, RootObject, OutErrorMessage, "Scene"))
    {
        return nullptr;
    }

    FString SchemaName;
    if (!TryReadStringField(*RootObject, "schema", SchemaName, OutErrorMessage, "Scene"))
    {
        return nullptr;
    }

    if (SchemaName != SceneSchemaName)
    {
        if (OutErrorMessage != nullptr)
        {
            *OutErrorMessage = "Unsupported scene schema '" + SchemaName + "'.";
        }
        return nullptr;
    }

    int32 Version = 0;
    if (!TryReadIntField(*RootObject, "version", Version, OutErrorMessage, "Scene"))
    {
        return nullptr;
    }

    if (Version != SceneSchemaVersion)
    {
        if (OutErrorMessage != nullptr)
        {
            *OutErrorMessage = "Unsupported scene version '" + std::to_string(Version) + "'.";
        }
        return nullptr;
    }

    const FSceneJsonValue* ActorsValue =
        FindRequiredField(*RootObject, "actors", OutErrorMessage, "Scene");
    if (ActorsValue == nullptr)
    {
        return nullptr;
    }

    const FSceneJsonValue::Array* ActorsArray = nullptr;
    if (!TryGetArray(*ActorsValue, ActorsArray, OutErrorMessage, "Scene.actors"))
    {
        return nullptr;
    }

    std::unique_ptr<FScene> Scene = std::make_unique<FScene>();
    for (const FSceneJsonValue& ActorValue : *ActorsArray)
    {
        FString ActorErrorMessage;
        if (!DeserializeActorValue(ActorValue, *Scene, &ActorErrorMessage))
        {
            if (OutErrorMessage != nullptr)
            {
                *OutErrorMessage = ActorErrorMessage;
            }
            return nullptr;
        }
    }

    return Scene;
}

std::unique_ptr<FScene> FSceneDeserializer::LoadFromFile(
    const std::filesystem::path& FilePath, FString* OutErrorMessage)
{
    std::ifstream File(FilePath, std::ios::binary);
    if (!File.is_open())
    {
        if (OutErrorMessage != nullptr)
        {
            *OutErrorMessage = "Failed to open scene file.";
        }
        return nullptr;
    }

    FString JsonSource((std::istreambuf_iterator<char>(File)), std::istreambuf_iterator<char>());
    if (JsonSource.empty() && File.bad())
    {
        if (OutErrorMessage != nullptr)
        {
            *OutErrorMessage = "Failed to read scene file.";
        }
        return nullptr;
    }

    return Deserialize(JsonSource, OutErrorMessage);
}
