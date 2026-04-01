#include "SceneSerializer.h"

#include "Engine/ViewPort/ViewportCameraState.h"
#include "SceneAssetPath.h"
#include "SceneJson.h"
#include "SceneTypeRegistry.h"
#include "Engine/Component/Core/ComponentProperty.h"
#include "Engine/Component/Core/SceneComponent.h"
#include "Engine/Component/Core/UnknownComponent.h"
#include "Engine/Game/Actor.h"
#include "Engine/Game/UnknownActor.h"
#include "Engine/Scene.h"
#include "Core/Math/Vector4.h"

#include <fstream>

namespace
{
    constexpr const char* SceneSchemaName = "JungleScene";
    constexpr int32       SceneSchemaVersion = 2;

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
                                             const FString& FieldName, FString* OutErrorMessage,
                                             const char* Context)
    {
        const auto Iterator = ObjectValue.find(FieldName);
        if (Iterator != ObjectValue.end())
        {
            return &Iterator->second;
        }

        if (OutErrorMessage != nullptr)
        {
            *OutErrorMessage = FString(Context) + " is missing required field '" + FieldName + "'.";
        }
        return nullptr;
    }

    bool TryReadStringField(const FSceneJsonValue::Object& ObjectValue, const FString& FieldName,
                            FString& OutValue, FString* OutErrorMessage, const char* Context)
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
                          bool& OutValue, FString* OutErrorMessage, const char* Context)
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
                          uint32& OutValue, FString* OutErrorMessage, const char* Context)
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

    bool TryReadOptionalUIntField(const FSceneJsonValue::Object& ObjectValue,
                                  const FString& FieldName, uint32& OutValue,
                                  bool& OutHasValue, FString* OutErrorMessage,
                                  const char* Context)
    {
        const auto Iterator = ObjectValue.find(FieldName);
        if (Iterator == ObjectValue.end())
        {
            OutHasValue = false;
            return true;
        }

        double NumberValue = 0.0;
        if (!Iterator->second.TryGetNumber(NumberValue) || NumberValue < 0.0)
        {
            if (OutErrorMessage != nullptr)
            {
                *OutErrorMessage =
                    FString(Context) + "." + FieldName + " must be a non-negative number.";
            }
            return false;
        }

        OutValue = static_cast<uint32>(NumberValue);
        OutHasValue = true;
        return true;
    }

    bool TryReadIntField(const FSceneJsonValue::Object& ObjectValue, const FString& FieldName,
                         int32& OutValue, FString* OutErrorMessage, const char* Context)
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

    bool TryReadIntValue(const FSceneJsonValue& Value, int32& OutValue, FString* OutErrorMessage,
                         const char* Context)
    {
        double NumberValue = 0.0;
        if (!Value.TryGetNumber(NumberValue))
        {
            if (OutErrorMessage != nullptr)
            {
                *OutErrorMessage = FString(Context) + " must be a number.";
            }
            return false;
        }

        OutValue = static_cast<int32>(NumberValue);
        return true;
    }

    bool TryReadFloatValue(const FSceneJsonValue& Value, float& OutValue,
                           FString* OutErrorMessage, const char* Context)
    {
        double NumberValue = 0.0;
        if (!Value.TryGetNumber(NumberValue))
        {
            if (OutErrorMessage != nullptr)
            {
                *OutErrorMessage = FString(Context) + " must be a number.";
            }
            return false;
        }

        OutValue = static_cast<float>(NumberValue);
        return true;
    }

    bool TryReadVector3(const FSceneJsonValue& Value, FVector& OutVector, FString* OutErrorMessage,
                        const char* Context)
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

    bool TryReadQuat(const FSceneJsonValue& Value, FQuat& OutQuat, FString* OutErrorMessage,
                     const char* Context)
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

    bool TryReadColor(const FSceneJsonValue& Value, FVector4& OutColor, FString* OutErrorMessage,
                      const char* Context)
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

    FSceneJsonValue SerializeComponentProperty(
        const Engine::Component::FComponentPropertyDescriptor& Descriptor)
    {
        using namespace Engine::Component;

        switch (Descriptor.Type)
        {
        case EComponentPropertyType::Bool:
            return FSceneJsonValue(Descriptor.BoolGetter ? Descriptor.BoolGetter() : false);

        case EComponentPropertyType::Int:
            return FSceneJsonValue(static_cast<double>(
                Descriptor.IntGetter ? Descriptor.IntGetter() : 0));

        case EComponentPropertyType::Float:
            return FSceneJsonValue(static_cast<double>(
                Descriptor.FloatGetter ? Descriptor.FloatGetter() : 0.0f));

        case EComponentPropertyType::String:
            return FSceneJsonValue(Descriptor.StringGetter ? Descriptor.StringGetter() : FString{});

        case EComponentPropertyType::AssetPath:
        {
            const FString AssetPath =
                Descriptor.StringGetter ? Descriptor.StringGetter() : FString{};
            return FSceneJsonValue(Engine::SceneIO::NormalizeSceneAssetPath(AssetPath));
        }

        case EComponentPropertyType::Vector3:
        {
            const FVector Value =
                Descriptor.VectorGetter ? Descriptor.VectorGetter() : FVector::ZeroVector;
            return MakeNumberArray({Value.X, Value.Y, Value.Z});
        }

        case EComponentPropertyType::Color:
        {
            const FColor Value =
                Descriptor.ColorGetter ? Descriptor.ColorGetter() : FColor::White();
            return MakeNumberArray({Value.r, Value.g, Value.b, Value.a});
        }
        }

        return {};
    }

    void BuildKnownComponentProperties(Engine::Component::USceneComponent& Component,
                                       FSceneJsonValue::Object& OutPropertiesObject)
    {
        Engine::Component::FComponentPropertyBuilder Builder;
        Component.DescribeProperties(Builder);

        for (const Engine::Component::FComponentPropertyDescriptor& Descriptor :
             Builder.GetProperties())
        {
            if (!Descriptor.bSerializeInScene || Descriptor.Key.empty())
            {
                continue;
            }

            OutPropertiesObject[Descriptor.Key] = SerializeComponentProperty(Descriptor);
        }
    }

    void LogComponentPropertyRestoreFailure(
        const Engine::Component::USceneComponent& Component, const FString& PropertyKey,
        const FString& ErrorMessage)
    {
        UE_LOG(SceneIO, ELogVerbosity::Error,
               "Failed to restore property '%s' on component '%s': %s",
               PropertyKey.c_str(), Component.GetTypeName(), ErrorMessage.c_str());
    }

    void ApplyKnownComponentProperties(const FSceneJsonValue::Object& PropertiesObject,
                                       Engine::Component::USceneComponent& Component)
    {
        using namespace Engine::Component;

        FComponentPropertyBuilder Builder;
        Component.DescribeProperties(Builder);

        for (const FComponentPropertyDescriptor& Descriptor : Builder.GetProperties())
        {
            if (!Descriptor.bSerializeInScene || Descriptor.Key.empty())
            {
                continue;
            }

            const auto PropertyIterator = PropertiesObject.find(Descriptor.Key);
            if (PropertyIterator == PropertiesObject.end())
            {
                continue;
            }

            const FSceneJsonValue& PropertyValue = PropertyIterator->second;
            FString PropertyError;
            const FString Context = "Component.properties." + Descriptor.Key;

            switch (Descriptor.Type)
            {
            case EComponentPropertyType::Bool:
            {
                bool Value = false;
                if (!PropertyValue.TryGetBool(Value))
                {
                    PropertyError = Context + " must be a bool.";
                    break;
                }

                if (Descriptor.BoolSetter)
                {
                    Descriptor.BoolSetter(Value);
                }
                continue;
            }

            case EComponentPropertyType::Int:
            {
                int32 Value = 0;
                if (!TryReadIntValue(PropertyValue, Value, &PropertyError, Context.c_str()))
                {
                    break;
                }

                if (Descriptor.IntSetter)
                {
                    Descriptor.IntSetter(Value);
                }
                continue;
            }

            case EComponentPropertyType::Float:
            {
                float Value = 0.0f;
                if (!TryReadFloatValue(PropertyValue, Value, &PropertyError, Context.c_str()))
                {
                    break;
                }

                if (Descriptor.FloatSetter)
                {
                    Descriptor.FloatSetter(Value);
                }
                continue;
            }

            case EComponentPropertyType::String:
            {
                FString Value;
                if (!PropertyValue.TryGetString(Value))
                {
                    PropertyError = Context + " must be a string.";
                    break;
                }

                if (Descriptor.StringSetter)
                {
                    Descriptor.StringSetter(Value);
                }
                continue;
            }

            case EComponentPropertyType::AssetPath:
            {
                FString Value;
                if (!PropertyValue.TryGetString(Value))
                {
                    PropertyError = Context + " must be a string.";
                    break;
                }

                if (Descriptor.StringSetter)
                {
                    Descriptor.StringSetter(Engine::SceneIO::NormalizeSceneAssetPath(Value));
                }
                continue;
            }

            case EComponentPropertyType::Vector3:
            {
                FVector Value = FVector::ZeroVector;
                if (!TryReadVector3(PropertyValue, Value, &PropertyError, Context.c_str()))
                {
                    break;
                }

                if (Descriptor.VectorSetter)
                {
                    Descriptor.VectorSetter(Value);
                }
                continue;
            }

            case EComponentPropertyType::Color:
            {
                FVector4 Value = FVector4::Zero();
                if (!TryReadColor(PropertyValue, Value, &PropertyError, Context.c_str()))
                {
                    break;
                }

                if (Descriptor.ColorSetter)
                {
                    Descriptor.ColorSetter(FColor(Value.X, Value.Y, Value.Z, Value.W));
                }
                continue;
            }
            }

            LogComponentPropertyRestoreFailure(Component, Descriptor.Key, PropertyError);
        }
    }

    FSceneJsonValue::Object
    BuildComponentObject(const AActor&                             OwnerActor,
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
        if (const Engine::Component::USceneComponent* ParentComponent = Component.GetAttachParent())
        {
            ComponentObject["parent_uuid"] = static_cast<double>(ParentComponent->UUID);
        }
        else
        {
            ComponentObject.erase("parent_uuid");
        }

        FSceneJsonValue::Object TransformObject;
        const FVector           Location = Component.GetRelativeLocation();
        const FQuat             Rotation = Component.GetRelativeQuaternion();
        const FVector           Scale = Component.GetRelativeScale3D();

        TransformObject["location"] = MakeNumberArray({Location.X, Location.Y, Location.Z});
        TransformObject["rotation_quat"] =
            MakeNumberArray({Rotation.X, Rotation.Y, Rotation.Z, Rotation.W});
        TransformObject["scale"] = MakeNumberArray({Scale.X, Scale.Y, Scale.Z});
        ComponentObject["transform"] = std::move(TransformObject);

        if (!Component.IsA(Engine::Component::UUnknownComponent::GetClass()))
        {
            FSceneJsonValue::Object PropertiesObject;
            BuildKnownComponentProperties(
                const_cast<Engine::Component::USceneComponent&>(Component), PropertiesObject);
            
            // 가상 함수 Serialize 호출 (저장)
            const_cast<Engine::Component::USceneComponent&>(Component).Serialize(false, &PropertiesObject);
            
            ComponentObject.erase("data");
            ComponentObject["properties"] = std::move(PropertiesObject);
        }
        return ComponentObject;
    }

    FSceneJsonValue SerializeComponent(const AActor&                             OwnerActor,
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
        const auto&            Components = Actor.GetOwnedComponents();
        ComponentsArray.reserve(Components.size());
        for (Engine::Component::USceneComponent* Component : Components)
        {
            if (Component != nullptr && Component->ShouldSerializeInScene())
            {
                ComponentsArray.push_back(SerializeComponent(Actor, *Component));
            }
        }

        ActorObject["components"] = std::move(ComponentsArray);
        return FSceneJsonValue(std::move(ActorObject));
    }

    bool ApplyComponentJson(const FSceneJsonValue::Object&      ComponentObject,
                            Engine::Component::USceneComponent& Component, FString* OutErrorMessage)
    {
        uint32 ComponentUuid = Component.UUID;
        if (!TryReadUIntField(ComponentObject, "uuid", ComponentUuid, OutErrorMessage, "Component"))
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
        if (!TryGetObject(*TransformValue, TransformObject, OutErrorMessage, "Component.transform"))
        {
            return false;
        }

        const FSceneJsonValue* LocationValue =
            FindRequiredField(*TransformObject, "location", OutErrorMessage, "Component.transform");
        const FSceneJsonValue* RotationValue = FindRequiredField(
            *TransformObject, "rotation_quat", OutErrorMessage, "Component.transform");
        const FSceneJsonValue* ScaleValue =
            FindRequiredField(*TransformObject, "scale", OutErrorMessage, "Component.transform");
        if (LocationValue == nullptr || RotationValue == nullptr || ScaleValue == nullptr)
        {
            return false;
        }

        FVector Location = FVector::ZeroVector;
        FVector Scale = FVector::OneVector;
        FQuat   Rotation = FQuat::Identity;
        if (!TryReadVector3(*LocationValue, Location, OutErrorMessage,
                            "Component.transform.location") ||
            !TryReadQuat(*RotationValue, Rotation, OutErrorMessage,
                         "Component.transform.rotation_quat") ||
            !TryReadVector3(*ScaleValue, Scale, OutErrorMessage, "Component.transform.scale"))
        {
            return false;
        }

        Component.SetRelativeLocation(Location);
        Component.SetRelativeRotation(Rotation);
        Component.SetRelativeScale3D(Scale);

        const auto PropertiesIterator = ComponentObject.find("properties");
        if (PropertiesIterator != ComponentObject.end())
        {
            if (const auto* PropertiesObject = PropertiesIterator->second.TryGetObject())
            {
                ApplyKnownComponentProperties(*PropertiesObject, Component);

                // 가상 함수 Serialize 호출 (로드)
                Component.Serialize(true, const_cast<FSceneJsonValue::Object*>(PropertiesObject));
            }
            else
            {
                UE_LOG(SceneIO, ELogVerbosity::Error,
                       "Component.properties on '%s' must be an object. Property restore was skipped.",
                       Component.GetTypeName());
            }
        }

        return true;
    }

    Engine::Component::USceneComponent*
    FindReusableComponent(const FString&                                     TypeName,
                          const TArray<Engine::Component::USceneComponent*>& ExistingComponents,
                          TArray<bool>&                                      ReusedFlags)
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
        struct FPendingComponentHierarchy
        {
            Engine::Component::USceneComponent* Component = nullptr;
            bool bIsRootComponent = false;
            bool bHasParentUuid = false;
            uint32 ParentUuid = 0;
        };

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

        bool                    bKnownActorType = false;
        std::unique_ptr<AActor> Actor(
            FSceneTypeRegistry::ConstructActor(ActorTypeName, &bKnownActorType));
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
        Actor->RefreshUUIDDebugComponent();

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

        const auto   ExistingComponents = Actor->GetOwnedComponents();
        TArray<bool> ReusedFlags(ExistingComponents.size(), false);
        TMap<uint32, Engine::Component::USceneComponent*> ComponentsByUuid;
        TArray<FPendingComponentHierarchy> PendingHierarchy;
        PendingHierarchy.reserve(ComponentsArray->size());

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
            if (!TryReadBoolField(*ComponentObject, "is_root", bIsRootComponent, OutErrorMessage,
                                  "Component"))
            {
                return false;
            }

            uint32 ParentUuid = 0;
            bool bHasParentUuid = false;
            if (!TryReadOptionalUIntField(*ComponentObject, "parent_uuid", ParentUuid,
                                          bHasParentUuid, OutErrorMessage, "Component"))
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

            ComponentsByUuid[Component->UUID] = Component;
            PendingHierarchy.push_back(FPendingComponentHierarchy{
                .Component = Component,
                .bIsRootComponent = bIsRootComponent,
                .bHasParentUuid = bHasParentUuid,
                .ParentUuid = ParentUuid});
        }

        for (const FPendingComponentHierarchy& Hierarchy : PendingHierarchy)
        {
            if (Hierarchy.Component != nullptr && Hierarchy.bIsRootComponent)
            {
                Actor->SetRootComponent(Hierarchy.Component);
            }
        }

        if (Actor->GetRootComponent() == nullptr && !Actor->GetOwnedComponents().empty())
        {
            Actor->SetRootComponent(Actor->GetOwnedComponents().front());
        }

        Engine::Component::USceneComponent* RootComponent = Actor->GetRootComponent();
        for (const FPendingComponentHierarchy& Hierarchy : PendingHierarchy)
        {
            Engine::Component::USceneComponent* Component = Hierarchy.Component;
            if (Component == nullptr || Component == RootComponent)
            {
                continue;
            }

            if (Hierarchy.bHasParentUuid)
            {
                const auto ParentIterator = ComponentsByUuid.find(Hierarchy.ParentUuid);
                if (ParentIterator != ComponentsByUuid.end() && ParentIterator->second != nullptr &&
                    ParentIterator->second != Component)
                {
                    Component->AttachToComponent(ParentIterator->second);
                    continue;
                }
            }

            if (RootComponent != nullptr)
            {
                Component->AttachToComponent(RootComponent);
            }
            else
            {
                Component->DetachFromParent();
            }
        }

        OutScene.AddActor(Actor.release());
        return true;
    }
} // namespace

