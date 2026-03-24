#pragma once

#include "Core/CoreMinimal.h"

#include <functional>

namespace Engine::Component
{
    enum class EComponentAssetPathKind : uint8
    {
        Any,
        FontFile,
        TextureImage,
        SceneFile
    };

    enum class EComponentPropertyType : uint8
    {
        Bool,
        Int,
        Float,
        String,
        Vector3,
        Color,
        AssetPath
    };

    struct FComponentPropertyOptions
    {
        bool bExposeInDetails = true;
        bool bSerializeInScene = true;
        float DragSpeed = 0.1f;
        EComponentAssetPathKind ExpectedAssetPathKind = EComponentAssetPathKind::Any;
    };

    struct FComponentPropertyDescriptor
    {
        FString Key;
        FWString DisplayLabel;
        EComponentPropertyType Type = EComponentPropertyType::String;
        bool bExposeInDetails = true;
        bool bSerializeInScene = true;
        float DragSpeed = 0.1f;
        EComponentAssetPathKind ExpectedAssetPathKind = EComponentAssetPathKind::Any;

        std::function<bool()> BoolGetter;
        std::function<void(bool)> BoolSetter;

        std::function<int32()> IntGetter;
        std::function<void(int32)> IntSetter;

        std::function<float()> FloatGetter;
        std::function<void(float)> FloatSetter;

        std::function<FString()> StringGetter;
        std::function<void(const FString&)> StringSetter;

        std::function<FVector()> VectorGetter;
        std::function<void(const FVector&)> VectorSetter;

        std::function<FColor()> ColorGetter;
        std::function<void(const FColor&)> ColorSetter;
    };

    // 컴포넌트 작성자는 builder에 속성을 한 번만 등록하면
    // Details와 Scene 직렬화가 같은 정의를 함께 사용합니다.
    class FComponentPropertyBuilder
    {
      public:
        const TArray<FComponentPropertyDescriptor>& GetProperties() const { return Properties; }
        void Clear() { Properties.clear(); }

        void AddBool(const FString& Key, const FWString& DisplayLabel,
                     std::function<bool()> Getter, std::function<void(bool)> Setter,
                     const FComponentPropertyOptions& Options = {})
        {
            FComponentPropertyDescriptor Descriptor;
            Descriptor.Key = Key;
            Descriptor.DisplayLabel = DisplayLabel;
            Descriptor.Type = EComponentPropertyType::Bool;
            Descriptor.bExposeInDetails = Options.bExposeInDetails;
            Descriptor.bSerializeInScene = Options.bSerializeInScene;
            Descriptor.DragSpeed = Options.DragSpeed;
            Descriptor.BoolGetter = std::move(Getter);
            Descriptor.BoolSetter = std::move(Setter);
            Properties.push_back(std::move(Descriptor));
        }

        void AddInt(const FString& Key, const FWString& DisplayLabel,
                    std::function<int32()> Getter, std::function<void(int32)> Setter,
                    const FComponentPropertyOptions& Options = {})
        {
            FComponentPropertyDescriptor Descriptor;
            Descriptor.Key = Key;
            Descriptor.DisplayLabel = DisplayLabel;
            Descriptor.Type = EComponentPropertyType::Int;
            Descriptor.bExposeInDetails = Options.bExposeInDetails;
            Descriptor.bSerializeInScene = Options.bSerializeInScene;
            Descriptor.DragSpeed = Options.DragSpeed;
            Descriptor.IntGetter = std::move(Getter);
            Descriptor.IntSetter = std::move(Setter);
            Properties.push_back(std::move(Descriptor));
        }