bool FSceneSerializer::Serialize(const FScene& Scene, const FViewportCameraState& CameraState,
                                 FString& OutJson, FString* OutErrorMessage)
{
    (void)OutErrorMessage;

    FSceneJsonValue::Object RootObject;
    RootObject["schema"] = SceneSchemaName;
    RootObject["version"] = static_cast<double>(SceneSchemaVersion);

    FSceneJsonValue::Object CameraObject;
    CameraObject["Location"] =
        MakeNumberArray({CameraState.Location.X, CameraState.Location.Y, CameraState.Location.Z});
    CameraObject["Rotation"] = MakeNumberArray(
        {CameraState.Rotation.X, CameraState.Rotation.Y, CameraState.Rotation.Z,
         CameraState.Rotation.W});
    CameraObject["FOV"]          = static_cast<double>(CameraState.FOV);
    CameraObject["NearClip"]   = static_cast<double>(CameraState.NearPlane);
    CameraObject["FarClip"]    = static_cast<double>(CameraState.FarPlane);
    RootObject["PerspectiveCamera"] = std::move(CameraObject);

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

bool FSceneSerializer::SaveToFile(const FScene& Scene, const FViewportCameraState& CameraState,
                                  const std::filesystem::path& FilePath,
                                  FString*                     OutErrorMessage)
{
    FString JsonText;
    if (!Serialize(Scene, CameraState, JsonText, OutErrorMessage))
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

std::unique_ptr<FScene> FSceneDeserializer::Deserialize(const FString&        JsonSource,
                                                        FViewportCameraState* OutCameraState,
                                                        FString*              OutErrorMessage)
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

    if (OutCameraState != nullptr)
    {
        const auto CameraIt = RootObject->find("PerspectiveCamera");
        if (CameraIt != RootObject->end())
        {
            const FSceneJsonValue::Object* CameraObject = nullptr;
            if (TryGetObject(CameraIt->second, CameraObject, nullptr, "PerspectiveCamera"))
            {
                const FSceneJsonValue* LocValue = FindRequiredField(*CameraObject, "Location", nullptr, "PerspectiveCamera");
                if (LocValue != nullptr)
                    TryReadVector3(*LocValue, OutCameraState->Location, nullptr, "PerspectiveCamera.Location");

                const FSceneJsonValue* RotValue = FindRequiredField(*CameraObject, "Rotation", nullptr, "PerspectiveCamera");
                if (RotValue != nullptr)
                    TryReadQuat(*RotValue, OutCameraState->Rotation, nullptr, "PerspectiveCamera.Rotation");

                float Val;
                const FSceneJsonValue* FovValue = FindRequiredField(*CameraObject, "FOV", nullptr, "PerspectiveCamera");
                if (FovValue != nullptr && TryReadFloatValue(*FovValue, Val, nullptr, "PerspectiveCamera.FOV"))
                    OutCameraState->FOV = Val;

                const FSceneJsonValue* NearValue = FindRequiredField(*CameraObject, "NearClip", nullptr, "PerspectiveCamera");
                if (NearValue != nullptr && TryReadFloatValue(*NearValue, Val, nullptr, "PerspectiveCamera.NearClip"))
                    OutCameraState->NearPlane = Val;

                const FSceneJsonValue* FarValue = FindRequiredField(*CameraObject, "FarClip", nullptr, "PerspectiveCamera");
                if (FarValue != nullptr && TryReadFloatValue(*FarValue, Val, nullptr, "PerspectiveCamera.FarClip"))
                    OutCameraState->FarPlane = Val;
            }
        }
    }

    return Scene;
}

std::unique_ptr<FScene> FSceneDeserializer::LoadFromFile(const std::filesystem::path& FilePath,
                                                         FViewportCameraState*        OutCameraState,
                                                         FString*                     OutErrorMessage)
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

    return Deserialize(JsonSource, OutCameraState, OutErrorMessage);
}