        void AddFloat(const FString& Key, const FWString& DisplayLabel,
                      std::function<float()> Getter, std::function<void(float)> Setter,
                      const FComponentPropertyOptions& Options = {})
        {
            FComponentPropertyDescriptor Descriptor;
            Descriptor.Key = Key;
            Descriptor.DisplayLabel = DisplayLabel;
            Descriptor.Type = EComponentPropertyType::Float;
            Descriptor.bExposeInDetails = Options.bExposeInDetails;
            Descriptor.bSerializeInScene = Options.bSerializeInScene;
            Descriptor.DragSpeed = Options.DragSpeed;
            Descriptor.FloatGetter = std::move(Getter);
            Descriptor.FloatSetter = std::move(Setter);
            Properties.push_back(std::move(Descriptor));
        }

        void AddString(const FString& Key, const FWString& DisplayLabel,
                       std::function<FString()> Getter,
                       std::function<void(const FString&)> Setter,
                       const FComponentPropertyOptions& Options = {})
        {
            FComponentPropertyDescriptor Descriptor;
            Descriptor.Key = Key;
            Descriptor.DisplayLabel = DisplayLabel;
            Descriptor.Type = EComponentPropertyType::String;
            Descriptor.bExposeInDetails = Options.bExposeInDetails;
            Descriptor.bSerializeInScene = Options.bSerializeInScene;
            Descriptor.DragSpeed = Options.DragSpeed;
            Descriptor.StringGetter = std::move(Getter);
            Descriptor.StringSetter = std::move(Setter);
            Properties.push_back(std::move(Descriptor));
        }

        void AddVector3(const FString& Key, const FWString& DisplayLabel,
                        std::function<FVector()> Getter,
                        std::function<void(const FVector&)> Setter,
                        const FComponentPropertyOptions& Options = {})
        {
            FComponentPropertyDescriptor Descriptor;
            Descriptor.Key = Key;
            Descriptor.DisplayLabel = DisplayLabel;
            Descriptor.Type = EComponentPropertyType::Vector3;
            Descriptor.bExposeInDetails = Options.bExposeInDetails;
            Descriptor.bSerializeInScene = Options.bSerializeInScene;
            Descriptor.DragSpeed = Options.DragSpeed;
            Descriptor.VectorGetter = std::move(Getter);
            Descriptor.VectorSetter = std::move(Setter);
            Properties.push_back(std::move(Descriptor));
        }

        void AddColor(const FString& Key, const FWString& DisplayLabel,
                      std::function<FColor()> Getter,
                      std::function<void(const FColor&)> Setter,
                      const FComponentPropertyOptions& Options = {})
        {
            FComponentPropertyDescriptor Descriptor;
            Descriptor.Key = Key;
            Descriptor.DisplayLabel = DisplayLabel;
            Descriptor.Type = EComponentPropertyType::Color;
            Descriptor.bExposeInDetails = Options.bExposeInDetails;
            Descriptor.bSerializeInScene = Options.bSerializeInScene;
            Descriptor.DragSpeed = Options.DragSpeed;
            Descriptor.ColorGetter = std::move(Getter);
            Descriptor.ColorSetter = std::move(Setter);
            Properties.push_back(std::move(Descriptor));
        }

        void AddAssetPath(const FString& Key, const FWString& DisplayLabel,
                          std::function<FString()> Getter,
                          std::function<void(const FString&)> Setter,
                          const FComponentPropertyOptions& Options = {})
        {
            FComponentPropertyDescriptor Descriptor;
            Descriptor.Key = Key;
            Descriptor.DisplayLabel = DisplayLabel;
            Descriptor.Type = EComponentPropertyType::AssetPath;
            Descriptor.bExposeInDetails = Options.bExposeInDetails;
            Descriptor.bSerializeInScene = Options.bSerializeInScene;
            Descriptor.DragSpeed = Options.DragSpeed;
            Descriptor.ExpectedAssetPathKind = Options.ExpectedAssetPathKind;
            Descriptor.StringGetter = std::move(Getter);
            Descriptor.StringSetter = std::move(Setter);
            Properties.push_back(std::move(Descriptor));
        }

      private:
        TArray<FComponentPropertyDescriptor> Properties;
    };
} // namespace Engine::Component
